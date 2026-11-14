# Landing page refresh

## Current task status, 14 November 2026

### Built and checked locally

- [x] Shorter landing page, larger product views, motion controls, and current launcher screenshots.
- [x] Original system art and a draggable shelf of all 44 systems.
- [x] Shoulder recheck: outer cutouts and all four caps verified. L2/R2 upper corners corrected; source, build, and HTTP assets match.
- [x] Jewelry-style hero rotation with hover, keyboard, visibility, and reduced-motion controls.
- [x] Dedicated Player 2 section with an iPhone-style phone and the actual 3D Pixel 2.
- [x] Playable two-screen demo, automatic Player 1, user-controlled Player 2, shared frames, pause, reset, and completion.
- [x] Nostalgic MP3/MP4 section with albums, wired earbuds, a mix CD, and working music previews.
- [x] Dark power-off/resume section with the bookmark artifact between Player 2 and music.
- [x] Expandable features with screenshots.
- [x] Guide audit: added the full 44-system reference plus Ports, fixed menu paths and circular sources, and checked the rebuilt guide destinations.
- [x] ZyperOS-only install page, live latest-release download buttons, and correct navigation highlighting.
- [x] Dedicated tip page with Zyper, original art, accurate hardware, clear payment information, and the existing checkout.
- [x] First-person creator voice across site prose. Author attribution and legal names remain accurate.
- [x] Zyper photo proportions fixed and checked at desktop and mobile sizes.
- [x] Exact Eric boot-speed credit under “A quicker start.”
- [x] Tip-page copy-link control, manual-copy fallback, and no-JavaScript checkout link checked.

- [x] Eric recognition: portrait section after the FAQ, first-person thank-you post, and links from the tip, About, and installation pages.
- [x] Post explains my gratitude, TortOS as the inspiration, and the disagreement without asserting Eric's private feelings.
- [x] Installation section uses the secondary background and matching text, borders, and focus colors in both themes.

### Open work

- [x] Full README feature coverage: all 48 feature bullets, nine chapters, 27 screenshot choices, charging screen, and charge-limit preview. Browser and build checks passed.

- [x] Shoulder clarification: all four buttons now have an abrupt underside step, from a thicker main body to a thin rear lip. The case stays unchanged. Source and exported geometry checks pass.
- [x] Standalone /download/ page with latest-release files, fallback links, and a separate installation guide. Primary download actions now point there.
- [x] Change the hero tour arrow to point down the page.
- [x] One shared header template, stylesheet, and mobile controller across all 31 pages, with correct active states.

Publication is separate from this local preview work.

### Delivery status

All changes remain in the local checkout. They are not committed, pushed, merged, or deployed.
The preview server remains available at http://127.0.0.1:8887/.

Latest link audit: the built homepage has no external anchors, and the site has no first-party GitHub Markdown help links.
Twenty-five links to other projects' Markdown documents remain in eight Sources sections. These are primary-source citations, not guide-navigation destinations.
Latest model audit: the source, build, and HTTP-served GLB and both posters match byte for byte. The GLB hash starts with `b045c4bd6888`.

## Goal and final design

Build a concise, polished landing page with meaningful interaction, current launcher captures, and original system art.
The user approved the first design and then directed the following additions.

- The hero uses the accurate Pixel 2 model with corrected rounded shoulder corners and lower cutouts. A slow turntable and studio reflections show its shape.
- Five screen choices show the current launcher. Supported choices also show light and dark appearance.
- A horizontal shelf contains all 44 system illustrations. Mouse drag, native touch scrolling, arrows, and keyboard navigation work together.
- Player 2 has its own section. An iPhone-style phone and the actual GLB model show one original browser game. Player 1 moves automatically; the visitor controls Player 2. The page explains that this is not NES emulation or a device connection.
- A dark power-off/resume section separates Player 2 and music. A book and lime bookmark represent keeping a place. Text limits auto-resume to games with state support.
- The music scene combines the accurate device, a native player capture, wired earbuds, and a mix CD. Three 30-second CC0 previews have real playback and seek controls.
- Nine feature chapters cover all 48 README feature bullets. Each has three screenshot choices, plus a dedicated charging screen and charge-limit preview.
- Every homepage link stays on the site. Six task guides, an About page, and the Eric story provide on-site destinations. GitHub Markdown help links no longer replace the guides.
- The install guide covers ZyperOS only. Direct macOS, Windows, DEB, RPM, card-image, and checksum links update from GitHub's latest release API. Verified fallback links work without JavaScript or API access.
- Install, Guide, and Blog navigation states follow the current page.
- The tip page presents the project in the creator's first-person voice. It uses the original artwork and Zyper's real photo. Its button opens the existing amount-editable Stripe checkout.

## Files and boundaries

