// SPDX-License-Identifier: GPL-3.0-only
import Darwin
import Foundation

/// D-Bus, as much of it as QEMU's D-Bus display needs, peer to peer (no bus
/// daemon): the wire format (little endian), a Unix socket that carries file
/// descriptors with its bytes, and the client's side of the EXTERNAL login.
/// https://dbus.freedesktop.org/doc/dbus-specification.html

public enum DBusError: Error, CustomStringConvertible {
    case protocolError(String)
    case remote(String, String)

    public var description: String {
        switch self {
        case let .protocolError(text): return "D-Bus: \(text)"
        case let .remote(name, text): return "D-Bus: \(name): \(text)"
        }
    }
}

public enum DBusType: UInt8 {
    case methodCall = 1, methodReturn, error, signal
}

public struct DBusMessage {
    public var type: DBusType
    public var flags: UInt8 = 0
    public var serial: UInt32 = 0
    public var path: String?
    public var interface: String?
    public var member: String?
    public var errorName: String?
    public var replySerial: UInt32?
    public var destination: String?
    public var sender: String?
    public var signature = ""
    public var body: [UInt8] = []
    /// The descriptors the message carries, in order (UNIX_FDS); a body's
    /// `h` is an index into them
    public var fds: [Int32] = []

    public static let noReplyExpected: UInt8 = 1

    public init(type: DBusType) { self.type = type }

    public var expectsReply: Bool { type == .methodCall && flags & DBusMessage.noReplyExpected == 0 }

    public static func call(path: String, interface: String, member: String,
                            signature: String = "", body: [UInt8] = [], fds: [Int32] = []) -> DBusMessage {
        var m = DBusMessage(type: .methodCall)
        m.path = path
        m.interface = interface
        m.member = member
        m.signature = signature
        m.body = body
        m.fds = fds
        return m
    }

    /// The answer to `call`, empty or with a body
    public static func reply(to call: DBusMessage, signature: String = "", body: [UInt8] = []) -> DBusMessage {
        var m = DBusMessage(type: .methodReturn)
        m.replySerial = call.serial
        m.destination = call.sender
        m.signature = signature
        m.body = body
        return m
    }

    public static func error(to call: DBusMessage, name: String, text: String) -> DBusMessage {
        var m = DBusMessage(type: .error)
        m.replySerial = call.serial
        m.destination = call.sender
        m.errorName = name
        var w = DBusWriter()
        w.string(text)
        m.signature = "s"
        m.body = w.bytes
        return m
    }

    /// The message as it goes on the wire
    public func encode() -> [UInt8] {
        var h = DBusWriter()
        h.byte(UInt8(ascii: "l"))
        h.byte(type.rawValue)
        h.byte(flags)
        h.byte(1)
        h.u32(UInt32(body.count))
        h.u32(serial)
        h.array(elementAlignment: 8) { w in
            func field(_ code: UInt8, _ sig: String, _ put: (inout DBusWriter) -> Void) {
                w.align(8)
                w.byte(code)
                w.signature(sig)
                put(&w)
            }
            if let path { field(1, "o") { $0.string(path) } }
            if let interface { field(2, "s") { $0.string(interface) } }
            if let member { field(3, "s") { $0.string(member) } }
            if let errorName { field(4, "s") { $0.string(errorName) } }
            if let replySerial { field(5, "u") { $0.u32(replySerial) } }
            if let destination { field(6, "s") { $0.string(destination) } }
            if let sender { field(7, "s") { $0.string(sender) } }
            if !signature.isEmpty { field(8, "g") { $0.signature(signature) } }
            if !fds.isEmpty { field(9, "u") { $0.u32(UInt32(fds.count)) } }
        }
        h.align(8)
        return h.bytes + body
    }

