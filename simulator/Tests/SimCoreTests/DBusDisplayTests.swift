// SPDX-License-Identifier: GPL-3.0-only
import CoreGraphics
import Darwin
import XCTest
@testable import SimCore

/// The D-Bus display's path (DBus.swift, DisplayClient.swift): the wire
/// format both ways, and the listener against a small fake of QEMU on a
/// socket pair, which logs in as GLib's server does, asks the listener's
/// properties as QEMU's proxy does, and hands it a frame in shared memory
/// (ScanoutMap, UpdateMap) and one over the socket (Scanout)
final class DBusDisplayTests: XCTestCase {
    func testMessageRoundTrip() throws {
        var w = DBusWriter()
        w.u32(7)
        w.string("hello")
        w.i32(-2)
        var m = DBusMessage.call(path: "/org/qemu/Display1/Console_0", interface: "org.qemu.Display1.Console",
                                 member: "RegisterListener", signature: "usi", body: w.bytes, fds: [-1])
        m.serial = 42
        m.flags = DBusMessage.noReplyExpected
        let bytes = m.encode()
        XCTAssertEqual(bytes[0], UInt8(ascii: "l"))
        XCTAssertEqual(try DBusMessage.length(of: bytes[...]), bytes.count)
        // The body starts on an 8-byte boundary, after the header's fields
        XCTAssertEqual((bytes.count - w.bytes.count) % 8, 0)
        let back = try DBusMessage.decode(bytes)
        XCTAssertEqual(back.type, .methodCall)
        XCTAssertEqual(back.serial, 42)
        XCTAssertFalse(back.expectsReply)
        XCTAssertEqual(back.path, "/org/qemu/Display1/Console_0")
        XCTAssertEqual(back.interface, "org.qemu.Display1.Console")
        XCTAssertEqual(back.member, "RegisterListener")
        XCTAssertEqual(back.signature, "usi")
        XCTAssertEqual(back.fds.count, 1)
        var r = DBusReader(back.body)
        XCTAssertEqual(try r.u32(), 7)
        XCTAssertEqual(try r.string(), "hello")
        XCTAssertEqual(try r.i32(), -2)
    }

    func testPropertiesAndSkip() throws {
        var w = DBusWriter()
        w.byte(1)                                   // a byte first, so the array's padding shows
        w.properties([("Interfaces", ["org.qemu.Display1.Listener.Unix.Map", "x"]), ("Empty", [])])
        w.u32(0xABCD)
        var r = DBusReader(w.bytes)
        XCTAssertEqual(try r.byte(), 1)
        try r.skip("a{sv}")
        XCTAssertEqual(try r.u32(), 0xABCD)
        XCTAssertEqual(r.pos, w.bytes.count)
    }

    /// The fake QEMU's side of one connection: the login GLib's server
    /// answers, then messages
    private func fakeLogin(_ s: FDSocket) throws {
        let auth = try s.readLine()
        XCTAssertTrue(auth.hasPrefix("\0AUTH EXTERNAL "), auth)
        let hex = String(getuid()).utf8.map { String(format: "%02x", $0) }.joined()
        XCTAssertTrue(auth.hasSuffix(hex), auth)
        try s.send("OK 0123456789abcdef0123456789abcdef\r\n")
        XCTAssertEqual(try s.readLine(), "NEGOTIATE_UNIX_FD")
        try s.send("AGREE_UNIX_FD\r\n")
        XCTAssertEqual(try s.readLine(), "BEGIN")
    }

    private func call(_ s: FDSocket, serial: UInt32, _ member: String, iface: String = "org.qemu.Display1.Listener.Unix.Map",
                      _ sig: String = "", _ body: [UInt8] = [], fds: [Int32] = [], reply: Bool = true) throws -> DBusMessage? {
        var m = DBusMessage.call(path: "/org/qemu/Display1/Listener", interface: iface, member: member,
                                 signature: sig, body: body, fds: fds)
        m.serial = serial
        if !reply { m.flags = DBusMessage.noReplyExpected }
        try s.send(m.encode(), fds: fds)
        guard reply else { return nil }
        let answer = try s.readMessage()
        XCTAssertEqual(answer.replySerial, serial)
        return answer
    }