`site/index.html` keeps shared color tokens and page content. `landing.css` and `landing.js` own layout, screen choices, system browsing, and motion controls.
`player-two.css` and `player-two.js` own the original Star Trail demo. `pixel2.js` shares the GLB renderer between the hero and the live game display.
`music.js` owns preview playback. `tools/site-music.py` makes the licensed excerpts. `site/music/credits.json` and `capture.json` record their sources.
`site/downloads.js` validates official release file URLs, updates download controls, and handles unavailable files and request failures.
`site/tip/index.html`, `tip.css`, and `tip.js` own the support page. `/tip` redirects to `/tip/`.
`mk/site.sh` assembles assets and content versions. `mk/blog.py` builds the guides and checks local links. Comparison posts retain their Sources requirement.

The generated music props and bookmark use the built-in image tool. Their prompts are in `site/music/ARTWORK.md` and `site/scenes/README.md`.
The model generator creates original geometry. The shoulder reference is https://tortos.games/manual#pixel-2.
Legal downloads remain exact copies of the repository's LICENSE, NOTICE, TRADEMARK.md, and THIRD-PARTY-LICENSES.md.

## Completed checks

- [x] Native launcher build and current screenshot captures; source and image hashes retained.
- [x] Blender renders and closed shoulder mesh checks.
- [x] Static build and `make check-blog`, covering 26 posts, images, links, metadata, sitemap, feed, and shared colors.
- [x] JavaScript and Python syntax checks; `git diff --check`.
- [x] Desktop and mobile layouts from 320 to 1440 pixels, with both color themes.
- [x] Screen selection, delayed requests, failed screenshots, and keyboard control.
- [x] All 44 systems, horizontal drag, arrow navigation, and mixed mouse/touch input.
- [x] Real music playback, pause, seek, next track, errors, recovery, and no automatic audio download.
- [x] Star Trail completion at 8/8 through real keyboard input, dash, replay, reset, arena bounds, and shared frame contents.
- [x] Actual GLB game texture, idle and offscreen rendering, WebGL failure/context loss, and static fallback.
- [x] Hero rotation, hover pause/resume, manual rotation, global pause, reduced motion, and visibility behavior.
- [x] No-JavaScript content, native audio controls, and feature disclosures.
- [x] Live release API returned HTTP 200 and v1.0.4 with six correct download destinations.
- [x] Download tests covered a newer release, missing files, API failure, retry, foreign URL rejection, and no JavaScript.
- [x] Navigation regression checks show Install on installation, Guide on guide pages, and Blog on the index.
- [x] Tip layouts at 320, 390, 768, 1024, and 1440 pixels; correct photo proportions; sharing success and failure; static checkout without JavaScript.

## Review fixes

Independent review found cached device code could ignore new motion controls. Build output now versions both dynamic imports, the GLB, styles, and changed images.
The old README video link now opens the retained MP4 and links separately to current screens.
A reproduced mixed-input bug blocked a touch selection after a mouse drag. The click guard now resets per gesture.
A reproduced keyboard issue reset the demo on Shift+Tab. The demo now uses R for reset, so keyboard navigation preserves progress.
The final download review found no actionable defects.

## Handoff

Work remains local on `codex/landing-page-refresh`.
The preview runs at http://127.0.0.1:8887 with HTTP Range support for audio seeking.
No push, merge, or deployment occurred.
The release API last returned v1.0.4 during verification. The download page follows the live release response and contains no unverified v1.0.5 file URLs.


## Audit closure

The final shoulder audit compares the actual exported and served GLB with the TortOS reference. All four cap meshes are closed. The served GLB hash starts with `b045c4bd6888`.
The guide audit found genuine gaps and repaired them. The complete system reference contains 45 rows: 44 systems and Ports. Runtime guides state their 1.0.5 scope, while the download page follows the published release.
The Eric portrait remains byte-for-byte identical to the supplied image. The homepage credit follows the FAQ. The post uses the creator's supplied account and keeps uncertain feelings conditional.
`make check-blog` passes for 26 posts and all generated destinations. Browser checks cover the new story filter, portrait layout, full system reference links, and both installation-section themes.


## Clarified shoulder and shared-header checks

The earlier shoulder checks verified the rounded outline but missed the required thickness step. The user clarified that the cutout sits underneath the rear lip. The new cap geometry keeps a 2.6 mm main pad and a 1.25 mm rear lip over the final 1.8 mm of depth. A small molded edge radius preserves the abrupt ledge.
The discarded case-bevel trial is not part of the final model. Mesh comparison confirms that only L1, L2, R1, and R2 changed. Ray checks on the exported GLB measure a 1.35 mm underside step for all four caps. The old uniform extrusion fails that check.
The shared header is rendered from `site/_includes/header.html` by `mk/site_header.py`, with `header.css` and `header.js`. It covers 31 generated HTML pages, including the 404 page. Desktop geometry, mobile keyboard controls, no-JavaScript navigation, active states, and real Install/Download navigation passed browser checks.
`/download/` now owns the release picker. The installation article only explains installation and links to that page. The hero's tour arrow points down.

