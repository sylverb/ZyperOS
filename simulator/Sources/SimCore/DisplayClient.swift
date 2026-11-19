// SPDX-License-Identifier: GPL-3.0-only
import CoreGraphics
import Darwin
import Foundation

/// The VM's screen from QEMU's D-Bus display (`-display dbus,p2p=yes`,
/// tools/qemu-run.sh), every frame the guest draws, through memory shared
/// with QEMU rather than copied over a socket.
///
/// The way in: a socket pair, one end handed to QEMU over its QMP socket
/// (`getfd`, then `add_client` with protocol `@dbus-display`), which makes
/// it a peer-to-peer D-Bus connection QEMU serves. On it, Console_0's
/// RegisterListener gets the end of a second pair, on which QEMU calls this
/// side as a display listener. Saying in its `Interfaces` property that it
/// takes org.qemu.Display1.Listener.Unix.Map, the listener gets ScanoutMap
/// (a descriptor of the memory QEMU keeps the scanout in, a file on macOS,
/// and its offset, size, stride and pixman format) whenever the guest flips
/// to another buffer, and UpdateMap (a rectangle, no pixels) whenever it
/// flushes one: the frame is read straight out of that memory. A QEMU that
/// cannot share sends Scanout and Update with the pixels instead, which are
/// taken too.
///
/// Off Linux, QEMU has no ivshmem device to share memory with a guest, and
/// its VNC server looks for changes on a timer of its own: this is the
/// shared memory QEMU offers on a Mac. docs/simulator.md says more.
public final class DBusDisplayClient {
    /// Called on the client's thread with every frame, BGRX in memory
    public var onFrame: ((CGImage) -> Void)?
    public private(set) var width = 0
    public private(set) var height = 0
    /// Frames read from memory shared with QEMU (ScanoutMap), not sent
    public private(set) var shared = false
    public private(set) var frames = 0

    private let qmpPath: String
    private var bus: DBusConnection?
    private var listener: DBusConnection?
    private var pixels = [UInt8]()
    private var map: (base: UnsafeMutableRawPointer, length: Int)?
    private var scanout: (data: UnsafeRawPointer, stride: Int)?
    private var loggedIn = DispatchSemaphore(value: 0)
    private var loginError: Error?

    public static let listenerInterfaces = ["org.qemu.Display1.Listener.Unix.Map"]

    public init(qmpPath: String) { self.qmpPath = qmpPath }

    /// ZYPEROS_SIM_DEBUG=1: each step of the way in, and each call of QEMU's
    static let debug = ProcessInfo.processInfo.environment["ZYPEROS_SIM_DEBUG"] != nil
    static func note(_ text: @autoclosure () -> String) {
        if debug { FileHandle.standardError.write(Data(("dbus display: " + text() + "\n").utf8)) }
    }

    /// Connects and registers as Console_0's listener; serve() then takes
    /// the frames
    public func connect(timeout: TimeInterval = 5) throws {
        let (busSocket, busPeer) = try FDSocket.pair()
        defer { Darwin.close(busPeer) }
        try QMP.addDisplayClient(qmpPath: qmpPath, fd: busPeer)
        DBusDisplayClient.note("QEMU took the connection (QMP add_client)")
        let bus = DBusConnection(socket: busSocket)
        self.bus = bus
        try bus.login()
        DBusDisplayClient.note("logged in")

        let (listenSocket, listenPeer) = try FDSocket.pair()
        defer { Darwin.close(listenPeer) }
        let listener = DBusConnection(socket: listenSocket)
        self.listener = listener
        // QEMU logs in on the listener's connection, and asks its first
        // questions, before it answers RegisterListener: this side's half of
        // that runs on a thread of its own from now
        Thread.detachNewThread { [weak self] in
            do { try listener.login() } catch { self?.loginError = error }
            DBusDisplayClient.note("the listener's connection: \(self?.loginError.map { "\($0)" } ?? "logged in")")
            self?.loggedIn.signal()
        }
        var w = DBusWriter()
        w.u32(0)
        _ = try bus.call(.call(path: "/org/qemu/Display1/Console_0", interface: "org.qemu.Display1.Console",
                               member: "RegisterListener", signature: "h", body: w.bytes, fds: [listenPeer]))
        DBusDisplayClient.note("Console_0 took the listener")
        guard loggedIn.wait(timeout: .now() + timeout) == .success else {
            throw DBusError.protocolError("QEMU did not log in on the listener's connection")
        }
        if let loginError { throw loginError }
    }

