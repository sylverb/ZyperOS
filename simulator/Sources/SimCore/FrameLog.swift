// SPDX-License-Identifier: GPL-3.0-only
import CoreGraphics
import Foundation

/// The frames as they came from the VM, timed where they came (the screen
/// client's thread, before the window draws them): how many a second the
/// window gets, and the first one a press changed (simctl fps, simctl
/// latency). Thread-safe.
public final class FrameLog {
    private let lock = NSLock()
    private var times = [Double]()
    private var recent = [(Double, CGImage)]()

    public init() {}

    public static func now() -> Double {
        Double(DispatchTime.now().uptimeNanoseconds) / 1e6
    }

    public func add(_ image: CGImage, at time: Double = FrameLog.now()) {
        lock.lock()
        defer { lock.unlock() }
        times.append(time)
        if times.count > 512 { times.removeFirst(times.count - 512) }
        recent.append((time, image))
        if recent.count > 120 { recent.removeFirst(recent.count - 120) }
    }

    /// Frames in the second before `now`
    public func perSecond(now: Double = FrameLog.now()) -> Int {
        lock.lock()
        defer { lock.unlock() }
        return times.filter { now - $0 <= 1000 }.count
    }

    /// The frames that came after `time`, oldest first
    public func frames(after time: Double) -> [(Double, CGImage)] {
        lock.lock()
        defer { lock.unlock() }
        return recent.filter { $0.0 > time }
    }

    /// A rectangle of an image's pixels, as one number: equal for equal
    /// pixels (FNV-1a over the rows); the whole image when `rect` is nil
    public static func hash(_ image: CGImage, rect: CGRect? = nil) -> UInt64 {
        guard let data = image.dataProvider?.data as Data? else { return 0 }
        let bpr = image.bytesPerRow, bpp = max(1, image.bitsPerPixel / 8)
        let r = (rect ?? CGRect(x: 0, y: 0, width: image.width, height: image.height))
            .intersection(CGRect(x: 0, y: 0, width: image.width, height: image.height))
        guard !r.isEmpty else { return 0 }
        var h: UInt64 = 0xcbf2_9ce4_8422_2325
        data.withUnsafeBytes { raw in
            let p = raw.bindMemory(to: UInt8.self)
            for y in Int(r.minY)..<Int(r.maxY) {
                let row = y * bpr
                for i in (row + Int(r.minX) * bpp)..<(row + Int(r.maxX) * bpp) where i < p.count {
                    h = (h ^ UInt64(p[i])) &* 0x100_0000_01b3
                }
            }
        }
        return h
    }
}
