// SPDX-License-Identifier: GPL-3.0-only
import AppKit
import SimCore
import SwiftUI

/// Where things are on the Pixel 2's front, in millimetres from the middle
/// of its face, up positive, from the dimensioned drawing that
/// tools/build-pixel2-model.py measures (63 x 81 mm). The side keys and the
/// side lights are drawn on the edges where they sit. ZyperOS's desk mode
/// draws the same device from the same numbers (src/device/deskface.c).
struct Geometry {
    static let marginX: CGFloat = 6, marginTop: CGFloat = 4, marginBottom: CGFloat = 2
    static let width: CGFloat = 63 + 2 * marginX
    static let height: CGFloat = 81 + marginTop + marginBottom
    /// Points a millimetre: zoom 1 draws the 48.8 mm screen 320 points wide,
    /// its 640 pixels one to one on a Retina display
    static func scale(zoom: Double) -> CGFloat { CGFloat(zoom) * 320 / 48.8 }

    let s: CGFloat
    func x(_ mm: CGFloat) -> CGFloat { (mm + Geometry.width / 2) * s }
    func y(_ mm: CGFloat) -> CGFloat { (Geometry.marginTop + 40.5 - mm) * s }
    func point(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: self.x(x), y: self.y(y)) }
    func size(_ w: CGFloat, _ h: CGFloat) -> CGSize { CGSize(width: w * s, height: h * s) }
}

extension Color {
    init(hex: UInt32, opacity: Double = 1) {
        self.init(red: Double(hex >> 16 & 0xFF) / 255, green: Double(hex >> 8 & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255, opacity: opacity)
    }
    static let bodyLight = Color(hex: 0xB4CC6C)
    static let body = Color(hex: 0x9CB849)
    static let bodyDark = Color(hex: 0x83A03A)
    static let recess = Color(hex: 0x708B32)
    static let capLight = Color(hex: 0x4F5154)
    static let cap = Color(hex: 0x3A3C3F)
    static let capDark = Color(hex: 0x2B2D2F)
    static let capDown = Color(hex: 0x1E2021)
    static let marking = Color(hex: 0xA4A7AA)
    static let glass = Color(hex: 0x0B0E0E)
    static let ledOff = Color(hex: 0x56723A)
}

/// Where a key's keyboard key shows beside it
enum HintSide { case above, below, leading, trailing, on }

/// A key's keyboard key, as a small keycap: over the key under the pointer,
/// over every key while Option is held
struct KeyCap: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.system(size: 10, weight: .bold, design: .rounded))
            .foregroundStyle(.white)
            .padding(.horizontal, 5)
            .frame(minWidth: 18, minHeight: 17)
            .background(
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(LinearGradient(colors: [Color(white: 0.17), Color(white: 0.09)],
                                         startPoint: .top, endPoint: .bottom))
                    .overlay(RoundedRectangle(cornerRadius: 4, style: .continuous)
                        .strokeBorder(LinearGradient(colors: [.white.opacity(0.28), .white.opacity(0.06)],
                                                     startPoint: .top, endPoint: .bottom), lineWidth: 0.6)))
            .shadow(color: .black.opacity(0.4), radius: 2.5, y: 1)
            .fixedSize()
            .allowsHitTesting(false)
    }
}

/// A keycap's width, near enough to set it beside a key: its padding and a
/// letter's width each
private func capWidth(_ text: String) -> CGFloat { max(18, 10 + 6.6 * CGFloat(text.count)) }

private extension View {
    /// The keycap beside a key of `size`, on `side`, nudged `dx` across, when
    /// `shown`
    func keyHint(_ text: String, side: HintSide, size: CGSize, dx: CGFloat = 0, shown: Bool) -> some View {
        let gap: CGFloat = 5, half: CGFloat = 8.5
        let off: CGSize
        switch side {
        case .above: off = CGSize(width: dx, height: -(size.height / 2 + gap + half))
        case .below: off = CGSize(width: dx, height: size.height / 2 + gap + half)
        case .leading: off = CGSize(width: -(size.width / 2 + gap + capWidth(text) / 2), height: 0)
        case .trailing: off = CGSize(width: size.width / 2 + gap + capWidth(text) / 2, height: 0)
        case .on: off = .zero
        }
        return overlay {
            if shown && !text.isEmpty {
                KeyCap(text: text).offset(off).transition(.opacity)
            }
        }
    }
}

