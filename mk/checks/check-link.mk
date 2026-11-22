CHECKS += check-link
#| the Link Cable's launcher side (`src/link.c`), on fakes for every process, network and protocol
#| action: offered on Gambatte alone and only on a device that has it; the adapter's last two bytes
#| as the network's name (`ZyperOS-Link-7F2A`) and the handheld's (`Pixel 2 7F2A`), a scan's names
#| read back to hosts (not File Transfer's, a home's or its own); the host's code four digits from
#| the kernel's random (the real maker's 20000 draws even), new each time it hosts under the same
#| name, the WPA2 password made from it alone, a typed code read as four digits or refused, and a
#| wrong one joining nothing; an address as Gambatte's twelve digits, and nothing that is not one;
#| the fourteen SETOPT lines for the server and the client and the one for off; the datagrams read
#| back, tabs flattened, anything else none; the host's network before its socket, nothing to
#| Diatom before a hello, the server's options, the welcome and one notice on it, a beat every half
#| second, a stranger told busy, six seconds with no beat a lost link (said once, the core's link
#| off); the joiner's scan again while it is looked at, its join with the typed code, no hello
#| before an address and one every half second until the welcome, then the client's options; a bye
#| said as left; each failure (no hotspot, no scan, no join, no answer, busy, Diatom refusing); the
#| game's end quiet; the beat's socket on 10.42.1.1 alone; the stop's bye twice with nothing to
#| Diatom; and `src/linknet.c`'s socket on the loopback

check-link: build-native/link-check
	@./build-native/link-check

build-native/link-check: tools/link-check.c src/link.c src/link.h src/linknet.c src/linknet.h \
                         src/hotspot.h FORCE
	@mkdir -p build-native
	$(CC) -std=gnu11 -Wall -Wextra -D_GNU_SOURCE -O1 -g \
	      -o $@ tools/link-check.c src/link.c src/linknet.c
