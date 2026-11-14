/**
 * The 1.0.5 cut for Reddit: what 1.0.5 added, and 1.0.3's and 1.0.4's USB
 * fixes, in under a minute, 4:5 for a phone's feed, where it plays muted.
 * Every feature is named in large words over the screen, so nothing depends
 * on the sound; the music is the tour's (src/music.ts) from its first
 * downbeat, and every scene is whole bars of it.
 *
 * It opens on NES Player 2 as a small story over six bars: MENU > Player 2 on
 * the handheld and its code, then the friend's phone in, its touch pad pressed
 * while the handheld plays on. Then the rest in the order the Reddit post
 * lists it: the new systems, button remapping, shaders and their strength, a
 * theme, folders and the list, the Scraper, a phone's internet, the
 * Achievements tab, a disc swapped and the pixel art; then 1.0.3's and
 * 1.0.4's USB File Transfer and Flash Drive, and the end, two beats past bar
 * 37, the handheld and the friend's phone in the same game again, while the
 * music fades. Each scene's tag is the release it came in. The
 * takes are the tour's (video/footage/takes.py), the phone's presses
 * video/footage/phone-presses.py's, and the screens the launcher's own.
 */
import { BPM, layout, Scene, Step } from "../timeline";

/** A beat in ms */
const B = 60000 / BPM;

/** The in-game menu's Shader values, as the README's picture of them shows
 * Balloon Fight (made in the QEMU machine with Diatom's own shaders; scripts/
 * prepare-assets.sh cuts it into one picture each), and the beats each holds */
export const SHADERS = [
  { name: "Off", file: "off", beats: 1 },
  { name: "Sharp-Shimmerless", file: "sharp", beats: 2 },
  { name: "CRT", file: "crt", beats: 2 },
  { name: "CRT Curved", file: "crt-curved", beats: 1 },
  { name: "Scanlines", file: "scanlines", beats: 1 },
  { name: "LCD Grid", file: "lcd", beats: 1 },
];
const shaderSteps: Step[] = SHADERS.map((s) => ({ image: `img/shader-${s.file}.png`, beats: s.beats }));

/** The friend's phone: the play page as it opens, then a finger on its d-pad and A */
const PRESSES = [
  { src: "img/play-phone.png", beats: 2.5 }, { src: "img/play-phone-right.png", beats: 1.5 },
  { src: "img/play-phone-right-a.png", beats: 1 }, { src: "img/play-phone-right.png", beats: 0.5 },
  { src: "img/play-phone.png", beats: 1 }, { src: "img/play-phone-a.png", beats: 0.5 },
  { src: "img/play-phone.png", beats: 0.5 }, { src: "img/play-phone-right.png", beats: 1.5 },
  { src: "img/play-phone-right-a.png", beats: 1 }, { src: "img/play-phone.png", beats: 1 },
];