/// The Pixel 2, drawn, with its screen showing the VM's
struct DeviceView: View {
    @EnvironmentObject var sim: Simulator
    /// A picture for the screen instead of the live layer: for drawing the
    /// whole device into an image (the control socket's window-shot)
    var still: CGImage?

    var body: some View {
        let g = Geometry(s: Geometry.scale(zoom: sim.zoom))
        TimelineView(.animation(paused: !sim.rumble.isOn)) { time in
            ZStack(alignment: .topLeading) {
                shoulders(g)
                sideKeys(g)
                bodyShell(g)
                edgeHints(g)
                screen(g)
                faceButtons(g)
                DPad(g: g)
                sideLights(g)
            }
            .frame(width: Geometry.width * g.s, height: Geometry.height * g.s, alignment: .topLeading)
            .offset(x: shake(time.date, g))
        }
        .animation(.easeOut(duration: 0.12), value: sim.showKeys)
    }

    /// The motor, as the device moving in the hand
    private func shake(_ date: Date, _ g: Geometry) -> CGFloat {
        guard sim.rumble.isOn else { return 0 }
        let t = date.timeIntervalSinceReferenceDate
        return CGFloat(sin(t * 95) * (0.25 + 0.45 * sim.rumble.strength)) * g.s
    }

    // ---- the parts ----------------------------------------------------

    /// The shell: lit from above, a bright rim along its top, a dark one
    /// along its bottom, a contact shadow and a soft one under it
    private func bodyShell(_ g: Geometry) -> some View {
        let shape = RoundedRectangle(cornerRadius: 3.9 * g.s, style: .continuous)
        return shape
            .fill(LinearGradient(colors: [.bodyLight, .body, .bodyDark], startPoint: .top, endPoint: .bottom))
            .overlay(shape.inset(by: 0.35 * g.s)
                .stroke(LinearGradient(colors: [.white.opacity(0.5), .white.opacity(0.08), .clear],
                                       startPoint: .top, endPoint: .center), lineWidth: 0.6 * g.s))
            .overlay(shape.stroke(LinearGradient(colors: [.clear, .black.opacity(0.28)],
                                                 startPoint: .center, endPoint: .bottom), lineWidth: 0.4 * g.s))
            .shadow(color: .black.opacity(0.3), radius: 0.5 * g.s, y: 0.3 * g.s)
            .shadow(color: .black.opacity(0.4), radius: 3.2 * g.s, y: 1.8 * g.s)
            .frame(width: 63 * g.s, height: 81 * g.s)
            .position(g.point(0, 0))
            .allowsHitTesting(false)
    }

    private func screen(_ g: Geometry) -> some View {
        let glass = RoundedRectangle(cornerRadius: 1.2 * g.s, style: .continuous)
        return ZStack {
            glass
                .fill(LinearGradient(colors: [Color(hex: 0x1A1E1E), .glass], startPoint: .top, endPoint: .bottom))
                .overlay(glass.stroke(LinearGradient(colors: [.white.opacity(0.16), .clear],
                                                     startPoint: .top, endPoint: .center), lineWidth: 0.5))
                .shadow(color: .black.opacity(0.35), radius: 0.3 * g.s, y: -0.1 * g.s)
                .frame(width: 55.1 * g.s, height: 43.8 * g.s)
                .position(g.point(0, 14.2))
            ZStack {
                if let still {
                    Image(decorative: still, scale: 1).resizable().interpolation(.none)
                } else {
                    ScreenLayer(sim: sim)
                }
                // The backlight as a shade over the picture: dark when off,
                // and only a hint of the level otherwise, so a dim setting
                // still leaves the picture readable on the Mac
                Rectangle().fill(.black)
                    .opacity(sim.power != .on || sim.state.screenLight == 0
                             ? 1 : 0.55 * (1 - sim.state.screenLight))
                    .animation(.easeOut(duration: 0.15), value: sim.state.screenLight)
                if sim.power != .on {
                    VStack(spacing: 0.9 * g.s) {
                        if sim.power == .starting {
                            ProgressView().controlSize(.small).tint(.gray)
                        } else {
                            Image(systemName: "power").font(.system(size: 3.2 * g.s, weight: .semibold))
                        }
                        Text(sim.power == .starting ? "Starting…" : "Off. Press POWER.")
                            .font(.system(size: 2.1 * g.s, weight: .semibold, design: .rounded))
                    }
                    .foregroundStyle(Color.white.opacity(0.45))
                }
            }
            .frame(width: 48.8 * g.s, height: 36.6 * g.s)
            .position(g.point(0, 15.55))
            Text("ZyperOS")
                .font(.system(size: 1.9 * g.s, weight: .semibold, design: .rounded))
                .foregroundStyle(Color.white.opacity(0.82))
                .position(g.point(0, -5.5))
        }
        .allowsHitTesting(false)
    }