    /// Answers QEMU's calls until it closes the connection (the VM went):
    /// returns then, throws on anything else
    public func serve() throws {
        guard let listener else { throw DBusError.protocolError("not connected") }
        do {
            while true {
                try handle(try listener.socket.readMessage(), on: listener)
            }
        } catch SocketError.closed {
            return
        }
    }

    /// Serves an already logged-in listener connection (the tests' fake QEMU)
    public func serve(connection: DBusConnection) throws {
        listener = connection
        try serve()
    }

    public func close() {
        listener?.socket.close()
        bus?.socket.close()
        unmap()
    }

    deinit { close() }

    // ---- QEMU's calls ---------------------------------------------------------

    private func handle(_ m: DBusMessage, on c: DBusConnection) throws {
        guard m.type == .methodCall else {
            m.fds.forEach { Darwin.close($0) }
            return
        }
        var r = DBusReader(m.body)
        var answer: DBusMessage? = .reply(to: m)
        DBusDisplayClient.note("\(m.interface ?? "").\(m.member ?? "")(\(m.signature)) \(m.fds.count) fds")
        switch (m.interface ?? "", m.member ?? "") {
        case ("org.freedesktop.DBus.Properties", "GetAll"):
            let iface = try r.string()
            var w = DBusWriter()
            w.properties(iface == "org.qemu.Display1.Listener" ? [("Interfaces", DBusDisplayClient.listenerInterfaces)] : [])
            answer = .reply(to: m, signature: "a{sv}", body: w.bytes)
        case ("org.freedesktop.DBus.Properties", "Get"):
            let iface = try r.string(), name = try r.string()
            if iface == "org.qemu.Display1.Listener" && name == "Interfaces" {
                var w = DBusWriter()
                w.signature("as")
                w.array(elementAlignment: 4) { a in DBusDisplayClient.listenerInterfaces.forEach { a.string($0) } }
                answer = .reply(to: m, signature: "v", body: w.bytes)
            } else {
                answer = .error(to: m, name: "org.freedesktop.DBus.Error.UnknownProperty", text: "\(iface).\(name)")
            }
        case (_, "ScanoutMap"):
            let handle = Int(try r.u32())
            let offset = Int(try r.u32()), w = Int(try r.u32()), h = Int(try r.u32())
            let stride = Int(try r.u32()), format = try r.u32()
            let fd = handle < m.fds.count ? m.fds[handle] : -1
            defer { m.fds.forEach { Darwin.close($0) } }
            if let error = scanoutMap(fd: fd, offset: offset, width: w, height: h, stride: stride, format: format) {
                answer = .error(to: m, name: "org.qemu.Display1.Error.Failed", text: error)
            }
        case (_, "UpdateMap"):
            let x = Int(try r.i32()), y = Int(try r.i32()), w = Int(try r.i32()), h = Int(try r.i32())
            if let s = scanout { copy(from: s.data, stride: s.stride, x: x, y: y, w: w, h: h) }
            emit()
        case (_, "Scanout"):
            let w = Int(try r.u32()), h = Int(try r.u32()), stride = Int(try r.u32()), format = try r.u32()
            let data = try r.byteArray()
            unmap()
            shared = false
            if DBusDisplayClient.bgrx(format) {
                resize(w, h)
                data.withUnsafeBytes { copy(from: $0.baseAddress!, stride: stride, x: 0, y: 0, w: w, h: h) }
                emit()
            }
        case (_, "Update"):
            let x = Int(try r.i32()), y = Int(try r.i32()), w = Int(try r.i32()), h = Int(try r.i32())
            let stride = Int(try r.u32()), format = try r.u32()
            let data = try r.byteArray()
            if DBusDisplayClient.bgrx(format) {
                // The rectangle's own pixels, its first row at the data's start
                data.withUnsafeBytes { patch(from: $0.baseAddress!, stride: stride, x: x, y: y, w: w, h: h) }
                emit()
            }
        case ("org.freedesktop.DBus.Peer", "Ping"), (_, "Disable"), (_, "MouseSet"), (_, "CursorDefine"):
            break
        default:
            m.fds.forEach { Darwin.close($0) }
            answer = .error(to: m, name: "org.freedesktop.DBus.Error.UnknownMethod",
                            text: "\(m.interface ?? "").\(m.member ?? "")")
        }
        if m.expectsReply, let answer { try c.send(answer) }
    }

