// SPDX-License-Identifier: GPL-3.0-only
import AppKit
import SimCore
import SwiftUI

/// ZyperOS Simulator: a handheld on the Mac's screen, around ZyperOS running
/// in QEMU, as Apple's Simulator does an iPhone. docs/simulator.md.
///
///   ZyperSimulator [--name NAME] [--device ID] [--qemu-run PATH] [--image DIR]
///
/// NAME is the VM's (tools/qemu-run.sh -n), "simulator" unless told; a VM by
/// that name already running is shown as it is, otherwise one is started.
/// ID is the handheld drawn round it (Handhelds.swift: "pixel2", the VM's
/// own, unless told; $ZYPEROS_SIM_DEVICE too).
@main
struct ZyperSimulatorApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    @StateObject private var sim = AppDelegate.makeSimulator()

    var body: some Scene {
        Window("ZyperOS Simulator", id: "device") {
            ContentView()
                .environmentObject(sim)
                .onAppear { delegate.attach(sim) }
        }
        .windowStyle(.hiddenTitleBar)
        .windowResizability(.contentSize)
        .commands { SimulatorCommands(sim: sim) }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var sim: Simulator?
    private var control: ControlServer?
    private var keys: Any?

    @MainActor
    static func makeSimulator() -> Simulator {
        let args = CommandLine.arguments
        func value(_ flag: String) -> String? {
            guard let i = args.firstIndex(of: flag), i + 1 < args.count else { return nil }
            return args[i + 1]
        }
        // The repository the app was built from (scripts/build-app.sh writes
        // it into Info.plist), for its tools/qemu-run.sh
        let repo = Bundle.main.object(forInfoDictionaryKey: "ZyperOSRepo") as? String
        let qemuRun = value("--qemu-run") ?? repo.map { $0 + "/tools/qemu-run.sh" } ?? "tools/qemu-run.sh"
        let device = value("--device") ?? ProcessInfo.processInfo.environment["ZYPEROS_SIM_DEVICE"]
        var handheld = Handheld.standard
        if let device {
            if let h = Handheld.named(device) { handheld = h } else {
                FileHandle.standardError.write(Data(
                    "ZyperSimulator: no handheld \(device) (\(Handheld.all.map(\.id).joined(separator: ", "))); the \(handheld.name)\n".utf8))
            }
        }
        return Simulator(name: value("--name") ?? "simulator", handheld: handheld, qemuRun: qemuRun,
                         image: value("--image") ?? ProcessInfo.processInfo.environment["ZYPEROS_QEMU_IMAGE"])
    }

    @MainActor
    func attach(_ sim: Simulator) {
        guard self.sim == nil else { return }
        self.sim = sim
        control = ControlServer(path: sim.vmDir + "/app.sock", sim: sim)
        control?.start()
        keys = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .keyUp, .flagsChanged]) { event in
            Keyboard.handle(event, sim: sim) ? nil : event
        }
        sim.boot()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ app: NSApplication) -> Bool { true }

    func applicationWillTerminate(_ notification: Notification) {
        control?.stop()
        MainActor.assumeIsolated { sim?.quitting() }
    }
}

struct ContentView: View {
    @EnvironmentObject var sim: Simulator

    var body: some View {
        // The device's column sets the window's height; the panel scrolls
        // in it
        let height = Geometry(sim.handheld, zoom: sim.zoom).height + 14 + 14 + 32 + 16
        HStack(spacing: 0) {
            // The handheld on a dark desk, lit from above, and under it a
            // line on what it is doing
            VStack(spacing: 14) {
                DeviceView()
                StatusBar()
            }
            .padding(.top, 14)
            .padding(.horizontal, 26)
            .padding(.bottom, 16)
            .frame(height: height)
            if sim.showPanel {
                HardwarePanel()
                    .frame(height: height)
                    .background(.regularMaterial)
                    .overlay(alignment: .leading) {
                        Rectangle().fill(.black.opacity(0.35)).frame(width: 1).ignoresSafeArea()
                    }
            }
        }
        // The backgrounds under the hidden title bar too; the content below it
        .background { LinearGradient(colors: [Color(white: 0.19), Color(white: 0.105)],
                                     startPoint: .top, endPoint: .bottom).ignoresSafeArea() }
        .fixedSize()
        .preferredColorScheme(.dark)
    }
}