    /// The length of the whole message at the start of `bytes`, once its
    /// fixed header (16 bytes) is there; nil before
    static func length(of bytes: ArraySlice<UInt8>) throws -> Int? {
        guard bytes.count >= 16 else { return nil }
        let b = bytes.startIndex
        guard bytes[b] == UInt8(ascii: "l") else {
            throw DBusError.protocolError("a big-endian message, which this side does not read")
        }
        func u32(_ at: Int) -> Int {
            Int(bytes[b + at]) | Int(bytes[b + at + 1]) << 8 | Int(bytes[b + at + 2]) << 16 | Int(bytes[b + at + 3]) << 24
        }
        let fields = u32(12), body = u32(4)
        guard fields < 1 << 26, body < 1 << 27 else { throw DBusError.protocolError("a message too large") }
        return (16 + fields + 7) / 8 * 8 + body
    }

    /// One whole message, as `length` measured it
    public static func decode(_ bytes: [UInt8]) throws -> DBusMessage {
        guard bytes.count >= 16, let type = DBusType(rawValue: bytes[1]) else {
            throw DBusError.protocolError("not a message")
        }
        var m = DBusMessage(type: type)
        var r = DBusReader(bytes)
        r.pos = 2
        m.flags = try r.byte()
        _ = try r.byte()
        let bodyLength = Int(try r.u32())
        m.serial = try r.u32()
        let fieldsEnd = Int(try r.u32()) + 16
        r.align(8)
        while r.pos < fieldsEnd {
            r.align(8)
            let code = try r.byte()
            let sig = try r.signature()
            switch (code, sig) {
            case (1, "o"): m.path = try r.string()
            case (2, "s"): m.interface = try r.string()
            case (3, "s"): m.member = try r.string()
            case (4, "s"): m.errorName = try r.string()
            case (5, "u"): m.replySerial = try r.u32()
            case (6, "s"): m.destination = try r.string()
            case (7, "s"): m.sender = try r.string()
            case (8, "g"): m.signature = try r.signature()
            case (9, "u"): m.fds = Array(repeating: -1, count: Int(try r.u32()))
            default: try r.skip(sig)
            }
        }
        r.align(8)
        guard r.pos + bodyLength <= bytes.count else { throw DBusError.protocolError("a body cut short") }
        m.body = Array(bytes[r.pos..<(r.pos + bodyLength)])
        return m
    }
}

/// Values into a message, aligned from its start (a body starts on 8)
public struct DBusWriter {
    public private(set) var bytes: [UInt8] = []

    public init() {}

    public mutating func align(_ n: Int) {
        while bytes.count % n != 0 { bytes.append(0) }
    }

    public mutating func byte(_ v: UInt8) { bytes.append(v) }

    public mutating func u32(_ v: UInt32) {
        align(4)
        withUnsafeBytes(of: v.littleEndian) { bytes += $0 }
    }

    public mutating func i32(_ v: Int32) { u32(UInt32(bitPattern: v)) }

    public mutating func bool(_ v: Bool) { u32(v ? 1 : 0) }

    public mutating func string(_ s: String) {
        let utf8 = Array(s.utf8)
        u32(UInt32(utf8.count))
        bytes += utf8
        bytes.append(0)
    }

    public mutating func signature(_ s: String) {
        let utf8 = Array(s.utf8)
        bytes.append(UInt8(utf8.count))
        bytes += utf8
        bytes.append(0)
    }

    /// An array: its length, then the elements `body` writes
    public mutating func array(elementAlignment: Int, _ body: (inout DBusWriter) -> Void) {
        align(4)
        let at = bytes.count
        u32(0)
        align(elementAlignment)
        let start = bytes.count
        body(&self)
        let length = UInt32(bytes.count - start).littleEndian
        withUnsafeBytes(of: length) { raw in
            for i in 0..<4 { bytes[at + i] = raw[i] }
        }
    }

    /// a{sv} with string arrays as values, all a listener's properties need
    public mutating func properties(_ values: [(String, [String])]) {
        array(elementAlignment: 8) { w in
            for (key, list) in values {
                w.align(8)
                w.string(key)
                w.signature("as")
                w.array(elementAlignment: 4) { a in list.forEach { a.string($0) } }
            }
        }
    }
}