    /// x8r8g8b8 and a8r8g8b8 (pixman's codes): B G R X in memory, as a
    /// CGImage's 32-bit little-endian skip-first
    static func bgrx(_ format: UInt32) -> Bool { format == 0x2002_0888 || format == 0x2002_8888 }

    private func scanoutMap(fd: Int32, offset: Int, width w: Int, height h: Int, stride: Int, format: UInt32) -> String? {
        guard fd >= 0, w > 0, h > 0, stride >= w * 4 else { return "a scanout of \(w)x\(h), stride \(stride)" }
        guard DBusDisplayClient.bgrx(format) else { return String(format: "pixman format 0x%08x", format) }
        let length = offset + stride * h
        // A new mapping for every one: QEMU's files are unlinked, and the
        // number of one that went may come back as another's
        guard let base = mmap(nil, length, PROT_READ, MAP_SHARED, fd, 0), base != MAP_FAILED else {
            return "mmap: \(String(cString: strerror(errno)))"
        }
        unmap()
        map = (base, length)
        scanout = (UnsafeRawPointer(base) + offset, stride)
        shared = true
        resize(w, h)
        return nil
    }

    private func unmap() {
        if let map { munmap(map.base, map.length) }
        map = nil
        scanout = nil
    }

    private func resize(_ w: Int, _ h: Int) {
        guard w != width || h != height else { return }
        width = w
        height = h
        pixels = [UInt8](repeating: 0, count: w * h * 4)
    }

    /// The rectangle of a whole frame `src` (rows `stride` apart) into ours
    private func copy(from src: UnsafeRawPointer, stride: Int, x: Int, y: Int, w: Int, h: Int) {
        let x0 = max(0, x), y0 = max(0, y), x1 = min(width, x + w), y1 = min(height, y + h)
        guard x1 > x0, y1 > y0 else { return }
        pixels.withUnsafeMutableBytes { dst in
            for row in y0..<y1 {
                memcpy(dst.baseAddress! + (row * width + x0) * 4, src + row * stride + x0 * 4, (x1 - x0) * 4)
            }
        }
    }

    /// A rectangle's own pixels (its first row at `src`) into ours at x, y
    private func patch(from src: UnsafeRawPointer, stride: Int, x: Int, y: Int, w: Int, h: Int) {
        guard x >= 0, y >= 0, x + w <= width, y + h <= height else { return }
        pixels.withUnsafeMutableBytes { dst in
            for row in 0..<h {
                memcpy(dst.baseAddress! + ((y + row) * width + x) * 4, src + row * stride, w * 4)
            }
        }
    }

    private func emit() {
        guard width > 0, height > 0, let provider = CGDataProvider(data: Data(pixels) as CFData),
              let image = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
                                  bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGBitmapInfo(rawValue: CGBitmapInfo.byteOrder32Little.rawValue |
                                                           CGImageAlphaInfo.noneSkipFirst.rawValue),
                                  provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent)
        else { return }
        frames += 1
        onFrame?(image)
    }
}

/// QEMU's monitor, for one thing: a D-Bus display client handed over
enum QMP {
    static func addDisplayClient(qmpPath: String, fd: Int32) throws {
        let s = try FDSocket(connectingTo: qmpPath)
        defer { s.close() }
        _ = try s.readLine()                                 // {"QMP": {...}}
        try s.send("{\"execute\":\"qmp_capabilities\"}\n")
        try answer(s)
        try s.send(Array("{\"execute\":\"getfd\",\"arguments\":{\"fdname\":\"zyperos-simulator\"}}\n".utf8), fds: [fd])
        try answer(s)
        try s.send("{\"execute\":\"add_client\",\"arguments\":{\"protocol\":\"@dbus-display\",\"fdname\":\"zyperos-simulator\"}}\n")
        try answer(s)
    }

    /// The command's answer; events on the way go by
    private static func answer(_ s: FDSocket) throws {
        while true {
            let line = try s.readLine()
            if line.contains("\"return\"") { return }
            if line.contains("\"error\"") { throw DBusError.protocolError("QMP: \(line)") }
        }
    }
}
