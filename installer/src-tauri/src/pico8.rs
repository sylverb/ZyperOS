// The player's own PICO-8: Lexaloffle's program, which they bought, from the
// "PICO-8 for Raspberry Pi" zip they downloaded (pico-8_0.2.7_raspi.zip). The
// installer never downloads it, ships it or keeps a copy: it reads only the
// zip the player picks on this computer, checks it, and copies two of its
// files to that player's card, pico8_64 (the 64-bit ARM build, the one the
// handheld runs) and pico8.dat, with its license.txt and a version.txt.
//
// The zip has one folder, pico-8/, with the 32-bit build (pico8), pico8_dyn,
// pico8_gpio, pico8_64, pico8.dat, the manual (pico-8_manual.txt; pico-8.txt
// in older zips), readme_raspi.txt, license.txt and lexaloffle-pico8.png. Any
// folder name is taken, but the files all come from one folder. The other
// zips (Windows, Mac, Linux on a PC) and the old Raspberry Pi ones without
// pico8_64 are refused, each with its own sentence for the page.
//
// Where the files go: on a card that has started, Bios/PICO-8 on its ZYPEROS
// volume, an ordinary copy, each file to a temporary name, read back and
// compared, then renamed into place (copy_to). On a card that never started
// (a fresh install, which every one is), there is no ZYPEROS yet: the files
// ride along in the image's boot partition, in pico8/, the way closed.rs
// puts the Brick's files in closed/, and the handheld's first start moves
// them to Bios/PICO-8.

use std::io::{self, Read, Seek};
use std::path::{Path, PathBuf};

use sha2::{Digest, Sha256};

use crate::writer::Progress;

/// Where PICO-8 goes on the games partition
pub const BIOS: [&str; 2] = ["Bios", "PICO-8"];

/// Its folder on the image's boot partition, for a card that never started
pub const FOLDER: &str = "pico8";

/// What the handheld runs, and its data
pub const PROGRAM: &str = "pico8_64";
pub const DATA: &str = "pico8.dat";
const LICENSE: &str = "license.txt";
const VERSION: &str = "version.txt";

/// A sane size for pico8_64 and pico8.dat: 0.2.7's are a few MB each
const MIN_BYTES: u64 = 100 << 10;
const MAX_BYTES: u64 = 64 << 20;
/// The license and the manual's first lines
const MAX_TEXT: u64 = 1 << 20;

pub const WINDOWS: &str =
    "This is PICO-8 for Windows. Choose the Raspberry Pi zip: only it has pico8_64, the build the handheld runs.";
pub const MAC: &str =
    "This is PICO-8 for Mac. Choose the Raspberry Pi zip: only it has pico8_64, the build the handheld runs.";
pub const LINUX_PC: &str =
    "This is PICO-8 for Linux on a PC. Choose the Raspberry Pi zip: only it has pico8_64, the build the handheld runs.";
pub const TOO_OLD: &str =
    "This PICO-8 is too old to have pico8_64. Download the newest Raspberry Pi zip from your Lexaloffle account.";
pub const NONE: &str = "There is no PICO-8 in this zip.";

/// The versions whose pico8_64 shows a black screen on the Pixel 2's chip
/// (RK3326)
const BLACK: [&str; 2] = ["0.2.5d", "0.2.5e"];

fn black(version: &str) -> String {
    format!(
        "PICO-8 {version} shows a black screen on this handheld. Download the newest Raspberry Pi zip from your \
         Lexaloffle account."
    )
}

/// A zip the player chose, as the page shows it
#[derive(serde::Serialize, Clone, Debug, PartialEq)]
pub struct Chosen {
    pub path: String,
    pub name: String,
    /// "0.2.7", or "" when neither the manual nor the zip's name says
    pub version: String,
    /// Why it can't be used, in the page's words; None when it can
    pub refused: Option<String>,
}

/// PICO-8 out of the zip, checked: the files for the card, by their names
/// there, each with its sum
#[derive(Debug)]
pub struct Pico8 {
    pub version: String,
    /// pico8_64 and pico8.dat first, then license.txt when the zip has it,
    /// then version.txt
    pub files: Vec<(String, Vec<u8>)>,
}

/// A zip entry that is a file: the folder it is in and its own name, with
/// '/' between a folder's parts
struct Entry {
    dir: String,
    base: String,
    index: usize,
    size: u64,
}

fn entries<R: Read + Seek>(zip: &mut zip::ZipArchive<R>) -> Vec<Entry> {
    let mut out = Vec::new();
    for index in 0..zip.len() {
        // The raw entry: its name and size, whatever its compression
        let Ok(f) = zip.by_index_raw(index) else { continue };
        if f.is_dir() || f.is_symlink() {
            continue;
        }
        let Ok(name) = f.name() else { continue };
        let name = name.replace('\\', "/");
        let mut parts: Vec<&str> = name.split('/').filter(|p| !p.is_empty()).collect();
        // What a Mac adds when it makes a zip: never the files themselves
        if parts.first().is_some_and(|p| p.eq_ignore_ascii_case("__MACOSX")) {
            continue;
        }
        let Some(base) = parts.pop() else { continue };
        out.push(Entry { dir: parts.join("/"), base: base.to_string(), index, size: f.size() });
    }
    out
}