/// Values out of a message
public struct DBusReader {
    let bytes: [UInt8]
    public var pos = 0

    public init(_ bytes: [UInt8]) { self.bytes = bytes }

    mutating func need(_ n: Int) throws {
        guard pos + n <= bytes.count else { throw DBusError.protocolError("a value past the message's end") }
    }

    public mutating func align(_ n: Int) { pos = (pos + n - 1) / n * n }

    public mutating func byte() throws -> UInt8 {
        try need(1)
        defer { pos += 1 }
        return bytes[pos]
    }

    public mutating func u32() throws -> UInt32 {
        align(4)
        try need(4)
        defer { pos += 4 }
        return UInt32(bytes[pos]) | UInt32(bytes[pos + 1]) << 8 | UInt32(bytes[pos + 2]) << 16 | UInt32(bytes[pos + 3]) << 24
    }

    public mutating func i32() throws -> Int32 { Int32(bitPattern: try u32()) }

    public mutating func string() throws -> String {
        let n = Int(try u32())
        try need(n + 1)
        defer { pos += n + 1 }
        return String(decoding: bytes[pos..<(pos + n)], as: UTF8.self)
    }

    public mutating func signature() throws -> String {
        let n = Int(try byte())
        try need(n + 1)
        defer { pos += n + 1 }
        return String(decoding: bytes[pos..<(pos + n)], as: UTF8.self)
    }

    /// ay, as bytes
    public mutating func byteArray() throws -> ArraySlice<UInt8> {
        let n = Int(try u32())
        try need(n)
        defer { pos += n }
        return bytes[pos..<(pos + n)]
    }

    /// Past one value of each complete type in `sig`
    public mutating func skip(_ sig: String) throws {
        var s = Substring(sig)
        while !s.isEmpty { try skipOne(&s) }
    }

    private mutating func skipOne(_ s: inout Substring) throws {
        guard let c = s.first else { return }
        s = s.dropFirst()
        switch c {
        case "y": _ = try byte()
        case "b", "i", "u", "h": _ = try u32()
        case "n", "q": align(2); try need(2); pos += 2
        case "x", "t", "d": align(8); try need(8); pos += 8
        case "s", "o": _ = try string()
        case "g": _ = try signature()
        case "v": try skip(try signature())
        case "a":
            let element = try DBusReader.completeType(&s)
            let n = Int(try u32())
            align(DBusReader.alignment(of: element.first ?? "y"))
            try need(n)
            pos += n
        case "(", "{":
            align(8)
            let close: Character = c == "(" ? ")" : "}"
            while let next = s.first, next != close { try skipOne(&s) }
            s = s.dropFirst()
        default:
            throw DBusError.protocolError("a type \(c) this side does not know")
        }
    }

    static func completeType(_ s: inout Substring) throws -> Substring {
        let start = s.startIndex
        try consumeOne(&s)
        return s.base[start..<s.startIndex]
    }

    private static func consumeOne(_ s: inout Substring) throws {
        guard let c = s.first else { throw DBusError.protocolError("a signature cut short") }
        s = s.dropFirst()
        switch c {
        case "a":
            try consumeOne(&s)
        case "(", "{":
            while let next = s.first, next != ")" && next != "}" { try consumeOne(&s) }
            guard !s.isEmpty else { throw DBusError.protocolError("a signature cut short") }
            s = s.dropFirst()
        default:
            break
        }
    }

    static func alignment(of c: Character) -> Int {
        switch c {
        case "y", "g", "v": return 1
        case "n", "q": return 2
        case "x", "t", "d", "(", "{": return 8
        default: return 4
        }
    }
}

/// A Unix stream socket that carries descriptors with its bytes
/// (SCM_RIGHTS): D-Bus's `h`, and QMP's getfd
public final class FDSocket {
    public let fd: Int32
    private var buffer = [UInt8]()
    private var received = [Int32]()
    private let sendLock = NSLock()
    private var closed = false