/// Under the device: on or off, which way the screen's frames come and how
/// many a second, and the window's two quick things
struct StatusBar: View {
    @EnvironmentObject var sim: Simulator

    var body: some View {
        HStack(spacing: 10) {
            Circle()
                .fill(sim.power == .on ? Color(hex: 0x5BE36A) : sim.power == .starting ? Color(hex: 0xFFB13D)
                      : Color(white: 0.4))
                .frame(width: 7, height: 7)
                .shadow(color: sim.power == .on ? Color(hex: 0x5BE36A).opacity(0.7) : .clear, radius: 3)
            Text(sim.name).font(.system(size: 12, weight: .semibold, design: .rounded)).fixedSize()
            if sim.power == .on {
                Spacer(minLength: 4)
                TimelineView(.periodic(from: .now, by: 1)) { _ in
                    Text("\(sim.frameLog.perSecond()) fps \u{00B7} \(sim.screenPath)")
                        .font(.system(size: 10.5, weight: .semibold, design: .rounded).monospacedDigit())
                        .padding(.horizontal, 8)
                        .padding(.vertical, 3)
                        .background(Capsule().fill(.white.opacity(0.08)))
                        .fixedSize()
                        .help("How the screen's frames reach the window (docs/simulator.md). \(sim.status)")
                }
            } else {
                Text(sim.status)
                    .font(.system(size: 11))
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.tail)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .help(sim.status)
            }
            Button { sim.saveScreenToDesktop() } label: { Image(systemName: "camera") }
                .buttonStyle(.borderless)
                .disabled(sim.power != .on)
                .help("Save a screenshot to the Desktop (Cmd S)")
            Button { withAnimation(.easeInOut(duration: 0.2)) { sim.showPanel.toggle() } } label: {
                Image(systemName: "sidebar.right")
            }
            .buttonStyle(.borderless)
            .help("The hardware panel (Cmd I)")
        }
        .foregroundStyle(.primary)
        .padding(.horizontal, 12)
        .padding(.vertical, 7)
        .background(Capsule().fill(.black.opacity(0.28)))
        .overlay(Capsule().stroke(.white.opacity(0.07), lineWidth: 1))
        .frame(width: Geometry(sim.handheld, zoom: sim.zoom).width)
    }
}

/// The keyboard as the handheld's buttons, the same keys px2sim takes from
/// QEMU's own window: arrows, x z s a (A B X Y), q w e r (L1 R1 L2 R2),
/// Return (START), Right Shift or Delete (SELECT), Esc or m (MENU), = and -
/// (volume), [ and ] (the brightness keys, where the handheld has them), p
/// (power), h (headphones). Keys with Command go to the menus.
enum Keyboard {
    private static let map: [UInt16: PadButton] = [
        126: .up, 125: .down, 123: .left, 124: .right,
        7: .a, 6: .b, 1: .x, 0: .y,
        12: .l1, 13: .r1, 14: .l2, 15: .r2,
        36: .start, 76: .start, 51: .select, 53: .menu, 46: .menu,
        24: .volup, 27: .voldown, 33: .brightdown, 30: .brightup, 35: .power,
    ]
    private static let rightShift: UInt16 = 60, h: UInt16 = 4

    /// A button's key, as its keycap says it (src/device/keymap.h's names)
    static func name(for button: PadButton) -> String {
        switch button {
        case .up: return "Up"
        case .down: return "Down"
        case .left: return "Left"
        case .right: return "Right"
        case .a: return "X"
        case .b: return "Z"
        case .x: return "S"
        case .y: return "A"
        case .l1: return "Q"
        case .r1: return "W"
        case .l2: return "E"
        case .r2: return "R"
        case .start: return "Return"
        case .select: return "R-Shift"
        case .menu: return "Esc"
        case .volup: return "="
        case .voldown: return "-"
        case .power: return "P"
        case .brightdown: return "["
        case .brightup: return "]"
        }
    }

