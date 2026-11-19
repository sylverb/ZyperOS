/**
 * The 1.0.6 video's scenes, for the ZyperOS Discord: what 1.0.6 added, most
 * of it asked for there, and the TrimUI Brick, its first release for a second
 * handheld. The tour's way (src/timeline.ts): the launcher's own frames
 * (video/footage/takes.py's 1.0.6 takes), scenes in whole bars of the music,
 * with the tour's `layout`, on the video's own track (src/v106/music.ts).
 *
 * Each scene's last piece of footage is a `fit` step: from one moment of its
 * take to another, played over the scene's length, so the scenes can be laid
 * on another track's bars and still show the same thing.
 *
 * A scene's `device` is the handheld its screen is in, the Pixel 2's unless
 * it says the Brick's; `pair` scenes stand both side by side, true to size,
 * each with its own take (`pair`).
 */
import { layout, Scene, Step } from "../timeline";
import { BEAT106 } from "./music";

export type Scene106 = Scene & {
  device?: "pixel2" | "brick";
  /** The pair's screens: the Pixel 2's and the Brick's */
  pair?: { pixel2: Step[]; brick: Step[] };
  /** A word beside the label: where the feature came from */
  ask?: "asked" | "reported";
};

const fit = (take: string, from: number, to: number): Step[] => [{ take, from, to, fit: true }];

/* On Level 1's 181 BPM bars (1.33 s each): scenes of two to four bars, the
 * opening and the Brick four each, the end card from bar 66 to two beats past
 * bar 72, where the music fades. The track's 56-bar loop comes round at bar 56
 * (Hold POWER), from its top again. */
