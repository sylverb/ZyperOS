// The Brick's closed files (closed.rs): their names, where they are in
// Creality's Sonic Pad SDK and their sums, from plastron's own lists, so the
// installer checks exactly what plastron's build checks.
fn closed_list() {
    use std::fmt::Write as _;
    let lists = [
        ("../../plastron/buildroot/package/pvr-ge8300/files.sha256", "package/libs/libgpu/ge8300/fbdev/glibc/"),
        ("../../plastron/buildroot/package/xr829-firmware/files.sha256", "package/firmware/linux-firmware/xr829/"),
    ];
    let mut out = String::from("pub const LIST: &[ClosedFile] = &[\n");
    for (list, folder) in lists {
        println!("cargo:rerun-if-changed={list}");
        let text = std::fs::read_to_string(list).unwrap_or_else(|e| panic!("reading {list}: {e}"));
        for line in text.lines().filter(|l| !l.trim().is_empty()) {
            let mut f = line.split_whitespace();
            let (sum, path) = (f.next().expect("a sum"), f.next().expect("a path"));
            let name = path.rsplit('/').next().expect("a name");
            writeln!(out, "    ClosedFile {{ name: {name:?}, path: \"{folder}{path}\", sha256: {sum:?} }},").unwrap();
        }
    }
    out.push_str("];\n");
    let dest = std::path::Path::new(&std::env::var("OUT_DIR").unwrap()).join("closed_list.rs");
    std::fs::write(dest, out).unwrap();
}

fn main() {
    closed_list();
    // Windows runs the installer as administrator: see windows-app.manifest
    let windows = tauri_build::WindowsAttributes::new().app_manifest(include_str!("windows-app.manifest"));
    tauri_build::try_build(tauri_build::Attributes::new().windows_attributes(windows))
        .expect("tauri build step failed");
}