    func testListenerAgainstFakeQEMU() throws {
        let (qemu, peer) = try FDSocket.pair()
        let client = DBusDisplayClient(qmpPath: "/nonexistent")
        var images = [CGImage]()
        let got = expectation(description: "two frames")
        got.expectedFulfillmentCount = 2
        client.onFrame = { images.append($0); got.fulfill() }
        let done = expectation(description: "served until QEMU closed")
        Thread.detachNewThread {
            defer { done.fulfill() }
            let c = DBusConnection(socket: FDSocket(fd: peer))
            do {
                try c.login()
                try client.serve(connection: c)
            } catch { XCTFail("\(error)") }
        }
        try fakeLogin(qemu)

        // QEMU's proxy reads the listener's properties first
        var w = DBusWriter()
        w.string("org.qemu.Display1.Listener")
        let props = try XCTUnwrap(try call(qemu, serial: 1, "GetAll", iface: "org.freedesktop.DBus.Properties", "s", w.bytes))
        XCTAssertEqual(props.type, .methodReturn)
        XCTAssertEqual(props.signature, "a{sv}")
        var r = DBusReader(props.body)
        _ = try r.u32()                             // the array's length
        r.align(8)
        XCTAssertEqual(try r.string(), "Interfaces")
        XCTAssertEqual(try r.signature(), "as")
        _ = try r.u32()
        XCTAssertEqual(try r.string(), "org.qemu.Display1.Listener.Unix.Map")

        // A 4x2 scanout in a file, 64 bytes into it, rows 32 bytes apart:
        // pixel (x, y) is B=x, G=y, R=0x80
        let path = NSTemporaryDirectory() + "dbus-display-test-\(getpid())"
        let fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0o600)
        XCTAssertGreaterThanOrEqual(fd, 0)
        unlink(path)
        var file = [UInt8](repeating: 0xEE, count: 64 + 32 * 2)
        for y in 0..<2 { for x in 0..<4 { file.replaceSubrange((64 + y * 32 + x * 4)..<(64 + y * 32 + x * 4 + 4), with: [UInt8(x), UInt8(y), 0x80, 0]) } }
        XCTAssertEqual(write(fd, file, file.count), file.count)
        w = DBusWriter()
        [0, 64, 4, 2, 32, 0x2002_0888].forEach { w.u32(UInt32($0)) }
        let mapped = try XCTUnwrap(try call(qemu, serial: 2, "ScanoutMap", "huuuuu", w.bytes, fds: [fd]))
        close(fd)
        XCTAssertEqual(mapped.type, .methodReturn)
        w = DBusWriter()
        [0, 0, 4, 2].forEach { w.i32(Int32($0)) }
        _ = try call(qemu, serial: 3, "UpdateMap", "iiii", w.bytes, reply: false)

        // A Scanout with its pixels, 2x1, green then blue
        w = DBusWriter()
        [2, 1, 8, 0x2002_0888].forEach { w.u32(UInt32($0)) }
        w.u32(8)
        [0, 0xFF, 0, 0, 0xFF, 0, 0, 0].forEach { w.byte(UInt8($0)) }
        _ = try call(qemu, serial: 4, "Scanout", iface: "org.qemu.Display1.Listener", "uuuuay", w.bytes, reply: false)

        wait(for: [got], timeout: 5)
        qemu.close()
        wait(for: [done], timeout: 5)
        XCTAssertEqual(images.count, 2)
        XCTAssertEqual(images.first?.width, 4)
        XCTAssertEqual(images.first?.height, 2)
        let px = Array(try XCTUnwrap(images.first?.dataProvider?.data as Data?))
        XCTAssertEqual(Array(px[(1 * 4 + 3) * 4..<(1 * 4 + 3) * 4 + 3]), [3, 1, 0x80], "pixel (3, 1) from the mapped file")
        XCTAssertEqual(images.last?.width, 2)
        let px2 = Array(try XCTUnwrap(images.last?.dataProvider?.data as Data?))
        XCTAssertEqual(Array(px2[4..<7]), [0xFF, 0, 0], "the Scanout's second pixel")
        XCTAssertFalse(client.shared, "a Scanout's pixels came over the socket")
    }
}
