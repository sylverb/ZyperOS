# ZyperOS Installer

Puts [ZyperOS](../README.md) on a microSD card for the
GKD Pixel 2 or the TrimUI Brick, or updates a card that already has it
without touching the games, music and saves on it. For macOS, Windows and
Linux.

Its window is in ZyperOS's design ([DESIGN.md](../DESIGN.md): Lexend, the
Beard and Salt & Pepper themes, Zyper).

## Download

Every ZyperOS release on the
[releases page](https://github.com/tarikbc/ZyperOS/releases/latest), from
1.0.0 on, carries the installer with ZyperOS's card image, at ZyperOS's
version. The GKD Pixel 2's image is there; the TrimUI Brick's comes later.

| | |
|---|---|
| **macOS** (Apple Silicon and Intel) | `ZyperOS-Installer-<version>-macOS.zip` |
| **Windows** 10 and 11 | `ZyperOS-Installer-<version>-Windows.exe` |
| **Linux** (Debian, Ubuntu, Mint) | `ZyperOS-Installer-<version>-amd64.deb` |
| **Linux** (Fedora) | `ZyperOS-Installer-<version>-x86_64.rpm` |

The release's other installer files (`-macOS-update.tar.gz`, the `.sig`
files and `latest.json`) are for the installer's own updates.

The first time it opens:

- **macOS:** unzip it and open the app. It isn't signed by Apple, so macOS
  stops it the first time: open *System Settings > Privacy & Security*,
  scroll down, and choose *Open Anyway*. Writing a card asks for your
  password, and macOS may ask to let the installer use removable volumes.
- **Windows:** the exe needs nothing installed. If Windows says it protected
  your PC, choose *More info* and *Run anyway*. It asks to run as
  administrator, which writing a card needs.
- **Linux:** install the package, `sudo apt install ./ZyperOS-Installer-*.deb`
  or `sudo dnf install ./ZyperOS-Installer-*.rpm`, and open ZyperOS
  Installer from your apps. Writing a card asks for your password.

## Using it

1. **Device:** the GKD Pixel 2 or the TrimUI Brick. A card that already has
   ZyperOS picks its device by itself.
2. **ZyperOS:** the latest release, downloaded from GitHub and checked
   against the release's `SHA256SUMS`; or an image file on your computer
   (`ZyperOS-v<version>-<device>.img.xz`, or a plain `.img`). Choose one, or
   drop it on the window. A file with a `SHA256SUMS` beside it that names it,
   as `make release` in plastron leaves them, is checked against it first. An
   image that isn't ZyperOS's, or is for the other device, is refused.
3. **Card:** put the card in. It shows up by itself, with what is on it.
4. **What to do:**
   - **Update** a card that already has ZyperOS (or TortOS) for that
     device: the system part is written, and the games, music, saves and
     settings stay.
   - **Fresh install** on any card: it is erased, and ZyperOS goes on.

Before anything is written, a sheet names the card, its reader, its disk and
its size; a fresh install also asks you to tick that you want that card
erased. Everything written is read back and compared. When it's done, the
card is ejected, ready to go in the handheld.

### What it never writes

- Only removable media is listed: an SD card in a reader (the Mac's built-in
  reader included) or a flash drive. Fixed disks are never offered, external
  ones included.
- The disk the computer runs from is never offered, removable or not: a Mac
  or a PC started from a USB drive, a Linux live stick.
- Just before the disk is opened, the card is looked for again: it must be
  the one confirmed, at the size confirmed. A card swapped in the same reader
  meanwhile is refused.
- A card with its lock switch down is shown, but can't be chosen.

### Its own updates

When it opens, the installer looks for a newer version of itself. If one is
out, a chip in the window's header says so:

- **macOS and Windows:** *Update to 1.0.7* downloads the new installer and
  checks its signature against the key this one carries; then *Restart to
  update* puts it in this one's place and opens it. Nothing downloads before
  the click, it never restarts by itself, and never while a card is being
  written: the chip waits for the write to end. The new one opens with no
  first-time question from macOS, as the installer downloaded it itself. On
  Windows the exe keeps its name, so shortcuts still work. A Mac app in a
  folder your account can't write to asks for an administrator's password.
- **Linux:** *Installer 1.0.7 is out* opens the releases page: update the
  package as you installed it.
- An update that didn't work says so, and its click opens the releases page.

A Mac app opened straight from Downloads runs from a read-only copy macOS
makes of it, even after *Open Anyway*: the update replaces the app in
Downloads and opens that one, and from then on macOS runs it where it is.

Installers up to 1.0.5 can't update themselves: their chip opens the releases
page.

### How each system writes

- **macOS:** the card's volumes are unmounted, and its raw disk is opened
  through `authopen`, Apple's tool for this: it asks for the password and
  hands the open disk back, so the installer never runs as root.
- **Windows:** the installer runs as administrator (its manifest asks UAC),
  and every volume on the card is locked or dismounted first.
- **Linux:** the window never runs as root: it runs itself again through
  `pkexec` (polkit asks for the password), in a helper mode with no window
  that writes the card and reports its progress.

## Building

Everything is built on a Mac: the macOS app natively, the Windows exe and the
Linux packages in Docker.

1. Install Rust with rustup, add the Intel target for the macOS app
   (`rustup target add x86_64-apple-darwin`), and either install the Tauri CLI
   (`cargo install tauri-cli --version "^2"`) or have Node, for
   `npx @tauri-apps/cli@2`.