    private func faceButtons(_ g: Geometry) -> some View {
        let well = RoundedRectangle(cornerRadius: 3.8 * g.s, style: .continuous)
        return ZStack(alignment: .topLeading) {
            // SELECT and START's well, sunk into the shell
            well.fill(LinearGradient(colors: [Color(hex: 0x66802D), .recess], startPoint: .top, endPoint: .bottom))
                .overlay(well.stroke(LinearGradient(colors: [.black.opacity(0.35), .clear],
                                                    startPoint: .top, endPoint: .center), lineWidth: 0.5 * g.s))
                .overlay(well.stroke(LinearGradient(colors: [.clear, .white.opacity(0.25)],
                                                    startPoint: .center, endPoint: .bottom), lineWidth: 0.25 * g.s))
                .frame(width: 16.4 * g.s, height: 7.6 * g.s)
                .position(g.point(0.1, -29.6))
                .allowsHitTesting(false)
            ForEach(face, id: \.button) { f in
                Key(button: f.button, label: f.label, shape: Circle(), size: g.size(2 * f.r, 2 * f.r), g: g,
                    labelSize: f.label.count == 1 ? 2.8 * g.s : 1.6 * g.s, hint: f.hint, hintDX: f.dx, help: f.help)
                    .position(g.point(f.x, f.y))
            }
        }
    }

    private struct Face {
        let button: PadButton; let label: String; let x, y, r: CGFloat; let hint: HintSide; var dx: CGFloat = 0
        let help: String
    }
    private var face: [Face] {
        [Face(button: .x, label: "X", x: 16.9, y: -14.8, r: 3.05, hint: .above, help: "X (s)"),
         Face(button: .y, label: "Y", x: 10.2, y: -21.5, r: 3.05, hint: .leading, help: "Y (a)"),
         Face(button: .a, label: "A", x: 23.6, y: -21.5, r: 3.05, hint: .trailing, help: "A (x)"),
         Face(button: .b, label: "B", x: 16.9, y: -28.2, r: 3.05, hint: .below, help: "B (z)"),
         Face(button: .select, label: "SE", x: -3.8, y: -29.6, r: 2.85, hint: .below, dx: -14,
              help: "SELECT (Right Shift, Delete)"),
         Face(button: .start, label: "ST", x: 4, y: -29.6, r: 2.85, hint: .below, dx: 14, help: "START (Return)")]
    }

    /// L1, L2, R2, R1 along the top edge, behind the shell
    private func shoulders(_ g: Geometry) -> some View {
        ZStack(alignment: .topLeading) {
            ForEach(shoulderKeys, id: \.button) { k in
                Key(button: k.button, label: k.label,
                    shape: UnevenRoundedRectangle(topLeadingRadius: 1.2 * g.s, topTrailingRadius: 1.2 * g.s),
                    size: g.size(k.w, 3.6), g: g, labelSize: 1.25 * g.s, labelOffset: -0.9 * g.s, hint: nil,
                    help: k.help)
                    .position(g.point(k.x, 40.9))
            }
        }
    }

    /// The keys of what stands out of the shell's edges, over the shell:
    /// the shoulders and the side keys themselves are behind it
    private func edgeHints(_ g: Geometry) -> some View {
        let places: [(PadButton, CGFloat, CGFloat)] =
            shoulderKeys.map { ($0.button, $0.x, 41.2) } +
            [(.volup, -29.3, 15.75), (.voldown, -29.3, 7.65), (.menu, 29.0, 17.1), (.power, 29.0, 6.65)]
        return ZStack(alignment: .topLeading) {
            ForEach(places, id: \.0) { button, x, y in
                if sim.showKeys || sim.hovered == button {
                    KeyCap(text: Keyboard.name(for: button))
                        .position(g.point(x, y))
                        .transition(.opacity)
                }
            }
        }
        .allowsHitTesting(false)
    }