    public init(fd: Int32) {
        self.fd = fd
        var one: Int32 = 1
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, socklen_t(MemoryLayout<Int32>.size))
    }

    public convenience init(connectingTo path: String) throws {
        let fd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard fd >= 0 else { throw SocketError.system("socket", errno) }
        var addr = try UnixSocket.address(path)
        let result = withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.connect(fd, $0, socklen_t(MemoryLayout<sockaddr_un>.size))
            }
        }
        if result != 0 {
            let err = errno
            Darwin.close(fd)
            throw SocketError.system("connect \(path)", err)
        }
        self.init(fd: fd)
    }

    /// Two connected ends, neither listening: one for this side, the other
    /// handed to QEMU
    public static func pair() throws -> (FDSocket, Int32) {
        var fds: [Int32] = [-1, -1]
        guard socketpair(AF_UNIX, SOCK_STREAM, 0, &fds) == 0 else { throw SocketError.system("socketpair", errno) }
        return (FDSocket(fd: fds[0]), fds[1])
    }

    public func close() {
        sendLock.lock()
        defer { sendLock.unlock() }
        guard !closed else { return }
        closed = true
        shutdown(fd, SHUT_RDWR)
        Darwin.close(fd)
        received.forEach { Darwin.close($0) }
        received.removeAll()
    }

    deinit { close() }

    // ---- sending ------------------------------------------------------------

    public func send(_ bytes: [UInt8], fds: [Int32] = []) throws {
        sendLock.lock()
        defer { sendLock.unlock() }
        var done = 0
        var pending = fds
        while done < bytes.count {
            let n: Int = bytes.withUnsafeBytes { raw -> Int in
                var iov = iovec(iov_base: UnsafeMutableRawPointer(mutating: raw.baseAddress! + done),
                                iov_len: bytes.count - done)
                return withUnsafeMutablePointer(to: &iov) { iovp -> Int in
                    var msg = msghdr()
                    msg.msg_iov = iovp
                    msg.msg_iovlen = 1
                    if pending.isEmpty { return sendmsg(fd, &msg, 0) }
                    // cmsghdr (12 bytes on Darwin) and the descriptors, 4-byte aligned
                    let dataLength = pending.count * 4
                    var control = [UInt8](repeating: 0, count: 12 + dataLength)
                    return control.withUnsafeMutableBytes { c -> Int in
                        c.storeBytes(of: socklen_t(12 + dataLength), toByteOffset: 0, as: socklen_t.self)
                        c.storeBytes(of: SOL_SOCKET, toByteOffset: 4, as: Int32.self)
                        c.storeBytes(of: SCM_RIGHTS, toByteOffset: 8, as: Int32.self)
                        for (i, d) in pending.enumerated() { c.storeBytes(of: d, toByteOffset: 12 + 4 * i, as: Int32.self) }
                        msg.msg_control = c.baseAddress
                        msg.msg_controllen = socklen_t(c.count)
                        return sendmsg(fd, &msg, 0)
                    }
                }
            }
            if n < 0 {
                if errno == EINTR { continue }
                throw SocketError.system("sendmsg", errno)
            }
            pending = []
            done += n
        }
    }

    public func send(_ text: String) throws { try send(Array(text.utf8)) }

    // ---- receiving ----------------------------------------------------------

    /// More bytes, and any descriptors with them, into the buffer
    private func fill() throws {
        var chunk = [UInt8](repeating: 0, count: 64 << 10)
        var control = [UInt8](repeating: 0, count: 512)
        let n: Int = chunk.withUnsafeMutableBytes { buf in
            control.withUnsafeMutableBytes { c in
                var iov = iovec(iov_base: buf.baseAddress, iov_len: buf.count)
                var msg = msghdr()
                return withUnsafeMutablePointer(to: &iov) { iovp in
                    msg.msg_iov = iovp
                    msg.msg_iovlen = 1
                    msg.msg_control = c.baseAddress
                    msg.msg_controllen = socklen_t(c.count)
                    var r: Int
                    repeat { r = recvmsg(fd, &msg, 0) } while r < 0 && errno == EINTR
                    if r > 0 {
                        var off = 0
                        let total = Int(msg.msg_controllen)
                        while off + 12 <= total {
                            let len = Int(c.load(fromByteOffset: off, as: socklen_t.self))
                            let level = c.load(fromByteOffset: off + 4, as: Int32.self)
                            let type = c.load(fromByteOffset: off + 8, as: Int32.self)
                            if len < 12 { break }
                            if level == SOL_SOCKET && type == SCM_RIGHTS {
                                for i in 0..<((len - 12) / 4) {
                                    received.append(c.load(fromByteOffset: off + 12 + 4 * i, as: Int32.self))
                                }
                            }
                            off += (len + 3) & ~3
                        }
                    }
                    return r
                }
            }
        }
        if n == 0 { throw SocketError.closed }
        if n < 0 { throw SocketError.system("recvmsg", errno) }
        buffer += chunk[0..<n]
    }

    /// One line, "\r\n" or "\n" taken off (the login's)
    public func readLine() throws -> String {
        while true {
            if let nl = buffer.firstIndex(of: 10) {
                var line = Array(buffer[..<nl])
                buffer.removeSubrange(...nl)
                if line.last == 13 { line.removeLast() }
                return String(decoding: line, as: UTF8.self)
            }
            try fill()
        }
    }

    /// The next whole message, with the descriptors it says it carries
    public func readMessage() throws -> DBusMessage {
        while true {
            if let length = try DBusMessage.length(of: buffer[...]), buffer.count >= length {
                var m = try DBusMessage.decode(Array(buffer[..<length]))
                buffer.removeSubrange(..<length)
                if !m.fds.isEmpty {
                    guard received.count >= m.fds.count else {
                        throw DBusError.protocolError("a message said \(m.fds.count) descriptors and fewer came")
                    }
                    m.fds = Array(received.prefix(m.fds.count))
                    received.removeFirst(m.fds.count)
                }
                return m
            }
            try fill()
        }
    }
}