## README feature coverage, 14 November 2026

The table maps each top-level README feature to visible landing content. Details remain in native disclosures.
The source behavior takes precedence over stale comments: `src/powerscreens.c` uses `IN_Y` for a one-time full charge. The battery guide now matches.

| README feature | Landing destination |
| --- | --- |
| Forty-four systems | #feature-systems |
| Ports | #feature-systems |
| Nintendo DS your way | #feature-systems |
| Nintendo 64 at full speed | #feature-systems |
| Dreamcast | #feature-systems |
| A home screen in pixel art | #feature-appearance |
| Eight languages | #feature-appearance |
| A first start that asks | #feature-appearance |
| An empty card that says what to do | #feature-appearance |
| Your folders, kept | #feature-library |
| Covers or a list | #feature-library |
| MENU in tabs | #feature-appearance |
| Your look | #feature-appearance |
| Search Games and Random Game | #feature-library |
| RetroAchievements | RetroAchievements card |
| Box art the device finds itself | #feature-artwork |
| Coming from Knulli, ROCKNIX, ArkOS or ROGUE OS | #feature-artwork |
| Rename a game from the handheld | #feature-library |
| Delete a game from the handheld | #feature-library |
| Storage, as a phone shows it | #feature-storage |
| Video snaps | #feature-artwork |
| A screensaver of your games | #feature-artwork |
| Auto Off waits for long jobs | #feature-care |
| Wi-Fi File Transfer | #feature-connections |
| USB that only charges until you say so | #feature-connections |
| Player 2 on a friend's phone | #feature-connections |
| System Update | #feature-connections |
| Autosave and resume | #feature-gameplay |
| Quick save and quick load, without opening a menu | #feature-gameplay |
| A QWERTY keyboard | #feature-library |
| Clocks per system | #feature-care |
| Turbo buttons | #feature-gameplay |
| Faster and slower | #feature-gameplay |
| Shaders, from sharp pixels to an old TV | #feature-gameplay |
| Every control, in the game | #feature-gameplay |
| Rumble on the Pixel 2 | #feature-gameplay |
| A USB-C headphone adapter on the Pixel 2 | #feature-sound |
| Bluetooth headphones | #feature-sound |
| Mono audio | #feature-sound |
| MP4, a media player for music, audiobooks and videos | #feature-sound |
| Menu music | #feature-sound |
| The top bar | #feature-care |
| The Pixel 2's side lights | #feature-care |
| Battery care | #battery-care |
| About | #feature-appearance |
| Play Time | #feature-library |
| A way out of anything | #feature-gameplay |
| Continue or from the start, favorites and sorting | #feature-library |

Verified locally:
- All 27 screenshot selections decode, display the requested image, and keep one selected control.
- All four charge-limit settings update the meter and explanation; keyboard arrows work.
- All nine chapters open with native keyboard controls and close the previous chapter.
- No horizontal overflow at 320, 390, 768 or 1440 pixels, in light and dark themes.
- No-script users keep native disclosures, default screenshots, content and guide links; inactive demo controls stay hidden.
- Reduced motion and Pause motion suppress transitions and screenshot animation.
- Failed screenshots preserve the previous image; retry works; late requests cannot replace the latest choice.
- `make check-blog` passes for 26 posts and the build renders the shared header on 31 pages. JavaScript syntax and `git diff --check` pass.
- 28 native captures refreshed with per-image source hashes. The charging, game details, shader menu, first-start language and update images changed.

Browser check script and screenshots: `build/feature-qa/`. Element screenshots of sections taller than the viewport can include an offscreen fixed skip link due to Chromium capture behavior; viewport captures confirm the link stays hidden until focus.

## Scraper screenshot correction, 14 November 2026

The user found the old Scraper panel in the new feature chapter. The earlier capture used the native launcher from this site branch (5116943), which predates the merged scraper-page and in-game tabs. Fresh timestamps did not mean current UI.

Recaptured 36 feature frames using the clean final checkout at main commit `71c6122259150af55603b3a45d326ca486cc693c`, its rebuilt native binary, and its own screenshot helper. The Scraper and in-game Display images changed; the other frames matched. Per-image provenance is recorded in `res/readme/captures.json`. The scraper command is `--scraper-page 1`, not `--scraper 2`.

The landing chapter now shows the full-page plan, and its copy and image descriptions match. The box-art guide uses the current A / Y / START flow, video selection, and one-second preview delay. Guide images and blog thumbnails now carry content hashes, as the landing images already do.

Verified: source, built and HTTP-served screenshot hashes match; the landing image and guide use the new capture; screenshot switching works; the mobile chapter has no overflow; guide and thumbnail hashes agree. `make check-blog` and `git diff --check` pass. Capture source guidance in `res/readme/README.md` prevents an older checkout from being mistaken for the current interface.