    private struct Shoulder { let button: PadButton; let label: String; let x, w: CGFloat; let help: String }
    private var shoulderKeys: [Shoulder] {
        [Shoulder(button: .l1, label: "L1", x: -23.9, w: 15.1, help: "L1 (q)"),
         Shoulder(button: .l2, label: "L2", x: -12.6, w: 7.4, help: "L2 (e)"),
         Shoulder(button: .r2, label: "R2", x: 12.6, w: 7.4, help: "R2 (r)"),
         Shoulder(button: .r1, label: "R1", x: 23.9, w: 15.1, help: "R1 (w)")]
    }

    /// The volume rocker on the left side, MENU and POWER on the right
    private func sideKeys(_ g: Geometry) -> some View {
        ZStack(alignment: .topLeading) {
            Key(button: .volup, label: "+", shape: UnevenRoundedRectangle(topLeadingRadius: 1.2 * g.s),
                size: g.size(3, 8), g: g, labelSize: 2 * g.s, labelOffset: -0.7 * g.s, labelX: true,
                hint: nil, help: "Volume up (=)")
                .position(g.point(-31.8, 15.75))
            Key(button: .voldown, label: "−", shape: UnevenRoundedRectangle(bottomLeadingRadius: 1.2 * g.s),
                size: g.size(3, 8), g: g, labelSize: 2 * g.s, labelOffset: -0.7 * g.s, labelX: true,
                hint: nil, help: "Volume down (-)")
                .position(g.point(-31.8, 7.65))
            Key(button: .menu, label: "", shape: Capsule(), size: g.size(3, 5.3), g: g, labelSize: 1,
                hint: nil, help: "MENU (Esc, m)")
                .position(g.point(31.8, 17.1))
            Key(button: .power, label: "", shape: Capsule(), size: g.size(3, 7.2), g: g, labelSize: 1,
                hint: nil, help: "POWER (p)")
                .position(g.point(31.8, 6.65))
            sideLabel("VOL", g).position(g.point(-34.6, 11.7))
            sideLabel("MENU", g).position(g.point(34.6, 17.1))
            sideLabel("POWER", g).position(g.point(34.6, 6.65))
        }
    }

    private func sideLabel(_ text: String, _ g: Geometry) -> some View {
        Text(text).font(.system(size: 1.3 * g.s, weight: .bold, design: .rounded))
            .foregroundStyle(Color(white: 0.5))
            .fixedSize()
            .rotationEffect(.degrees(-90))
            .allowsHitTesting(false)
    }

    /// The four lights under POWER, top to bottom battery4, battery3,
    /// battery2, and the bottom one, red (battery0) or green (battery1):
    /// lit, each glows
    private func sideLights(_ g: Geometry) -> some View {
        let green = Color(hex: 0x5BE36A), red = Color(hex: 0xFF4A3D), amber = Color(hex: 0xFFB13D)
        let lights: [(CGFloat, Color?)] = [
            (1.4, sim.state.led(4) ? green : nil),
            (-4.03, sim.state.led(3) ? green : nil),
            (-9.67, sim.state.led(2) ? green : nil),
            (-15.3, sim.state.led(0) && sim.state.led(1) ? amber :
                    sim.state.led(0) ? red : sim.state.led(1) ? green : nil),
        ]
        return ZStack(alignment: .topLeading) {
            ForEach(0..<lights.count, id: \.self) { i in
                let (y, color) = lights[i]
                Capsule()
                    .fill(LinearGradient(colors: color.map { [$0.opacity(0.85), $0] }
                                         ?? [Color(hex: 0x445C2C), .ledOff],
                                         startPoint: .top, endPoint: .bottom))
                    .overlay(Capsule().stroke(.black.opacity(0.35), lineWidth: 0.1 * g.s))
                    .overlay(Capsule().fill(.white.opacity(color == nil ? 0.08 : 0.45))
                        .frame(width: 0.4 * g.s, height: 1.1 * g.s).offset(y: -0.7 * g.s))
                    .shadow(color: color ?? .clear, radius: color == nil ? 0 : 1.6 * g.s)
                    .shadow(color: color?.opacity(0.6) ?? .clear, radius: color == nil ? 0 : 0.5 * g.s)
                    .frame(width: 1.3 * g.s, height: 3.4 * g.s)
                    .position(g.point(32.0, y))
                    .animation(.easeOut(duration: 0.12), value: color)
            }
        }
        .allowsHitTesting(false)
        .help("The side lights (gauge)")
    }
}