    /// A button's tooltip: what it is and every key that presses it
    static func help(for button: PadButton) -> String {
        switch button {
        case .up, .down, .left, .right: return "D-pad (arrow keys)"
        case .a: return "A (x)"
        case .b: return "B (z)"
        case .x: return "X (s)"
        case .y: return "Y (a)"
        case .l1: return "L1 (q)"
        case .r1: return "R1 (w)"
        case .l2: return "L2 (e)"
        case .r2: return "R2 (r)"
        case .start: return "START (Return)"
        case .select: return "SELECT (Right Shift, Delete)"
        case .menu: return "MENU (Esc, m)"
        case .volup: return "Volume up (=)"
        case .voldown: return "Volume down (-)"
        case .power: return "POWER (p)"
        case .brightdown: return "Brightness down, F1 ([)"
        case .brightup: return "Brightness up, F2 (])"
        }
    }

    @MainActor
    static func handle(_ event: NSEvent, sim: Simulator) -> Bool {
        if event.modifierFlags.contains(.command) { return false }
        // Typing in the panel's fields stays typing
        if NSApp.keyWindow?.firstResponder is NSText { return false }
        // Option: every key's keyboard key on it, while held
        if event.type == .flagsChanged {
            let option = event.modifierFlags.contains(.option)
            if sim.showKeys != option { sim.showKeys = option }
        }
        switch event.type {
        case .flagsChanged where event.keyCode == rightShift:
            if event.modifierFlags.contains(.shift) { sim.press(.select) } else { sim.release(.select) }
            return true
        case .keyDown where event.keyCode == h:
            if !event.isARepeat { sim.setHeadphones(!sim.state.headphones) }
            return true
        case .keyDown, .keyUp:
            guard let button = map[event.keyCode], sim.handheld.has(button) else { return false }
            if event.type == .keyUp {
                sim.release(button)
            } else if !event.isARepeat {
                if button == .power && sim.power == .off { sim.start() } else { sim.press(button) }
            }
            return true
        default:
            return false
        }
    }
}

struct SimulatorCommands: Commands {
    @ObservedObject var sim: Simulator

    var body: some Commands {
        CommandGroup(replacing: .newItem) {}
        CommandMenu("Device") {
            SwiftUI.Button(sim.power == .off ? "Turn On" : "Power Button") { sim.powerButton() }
                .keyboardShortcut("p", modifiers: [.command, .shift])
            SwiftUI.Button("Shut Down") { sim.shutdown() }
                .disabled(sim.power == .off)
            Divider()
            Toggle("Charging Cable", isOn: Binding(get: { sim.state.cable }, set: { sim.setCable($0) }))
                .keyboardShortcut("u", modifiers: [.command, .shift])
            Picker("Cable Into", selection: Binding(get: { sim.state.kind }, set: { sim.setKind($0) })) {
                Text("Wall Charger").tag(1)
                Text("Computer").tag(2)
            }
            Menu("Battery") {
                ForEach([100, 80, 50, 20, 15, 10, 5, 1], id: \.self) { pct in
                    SwiftUI.Button("\(pct)%") { sim.setBattery(pct) }
                }
            }
            Toggle("Headphones", isOn: Binding(get: { sim.state.headphones }, set: { sim.setHeadphones($0) }))
                .keyboardShortcut("h", modifiers: [.command, .shift])
            Divider()
            SwiftUI.Button("Volume Up") { sim.tap([.volup], milliseconds: 80) }
                .keyboardShortcut(.upArrow, modifiers: [.command])
            SwiftUI.Button("Volume Down") { sim.tap([.voldown], milliseconds: 80) }
                .keyboardShortcut(.downArrow, modifiers: [.command])
            Divider()
            SwiftUI.Button("Save Screenshot") { sim.saveScreenToDesktop() }
                .keyboardShortcut("s", modifiers: [.command])
            SwiftUI.Button("Copy Screen") { sim.copyScreen() }
                .keyboardShortcut("c", modifiers: [.command, .control])
        }
        CommandGroup(after: .toolbar) {
            Toggle("Hardware Panel", isOn: $sim.showPanel)
                .keyboardShortcut("i", modifiers: [.command])
            Divider()
            ForEach([(1.0, "1"), (1.25, "2"), (1.5, "3"), (2.0, "4")], id: \.0) { zoom, key in
                SwiftUI.Button("Zoom \(Int(zoom * 100))%") { sim.zoom = zoom }
                    .keyboardShortcut(KeyEquivalent(Character(key)), modifiers: [.command])
            }
        }
    }
}