/// An entry's bytes, whole, when it is at most `max` (and the zip's own
/// check of them, its CRC, holds)
fn whole<R: Read + Seek>(zip: &mut zip::ZipArchive<R>, e: &Entry, max: u64) -> Option<Vec<u8>> {
    if e.size > max {
        return None;
    }
    let f = zip.by_index(e.index).ok()?;
    let mut v = Vec::with_capacity(e.size as usize);
    f.take(max + 1).read_to_end(&mut v).ok()?;
    (v.len() as u64 == e.size).then_some(v)
}

/// An entry's first bytes
fn head<R: Read + Seek>(zip: &mut zip::ZipArchive<R>, e: &Entry, n: u64) -> Option<Vec<u8>> {
    let f = zip.by_index(e.index).ok()?;
    let mut v = Vec::new();
    f.take(n).read_to_end(&mut v).ok()?;
    Some(v)
}

/// An ELF file's machine, little-endian ones only: 0xB7 AArch64, 0x28 32-bit
/// ARM, 0x3E x86-64, 0x03 i386
fn elf_machine(b: &[u8]) -> Option<u16> {
    (b.len() >= 20 && b[..4] == [0x7F, b'E', b'L', b'F'] && b[5] == 1).then(|| u16::from_le_bytes([b[18], b[19]]))
}

/// pico8_64 as the handheld needs it: an ELF64, little-endian, AArch64
/// program (an executable or a position-independent one)
fn is_arm64_program(b: &[u8]) -> bool {
    b.len() >= 20 && b[4] == 2 && elf_machine(b) == Some(0xB7) && matches!(u16::from_le_bytes([b[16], b[17]]), 2 | 3)
}

/// "0.2.7" from a "PICO-8 v0.2.7" token, as the manual's first lines have it
fn version_in(text: &str) -> Option<String> {
    for line in text.lines().take(20) {
        let lower = line.to_ascii_lowercase();
        let Some(at) = lower.find("pico-8 v") else { continue };
        if let Some(v) = token(&lower[at + "pico-8 v".len()..]) {
            return Some(v);
        }
    }
    None
}

/// A version at the start of `s`: digits, letters and dots, from a digit,
/// with a dot in it ("0.2.7", "0.2.5e")
fn token(s: &str) -> Option<String> {
    let v: String = s.chars().take_while(|c| c.is_ascii_alphanumeric() || *c == '.').collect();
    let v = v.trim_end_matches('.').to_ascii_lowercase();
    (v.len() <= 16 && v.starts_with(|c: char| c.is_ascii_digit()) && v.contains('.')).then_some(v)
}

/// The version in a zip's name, pico-8_<version>_raspi.zip
fn version_in_name(name: &str) -> Option<String> {
    let lower = name.to_ascii_lowercase();
    let rest = lower.strip_prefix("pico-8_").or_else(|| lower.strip_prefix("pico8_"))?;
    token(rest.split('_').next()?)
}

/// PICO-8 for Raspberry Pi from a zip's bytes (`name` the zip's file name,
/// for the version when its manual has none), or why not, in the page's
/// words
pub fn read<R: Read + Seek>(reader: R, name: &str) -> Result<Pico8, String> {
    let mut zip = zip::ZipArchive::new(reader).map_err(|_| NONE.to_string())?;
    let all = entries(&mut zip);
    fn named<'a>(all: &'a [Entry], base: &'a str) -> impl Iterator<Item = &'a Entry> {
        all.iter().filter(move |e| e.base.eq_ignore_ascii_case(base))
    }
    // The folder with pico8_64 and pico8.dat, the shallowest first
    let mut programs: Vec<&Entry> = named(&all, PROGRAM).collect();
    programs.sort_by_key(|e| (e.dir.matches('/').count(), e.dir.clone()));
    for program in programs {
        let in_dir = |base: &str| all.iter().find(|e| e.dir == program.dir && e.base.eq_ignore_ascii_case(base));
        let Some(data) = in_dir(DATA) else { continue };
        if !(MIN_BYTES..=MAX_BYTES).contains(&program.size) || !(MIN_BYTES..=MAX_BYTES).contains(&data.size) {
            continue;
        }
        let Some(program_bytes) = whole(&mut zip, program, MAX_BYTES).filter(|b| is_arm64_program(b)) else {
            continue;
        };
        let Some(data_bytes) = whole(&mut zip, data, MAX_BYTES) else { continue };
        let manual = in_dir("pico-8_manual.txt").or_else(|| in_dir("pico-8.txt"));
        let version = manual
            .and_then(|m| head(&mut zip, m, 8192))
            .and_then(|b| version_in(&String::from_utf8_lossy(&b)))
            .or_else(|| version_in_name(name))
            .unwrap_or_default();
        if BLACK.contains(&version.as_str()) {
            return Err(black(&version));
        }
        let mut files = vec![(PROGRAM.to_string(), program_bytes), (DATA.to_string(), data_bytes)];
        if let Some(license) = in_dir(LICENSE).and_then(|l| whole(&mut zip, l, MAX_TEXT)) {
            files.push((LICENSE.to_string(), license));
        }
        files.push((VERSION.to_string(), format!("{version}\n").into_bytes()));
        return Ok(Pico8 { version, files });
    }
    // Not the Raspberry Pi build: which one it is, to say what to choose
    if named(&all, "pico8.exe").next().is_some() {
        return Err(WINDOWS.into());
    }
    if all.iter().any(|e| e.dir.split('/').any(|p| p.eq_ignore_ascii_case("PICO-8.app"))) {
        return Err(MAC.into());
    }
    if named(&all, PROGRAM).next().is_none() {
        let machines: Vec<u16> = named(&all, "pico8")
            .filter_map(|e| head(&mut zip, e, 64).as_deref().and_then(elf_machine))
            .collect();
        if machines.iter().any(|m| matches!(m, 0x3E | 0x03)) {
            return Err(LINUX_PC.into());
        }
        if machines.contains(&0x28) {
            return Err(TOO_OLD.into());
        }
    }
    Err(NONE.into())
}