/// A key: pressed while the mouse holds it, shown pressed while the VM holds
/// it; it stands up from the shell, lit from above, and sinks when pressed
private struct Key<S: Shape>: View {
    @EnvironmentObject var sim: Simulator
    let button: PadButton
    let label: String
    let shape: S
    let size: CGSize
    let g: Geometry
    let labelSize: CGFloat
    var labelOffset: CGFloat = 0
    var labelX = false
    /// nil: drawn by the device over the shell (the shoulders')
    var hint: HintSide? = .above
    var hintDX: CGFloat = 0
    let help: String
    @State private var hovering = false

    var body: some View {
        let down = sim.held.contains(button)
        shape
            .fill(down
                  ? LinearGradient(colors: [Color(hex: 0x141516), .capDown], startPoint: .top, endPoint: .bottom)
                  : LinearGradient(colors: [.capLight, .cap, .capDark], startPoint: .top, endPoint: .bottom))
            .overlay(shape.stroke(LinearGradient(colors: down ? [.black.opacity(0.6), .clear]
                                                              : [.white.opacity(0.26), .clear],
                                                 startPoint: .top, endPoint: .center), lineWidth: 0.9))
            .overlay(shape.stroke(.black.opacity(0.45), lineWidth: 0.5))
            .overlay(
                Text(label).font(.system(size: labelSize, weight: .bold, design: .rounded))
                    .foregroundStyle(Color.marking.opacity(down ? 0.75 : 1))
                    .shadow(color: .black.opacity(0.5), radius: 0, y: -0.5)
                    .offset(x: labelX ? labelOffset : 0, y: labelX ? 0 : labelOffset))
            .frame(width: size.width, height: size.height)
            .shadow(color: .black.opacity(down ? 0.2 : 0.42), radius: down ? 0.3 * g.s : 0.7 * g.s,
                    y: down ? 0.1 * g.s : 0.4 * g.s)
            .offset(y: down ? 0.16 * g.s : 0)
            .animation(.easeOut(duration: 0.06), value: down)
            .keyHint(Keyboard.name(for: button), side: hint ?? .on, size: size, dx: hintDX,
                     shown: hint != nil && (sim.showKeys || hovering))
            .contentShape(shape)
            .onHover { inside in
                hovering = inside
                if inside { sim.hovered = button } else if sim.hovered == button { sim.hovered = nil }
            }
            .gesture(DragGesture(minimumDistance: 0)
                .onChanged { _ in sim.press(button) }
                .onEnded { _ in sim.release(button) })
            .help(help)
    }
}

/// The d-pad: one direction at a time, the one the pointer is furthest
/// along, rolling to the next as it moves, as a thumb does. It stands in a
/// well in the shell, its held arm tipped down.
private struct DPad: View {
    @EnvironmentObject var sim: Simulator
    let g: Geometry
    @State private var current: PadButton?
    @State private var hovering: PadButton?

    var body: some View {
        let size = 18 * g.s
        let arm: CGFloat = 3.2 / 9
        ZStack {
            Cross(arm: arm * 1.12)
                .fill(LinearGradient(colors: [Color(hex: 0x5E7826), Color(hex: 0x6E8A30)],
                                     startPoint: .top, endPoint: .bottom))
                .overlay(Cross(arm: arm * 1.12).stroke(.black.opacity(0.25), lineWidth: 0.4 * g.s))
                .frame(width: size * 1.1, height: size * 1.1)
            Cross(arm: arm)
                .fill(LinearGradient(colors: [.capLight, .cap, .capDark], startPoint: .top, endPoint: .bottom))
                .overlay(Cross(arm: arm).stroke(LinearGradient(colors: [.white.opacity(0.24), .clear],
                                                               startPoint: .top, endPoint: .center), lineWidth: 0.9))
                .overlay(Cross(arm: arm).stroke(.black.opacity(0.45), lineWidth: 0.5))
                .overlay(heldArm(size, arm))
                .overlay(Circle()
                    .fill(LinearGradient(colors: [.capDark, .capLight.opacity(0.9)], startPoint: .top, endPoint: .bottom))
                    .frame(width: size * 0.2, height: size * 0.2))
                .overlay(arrows(size))
                .frame(width: size, height: size)
                .shadow(color: .black.opacity(0.42), radius: 0.7 * g.s, y: 0.4 * g.s)
            ForEach([PadButton.up, .down, .left, .right], id: \.self) { b in
                if sim.showKeys || hovering == b {
                    KeyCap(text: Keyboard.name(for: b))
                        .offset(hintOffset(b, size))
                        .transition(.opacity)
                }
            }
        }
        .frame(width: size * 1.1, height: size * 1.1)
        .contentShape(Cross(arm: arm))
        .onContinuousHover { phase in
            if case let .active(p) = phase { hovering = direction(p, size * 1.1) } else { hovering = nil }
        }
        .gesture(DragGesture(minimumDistance: 0)
            .onChanged { v in roll(to: direction(v.location, size * 1.1)) }
            .onEnded { _ in roll(to: nil) })
        .help("D-pad (arrow keys)")
        .position(g.point(-16.9, -21.0))
    }

