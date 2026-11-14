#!/bin/bash
# Tries the installer's own update end to end on this Mac, with nothing
# published: two debug builds of this checkout, an "old" one and a "new" one,
# signed with a throwaway key; a latest.json for the new one served on
# 127.0.0.1; the old app opened twice, once as it is and once marked as a
# download the user allowed (com.apple.quarantine, as Safari and "Open
# Anyway" leave it). Each time it must update itself and open again as the
# new one, and the new app must carry no quarantine mark, so macOS asks
# nothing more when it opens.
#
# The apps are "ZyperOS Installer Test" (their own identifier, no window),
# and update with no click: debug builds do with ZYPEROS_INSTALLER_SELFUPDATE_TEST
# (main.rs). Nothing here touches a card, the real installer or GitHub.
#
#     tools/try-selfupdate.sh [work folder]
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
WORK=${1:-$(mktemp -d -t zyperos-selfupdate)}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
PORT=8767
NAME="ZyperOS Installer Test"
LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
if cargo tauri --version >/dev/null 2>&1; then TAURI="cargo tauri"; else TAURI="npx --yes @tauri-apps/cli@2"; fi

say() { printf '\n== %s\n' "$*"; }

say "A throwaway key"
$TAURI signer generate --ci -p "" -w "$WORK/test.key" -f > /dev/null
export TAURI_SIGNING_PRIVATE_KEY="$(cat "$WORK/test.key")" TAURI_SIGNING_PRIVATE_KEY_PASSWORD=""

for v in 1.0.0 1.0.1; do
	say "The app at $v"
	cat > "$WORK/conf-$v.json" <<-EOF
	{"version": "$v", "productName": "$NAME", "identifier": "com.tarikbc.zyperos-installer.selfupdate-test",
	 "plugins": {"updater": {"pubkey": "$(cat "$WORK/test.key.pub")", "endpoints": ["http://127.0.0.1:$PORT/latest.json"],
	  "dangerousInsecureTransportProtocol": true, "requireSignedVersion": true}},
	 "bundle": {"createUpdaterArtifacts": true},
	 "app": {"windows": [{"label": "main", "title": "$NAME", "width": 780, "height": 720, "visible": false}]}}
	EOF
	(cd src-tauri && $TAURI build --debug --bundles app --config "$WORK/conf-$v.json" > "$WORK/build-$v.log" 2>&1) ||
		{ tail -20 "$WORK/build-$v.log"; exit 1; }
	rm -rf "$WORK/$v" && mkdir -p "$WORK/$v"
	ditto src-tauri/target/debug/bundle/macos "$WORK/$v"
done

say "Its latest.json, on 127.0.0.1:$PORT"
SERVE=$WORK/serve
rm -rf "$SERVE" && mkdir -p "$SERVE"
cp "$WORK/1.0.1/$NAME.app.tar.gz" "$SERVE/ZyperOS-Installer-1.0.1-macOS-update.tar.gz"
cp "$WORK/1.0.1/$NAME.app.tar.gz.sig" "$SERVE/ZyperOS-Installer-1.0.1-macOS-update.tar.gz.sig"
# latest.json names a Windows file too: this one stands in for it
cp "$SERVE/ZyperOS-Installer-1.0.1-macOS-update.tar.gz" "$SERVE/ZyperOS-Installer-1.0.1-Windows.exe"
cp "$SERVE/ZyperOS-Installer-1.0.1-macOS-update.tar.gz.sig" "$SERVE/ZyperOS-Installer-1.0.1-Windows.exe.sig"
python3 tools/latest-json.py --version 1.0.1 --dir "$SERVE" --output "$SERVE/latest.json" > /dev/null
sed -i '' "s#https://github.com/tarikbc/ZyperOS/releases/download/v1.0.1/#http://127.0.0.1:$PORT/#" "$SERVE/latest.json"
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$SERVE" > "$WORK/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; pkill -f "$WORK/.*/$NAME.app/Contents/MacOS/" 2>/dev/null || true' EXIT
sleep 1

fail=0
for case in plain allowed; do
	say "1.0.0 updating itself, $case"
	DIR=$WORK/case-$case
	rm -rf "$DIR" && mkdir -p "$DIR"
	APP=$DIR/$NAME.app
	ditto "$WORK/1.0.0/$NAME.app" "$APP"
	if [ $case = allowed ]; then
		# What a download the user let open looks like: flag 0x40 is "allowed"
		xattr -r -w com.apple.quarantine "01c1;$(printf %x "$(date +%s)");Safari;$(uuidgen)" "$APP"
	fi
	open -g -j -n --env ZYPEROS_INSTALLER_SELFUPDATE_TEST="$DIR/log.txt" "$APP"
	for _ in $(seq 1 60); do
		grep -q -E 'up to date|failed' "$DIR/log.txt" 2>/dev/null && break
		sleep 0.5
	done
	cat "$DIR/log.txt" 2>/dev/null || echo "(no log: the app didn't run)"
	now=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$APP/Contents/Info.plist")
	marks=$(xattr -lr "$APP" | grep -c com.apple.quarantine || true)
	echo "the app is now $now, with $marks quarantine marks"
	if [ "$now" != 1.0.1 ] || [ "$marks" != 0 ] || ! grep -q 'running 1.0.1' "$DIR/log.txt" 2>/dev/null; then
		echo "FAILED: expected it to run as 1.0.1 with no quarantine mark" >&2
		fail=1
	fi
	pkill -f "$APP/Contents/MacOS/" 2>/dev/null || true
	sleep 1
	"$LSREGISTER" -u "$APP" 2>/dev/null || true
done

say "Done, in $WORK"
[ $fail = 0 ] && echo "Both updated themselves and opened again as 1.0.1, with no quarantine mark."
exit $fail