export const REDDIT_SCENES: Scene[] = [
  // "ZyperOS 1.0.5" for the first four beats, over Super Mario Bros. 3 and the
  // in-game menu down to Player 2; A, and its code to scan, held
  { id: "p2", tag: "1.0.5", bars: 3, pose: "main",
    screen: [{ image: "img/smb3.png", beats: 2 }, { take: "player2", from: 0, to: 1600 },
             { take: "player2", from: 1600, to: "fill", rate: 0.71 }],
    label: "Player 2 on a friend's phone", sub: "MENU > Player 2 shows a code to scan", labelAt: 4 },
  // The phone in as the panel says connected; the handheld back in the game
  // while the friend's finger presses the d-pad and A
  { id: "p2play", tag: "1.0.5", bars: 3, pose: "split",
    screen: [{ take: "player2", from: 3100, to: 4700, rate: 0.8 }, { image: "img/smb3.png" }],
    label: "Scan it, and they're in", sub: "No app, no account. The game and a touch pad on their phone.",
    browser: { phone: "img/play-phone.png", phonePic: "img/smb3.png", phoneAt: 0.5, phoneSeq: PRESSES } },
  // The four new systems, each console's own start
  { id: "newsys", tag: "1.0.5", bars: 2, pose: "main", screen: [{ take: "newsys", from: 0, to: "fill", rate: 2 }],
    label: "PSP, and three more systems", sub: "Doom, ColecoVision and TIC-80" },
  // R1 to Controls, the handheld leaning toward each button, Y given another
  // button, then in on the handheld as L1 tilts it back
  { id: "remap", tag: "1.0.5", bars: 4, pose: "main", screen: [{ take: "gm-remap", from: 0, to: "fill", rate: 0.94 }],
    label: "Remap any button", sub: "Per game or per system, in the game's menu",
    punch: [{ at: 10, x: 0.22, y: 0.62, zoom: 1.55 }] },
  // Diatom's six looks on Balloon Fight, then the Display tab: Scanlines, and
  // its Strength stepped down
  { id: "shaders", tag: "1.0.5", bars: 3, pose: "main",
    screen: [...shaderSteps, { take: "gm-strength", from: 1500, to: "fill", rate: 1.8 }],
    label: "Sharp pixels, or an old TV", sub: "Shaders, with a Strength from 10% to 100%" },
  { id: "theme", tag: "1.0.5", bars: 2, pose: "main", screen: [{ take: "look", from: 6760, to: "fill", rate: 1.5 }],
    label: "A theme of your own", sub: "Light or dark, in the accent you pick" },
  // Into a folder and back up, then START for the list and back
  { id: "folders", tag: "1.0.5", bars: 3, pose: "main", screen: [{ take: "folders", from: 2400, to: "fill", rate: 1.55 }],
    label: "Folders, and a list", sub: "START turns any shelf into a list" },
  // MENU > Scraper: the plan, a run with the covers landing, done
  { id: "scraper", tag: "1.0.5", bars: 3, pose: "main",
    screen: [{ take: "scraper-page", from: 0, to: 1000, rate: 1.25 }, { take: "scraper-page", from: 1900, to: 8500, rate: 2.2 },
             { take: "scraper-page", from: 9520, to: "fill" }],
    label: "A new scraper", sub: "Box art for your whole library, as it finds it" },
  // Phone on the cable, its tethering, and File Transfer in its browser
  { id: "phone", tag: "1.0.5", bars: 2, pose: "split", screen: [{ take: "phone", from: 0, to: "fill", rate: 1.55 }],
    label: "Internet from your phone", sub: "Android USB tethering. No Wi-Fi adapter.",
    browser: { phone: "img/portal-phone.png", phoneAt: 4.5 } },
  { id: "cheevos", tag: "1.0.5", bars: 2, pose: "main", screen: [{ take: "gm-cheevos", from: 0, to: "fill", rate: 1.08 }],
    label: "An Achievements tab", sub: "In the game's menu. PSP and PS1 .chd too." },
  { id: "disc", tag: "1.0.5", bars: 2, pose: "main", screen: [{ take: "gm-disc", from: 0, to: "fill", rate: 1.15 }],
    label: "Swap discs in the game", sub: "For multi-disc PS1 games" },
  { id: "art", tag: "1.0.5", bars: 2, pose: "main", screen: [{ take: "home", from: 2100, to: "fill", rate: 1.28 }],
    label: "New pixel art for every system", sub: "Drawn for ZyperOS, and it moves" },
  // 1.0.3: USB File Transfer at zyperos.local, each step ticked as it connects
  { id: "usbsteps", tag: "1.0.3", bars: 2, pose: "main", screen: [{ take: "usbsteps", from: 0, to: "fill", rate: 1.28 }],
    label: "USB File Transfer at zyperos.local", sub: "Each step ticks off as it connects" },
  // 1.0.4: Flash Drive
  { id: "flashdrive", tag: "1.0.4", bars: 2, pose: "main", screen: [{ take: "flashdrive", from: 0, to: "fill", rate: 1.28 }],
    label: "A plain flash drive", sub: "Any computer, no driver" },
  // Back to Player 2: the handheld and the friend's phone in the same game
  { id: "end", bars: 2.5, pose: "hero", screen: [{ image: "img/smb3.png" }],
    browser: { phone: "img/play-phone.png", phonePic: "img/smb3.png", phoneAt: 1,
      phoneSeq: [{ src: "img/play-phone.png", beats: 2 }, { src: "img/play-phone-right.png", beats: 2 }, { src: "img/play-phone-right-a.png", beats: 1 },
        { src: "img/play-phone-right.png", beats: 1 }] } },
];

export const REDDIT = layout(REDDIT_SCENES);
/** The hook's "ZyperOS 1.0.5" gives way to the first label on this beat */
export const HOOK_BEATS = 4;
export const BEAT_MS = B;
