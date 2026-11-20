#!/bin/sh
# A stand-in for a port's first start, for tools/portout-check.sh --shots:
# what a port's script and PortMaster print while they install, slowly, as
# the loading screen would show it. Lines, a progress line rewritten in
# place with carriage returns, colors, a line of ZyperOS's own (portkeys:,
# which the screen passes over), a line too long for the box, and a line of
# German with letters past ASCII.
step=${PORTOUT_STEP:-0.5}

echo "Star Farm: first start"
sleep "$step"
printf '\033[1;34mharbourmaster\033[0m: checking runtimes for this port\n'
sleep "$step"
echo "Downloading weston_pkg_0.2.squashfs (31.0 MB)"
sleep "$step"
for p in 4:1.2 18:5.6 37:11.5 59:18.3 81:25.1 100:31.0; do
	printf '\r  %3d%%  %s MB of 31.0 MB' "${p%%:*}" "${p#*:}"
	sleep "$step"
done
printf '\n'
echo "portkeys: volume 9 -> 10 (VOL+)"
echo "Unpacking setup_star_farm_1.10_(64bit).exe with innoextract: app/data/StreamingAssets/aa/StandaloneLinux64/defaultlocalgroup_assets_all_3f5c1f.bundle"
sleep "$step"
echo "Größe geprüft: 1,2 GB frei"
sleep "$step"
printf '\033[32mDone.\033[0m Starting the game\n'
sleep "$step"
