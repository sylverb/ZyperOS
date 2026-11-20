// SPDX-License-Identifier: GPL-3.0-only
import SimCore
import SwiftUI

/// The parts a player can't press: the battery, the cable, the headphones,
/// and what the device shows of them
struct HardwarePanel: View {
    @EnvironmentObject var sim: Simulator
    @State private var battery: Double = 100
    @State private var dragging = false

    var body: some View {
        Form {
            Section {
                HStack(spacing: 10) {
                    Image(systemName: batterySymbol)
                        .font(.system(size: 20))
                        .symbolRenderingMode(.palette)
                        .foregroundStyle(batteryColor, .secondary)
                        .frame(width: 30)
                    Slider(value: $battery, in: 0...100, step: 1) { editing in
                        dragging = editing
                        if !editing { sim.setBattery(Int(battery)) }
                    }
                    Text("\(Int(battery))%")
                        .font(.system(.body, design: .rounded).monospacedDigit().weight(.semibold))
                        .frame(width: 42, alignment: .trailing)
                }
                HStack(spacing: 6) {
                    ForEach([100, 50, 15, 5, 1], id: \.self) { pct in
                        SwiftUI.Button("\(pct)%") { battery = Double(pct); sim.setBattery(pct) }
                            .buttonStyle(.bordered)
                            .controlSize(.small)
                    }
                }
                Toggle(isOn: Binding(get: { sim.state.cable }, set: { sim.setCable($0) })) {
                    Label("Charging cable", systemImage: "powerplug")
                }
                Picker("Into", selection: Binding(get: { sim.state.kind }, set: { sim.setKind($0) })) {
                    Text("Wall charger").tag(1)
                    Text("Computer").tag(2)
                }
                .pickerStyle(.segmented)
                .disabled(!sim.state.cable)
                Text(chargeText).font(.caption).foregroundStyle(.secondary)
            } header: {
                Label("Power", systemImage: "bolt.fill")
            }
            Section {
                Toggle(isOn: Binding(get: { sim.state.headphones }, set: { sim.setHeadphones($0) })) {
                    Label("Headphones", systemImage: "headphones")
                }
            } header: {
                Label("Audio", systemImage: "speaker.wave.2.fill")
            }
            Section {
                LabeledContent("Backlight") {
                    if sim.state.backlightPower != 0 {
                        Text("off").foregroundStyle(.secondary)
                    } else {
                        Gauge(value: Double(sim.state.backlight), in: 0...255) { EmptyView() }
                            .gaugeStyle(.accessoryLinearCapacity)
                            .tint(.yellow)
                            .frame(width: 90)
                    }
                }
                if !sim.handheld.lights.isEmpty {
                    LabeledContent("Side lights") {
                        HStack(spacing: 5) {
                            ForEach(0..<sim.handheld.lights.count, id: \.self) { i in
                                let l = sim.handheld.lights[i]
                                let g = l.green.map { sim.state.led($0) } ?? false
                                let r = l.red.map { sim.state.led($0) } ?? false
                                light(g && r ? .orange : r ? .red : g ? .green : nil)
                            }
                        }
                    }
                }
                LabeledContent("Rumble") {
                    Text(sim.rumble.isOn ? "\(Int(sim.rumble.strength * 100))%" : "off")
                        .foregroundStyle(sim.rumble.isOn ? .primary : .secondary)
                }
            } header: {
                Label("Shown by the device", systemImage: "eye")
            }
            Section {
                Grid(alignment: .leading, horizontalSpacing: 10, verticalSpacing: 6) {
                    ForEach(keys.filter { $0.2.allSatisfy(sim.handheld.has) }, id: \.1) { caps, button, _ in
                        GridRow {
                            HStack(spacing: 3) { ForEach(caps, id: \.self) { KeyCap(text: $0) } }
                            Text(button).foregroundStyle(.secondary)
                        }
                    }
                }
                .font(.caption)
                Text("Hold Option to see them on the keys.").font(.caption).foregroundStyle(.secondary)
            } header: {
                Label("Keyboard", systemImage: "keyboard")
            }
            Section {
                HStack {
                    SwiftUI.Button { sim.powerButton() } label: {
                        Label(sim.power == .off ? "Turn On" : "Power", systemImage: "power")
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(sim.power == .off ? .green : .gray)
                    SwiftUI.Button { sim.shutdown() } label: { Label("Shut Down", systemImage: "stop.circle") }
                        .disabled(sim.power == .off)
                }
                SwiftUI.Button { sim.saveScreenToDesktop() } label: {
                    Label("Save Screenshot", systemImage: "camera")
                }
                .disabled(sim.power != .on)
            } header: {
                Label("Device", systemImage: "gamecontroller")
            }
        }
        .formStyle(.grouped)
        .scrollContentBackground(.hidden)
        .frame(width: 280)
        .disabled(sim.power == .starting)
        .onChange(of: sim.state.battery) { _, new in if !dragging { battery = Double(new) } }
        .onAppear { battery = Double(sim.state.battery) }
    }

    private func light(_ color: Color?) -> some View {
        Capsule()
            .fill(color ?? Color.white.opacity(0.12))
            .frame(width: 6, height: 13)
            .shadow(color: color ?? .clear, radius: color == nil ? 0 : 3)
    }

    /// The keyboard's keys, what they press, and the buttons the handheld
    /// needs for the row to show
    private let keys: [([String], String, [PadButton])] = [
        (["\u{2190}", "\u{2191}", "\u{2193}", "\u{2192}"], "d-pad", [.up]),
        (["X", "Z", "S", "A"], "A B X Y", [.a]), (["Q", "W", "E", "R"], "L1 R1 L2 R2", [.l1]),
        (["Return"], "START", [.start]), (["R-Shift", "Delete"], "SELECT", [.select]),
        (["Esc", "M"], "MENU", [.menu]), (["-", "="], "volume", [.volup]),
        (["[", "]"], "brightness (F1 F2)", [.brightdown]), (["P"], "POWER", [.power]), (["H"], "headphones", []),
    ]

    private var batterySymbol: String {
        if sim.state.cable { return "battery.100percent.bolt" }
        let b = sim.state.battery
        return b > 87 ? "battery.100percent" : b > 62 ? "battery.75percent" : b > 37 ? "battery.50percent"
             : b > 12 ? "battery.25percent" : "battery.0percent"
    }

    private var batteryColor: Color {
        sim.state.cable ? .green : sim.state.battery <= 10 ? .red : sim.state.battery <= 20 ? .orange : .primary
    }

    private var chargeText: String {
        guard sim.state.cable else {
            return sim.power == .off ? "Off; the cable starts it, as a charger does" : "On the battery"
        }
        return sim.state.battery >= 100 ? "Full, on the cable" : "Charging"
    }
}
