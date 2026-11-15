#!/bin/sh
# The Pixel 2's Wi-Fi watcher (plastron's px2 /usr/lib/zyperos/wifi-watch)
# against a stand-in wpa_cli and log, with its times cut to seconds: the
# signal lines, the line when the link drops, and the one retry after a drop
# that cut a handshake, never more, and never for a network that has not
# joined. Runs with the computer's sh; the device's is busybox 1.37's ash.
set -u
WATCH=plastron/buildroot/board/px2/rootfs-overlay/usr/lib/zyperos/wifi-watch
T=$(mktemp -d "${TMPDIR:-/tmp}/wifi-watch-check.XXXXXX")
fails=0
pid=

cleanup() {
	[ -n "$pid" ] && kill "$pid" 2> /dev/null
	rm -rf "$T"
}
trap cleanup EXIT

ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# The stand-in: signal_poll answers what $T/signal holds; anything else is
# written to $T/cli.log, one call a line
cat > "$T/wpa_cli" << EOF
#!/bin/sh
shift 2
if [ "\$1" = signal_poll ]; then
	cat "$T/signal"
else
	echo "\$*" >> "$T/cli.log"
	echo OK
fi
EOF
chmod +x "$T/wpa_cli"
: > "$T/wifi.log"
: > "$T/cli.log"
signal() { printf 'RSSI=%s\nLINKSPEED=72\nNOISE=9999\nFREQUENCY=2442\n' "$1" > "$T/signal"; }
say() { printf '%s.000000: wlan0: %s\n' "$(date +%s)" "$1" >> "$T/wifi.log"; }

# Wait up to $3 s for $1 lines matching $2 in the log
lines() { grep -c -- "$1" "$T/wifi.log"; }
wait_lines() {
	i=0
	while [ "$(lines "$2")" -lt "$1" ] && [ $i -lt $(($3 * 10)) ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ "$(lines "$2")" -ge "$1" ]
}
calls() { grep -c -- "$1" "$T/cli.log"; }

signal -60
WIFI_LOG="$T/wifi.log" WPA_CLI="$T/wpa_cli" WIFI_FIFO="$T/fifo" LOOK=1 EVERY=4 JUMP=8 \
	sh "$WATCH" < /dev/null > "$T/out" 2>&1 &
pid=$!
sleep 1

echo "wifi-watch:"
# Not joined yet: no signal line, though the signal answers
sleep 1.5
[ "$(lines 'zyperos: signal')" -eq 0 ] && ok "no signal line before a join" || bad "a signal line before any join"

say "CTRL-EVENT-CONNECTED - Connection to aa:bb:cc:dd:ee:ff completed [id=0 id_str=]"
wait_lines 1 'zyperos: signal -60 dBm, 72 Mbps, 2442 MHz' 4 \
	&& ok "a signal line soon after the join" || bad "no signal line after the join"

signal -75
wait_lines 1 'zyperos: signal -75 dBm' 4 && ok "a jump of 15 dB is told at once" || bad "the jump was not told"

signal -77
sleep 2
[ "$(lines 'zyperos: signal -77')" -eq 0 ] && ok "2 dB is not a jump" || bad "a 2 dB move was told before EVERY"
wait_lines 1 'zyperos: signal -77 dBm' 6 && ok "a line again after EVERY" || bad "no line after EVERY"

say "CTRL-EVENT-DISCONNECTED bssid=aa:bb:cc:dd:ee:ff reason=0"
wait_lines 1 'zyperos: link lost; last look [0-9]* s before: -77 dBm, 72 Mbps' 3 \
	&& ok "the drop says the last look" || bad "no line for the drop"

n=$(lines 'zyperos: signal')
sleep 2
[ "$(lines 'zyperos: signal')" -eq "$n" ] && ok "no signal lines while not joined" || bad "signal lines while not joined"

say 'CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="Home reason=WRONG_KEY" auth_failures=1 duration=10 reason=WRONG_KEY'
wait_lines 1 'zyperos: network 0 joined with this password before' 3 \
	&& ok "a cut handshake on a network that joined: tried again" || bad "the cut handshake was not tried again"
[ "$(calls '^disable_network 0$')" -eq 1 ] && [ "$(calls '^enable_network 0$')" -eq 1 ] \
	&& [ "$(sed -n 1p "$T/cli.log")" = "disable_network 0" ] \
	&& ok "disable_network, then enable_network" || bad "wrong calls: $(tr '\n' ';' < "$T/cli.log")"

say 'CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="Home" auth_failures=2 duration=20 reason=WRONG_KEY'
sleep 1.5
[ "$(calls 'enable_network 0')" -eq 1 ] && ok "once per join: the second one waits" || bad "tried again twice without a join"

say 'CTRL-EVENT-SSID-TEMP-DISABLED id=3 ssid="Cafe" auth_failures=1 duration=10 reason=WRONG_KEY'
sleep 1.5
[ "$(calls 'network 3')" -eq 0 ] && ok "a network that never joined keeps its wait" || bad "a never-joined network was retried"

say 'CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="Home" auth_failures=1 duration=10 reason=CONN_FAILED'
sleep 1.5
[ "$(calls 'enable_network 0')" -eq 1 ] && ok "another reason keeps its wait" || bad "a CONN_FAILED wait was cut"

say "CTRL-EVENT-CONNECTED - Connection to aa:bb:cc:dd:ee:ff completed [id=0 id_str=]"
sleep 1.5
say 'CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="Home" auth_failures=1 duration=10 reason=WRONG_KEY'
wait_lines 2 'zyperos: network 0 joined' 3 && [ "$(calls 'enable_network 0')" -eq 2 ] \
	&& ok "a new join arms it again" || bad "not armed again after a join"

signal -9999
say "CTRL-EVENT-CONNECTED - Connection to aa:bb:cc:dd:ee:ff completed [id=0 id_str=]"
n=$(lines 'zyperos: signal')
sleep 2
[ "$(lines 'zyperos: signal')" -eq "$n" ] && ok "the drivers' -9999 is no signal" || bad "-9999 was told as a signal"

! grep 'zyperos:' "$T/wifi.log" | grep -q -i 'home\|cafe' && ok "no network name in its lines" || bad "a network name in its lines"

say "CTRL-EVENT-TERMINATING"
i=0
while kill -0 "$pid" 2> /dev/null && [ $i -lt 30 ]; do sleep 0.1; i=$((i + 1)); done
if kill -0 "$pid" 2> /dev/null; then bad "still running after the supplicant ended"; else ok "ends with the supplicant"; pid=; fi
[ ! -e "$T/fifo" ] && ok "its pipe is gone" || bad "its pipe is left"
[ ! -s "$T/out" ] && ok "nothing on its output" || bad "it printed: $(head -3 "$T/out")"

if [ $fails -gt 0 ]; then
	echo "wifi-watch: $fails check(s) failed"
	sed 's/^/  log: /' "$T/wifi.log" | tail -20
	exit 1
fi
echo "wifi-watch: every check passed"
