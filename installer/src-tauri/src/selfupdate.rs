// The installer's own updates. On open the page asks whether a newer
// installer is out. On macOS and Windows this one can then put the new one in
// its own place and restart: Tauri's updater checks the latest.json each
// release carries (tauri.conf.json's plugins.updater names it and the key its
// files are signed with), downloads the file for this system and checks its
// signature. Where it can't (Linux's packages, which are the package
// manager's; a Mac app run from where it was downloaded), or when that look
// fails, release.rs's look at the releases says whether a newer one is out,
// and the page offers the releases page, as it always did.
//
// Never while a card is being written: main.rs refuses the restart during a
// write, and a write while the new installer goes in place.

use std::path::{Path, PathBuf};
use std::sync::atomic::AtomicBool;
#[cfg(any(target_os = "macos", windows))]
use std::sync::Mutex;

/// What the page shows of a newer installer
#[derive(serde::Serialize, Clone, Debug, PartialEq)]
pub struct Offer {
    pub version: String,
    /// Whether it can replace this one; if not, its click is the releases page
    pub in_place: bool,
    /// Why not, when there is something to do about it
    pub why: Option<String>,
}

/// Whether this copy can replace itself, from where it runs
#[derive(Debug, PartialEq)]
pub enum Way {
    InPlace,
    /// The releases page, and why, when that helps
    Page(Option<&'static str>),
}

pub const MOVE_TO_APPLICATIONS: &str =
    "Move ZyperOS Installer to your Applications folder, and it updates itself from then on.";

/// The way for the executable at `exe`, on this system
pub fn way(exe: &Path) -> Way {
    if cfg!(not(any(target_os = "macos", windows))) {
        return Way::Page(None);
    }
    if cfg!(target_os = "macos") && translocated(exe) {
        return Way::Page(Some(MOVE_TO_APPLICATIONS));
    }
    Way::InPlace
}

/// A Mac app opened from where it was downloaded, still quarantined, runs
/// from a read-only copy macOS makes of it (App Translocation), which can
/// never be replaced. Moved to Applications in the Finder, it runs from there.
fn translocated(exe: &Path) -> bool {
    exe.components().any(|c| c.as_os_str() == "AppTranslocation")
}

/// Whether `remote` is an update for `current`: a release, never a beta, and
/// newer by semver's precedence (build metadata doesn't count). The updater's
/// own rule is `remote > current`; this one also refuses a pre-release, as the
/// release check does (GitHub's latest never names one either).
#[cfg(any(target_os = "macos", windows, test))]
pub fn is_update(current: &semver::Version, remote: &semver::Version) -> bool {
    remote.pre.is_empty() && remote.cmp_precedence(current).is_gt()
}

/// The file a replaced exe is kept as until the next start: Windows lets a
/// running exe be renamed, never deleted
#[cfg(any(windows, test))]
pub fn kept_aside(exe: &Path) -> PathBuf {
    let mut name = exe.file_name().unwrap_or_default().to_os_string();
    name.push(".old");
    exe.with_file_name(name)
}

/// The exe a previous update set aside, removed. On start, when it may still
/// be closing: tried for a few seconds.
#[cfg(any(windows, test))]
pub fn remove_kept_aside(exe: &Path) {
    let old = kept_aside(exe);
    if !old.exists() {
        return;
    }
    std::thread::spawn(move || {
        for _ in 0..20 {
            if std::fs::remove_file(&old).is_ok() || !old.exists() {
                return;
            }
            std::thread::sleep(std::time::Duration::from_millis(250));
        }
    });
}

/// Puts `bytes` in the place of the file at `exe`, which may be running: the
/// new file is written beside it, the current one renamed aside
/// ([`kept_aside`]), the new one renamed into its place. A failure on the way
/// leaves the current one where it was. Windows' portable exe; the updater's
/// own install only knows Windows' installers.
#[cfg(any(windows, test))]
pub fn replace_exe(exe: &Path, bytes: &[u8]) -> std::io::Result<()> {
    // A virus scanner may hold the new file for a moment after it is written
    replace_exe_with(exe, bytes, &|from, to| {
        let mut tries = 0;
        loop {
            match std::fs::rename(from, to) {
                Err(_) if tries < 5 => {
                    tries += 1;
                    std::thread::sleep(std::time::Duration::from_millis(200));
                }
                result => return result,
            }
        }
    })
}

#[cfg(any(windows, test))]
fn replace_exe_with(exe: &Path, bytes: &[u8], rename: &dyn Fn(&Path, &Path) -> std::io::Result<()>) -> std::io::Result<()> {
    use std::io::Write;
    let mut name = exe.file_name().unwrap_or_default().to_os_string();
    name.push(".new");
    let staged = exe.with_file_name(name);
    let old = kept_aside(exe);
    // A previous update's, if removing it on start didn't work
    let _ = std::fs::remove_file(&old);
    let written = (|| {
        let mut file = std::fs::File::create(&staged)?;
        file.write_all(bytes)?;
        file.sync_all()
    })();
    if let Err(e) = written {
        let _ = std::fs::remove_file(&staged);
        return Err(e);
    }
    if let Err(e) = rename(exe, &old) {
        let _ = std::fs::remove_file(&staged);
        return Err(e);
    }
    if let Err(e) = rename(&staged, exe) {
        // The current one back where it was
        let _ = std::fs::rename(&old, exe);
        let _ = std::fs::remove_file(&staged);
        return Err(e);
    }
    Ok(())
}

/// What the page says when the new installer couldn't go in place
#[cfg(any(target_os = "macos", windows))]
fn failed(what: &str, e: impl std::fmt::Display) -> String {
    format!("{what}: {e}")
}

/// The update found on open, its file once downloaded and checked, and
/// whether it is going in place right now (then no write starts).
#[derive(Default)]
pub struct Pending {
    /// This executable's path, read on start, before an update renames it
    pub exe: PathBuf,
    pub installing: AtomicBool,
    #[cfg(any(target_os = "macos", windows))]
    update: Mutex<Option<tauri_plugin_updater::Update>>,
    #[cfg(any(target_os = "macos", windows))]
    bytes: Mutex<Option<Vec<u8>>>,
}

impl Pending {
    pub fn new(exe: PathBuf) -> Self {
        Pending { exe, ..Default::default() }
    }
}

/// Looks for a newer installer with Tauri's updater. `Ok(None)`: this is the
/// newest. An error: the look didn't work (no latest.json in the latest
/// release, as up to 1.0.5; no network), and release.rs's look decides.
#[cfg(any(target_os = "macos", windows))]
pub async fn check<R: tauri::Runtime>(app: &tauri::AppHandle<R>, pending: &Pending) -> Result<Option<Offer>, String> {
    use tauri_plugin_updater::UpdaterExt;
    let updater = app
        .updater_builder()
        .version_comparator(|current, release| is_update(&current, &release.version))
        .timeout(std::time::Duration::from_secs(30))
        .build()
        .map_err(|e| failed("Setting up the update", e))?;
    let Some(update) = updater.check().await.map_err(|e| failed("Looking for an update", e))? else {
        return Ok(None);
    };
    let offer = Offer { version: update.version.clone(), in_place: true, why: None };
    *pending.update.lock().unwrap() = Some(update);
    *pending.bytes.lock().unwrap() = None;
    Ok(Some(offer))
}

/// Downloads the update found by [`check`] and checks its signature against
/// the key in tauri.conf.json (the updater does both), keeping it for
/// [`install`]. `progress` gets the bytes so far and the whole, when known.
#[cfg(any(target_os = "macos", windows))]
pub async fn download(pending: &Pending, mut progress: impl FnMut(u64, Option<u64>)) -> Result<(), String> {
    let update = pending.update.lock().unwrap().clone().ok_or("There is no update to download.")?;
    let mut done = 0u64;
    let bytes = update
        .download(
            |chunk, total| {
                done += chunk as u64;
                progress(done, total);
            },
            || {},
        )
        .await
        .map_err(|e| failed("Downloading the update", e))?;
    *pending.bytes.lock().unwrap() = Some(bytes);
    Ok(())
}

/// Puts the downloaded installer in this one's place. Its signature is
/// checked again first. The caller restarts.
#[cfg(any(target_os = "macos", windows))]
pub fn install(pending: &Pending) -> Result<(), String> {
    let update = pending.update.lock().unwrap().clone().ok_or("There is no update to install.")?;
    let bytes = pending.bytes.lock().unwrap().take().ok_or("The update isn't downloaded yet.")?;
    #[cfg(target_os = "macos")]
    {
        // The updater's own: the new app unpacked beside this one and swapped
        // in one step, asking for an administrator only when the app's
        // folder isn't writable
        update.install(&bytes).map_err(|e| failed("Putting the new installer in place", e))
    }
    #[cfg(windows)]
    {
        update.verify(&bytes).map_err(|e| failed("Checking the update", e))?;
        // A Windows program starts with "MZ"
        if !bytes.starts_with(b"MZ") {
            return Err("The update isn't a Windows program, so it wasn't used.".into());
        }
        replace_exe(&pending.exe, &bytes).map_err(|e| failed("Putting the new installer in place", e))
    }
}

/// Opens the installer now in this one's place, and ends this one
#[cfg(any(target_os = "macos", windows))]
pub fn restart<R: tauri::Runtime>(app: &tauri::AppHandle<R>, pending: &Pending) -> ! {
    #[cfg(windows)]
    {
        // By the path read on start: this exe's own name now is the .old one.
        // It runs as administrator already, so the new one does too, with no
        // second prompt
        let _ = std::process::Command::new(&pending.exe).spawn();
        app.cleanup_before_exit();
        std::process::exit(0);
    }
    #[cfg(target_os = "macos")]
    {
        let _ = pending;
        // Through LaunchServices, the new bundle at the same path
        app.restart()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn v(s: &str) -> semver::Version {
        semver::Version::parse(s).unwrap()
    }

    #[test]
    fn updates_only_to_a_newer_release() {
        assert!(is_update(&v("1.0.6"), &v("1.0.7")));
        assert!(is_update(&v("1.0.9"), &v("1.0.10")), "numbers, not text");
        assert!(is_update(&v("1.9.0"), &v("2.0.0")));
        assert!(!is_update(&v("1.0.7"), &v("1.0.7")), "the same version");
        assert!(!is_update(&v("1.0.7"), &v("1.0.6")), "never back");
        assert!(!is_update(&v("1.0.6"), &v("1.0.7-beta.1")), "never a beta");
        assert!(is_update(&v("1.0.7-beta.1"), &v("1.0.7")), "a beta to its release");
        assert!(!is_update(&v("1.0.7"), &v("1.0.7+build.5")), "build metadata isn't newer");
    }

    #[test]
    fn knows_where_it_can_replace_itself() {
        let applications = Path::new("/Applications/ZyperOS Installer.app/Contents/MacOS/zyperos-installer");
        let translocated = Path::new(
            "/private/var/folders/x/T/AppTranslocation/4D5E/d/ZyperOS Installer.app/Contents/MacOS/zyperos-installer",
        );
        if cfg!(target_os = "macos") {
            assert_eq!(way(applications), Way::InPlace);
            assert_eq!(way(translocated), Way::Page(Some(MOVE_TO_APPLICATIONS)));
        } else if cfg!(windows) {
            assert_eq!(way(Path::new(r"C:\Users\me\Downloads\ZyperOS-Installer-1.0.6-Windows.exe")), Way::InPlace);
        } else {
            assert_eq!(way(Path::new("/usr/bin/zyperos-installer")), Way::Page(None));
        }
    }

    /// What tools/latest-json.py writes for made-up signatures: the file each
    /// release carries, made the same way
    fn example_latest_json() -> serde_json::Value {
        let script = Path::new(env!("CARGO_MANIFEST_DIR")).join("../tools/latest-json.py");
        // python3 on macOS and Linux; Windows has python, and a python3
        // that may only open the Store
        for python in ["python3", "python"] {
            if let Ok(out) = std::process::Command::new(python).arg(&script).arg("--example").output() {
                if out.status.success() {
                    return serde_json::from_slice(&out.stdout).unwrap();
                }
            }
        }
        panic!("this test runs tools/latest-json.py, which needs Python 3");
    }

    #[test]
    fn the_releases_latest_json_is_what_the_updater_reads() {
        let json = example_latest_json();
        assert_eq!(json["version"], "1.0.7");
        let base = "https://github.com/tarikbc/ZyperOS/releases/download/v1.0.7/";
        assert_eq!(json["platforms"]["windows-x86_64"]["url"], format!("{base}ZyperOS-Installer-1.0.7-Windows.exe"));
        assert!(json["platforms"].get("linux-x86_64").is_none(), "no Linux entry: its packages aren't replaced");
        #[cfg(any(target_os = "macos", windows))]
        {
            // The updater's own reading of it
            let release: tauri_plugin_updater::RemoteRelease = serde_json::from_value(json).unwrap();
            assert_eq!(release.version, v("1.0.7"));
            let mac = format!("{base}ZyperOS-Installer-1.0.7-macOS-update.tar.gz");
            for target in ["darwin-aarch64", "darwin-x86_64"] {
                assert_eq!(release.download_url(target).unwrap().as_str(), mac, "one app for both Macs");
                assert!(release.signature(target).unwrap().ends_with("macOS-update.tar.gz"));
            }
            assert!(release.signature("windows-x86_64").unwrap().ends_with("Windows.exe"));
            assert!(release.download_url("linux-x86_64").is_err());
            assert!(is_update(&v("1.0.6"), &release.version) && !is_update(&v("1.0.7"), &release.version));
        }
    }

    /// tauri.conf.json's plugins.updater: the key, the address of
    /// latest.json, and the signed version required
    #[test]
    fn the_updater_is_set_up() {
        let conf: serde_json::Value =
            serde_json::from_str(include_str!("../tauri.conf.json")).expect("tauri.conf.json reads");
        let updater = &conf["plugins"]["updater"];
        assert_eq!(
            updater["endpoints"],
            serde_json::json!(["https://github.com/tarikbc/ZyperOS/releases/latest/download/latest.json"])
        );
        assert_eq!(updater["requireSignedVersion"], true);
        // A minisign public key, base64'd as tauri signer writes it
        let key = updater["pubkey"].as_str().unwrap();
        let key = String::from_utf8(base64_decode(key)).unwrap();
        assert!(key.starts_with("untrusted comment: minisign public key: "), "{key}");
        #[cfg(any(target_os = "macos", windows))]
        {
            let config: tauri_plugin_updater::Config = serde_json::from_value(updater.clone()).unwrap();
            assert!(config.require_signed_version && !config.allow_downgrades);
            assert!(!config.dangerous_insecure_transport_protocol);
        }
    }

    fn base64_decode(s: &str) -> Vec<u8> {
        let value = |c: u8| -> u32 {
            match c {
                b'A'..=b'Z' => (c - b'A') as u32,
                b'a'..=b'z' => (c - b'a' + 26) as u32,
                b'0'..=b'9' => (c - b'0' + 52) as u32,
                b'+' => 62,
                b'/' => 63,
                _ => panic!("not base64: {c}"),
            }
        };
        let digits: Vec<u32> = s.bytes().filter(|&c| c != b'=').map(value).collect();
        let mut out = Vec::new();
        for chunk in digits.chunks(4) {
            let n = chunk.iter().enumerate().fold(0u32, |n, (i, d)| n | d << (18 - 6 * i));
            out.extend_from_slice(&n.to_be_bytes()[1..chunk.len()]);
        }
        out
    }

    fn folder(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("zyperos-installer-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn replaces_an_exe_and_keeps_the_old_one_aside() {
        let dir = folder("swap");
        let exe = dir.join("ZyperOS-Installer-1.0.6-Windows.exe");
        std::fs::write(&exe, b"MZ old").unwrap();
        // A previous update's leftover goes first
        std::fs::write(kept_aside(&exe), b"MZ older").unwrap();
        replace_exe(&exe, b"MZ new").unwrap();
        assert_eq!(std::fs::read(&exe).unwrap(), b"MZ new", "the same name, so shortcuts still work");
        assert_eq!(std::fs::read(kept_aside(&exe)).unwrap(), b"MZ old");
        assert!(!dir.join("ZyperOS-Installer-1.0.6-Windows.exe.new").exists());
        remove_kept_aside(&exe);
        for _ in 0..40 {
            if !kept_aside(&exe).exists() {
                break;
            }
            std::thread::sleep(std::time::Duration::from_millis(50));
        }
        assert!(!kept_aside(&exe).exists(), "removed on the next start");
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn a_failed_swap_leaves_the_old_exe_in_place() {
        let dir = folder("swap-fail");
        let exe = dir.join("zyperos-installer.exe");
        std::fs::write(&exe, b"MZ old").unwrap();
        // The new file can't take the exe's name (a virus scanner holding it,
        // say): the old one goes back
        let rename = |from: &Path, to: &Path| {
            if from.extension().is_some_and(|e| e == "new") {
                Err(std::io::Error::other("held"))
            } else {
                std::fs::rename(from, to)
            }
        };
        assert!(replace_exe_with(&exe, b"MZ new", &rename).is_err());
        assert_eq!(std::fs::read(&exe).unwrap(), b"MZ old");
        assert!(!kept_aside(&exe).exists() && !dir.join("zyperos-installer.exe.new").exists());
        // The old one can't be moved aside: nothing changes
        let rename = |_: &Path, _: &Path| Err(std::io::Error::other("denied"));
        assert!(replace_exe_with(&exe, b"MZ new", &rename).is_err());
        assert_eq!(std::fs::read(&exe).unwrap(), b"MZ old");
        assert!(!dir.join("zyperos-installer.exe.new").exists());
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