export const SCENES106: Scene106[] = [
  // Both handhelds, each on its own home, the release's name beside them,
  // all drawn at frame 0 (the poster before play), from just after Zyper's
  // pop-up of the menu's song (2.6 s into a live take)
  { id: "open", bars: 4, pose: "hero", enter: "cut", screen: [],
    pair: { pixel2: fit("home6", 2700, 7990), brick: fit("brick-home", 2700, 7990) } },
  // The Brick alone: its home, into the SNES shelf, along it
  { id: "brick", bars: 4, pose: "main", device: "brick", enter: "whip", screen: fit("brick-home", 3000, 9490),
    label: "Now on the TrimUI Brick", sub: "From its own microSD card. Take it out, and the Brick is as it was." },
  // MENU > View > Grid: START from Covers to Grid, along, down and a page
  { id: "grid", bars: 4, pose: "main", enter: "whip", screen: fit("grid", 3800, 11620),
    label: "Grid view", sub: "Covers in their shelf's own shape", ask: "asked" },
  { id: "homegrid", bars: 2, pose: "main", enter: "cut", screen: fit("homegrid", 4250, 9210),
    label: "The whole home at once", sub: "START steps through its layouts", ask: "asked" },
  { id: "homesys", bars: 2, pose: "main", enter: "whip", screen: fit("homesys", 0, 3320),
    label: "Pick the home's systems", sub: "And Recents: the ten games you played last", ask: "asked" },
  // The SG-1000's shelf, its games with no art each on its own card
  { id: "namecard", bars: 3, pose: "main", enter: "whip", screen: fit("namecard", 2350, 8330),
    label: "A card for every game with no art", sub: "Here on the new SG-1000 shelf" },
  { id: "screencolors", bars: 3, pose: "main", enter: "whip", screen: fit("screencolors", 0, 4000),
    label: "Screen Colors", sub: "Six looks, or your own. On the Pixel 2.", ask: "asked" },
  { id: "night", bars: 2, pose: "main", enter: "cut", screen: fit("night", 0, 3260),
    label: "Night Mode", sub: "Warmer at night, by your hours. On the Pixel 2.", ask: "asked" },
  // The Accent list repainting everything, then the dark background's list
  { id: "hover", bars: 3, pose: "main", enter: "whip",
    screen: [{ take: "hover", from: 0, to: 2480, rate: 1.3 }, ...fit("hover", 3470, 5270)],
    label: "See your theme as you pick it", sub: "The whole screen, under the cursor" },
  { id: "foldercolor", bars: 2, pose: "main", enter: "cut", screen: fit("foldercolor", 0, 4130),
    label: "Folder Color", sub: "One accent for every folder" },
  { id: "undo", bars: 2, pose: "main", enter: "whip", screen: fit("undo", 0, 2860),
    label: "Undo Load", sub: "Loaded the wrong slot? Go back.", ask: "asked" },
  { id: "stats", bars: 3, pose: "main", enter: "cut", screen: fit("stats", 0, 3400),
    label: "On-Screen Stats", sub: "FPS, temperature, battery and the time" },
  { id: "dmode", bars: 2, pose: "main", enter: "whip", screen: fit("dmode", 0, 3300),
    label: "See what a setting does", sub: "Each mode on the game's own picture",
    punch: [{ at: 1, x: 0.3, y: 0.42, zoom: 1.35 }] },
  { id: "lcd", bars: 3, pose: "main", enter: "cut", screen: fit("lcd", 0, 3600),
    label: "LCD No Backlight", sub: "The Game Boy's screen, as it was. On the Pixel 2.", ask: "asked",
    punch: [{ at: 2, x: 0.15, y: 0.5, zoom: 1.5 }] },
  { id: "timing", bars: 3, pose: "main", enter: "whip", screen: fit("timing", 0, 3630),
    label: "Smooth or Exact", sub: "Even motion, or the console's own rate" },
  { id: "radl", bars: 3, pose: "main", enter: "whip", screen: fit("radl", 0, 3900),
    label: "Achievements, ahead of time", sub: "Download them all, then play offline", ask: "asked" },
  { id: "lshader", bars: 3, pose: "main", enter: "cut", screen: fit("lshader", 0, 3660),
    label: "Shaders on ZyperOS itself", sub: "CRT on the home, shelves and menus. On the Pixel 2." },
  // File Transfer's page in a computer's browser: several games picked
  { id: "xfer", bars: 4, pose: "split", enter: "whip", screen: fit("xfer-hand", 0, 9300),
    label: "File Transfer, many at once", sub: "Pick several, then move or delete them", ask: "asked",
    browser: { laptop: "img/xfer-pick.png", laptopAt: 2 } },
  { id: "locked", bars: 2, pose: "split", enter: "cut", screen: fit("xfer-hand", 0, 9300),
    label: "Console folders stay put", sub: "So ZyperOS always finds your games",
    browser: { laptop: "img/xfer-locked.png", laptopAt: -1 } },
  { id: "portline", bars: 2, pose: "main", enter: "whip", screen: fit("portline", 0, 3810),
    label: "A port that's busy says so", sub: "And how to go back, if a first start takes hours", ask: "reported" },
  { id: "powerhold", bars: 2, pose: "main", enter: "whip", screen: fit("powerhold", 0, 2560),
    label: "Hold POWER to turn off", sub: "Let go to cancel", ask: "asked" },
  // The PSP's numbers, from the release notes, beside its home
  { id: "psp", bars: 4, pose: "split", enter: "whip", screen: fit("psp-home", 0, 6310),
    label: "PSP, much faster", sub: "Measured on a Pixel 2", ask: "asked" },
  // Both again, the same Grid on each
  { id: "pair", bars: 4, pose: "hero", enter: "cut", screen: [],
    pair: { pixel2: fit("grid", 3800, 12430), brick: fit("brick-grid", 3800, 12430) },
    label: "Same ZyperOS on both", sub: "The GKD Pixel 2 and the TrimUI Brick" },
  // Six bars and two beats: the downbeat of bar 72 rings while the music fades
  { id: "end", bars: 6.5, pose: "hero", enter: "cut", screen: [],
    pair: { pixel2: fit("home6", 2700, 9500), brick: fit("brick-home", 2700, 9500) } },
];

export const R106 = layout(SCENES106, BEAT106);

/** The pair's clips, laid as the main screen's are: one layout a handheld,
 * over the same scenes, with the pair's steps as each scene's screen */
const pairOf = (which: "pixel2" | "brick") =>
  layout(SCENES106.map((sc) => ({ ...sc, screen: sc.pair ? sc.pair[which] : [] })), BEAT106);
export const PAIR = { pixel2: pairOf("pixel2"), brick: pairOf("brick") };

/** The PSP's numbers (docs/releases/v1.0.6.md): frames a second before and after */
export const PSP_ROWS = [
  { name: "MotorStorm: Arctic Edge", from: 3, to: 19 },
  { name: "God of War: Chains of Olympus, its menu", from: 17, to: 31 },
];
