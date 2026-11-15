# Releases

Every ZyperOS version is one release on GitHub, tagged `v<version>`, with
these files for each device in [`config/devices`](../../config/devices)
(`<id>` is its id: `pixel2`, the GKD Pixel 2, today):

| File | What |
|---|---|
| `ZyperOS-v<version>-<id>.img.xz` | The device's card image |
| `ZyperOS-v<version>-<id>-sources.tar` | The licenses and sources of everything in it; for the libretro cores that `mk/fetch-vendor.sh` fetches, its `CORES-SOURCE.txt` points to the cores release |
| `ZyperOS-v<version>-<id>-update.zpk` | System Update's pack: every file of the image's ZyperOS folder (docs/updates.md) |
| `ZyperOS-<id>-update.txt` | System Update's list of those files, signed; no version in its name, so `releases/latest/download/` finds the newest |
| `ZyperOS-v<version>-<id>-system.zpk` | System Update's pack of the system itself (root, kernel, device tree), for a card with two system slots |
| `ZyperOS-<id>-system.txt` | Its list, signed, also with no version in its name |

and once for the release:

| File | What |
|---|---|
| `ZyperOS-Installer-<version>-macOS.zip` | ZyperOS Installer for macOS (Apple Silicon and Intel) |
| `ZyperOS-Installer-<version>-Windows.exe` | ZyperOS Installer for Windows 10 and 11 |
| `ZyperOS-Installer-<version>-amd64.deb` | ZyperOS Installer for Debian, Ubuntu and Mint |
| `ZyperOS-Installer-<version>-x86_64.rpm` | ZyperOS Installer for Fedora |
| `ZyperOS-Installer-<version>-macOS-update.tar.gz` and `.sig` | The macOS app again, for the installer's own update, with its signature |
| `ZyperOS-Installer-<version>-Windows.exe.sig` | The Windows exe's signature, for the installer's own update |
| `latest.json` | What the installer's own update reads (`releases/latest/download/latest.json`): the version, and each update file with its signature |
| `SHA256SUMS` | The checksums of all of them |

The installer has ZyperOS's version and downloads the newest published
release's image, checked against its `SHA256SUMS`. A draft or a pre-release
(a version with a hyphen, `0.2.0-beta.1`) is never offered.

## The devices

`config/devices` lists the devices a release carries, one id a line. A device
joins it with its first release, never before, and then stays: System Update
and the installer read `releases/latest`, so a release without a device's
files would leave that device with nothing to find. Each listed device has
its profile (`src/device/<id>_profile.c`) and its `res/update/min-system-<id>`,
the oldest system image its ZyperOS folder runs on (`make
check-release-devices` checks both). From the list:

- `tools/release.sh` builds each device with plastron's `make release-<id>`,
  names, signs and checks its files, and writes one `SHA256SUMS` over all of
  them.
- The release workflow refuses a draft whose `SHA256SUMS` lacks a listed
  device's image.
- ZyperOS Installer offers the devices the newest release has an image
  (`ZyperOS-v<version>-<id>.img.xz`) for, and zyperos.com's download page has
  a card image button for each; a device with no image there is not offered.
- The Pages workflow counts every listed device's image downloads for the
  README's badge.

The TrimUI Brick is not listed yet: its image needs the Brick's GPU files
(`BRICK_GPU` in plastron), which the Mac that makes releases doesn't have,
and its test kits have to pass first.

## Tested on

A release waits until each listed device passed its device test for that
version, on a real device with the release's own files. The release notes
say so, one line a device:

```
Tested on <id>: <who tested it, and on what>
```

for example `Tested on pixel2: Tarik, the release image on two Pixel 2s and
an update from 1.0.5`. The line starts at the beginning of its line, with no
list mark: "Tested on", the id exactly as `config/devices` has it, a colon, a
space, then the words. Put the lines in a `## Tested` section before Thanks,
a blank line between them. Both `--publish` forms of `tools/release.sh`
refuse a release whose notes lack the line for any listed device, name a
device that isn't listed, or have a "Tested on" line in another form
(`tools/release-devices.sh tested docs/releases/v<version>.md` says the same
without publishing). The section stays on GitHub: the Discord post leaves it
out, as it does Install and Thanks.

## Making one

1. Set the version: `tools/set-version.sh 0.2.0` (the Makefile's `VERSION`,
   and the installer's `Cargo.toml` and `Cargo.lock`; `make check-installer`
   says when they disagree). Merge it to main.
2. Write the release notes in `docs/releases/v<version>.md` and merge them.
   Their Thanks section names the sources tar, and the cores release
   (`https://github.com/tarikbc/ZyperOS/releases/tag/<CORES_TAG>`, the tag in
   `mk/fetch-vendor.sh`) for the libretro cores' sources.
3. On the Mac, in the main checkout on main, with plastron's checkout on its
   main: `make release` builds each listed device's image from this commit
   with plastron's release build (clean, so it takes a while, holding
   `/tmp/plastron-build-lock`) into `out/release/v<version>/`, signs System
   Update's lists when the key is there, and checks them. Nothing leaves the
   Mac. Try those exact files on each device: File Transfer's "Update
   ZyperOS" takes the update packs and lists from the computer. Each device
   that passes gets its `Tested on` line in the notes, merged to main.
   Then `tools/release.sh --publish-built` publishes the files as they are,
   with no second build, as step 4 does: it refuses unless they were built
   from main as GitHub has it now, or from the commit before main's changes
   to the notes' `Tested on` lines and nothing else, and still match their
   `SHA256SUMS`.
4. Or `make release PUBLISH=1` builds them again the same way (the notes need
   their `Tested on` lines before it starts), signs System Update's lists
   with the key in `~/.config/zyperos/update-signing-key.pem`
   (`ZYPEROS_UPDATE_KEY`; it refuses without it once `res/update/keys` has a
   key), makes a **draft** release `v<version>` with each device's image, its
   sources, its update packs and lists, and `SHA256SUMS`, and pushes the
   tag. If a device's ZyperOS folder needs something new from its system
   image (a library, a driver), raise its `res/update/min-system-<id>` to
   this version first: devices with an older image are then told to use the
   installer.
5. The tag starts **Release** on GitHub (`.github/workflows/release.yml`): it
   builds the installer for macOS, Windows and Linux with its tests, signs
   the macOS and Windows update files with the installer's updater key (the
   `INSTALLER_UPDATER_KEY` secret; see `installer/README.md`), checks those
   signatures against the key the installer carries, adds the files and
   `latest.json` to the draft, puts them in `SHA256SUMS`, and publishes the
   release. A tag pushed with no draft waiting is refused, and so is one
   whose update files don't check out against that key.
6. `tools/release.sh` waits for that, then posts the notes in the Discord's
   #releases (`tools/discord-release.py`, with Zyper's token from
   `~/.config/zyperos/discord-bot.token`): the first paragraph, What's new,
   Updating and Good to know, with the release's link for the rest. If the
   wait was stopped, `tools/discord-release.py v<version>` posts them.

Why the image is built on the Mac and the installer on GitHub: the image's
inputs are on the Mac (the cores in `vendor/`, part fetched by
`mk/fetch-vendor.sh` and part built, and plastron's build volume), and the
installer builds anywhere, so GitHub's machines build it on all three
systems, the Mac's own included.