/// A file opened for reading, never through a link at its name: on Linux the
/// helper reads the zip as root, from a path the window gave it
fn open_no_follow(path: &Path) -> io::Result<std::fs::File> {
    let mut opts = std::fs::OpenOptions::new();
    opts.read(true);
    #[cfg(unix)]
    std::os::unix::fs::OpenOptionsExt::custom_flags(&mut opts, libc::O_NOFOLLOW);
    let file = opts.open(path)?;
    if !file.metadata()?.is_file() {
        return Err(io::Error::other("not a file"));
    }
    Ok(file)
}

/// PICO-8 out of the zip at `path`, checked (read), or why not. Called each
/// time it is needed, so what goes on the card is what was checked just
/// before.
pub fn load(path: &Path) -> Result<Pico8, String> {
    let name = path.file_name().unwrap_or_default().to_string_lossy().into_owned();
    let file = open_no_follow(path).map_err(|_| NONE.to_string())?;
    read(io::BufReader::new(file), &name)
}

/// What the page shows of the zip at `path`
pub fn look(path: &Path) -> Chosen {
    // Its real path, so the helper (which follows no link) opens this one
    let path = std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    let name = path.file_name().unwrap_or_default().to_string_lossy().into_owned();
    let (version, refused) = match load(&path) {
        Ok(p) => (p.version, None),
        Err(e) => (String::new(), Some(e)),
    };
    Chosen { path: path.to_string_lossy().into_owned(), name, version, refused }
}

/// Which PICO-8 a mounted games partition has in Bios/PICO-8: its
/// version.txt's version ("" without one), or None when there is no
/// pico8_64
pub fn on_card(root: &Path) -> Option<String> {
    let dir = BIOS.iter().fold(root.to_path_buf(), |p, part| p.join(part));
    if !dir.join(PROGRAM).is_file() {
        return None;
    }
    let text = std::fs::read(dir.join(VERSION)).unwrap_or_default();
    let text = String::from_utf8_lossy(&text[..text.len().min(64)]).into_owned();
    Some(token(text.trim()).unwrap_or_default())
}

fn sum(bytes: &[u8]) -> [u8; 32] {
    Sha256::digest(bytes).into()
}

/// A file's bytes as the card has them, not as this computer's memory does:
/// the Mac reads past its cache, Linux drops the file's cached pages first
fn read_back(path: &Path) -> io::Result<Vec<u8>> {
    let mut file = std::fs::File::open(path)?;
    #[cfg(target_os = "macos")]
    {
        use std::os::unix::io::AsRawFd;
        // SAFETY: a valid descriptor, and F_NOCACHE takes an int
        unsafe { libc::fcntl(file.as_raw_fd(), libc::F_NOCACHE, 1) };
    }
    #[cfg(target_os = "linux")]
    {
        use std::os::unix::io::AsRawFd;
        // SAFETY: a valid descriptor; advice only
        unsafe { libc::posix_fadvise(file.as_raw_fd(), 0, 0, libc::POSIX_FADV_DONTNEED) };
    }
    let mut v = Vec::new();
    file.read_to_end(&mut v)?;
    Ok(v)
}

/// `bytes` into a new file at `path`, on the card when it returns
fn write_synced(path: &Path, bytes: &[u8]) -> io::Result<()> {
    let _ = std::fs::remove_file(path);
    let mut file = std::fs::OpenOptions::new().write(true).create_new(true).open(path)?;
    io::Write::write_all(&mut file, bytes)?;
    file.sync_all()
}

/// The temporary name a file is written to first, in the same folder
fn part(dir: &Path, name: &str) -> PathBuf {
    dir.join(format!("{name}.part"))
}

