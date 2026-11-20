// SPDX-License-Identifier: GPL-3.0-only
import AppKit
import SimCore
import SwiftUI

/// The window driven from a shell (simulator/simctl): a Unix socket in the
/// VM's folder, app.sock, one command a line (ControlCommand) and one JSON
/// line back. For checking the simulator, and ZyperOS in it, with no clicks.
final class ControlServer {
    private let path: String
    private weak var sim: Simulator?
    private var listener: UnixSocket?

    init(path: String, sim: Simulator) {
        self.path = path
        self.sim = sim
    }

    func start() {
        do {
            try FileManager.default.createDirectory(atPath: (path as NSString).deletingLastPathComponent,
                                                    withIntermediateDirectories: true)
            listener = try UnixSocket.listen(at: path)
        } catch {
            NSLog("ZyperOS Simulator: no control socket: \(error)")
            return
        }
        Thread.detachNewThread { [weak self] in
            while let self, let listener = self.listener, let client = try? listener.accept() {
                Thread.detachNewThread { self.serve(client) }
            }
        }
    }

    func stop() {
        listener?.close()
        unlink(path)
    }

    private func serve(_ client: UnixSocket) {
        var pending = [UInt8]()
        while let bytes = try? client.readSome(), !bytes.isEmpty {
            pending += bytes
            while let end = pending.firstIndex(of: 10) {
                let line = String(decoding: pending[..<end], as: UTF8.self)
                pending.removeSubrange(...end)
                try? client.write(answer(line) + "\n")
            }
        }
        client.close()
    }

    private func answer(_ line: String) -> String {
        guard let command = ControlCommand(line: line) else {
            return json(["ok": false, "error": "usage: \(ControlCommand.usage)"])
        }
        if case let .waitFrames(n) = command { return waitFrames(n) }
        if case let .latency(buttons, region) = command { return latency(buttons, region: region) }
        if case let .tap(buttons, ms) = command {
            onMain { $0.tap(buttons, milliseconds: ms) }
            Thread.sleep(forTimeInterval: Double(ms) / 1000 + 0.05)
            return json(["ok": true])
        }
        return DispatchQueue.main.sync { MainActor.assumeIsolated { run(command) } }
    }

    @MainActor
    private func run(_ command: ControlCommand) -> String {
        guard let sim else { return json(["ok": false, "error": "no simulator"]) }
        switch command {
        case .state:
            let s = sim.state
            return json(["ok": true, "name": sim.name, "device": sim.handheld.id, "power": sim.power.rawValue,
                         "ssh_port": sim.sshPort, "frames": sim.frameCount,
                         "screen_path": sim.screenPath, "fps": sim.frameLog.perSecond(),
                         "screen": [sim.frame?.width ?? 0, sim.frame?.height ?? 0],
                         "leds": s.leds, "backlight": s.backlight, "bl_power": s.backlightPower,
                         "battery": s.battery, "cable": s.cable, "kind": s.kind == 2 ? "computer" : "wall",
                         "headphones": s.headphones,
                         "rumble": [sim.rumble.strong, sim.rumble.weak],
                         "held": sim.held.map(\.rawValue).sorted()])
        case let .press(buttons):
            if let b = buttons.first(where: { !sim.handheld.has($0) }) {
                return json(["ok": false, "error": "the \(sim.handheld.name) has no \(b.rawValue)"])
            }
            buttons.forEach(sim.press)
        case let .release(buttons): buttons.forEach(sim.release)
        case let .battery(pct): sim.setBattery(pct)
        case let .cable(on): sim.setCable(on)
        case let .kind(k): sim.setKind(k)
        case let .headphones(on): sim.setHeadphones(on)
        case .power: sim.powerButton()
        case .shutdown: sim.shutdown()
        case let .screenshot(path):
            guard sim.saveScreen(to: path) else { return json(["ok": false, "error": "no frame yet"]) }
            return json(["ok": true, "path": path])
        case let .windowShot(path):
            let renderer = ImageRenderer(content: DeviceView(still: sim.frame).environmentObject(sim))
            renderer.scale = 2
            guard let image = renderer.cgImage, Simulator.writePNG(image, to: path) else {
                return json(["ok": false, "error": "could not draw the window"])
            }
            return json(["ok": true, "path": path])
        case .quit:
            DispatchQueue.main.async { NSApp.terminate(nil) }
        case let .keys(on): sim.showKeys = on
        case .fps:
            return json(["ok": true, "fps": sim.frameLog.perSecond(), "screen_path": sim.screenPath])
        case .tap, .waitFrames, .latency:
            break
        }
        return json(["ok": true])
    }

    private func waitFrames(_ n: Int) -> String {
        let start = DispatchQueue.main.sync { MainActor.assumeIsolated { sim?.frameCount ?? 0 } }
        let deadline = Date().addingTimeInterval(10)
        while Date() < deadline {
            let now = DispatchQueue.main.sync { MainActor.assumeIsolated { sim?.frameCount ?? 0 } }
            if now - start >= n { return json(["ok": true, "frames": now - start]) }
            Thread.sleep(forTimeInterval: 0.02)
        }
        return json(["ok": false, "error": "fewer than \(n) frames in 10 s"])
    }

    /// Press to photon, as far as the window can see it: from the tap (its
    /// press sent down px2sim's link) to the first frame, as it came from
    /// the VM, whose pixels (in the region, if one) differ from the frame
    /// before the press
    private func latency(_ buttons: [PadButton], region: [Int]?) -> String {
        let rect = region.map { CGRect(x: $0[0], y: $0[1], width: $0[2], height: $0[3]) }
        guard let (frame, log) = DispatchQueue.main.sync(execute: {
            MainActor.assumeIsolated { () -> (CGImage, FrameLog)? in
                guard let sim, let f = sim.frame else { return nil }
                return (f, sim.frameLog)
            }
        }) else { return json(["ok": false, "error": "no frame yet"]) }
        let before = FrameLog.hash(frame, rect: rect)
        let t0 = FrameLog.now()
        onMain { $0.tap(buttons, milliseconds: 80) }
        var seen = t0
        while FrameLog.now() - t0 < 3000 {
            for (t, image) in log.frames(after: seen) {
                seen = t
                if FrameLog.hash(image, rect: rect) != before {
                    Thread.sleep(forTimeInterval: 0.15)
                    return json(["ok": true, "press_to_frame_ms": (t - t0).rounded(toPlaces: 1)])
                }
            }
            Thread.sleep(forTimeInterval: 0.001)
        }
        return json(["ok": false, "error": "no frame changed in 3 s"])
    }

    private func onMain(_ body: @escaping @MainActor (Simulator) -> Void) {
        DispatchQueue.main.async { MainActor.assumeIsolated { if let sim = self.sim { body(sim) } } }
    }

    private func json(_ object: [String: Any]) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return "{\"ok\":false}"
        }
        return String(decoding: data, as: UTF8.self)
    }
}

private extension Double {
    func rounded(toPlaces n: Int) -> Double {
        let k = pow(10, Double(n))
        return (self * k).rounded() / k
    }
}