2. Run `tools/release.sh` in this folder. It checks the page against
   ZyperOS's design (`tools/check-design.sh`: the colors against
   `res/web/app.css`, the fonts and marks against `res/`), checks that
   `THIRD-PARTY.md` is the list `tools/third-party-list.py` makes now and is
   committed, runs the tests, then builds all four files into `dist/<version>/` with their
   `SHA256SUMS`. Nothing is published. The first run takes a while: it
   builds the Linux image from `tools/linux.Dockerfile` and fetches the
   Windows SDK for `cargo-xwin`.

The installer is released with ZyperOS, at ZyperOS's version
(`tools/set-version.sh` at the top sets both): `make release PUBLISH=1` makes
the release with the card image, and GitHub builds these four files for it
on the tag (`.github/workflows/release.yml`; see
[docs/releases](../docs/releases/README.md)). GitHub also builds the Linux
packages and the Windows exe, with the tests, for every pull request that
changes this folder (`.github/workflows/installer.yml`).

For a quick look while working:

```bash
cd src-tauri && cargo test
```

```bash
cd src-tauri && npx --yes @tauri-apps/cli@2 dev
```

`ZYPEROS_IMAGE` (Pixel 2) and `ZYPEROS_BRICK_IMAGE` (Brick) name a local
image the release row uses in place of a download, for trying a build before
it is published. The page also opens in a plain browser, on made-up cards
and a made-up write, to look at the design: serve `ui/` and open it (add
`?release` for a release to download, `?nocard` for no card, and
`?update=available` for a newer installer to update to; `downloading`,
`ready`, `installing`, `failed` and `page` show the chip's other states).

### The installer's own updates

Tauri's updater ([tauri-plugin-updater](https://v2.tauri.app/plugin/updater/))
does the looking, the download and the signature check (`src/selfupdate.rs`):

- It reads `https://github.com/tarikbc/ZyperOS/releases/latest/download/latest.json`
  (`plugins.updater` in `src-tauri/tauri.conf.json`). Each release carries
  one, written by `tools/latest-json.py` in `.github/workflows/release.yml`:
  the macOS app as `ZyperOS-Installer-<version>-macOS-update.tar.gz` for both
  kinds of Mac, and the Windows exe itself, each with its signature. No Linux
  entry. When the latest release has none, or the look fails, the
  installer asks GitHub's releases list instead (`src/release.rs`), and the
  chip opens the releases page.
- The files are signed with the updater key, a minisign key that `tauri
  signer` makes. Its public half is `plugins.updater.pubkey`; the signature
  must also name the version `latest.json` announces (`requireSignedVersion`),
  so an older release can't be passed off as a newer one.
- macOS: the updater's own install swaps the new app in for the old one. An
  app macOS runs translocated (from a read-only copy, as it runs a download
  opened where it was saved) has the original replaced instead, found as
  that copy's mount source (`statfs`), and the original opened.
  Windows: the updater only installs Windows installers, and this exe is
  portable, so the installer renames itself to `<name>.old`, puts the new exe
  in its place, starts it, and the new one removes the `.old` file.

`tools/try-selfupdate.sh` tries it end to end on a Mac, with nothing
published: two debug builds signed with a throwaway key, a `latest.json` on
`127.0.0.1`, and the old app updating itself three times: as it is, marked
as a download the user allowed and moved, and marked as one allowed but left
where it was saved (which macOS translocates). Each time it must open again
as the new one, where the user has it, with no quarantine mark. Debug builds update with no click when
`ZYPEROS_INSTALLER_SELFUPDATE_TEST` names a file to log the steps to.

#### The updater key

The private key is on the Mac that makes releases, in
`~/.config/zyperos/installer-updater.key` (no password; never in the
repository), and in the repository's `INSTALLER_UPDATER_KEY` secret, which
only a release tag's build reads (a pull request signs with a throwaway key):

```bash
gh secret set INSTALLER_UPDATER_KEY --repo tarikbc/ZyperOS < ~/.config/zyperos/installer-updater.key
```

A release whose files don't check out against `plugins.updater.pubkey` stops
before anything is uploaded. Keep a copy of the key somewhere safe. If it is
ever lost or leaked, make a new one
(`npx --yes @tauri-apps/cli@2 signer generate --ci -p "" -w <file>`), put its
`.pub` file's text in `plugins.updater.pubkey`, and set the secret again: the
installers already out there refuse the next release's files, so for that one
update their chip opens the releases page, and from then on they use the new
key.

## Thanks

The disk writer, the card listing and the way each system opens a disk
started in Eric Reinsmidt's
[TortOS Installer](https://github.com/ericreinsmidt/TortOS-Installer), whose
history this folder keeps (a subtree, as `diatom/` is). Thank you, Eric.

## License

ZyperOS Installer is under the GNU General Public License, version 3
(`LICENSE`), with the additional terms in the repository's `NOTICE`. The name
and Zyper are trademarks; see the repository's `TRADEMARK.md`. It started from TortOS Installer by
Eric Reinsmidt, in its MIT-licensed versions; that code keeps his MIT notice,
in the repository's `LICENSES/MIT-TortOS-Installer.txt`.

The fonts in `ui/fonts` are Lexend, under the SIL Open Font License
(`ui/fonts/OFL.txt`).

What the apps are built from is listed in [THIRD-PARTY.md](THIRD-PARTY.md):
every Rust crate compiled into them, with its license, its repository and
which apps carry it, and Tauri, the system web views and the fonts.
`tools/third-party-list.py` makes it from `src-tauri/Cargo.lock` and the font
files; never edit it by hand. After changing a dependency, make it again with
`python3 tools/third-party-list.py` in this folder and commit it:
`tools/release.sh` stops when it is out of date.