/// PICO-8 into Bios/PICO-8 on a mounted games partition: every file to a
/// temporary name, then each read back and compared with the zip's, then all
/// renamed into place, so a copy that fails or is stopped leaves the card's
/// PICO-8 as it was. Progress counts pico8_64 and pico8.dat (Pico8 while
/// they are written, Pico8Check while they are read back).
pub fn copy_to(
    volume: &Path,
    p8: &Pico8,
    progress: &mut dyn FnMut(Progress),
    cancelled: &dyn Fn() -> bool,
) -> Result<(), String> {
    let dir = BIOS.iter().fold(volume.to_path_buf(), |p, part| p.join(part));
    std::fs::create_dir_all(&dir).map_err(|e| format!("Making Bios/PICO-8 on the card: {e}. Nothing was written."))?;
    let clean = || {
        for (name, _) in &p8.files {
            let _ = std::fs::remove_file(part(&dir, name));
        }
    };
    let big = 2u64;
    let step = |i: usize| (i as u64).min(big);
    let result = (|| {
        for (i, (name, bytes)) in p8.files.iter().enumerate() {
            if cancelled() {
                return Err("cancelled".to_string());
            }
            progress(Progress::Pico8 { done: step(i), total: big });
            write_synced(&part(&dir, name), bytes).map_err(|e| format!("Copying {name} to the card: {e}"))?;
        }
        progress(Progress::Pico8 { done: big, total: big });
        for (i, (name, bytes)) in p8.files.iter().enumerate() {
            if cancelled() {
                return Err("cancelled".to_string());
            }
            progress(Progress::Pico8Check { done: step(i), total: big });
            let back = read_back(&part(&dir, name)).map_err(|e| format!("Reading {name} back from the card: {e}"))?;
            if sum(&back) != sum(bytes) {
                return Err(format!(
                    "The card didn't read back {name} as the zip has it. The card may be failing: try again, or \
                     another card."
                ));
            }
        }
        progress(Progress::Pico8Check { done: big, total: big });
        Ok(())
    })();
    if let Err(e) = result {
        clean();
        return Err(e);
    }
    for (name, _) in &p8.files {
        let (from, to) = (part(&dir, name), dir.join(name));
        std::fs::rename(&from, &to)
            .or_else(|_| std::fs::remove_file(&to).and_then(|_| std::fs::rename(&from, &to)))
            .map_err(|e| {
                clean();
                format!("Putting {name} in Bios/PICO-8: {e}")
            })?;
    }
    Ok(())
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use std::io::{Cursor, Write};
    use zip::write::SimpleFileOptions;

    /// An ELF header's first 64 bytes for `machine`, then filler: never
    /// Lexaloffle's program, only its shape
    pub(crate) fn elf(class: u8, machine: u16, len: usize) -> Vec<u8> {
        let mut b = vec![0u8; len];
        b[..4].copy_from_slice(&[0x7F, b'E', b'L', b'F']);
        b[4] = class;
        b[5] = 1;
        b[6] = 1;
        b[16..18].copy_from_slice(&3u16.to_le_bytes());
        b[18..20].copy_from_slice(&machine.to_le_bytes());
        for (i, x) in b.iter_mut().enumerate().skip(64) {
            *x = (i * 13 % 251) as u8;
        }
        b
    }

    /// Made-up data the size of a pico8.dat
    pub(crate) fn dat(len: usize) -> Vec<u8> {
        (0..len).map(|i| (i * 7 % 253) as u8).collect()
    }

    pub(crate) const MANUAL: &str = "====================\n\n\tPICO-8 v0.2.7\n\thttps://www.pico-8.com\n\t(c) Copyright\n";

    /// A zip made here, with these entries (name, bytes), deflated
    pub(crate) fn zip_of(files: &[(&str, &[u8])]) -> Vec<u8> {
        let mut w = zip::ZipWriter::new(Cursor::new(Vec::new()));
        let opts = SimpleFileOptions::default().compression_method(zip::CompressionMethod::Deflated);
        for (name, bytes) in files {
            if name.ends_with('/') {
                w.add_directory(name.trim_end_matches('/'), opts).unwrap();
            } else {
                w.start_file(*name, opts).unwrap();
                w.write_all(bytes).unwrap();
            }
        }
        w.finish().unwrap().into_inner()
    }

    /// A Raspberry Pi zip as Lexaloffle's are laid out, with made-up files
    pub(crate) fn raspi(manual: &str) -> Vec<u8> {
        let program = elf(2, 0xB7, 300 << 10);
        let arm32 = elf(1, 0x28, 200 << 10);
        let data = dat(400 << 10);
        zip_of(&[
            ("pico-8/", b""),
            ("pico-8/pico8", &arm32),
            ("pico-8/pico8_dyn", &arm32),
            ("pico-8/pico8_gpio", &arm32),
            ("pico-8/pico8_64", &program),
            ("pico-8/pico8.dat", &data),
            ("pico-8/pico-8_manual.txt", manual.as_bytes()),
            ("pico-8/readme_raspi.txt", b"readme"),
            ("pico-8/license.txt", b"the license"),
            ("pico-8/lexaloffle-pico8.png", b"png"),
        ])
    }

    fn check(zip: Vec<u8>, name: &str) -> Result<Pico8, String> {
        read(Cursor::new(zip), name)
    }

    #[test]
    fn takes_the_raspberry_pi_zip() {
        let p = check(raspi(MANUAL), "pico-8_0.2.7_raspi.zip").unwrap();
        assert_eq!(p.version, "0.2.7");
        let names: Vec<&str> = p.files.iter().map(|(n, _)| n.as_str()).collect();
        assert_eq!(names, ["pico8_64", "pico8.dat", "license.txt", "version.txt"]);
        assert_eq!(p.files[0].1, elf(2, 0xB7, 300 << 10), "pico8_64, and not the 32-bit pico8");
        assert_eq!(p.files[1].1, dat(400 << 10));
        assert_eq!(p.files[2].1, b"the license");
        assert_eq!(p.files[3].1, b"0.2.7\n");
    }

    #[test]
    fn takes_any_folder_but_one_folder() {
        let program = elf(2, 0xB7, 200 << 10);
        let data = dat(200 << 10);
        // Another top folder's name, and the files at the zip's top
        assert!(check(zip_of(&[("PICO8 copy/pico8_64", &program), ("PICO8 copy/pico8.dat", &data)]), "x.zip").is_ok());
        assert!(check(zip_of(&[("pico8_64", &program), ("pico8.dat", &data)]), "x.zip").is_ok());
        // pico8_64 and pico8.dat in two folders: not one PICO-8
        let apart = zip_of(&[("a/pico8_64", &program), ("b/pico8.dat", &data)]);
        assert_eq!(check(apart, "x.zip").unwrap_err(), NONE);
        // What a Mac adds when it zips a folder again is not taken
        let resourced = zip_of(&[
            ("__MACOSX/pico-8/pico8_64", &program),
            ("__MACOSX/pico-8/pico8.dat", &data),
            ("pico-8/pico8_64", &program),
            ("pico-8/pico8.dat", &data),
        ]);
        assert_eq!(check(resourced, "x.zip").unwrap().files[0].1, program);
    }

    #[test]
    fn reads_the_version_from_the_manual_or_the_name() {
        assert_eq!(version_in(MANUAL).as_deref(), Some("0.2.7"));
        assert_eq!(version_in("PICO-8 v0.2.6b\n").as_deref(), Some("0.2.6b"));
        assert_eq!(version_in("pico-8 V0.1.12c.").as_deref(), Some("0.1.12c"));
        assert_eq!(version_in("PICO-8 manual\n"), None);
        assert_eq!(version_in_name("pico-8_0.2.6b_raspi.zip").as_deref(), Some("0.2.6b"));
        assert_eq!(version_in_name("PICO-8_0.2.7_raspi (1).zip").as_deref(), Some("0.2.7"));
        assert_eq!(version_in_name("raspi.zip"), None);
        // An older zip's manual is pico-8.txt
        let program = elf(2, 0xB7, 200 << 10);
        let data = dat(200 << 10);
        let old = zip_of(&[("pico-8/pico8_64", &program), ("pico-8/pico8.dat", &data), ("pico-8/pico-8.txt", b"PICO-8 v0.2.6\n")]);
        assert_eq!(check(old, "pico.zip").unwrap().version, "0.2.6");
        // No manual: the name; neither: none, and "PICO-8" alone on the page
        let bare = zip_of(&[("pico-8/pico8_64", &program), ("pico-8/pico8.dat", &data)]);
        assert_eq!(check(bare.clone(), "pico-8_0.2.6b_raspi.zip").unwrap().version, "0.2.6b");
        let p = check(bare, "my pico.zip").unwrap();
        assert_eq!(p.version, "");
        assert_eq!(p.files.last().unwrap(), &("version.txt".to_string(), b"\n".to_vec()));
        assert_eq!(p.files.len(), 3, "no license.txt in this zip: none goes");
    }

    #[test]
    fn refuses_the_other_zips() {
        let data = dat(300 << 10);
        let windows = zip_of(&[("pico-8/pico8.exe", b"MZ"), ("pico-8/pico8.dat", &data), ("pico-8/SDL2.dll", b"MZ")]);
        assert_eq!(check(windows, "pico-8_0.2.7_windows.zip").unwrap_err(), WINDOWS);
        let mac = zip_of(&[
            ("PICO-8.app/", b""),
            ("PICO-8.app/Contents/MacOS/pico8", b"\xCF\xFA\xED\xFE"),
            ("PICO-8.app/Contents/Resources/pico8.dat", &data),
        ]);
        assert_eq!(check(mac, "pico-8_0.2.7_osx.zip").unwrap_err(), MAC);
        let amd64 = elf(2, 0x3E, 300 << 10);
        let linux = zip_of(&[("pico-8/pico8", &amd64), ("pico-8/pico8_dyn", &amd64), ("pico-8/pico8.dat", &data)]);
        assert_eq!(check(linux, "pico-8_0.2.7_amd64.zip").unwrap_err(), LINUX_PC);
        let i386 = elf(1, 0x03, 300 << 10);
        let linux32 = zip_of(&[("pico-8/pico8", &i386), ("pico-8/pico8.dat", &data)]);
        assert_eq!(check(linux32, "pico-8_0.1.11_i386.zip").unwrap_err(), LINUX_PC);
        let arm32 = elf(1, 0x28, 300 << 10);
        let old = zip_of(&[("pico-8/pico8", &arm32), ("pico-8/pico8.dat", &data)]);
        assert_eq!(check(old, "pico-8_0.1.12c_raspi.zip").unwrap_err(), TOO_OLD);
        let photos = zip_of(&[("beach/1.jpg", b"jpeg"), ("beach/2.jpg", b"jpeg")]);
        assert_eq!(check(photos, "beach-photos.zip").unwrap_err(), NONE);
        assert_eq!(check(b"not a zip at all".to_vec(), "pico-8_0.2.7_raspi.zip").unwrap_err(), NONE);
        assert_eq!(check(Vec::new(), "empty.zip").unwrap_err(), NONE);
    }

    #[test]
    fn refuses_the_versions_with_a_black_screen() {
        for v in ["0.2.5d", "0.2.5e"] {
            let e = check(raspi(&format!("PICO-8 v{v}\n")), "pico.zip").unwrap_err();
            assert_eq!(e, format!("PICO-8 {v} shows a black screen on this handheld. Download the newest Raspberry Pi zip from your Lexaloffle account."));
        }
        // From the name too, when the manual says nothing
        let e = check(raspi("A manual\n"), "pico-8_0.2.5e_raspi.zip").unwrap_err();
        assert!(e.starts_with("PICO-8 0.2.5e shows a black screen"), "{e}");
        assert!(check(raspi("PICO-8 v0.2.5c\n"), "pico.zip").is_ok());
        assert!(check(raspi("PICO-8 v0.2.5g\n"), "pico.zip").is_ok());
    }

    #[test]
    fn checks_pico8_64_itself() {
        let data = dat(300 << 10);
        let with = |program: &[u8]| check(zip_of(&[("pico-8/pico8_64", program), ("pico-8/pico8.dat", &data)]), "p.zip");
        assert!(with(&elf(2, 0xB7, 300 << 10)).is_ok());
        // An x86-64 program, a 32-bit one, a big-endian one, not a program
        assert_eq!(with(&elf(2, 0x3E, 300 << 10)).unwrap_err(), NONE);
        assert_eq!(with(&elf(1, 0xB7, 300 << 10)).unwrap_err(), NONE);
        let mut big_endian = elf(2, 0xB7, 300 << 10);
        big_endian[5] = 2;
        assert_eq!(with(&big_endian).unwrap_err(), NONE);
        let mut object = elf(2, 0xB7, 300 << 10);
        object[16] = 1;
        assert_eq!(with(&object).unwrap_err(), NONE, "a relocatable object, not a program");
        assert_eq!(with(&dat(300 << 10)).unwrap_err(), NONE);
        // Too small to be PICO-8 (a header and nothing), and pico8.dat too
        assert_eq!(with(&elf(2, 0xB7, 4096)).unwrap_err(), NONE);
        let tiny = zip_of(&[("pico-8/pico8_64", &elf(2, 0xB7, 300 << 10)), ("pico-8/pico8.dat", b"small")]);
        assert_eq!(check(tiny, "p.zip").unwrap_err(), NONE);
        // No pico8.dat
        let alone = zip_of(&[("pico-8/pico8_64", &elf(2, 0xB7, 300 << 10))]);
        assert_eq!(check(alone, "p.zip").unwrap_err(), NONE);
    }

    #[test]
    fn a_zip_on_disk_is_looked_at_and_loaded() {
        let dir = temp("look");
        let path = dir.join("pico-8_0.2.7_raspi.zip");
        std::fs::write(&path, raspi(MANUAL)).unwrap();
        let c = look(&path);
        assert_eq!((c.name.as_str(), c.version.as_str(), c.refused.as_deref()), ("pico-8_0.2.7_raspi.zip", "0.2.7", None));
        let bad = dir.join("pico-8_0.2.7_windows.zip");
        std::fs::write(&bad, zip_of(&[("pico-8/pico8.exe", b"MZ")])).unwrap();
        assert_eq!(look(&bad).refused.as_deref(), Some(WINDOWS));
        assert_eq!(look(&dir.join("gone.zip")).refused.as_deref(), Some(NONE));
        assert_eq!(look(&dir).refused.as_deref(), Some(NONE), "a folder is not a zip");
        // The helper follows no link to a zip
        #[cfg(unix)]
        {
            let link = dir.join("link.zip");
            std::os::unix::fs::symlink(&path, &link).unwrap();
            assert!(load(&link).is_err());
            assert_eq!(look(&link).path, std::fs::canonicalize(&path).unwrap().to_string_lossy(), "its real path");
        }
    }

    pub(crate) fn temp(name: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("zyperos-pico8-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn listing(dir: &Path) -> Vec<String> {
        let mut v: Vec<String> =
            std::fs::read_dir(dir).unwrap().map(|e| e.unwrap().file_name().to_string_lossy().into_owned()).collect();
        v.sort();
        v
    }

    #[test]
    fn copies_into_bios_pico8_through_temporary_names() {
        let volume = temp("volume");
        let p = check(raspi(MANUAL), "pico-8_0.2.7_raspi.zip").unwrap();
        let mut seen = Vec::new();
        // Halfway through, while the files are read back: each one is still
        // under its temporary name, and none is in place
        let dir = volume.join("Bios/PICO-8");
        let mut looked = false;
        copy_to(
            &volume,
            &p,
            &mut |s| {
                if let Progress::Pico8Check { done: 1, .. } = s {
                    assert_eq!(
                        listing(&dir),
                        ["license.txt.part", "pico8.dat.part", "pico8_64.part", "version.txt.part"]
                    );
                    looked = true;
                }
                seen.push(s);
            },
            &|| false,
        )
        .unwrap();
        assert!(looked);
        assert_eq!(listing(&dir), ["license.txt", "pico8.dat", "pico8_64", "version.txt"]);
        assert_eq!(std::fs::read(dir.join("pico8_64")).unwrap(), p.files[0].1);
        assert_eq!(std::fs::read(dir.join("pico8.dat")).unwrap(), p.files[1].1);
        assert_eq!(std::fs::read_to_string(dir.join("version.txt")).unwrap(), "0.2.7\n");
        assert_eq!(seen.first(), Some(&Progress::Pico8 { done: 0, total: 2 }));
        assert!(seen.contains(&Progress::Pico8 { done: 1, total: 2 }));
        assert_eq!(seen.last(), Some(&Progress::Pico8Check { done: 2, total: 2 }));
        // And the card now says it has it
        assert_eq!(on_card(&volume).as_deref(), Some("0.2.7"));

        // A newer one over it: replaced whole
        let newer = check(raspi("PICO-8 v0.2.8\n"), "pico.zip").unwrap();
        copy_to(&volume, &newer, &mut |_| {}, &|| false).unwrap();
        assert_eq!(on_card(&volume).as_deref(), Some("0.2.8"));
        assert_eq!(listing(&dir), ["license.txt", "pico8.dat", "pico8_64", "version.txt"]);
    }

    #[test]
    fn a_stopped_copy_leaves_the_card_as_it_was() {
        let volume = temp("stopped");
        let old = check(raspi(MANUAL), "p.zip").unwrap();
        copy_to(&volume, &old, &mut |_| {}, &|| false).unwrap();
        let newer = check(raspi("PICO-8 v0.2.8\n"), "p.zip").unwrap();
        // Stopped once both are written, before the read-back
        let stop = std::cell::Cell::new(false);
        let e = copy_to(
            &volume,
            &newer,
            &mut |s| {
                if s == (Progress::Pico8 { done: 2, total: 2 }) {
                    stop.set(true);
                }
            },
            &|| stop.get(),
        )
        .unwrap_err();
        assert_eq!(e, "cancelled");
        let dir = volume.join("Bios/PICO-8");
        assert_eq!(listing(&dir), ["license.txt", "pico8.dat", "pico8_64", "version.txt"], "no .part left");
        assert_eq!(on_card(&volume).as_deref(), Some("0.2.7"));
    }

    /// A boot partition as the Pixel 2's image has it, `mib` MiB: FAT,
    /// PX2BOOT, a/ with its kernel (`kernel` bytes of it), bootstate.txt
    fn px2boot(mib: usize, kernel: usize) -> Vec<u8> {
        let mut disk = Cursor::new(vec![0u8; mib << 20]);
        fatfs::format_volume(&mut disk, fatfs::FormatVolumeOptions::new().volume_label(*b"PX2BOOT    ")).unwrap();
        {
            let fs = fatfs::FileSystem::new(&mut disk, fatfs::FsOptions::new()).unwrap();
            {
                let a = fs.root_dir().create_dir("a").unwrap();
                a.create_file("Image").unwrap().write_all(&dat(kernel)).unwrap();
                fs.root_dir().create_file("bootstate.txt").unwrap().write_all(b"slot=a\n").unwrap();
            }
            fs.unmount().unwrap();
        }
        disk.into_inner()
    }

    /// A Pixel 2 image: an MBR, its boot partition at 1 MiB, then a root
    fn pixel_image(boot: &[u8]) -> Vec<u8> {
        let mut img = vec![0x5Au8; (1 << 20) + boot.len() + (1 << 20)];
        img[..512].fill(0);
        let e = 446;
        img[e + 4] = 0x0C;
        img[e + 8..e + 12].copy_from_slice(&2048u32.to_le_bytes());
        img[e + 12..e + 16].copy_from_slice(&((boot.len() / 512) as u32).to_le_bytes());
        img[e + 16 + 4] = 0x83;
        img[e + 16 + 8..e + 16 + 12].copy_from_slice(&((2048 + boot.len() / 512) as u32).to_le_bytes());
        img[e + 16 + 12..e + 16 + 16].copy_from_slice(&2048u32.to_le_bytes());
        img[510] = 0x55;
        img[511] = 0xAA;
        img[1 << 20..(1 << 20) + boot.len()].copy_from_slice(boot);
        img
    }

    fn opener(img: Vec<u8>) -> impl Fn() -> io::Result<Box<dyn Read + Send>> {
        move || Ok(Box::new(Cursor::new(img.clone())) as Box<dyn Read + Send>)
    }

    #[test]
    fn rides_along_in_the_pixels_boot_partition() {
        use crate::closed::tests::read_in;
        let p = check(raspi(MANUAL), "pico-8_0.2.7_raspi.zip").unwrap();
        let boot = px2boot(8, 3 << 20);
        let image = pixel_image(&boot);
        let (at, new_boot) = crate::closed::boot_with(&opener(image.clone()), &[(FOLDER, &p.files)]).unwrap();
        assert_eq!((at, new_boot.len()), (1 << 20, boot.len()));
        assert_eq!(read_in(&new_boot, "pico8/pico8_64").unwrap(), p.files[0].1);
        assert_eq!(read_in(&new_boot, "pico8/pico8.dat").unwrap(), p.files[1].1);
        assert_eq!(read_in(&new_boot, "pico8/license.txt").unwrap(), b"the license");
        assert_eq!(read_in(&new_boot, "pico8/version.txt").unwrap(), b"0.2.7\n");
        assert_eq!(read_in(&new_boot, "a/Image").unwrap(), dat(3 << 20), "the kernel as it was");
        assert_eq!(read_in(&new_boot, "bootstate.txt").unwrap(), b"slot=a\n");
        // The image as the writer reads it, twice: the new boot partition in
        // place, the rest the image's own, still the Pixel 2's
        let read = || {
            let mut v = Vec::new();
            crate::closed::Overlay::new(Cursor::new(image.clone()), at, std::sync::Arc::new(new_boot.clone()))
                .read_to_end(&mut v)
                .unwrap();
            v
        };
        let written = read();
        assert_eq!(written, read());
        assert_eq!(written[..1 << 20], image[..1 << 20]);
        assert_eq!(written[(1 << 20) + boot.len()..], image[(1 << 20) + boot.len()..]);
        let dir = temp("ride");
        std::fs::write(dir.join("written.img"), &written).unwrap();
        assert_eq!(crate::image::device_of(&dir.join("written.img")).unwrap(), Some(crate::cards::Device::Pixel2));
    }

    #[test]
    fn rides_along_with_the_bricks_files_in_one_write() {
        use crate::closed::tests::{boot_fat, read_in};
        let p = check(raspi(MANUAL), "pico-8_0.2.7_raspi.zip").unwrap();
        let fat = boot_fat();
        let mut image = vec![0u8; (1 << 20) + fat.len() + (1 << 20)];
        image[(1 << 20)..(1 << 20) + fat.len()].copy_from_slice(&fat);
        let table = crate::writer::tests::gpt_with(&[(2048, (fat.len() as u64) / 512)]);
        image[..table.len()].copy_from_slice(&table);
        let closed = vec![("fw_xr829.bin".to_string(), b"wifi".to_vec())];
        let folders: [(&str, &crate::closed::Files); 2] = [(crate::closed::FOLDER, &closed), (FOLDER, &p.files)];
        let (at, boot) = crate::closed::boot_with(&opener(image), &folders).unwrap();
        assert_eq!(at, 1 << 20);
        assert_eq!(read_in(&boot, "closed/fw_xr829.bin").unwrap(), b"wifi");
        assert_eq!(read_in(&boot, "pico8/pico8_64").unwrap(), p.files[0].1);
        assert_eq!(read_in(&boot, "a/Image.lz4").unwrap(), b"kernel");
    }

    #[test]
    fn refuses_when_it_does_not_fit_in_the_boot_partition() {
        use crate::closed::tests::read_in;
        // 1.5 MB of PICO-8 (made up) into a 4 MiB boot partition with a
        // 3 MB kernel: it doesn't fit, and nothing is changed
        let program = elf(2, 0xB7, 700 << 10);
        let data = dat(800 << 10);
        let p = check(zip_of(&[("pico-8/pico8_64", &program), ("pico-8/pico8.dat", &data)]), "pico-8_0.2.7_raspi.zip")
            .unwrap();
        let full = px2boot(4, 3 << 20);
        let e = crate::closed::into_boot(full, &[(FOLDER, &p.files)]).unwrap_err();
        assert!(e.starts_with("Your PICO-8 doesn't fit in the card image's boot partition: it needs 1."), "{e}");
        assert!(e.contains("Add PICO-8") && e.ends_with("Nothing was written."), "{e}");
        // With room for it, it goes
        let roomy = px2boot(4, 1 << 20);
        let boot = crate::closed::into_boot(roomy, &[(FOLDER, &p.files)]).unwrap();
        assert_eq!(read_in(&boot, "pico8/pico8.dat").unwrap(), data);
        // And on a boot partition that already has it (written over), the
        // files there count as room
        let again = crate::closed::into_boot(boot, &[(FOLDER, &p.files)]).unwrap();
        assert_eq!(read_in(&again, "pico8/pico8_64").unwrap(), program);
    }

    #[test]
    fn knows_the_pico8_on_a_card() {
        let root = temp("oncard");
        assert_eq!(on_card(&root), None);
        let dir = root.join("Bios/PICO-8");
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("version.txt"), "0.2.7\n").unwrap();
        assert_eq!(on_card(&root), None, "a version.txt alone is not PICO-8");
        std::fs::write(dir.join("pico8_64"), b"x").unwrap();
        assert_eq!(on_card(&root).as_deref(), Some("0.2.7"));
        std::fs::remove_file(dir.join("version.txt")).unwrap();
        assert_eq!(on_card(&root).as_deref(), Some(""), "put there by hand: PICO-8, no version");
        std::fs::write(dir.join("version.txt"), "<b>nonsense</b>").unwrap();
        assert_eq!(on_card(&root).as_deref(), Some(""));
    }
}