/// One D-Bus connection, peer to peer, as the client of the login: QEMU
/// is the server of both of its display's connections
public final class DBusConnection {
    public let socket: FDSocket
    private var serial: UInt32 = 0
    private let lock = NSLock()

    public init(socket: FDSocket) { self.socket = socket }

    /// EXTERNAL, as this user, with descriptors on: what GLib's server takes
    public func login() throws {
        let uid = String(getuid()).utf8.map { String(format: "%02x", $0) }.joined()
        try socket.send([0] + Array("AUTH EXTERNAL \(uid)\r\n".utf8))
        let ok = try socket.readLine()
        guard ok.hasPrefix("OK ") else { throw DBusError.protocolError("the login was refused: \(ok)") }
        try socket.send("NEGOTIATE_UNIX_FD\r\n")
        let fds = try socket.readLine()
        guard fds == "AGREE_UNIX_FD" else { throw DBusError.protocolError("no descriptors on this connection: \(fds)") }
        try socket.send("BEGIN\r\n")
    }

    /// Sent with the next serial; the serial
    @discardableResult
    public func send(_ message: DBusMessage) throws -> UInt32 {
        lock.lock()
        serial &+= 1
        if serial == 0 { serial = 1 }
        var m = message
        m.serial = serial
        lock.unlock()
        try socket.send(m.encode(), fds: m.fds)
        return m.serial
    }

    /// A call and its answer, the messages between them dropped
    public func call(_ message: DBusMessage) throws -> DBusMessage {
        let s = try send(message)
        while true {
            let m = try socket.readMessage()
            m.fds.forEach { Darwin.close($0) }
            guard m.replySerial == s else { continue }
            if m.type == .error {
                var r = DBusReader(m.body)
                throw DBusError.remote(m.errorName ?? "error", (try? r.string()) ?? "")
            }
            return m
        }
    }
}
