#!/bin/sh
# wget for your PICO-8 (sd/pixel2/PICO-8.sh): on the card as
# ZyperOS/pico8/bin/wget, first on PICO-8's PATH. POSIX sh: busybox's ash on
# the device, the computer's sh in the check (tools/pico8-wget-check.sh).
#
# pico8_64, the PICO-8 for Raspberry Pi, has no HTTP of its own: no libcurl,
# no sockets. Splore lists and downloads every cart with system(), as
#   wget "URL" -q -O "FILE"
# and posts with --post-file=FILE on the end (its readme: "PICO-8 uses wget
# for bbs downloads"; config.txt's use_wget changes nothing in this build).
# The system's wget is busybox's, built without HTTPS ("not an http or ftp
# url"), and every cart is HTTPS: https://carts.lexaloffle.com/<id>.p8.png,
# and get_cart.php over http redirects there. So Splore listed carts and
# then could download none of them (the device test, 2026-11-19, with a Wi-Fi
# adapter: "could not connect to bbs").
#
# So those calls go to curl, the one the launcher talks with (src/net.c),
# over OpenSSL, with the certificates checked against ZyperOS's own bundle
# beside the launcher (res/ssl), as the launcher's are. Its options file,
# $HOME/.curlrc, is read as the launcher's curl reads it (no -q), so a proxy
# there for a test on the USB cable serves PICO-8 too. A file is written
# under a name of its own and renamed when it is whole, so a download cut
# short never leaves Splore half a cart it takes for a whole one.
#
# Anything else, or a system with no curl: the system's own wget, the next
# one on PATH, with the same arguments.
here=$(cd "${0%/*}" 2> /dev/null && pwd)

# The next wget: PATH without this folder, however it is spelled there. And
# never this one again, should a PATH name it some other way still
system_wget() {
	rest=
	set -f
	IFS=:
	for d in $PATH; do
		[ "$(cd "$d" 2> /dev/null && pwd)" = "$here" ] || rest=${rest:+$rest:}$d
	done
	unset IFS
	set +f
	PATH=$rest
	export PATH
	if [ -n "${ZYPEROS_P8WGET:-}" ] || [ -z "$rest" ] || ! command -v wget > /dev/null 2>&1; then
		echo "pico8 wget: no other wget on PATH" >&2
		exit 127
	fi
	ZYPEROS_P8WGET=1
	export ZYPEROS_P8WGET
	exec wget "$@"
}

command -v curl > /dev/null 2>&1 || system_wget "$@"

url= out= post= want=
for a in "$@"; do
	case $want in
	out) out=$a; want=; continue ;;
	post) post=$a; want=; continue ;;
	esac
	case $a in
	-q | --quiet) ;;
	-O) want=out ;;
	--post-file) want=post ;;
	--post-file=*) post=${a#*=} ;;
	-*) system_wget "$@" ;;
	*) [ -z "$url" ] || system_wget "$@"; url=$a ;;
	esac
done
[ -n "$url" ] && [ -n "$out" ] && [ -z "$want" ] || system_wget "$@"

# ZyperOS's certificate bundle, two folders up (ZyperOS/cacert.pem), as
# mk/payload.sh lays the card out; without it, curl's own
ca="$here/../../cacert.pem"
set -- -f -sS -L --max-redirs 10 --proto =http,https --proto-redir =http,https \
	--connect-timeout 20 --max-time 180
[ -s "$ca" ] && set -- "$@" --cacert "$ca"
[ -n "$post" ] && set -- "$@" --data-binary "@$post"

if [ "$out" = - ]; then
	exec curl "$@" -o - "$url"
fi
part="$out.part$$"
curl "$@" -o "$part" "$url"
rc=$?
if [ $rc -eq 0 ]; then
	# An empty reply leaves curl no file to write: wget's is there, empty
	[ -f "$part" ] || : > "$part"
	mv -f "$part" "$out" && exit 0
	rc=1
fi
rm -f "$part"
exit $rc
