/**
 * The 1.0.6 video's music, its own (the tour and the Reddit cut keep
 * src/music.ts's): Tarik's pick, "Level 1" from Juhani Junkala's "5 Chiptunes
 * (Action)", CC0 (LICENSES.md has the page). scripts/prepare-assets.sh fetches
 * it into a cache outside the repository, checks it, and writes
 * public/music/level1-loop.wav: the file from its first downbeat, its 56-bar
 * loop played once and then from the top again, so the video runs past the
 * file's own length without its fade.
 *
 * Measured (LICENSES.md says how): exactly 181 BPM, the first downbeat
 * 0.0259 s into the file as ffmpeg decodes it, and the last 4 of its 60 bars
 * are bars 0 to 3 again, fading: the loop is 56 bars, 74.25 s.
 *
 * To change the track: change this object (and prepare-assets.sh's fetch),
 * then lay src/v106/timeline.ts's scenes on the new track's bars again.
 */
export const TRACK106 = {
  /** in public/music/, made by scripts/prepare-assets.sh, its first sample on bar 0 */
  file: "level1-loop.wav",
  title: "Level 1",
  artist: "Juhani Junkala",
  license: "CC0",
  page: "https://opengameart.org/content/5-chiptunes-action",
  bpm: 181,
  /** the downbeat the video starts on, in seconds into `file` */
  startS: 0,
  /** its level in the mix: Level 1 is mastered 8.5 dB louder than Leap (-7.6
   * LUFS against -16.1), so 0.9, the tour's, less 8.5 dB keeps the interface
   * sounds where they were */
  gain: 0.34,
};

/** A beat in frames at 30 fps: 9.94 at 181 BPM */
export const BEAT106 = (60 * 30) / TRACK106.bpm;