    private func hintOffset(_ b: PadButton, _ size: CGFloat) -> CGSize {
        let d = size * 0.5 + 12
        switch b {
        case .up: return CGSize(width: 0, height: -d)
        case .down: return CGSize(width: 0, height: d)
        case .left: return CGSize(width: -d - 8, height: 0)
        default: return CGSize(width: d + 8, height: 0)
        }
    }

    private func heldArm(_ size: CGFloat, _ arm: CGFloat) -> some View {
        let a = size / 2 * arm
        return ZStack {
            if let b = PadButton.allCases.first(where: { [.up, .down, .left, .right].contains($0) && sim.held.contains($0) }) {
                let vertical = b == .up || b == .down
                RoundedRectangle(cornerRadius: a * 0.25)
                    .fill(.black.opacity(0.38))
                    .frame(width: vertical ? 2 * a : size / 2 - a, height: vertical ? size / 2 - a : 2 * a)
                    .offset(x: b == .left ? -(size / 4 + a / 2) : b == .right ? size / 4 + a / 2 : 0,
                            y: b == .up ? -(size / 4 + a / 2) : b == .down ? size / 4 + a / 2 : 0)
            }
        }
    }

    private func arrows(_ size: CGFloat) -> some View {
        ZStack {
            ForEach([PadButton.up, .right, .down, .left], id: \.self) { b in
                Image(systemName: "arrowtriangle.up.fill")
                    .font(.system(size: size * 0.1))
                    .foregroundStyle(sim.held.contains(b) ? Color.white.opacity(0.85) : Color.marking.opacity(0.75))
                    .offset(y: -size * 0.34)
                    .rotationEffect(.degrees(b == .up ? 0 : b == .right ? 90 : b == .down ? 180 : 270))
            }
        }
    }

    private func direction(_ p: CGPoint, _ size: CGFloat) -> PadButton {
        let dx = p.x - size / 2, dy = p.y - size / 2
        return abs(dx) > abs(dy) ? (dx > 0 ? .right : .left) : (dy > 0 ? .down : .up)
    }

    private func roll(to next: PadButton?) {
        guard next != current else { return }
        if let current { sim.release(current) }
        if let next { sim.press(next) }
        current = next
    }
}

private struct Cross: Shape {
    /// Half an arm's width, as a share of half the cross
    let arm: CGFloat

    func path(in r: CGRect) -> Path {
        let c = CGPoint(x: r.midX, y: r.midY), h = r.width / 2, a = h * arm
        var p = Path()
        p.addRoundedRect(in: CGRect(x: c.x - a, y: r.minY, width: 2 * a, height: r.height),
                         cornerSize: CGSize(width: a * 0.25, height: a * 0.25))
        p.addRoundedRect(in: CGRect(x: r.minX, y: c.y - a, width: r.width, height: 2 * a),
                         cornerSize: CGSize(width: a * 0.25, height: a * 0.25))
        return p
    }
}

/// The VM's screen as a layer, its contents swapped for each frame without
/// SwiftUI drawing anything
struct ScreenLayer: NSViewRepresentable {
    let sim: Simulator

    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        view.wantsLayer = true
        let layer = view.layer!
        layer.backgroundColor = NSColor.black.cgColor
        layer.contentsGravity = .resize
        layer.magnificationFilter = .nearest
        layer.minificationFilter = .trilinear
        layer.contents = sim.frame
        sim.frameSink = { [weak layer] image in
            CATransaction.begin()
            CATransaction.setDisableActions(true)
            layer?.contents = image
            CATransaction.commit()
        }
        return view
    }

    func updateNSView(_ view: NSView, context: Context) {}
}
