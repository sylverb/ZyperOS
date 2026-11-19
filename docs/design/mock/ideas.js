/* ZyperOS ideas labs: what the 1.0.6 screens add to the mock kit, and a way
 * to play them. Loaded after tokens.js and kit.js (and sounds.js), by the
 * *-ideas-lab.html pages; tools/mockpage.py inlines it like the rest.
 *
 * kit.js draws 1.0.4's panel; 1.0.6 has MENU's tab row, option lists with a
 * picture column, scales, button glyphs per handheld and the one foot of
 * keys. These are here, in the launcher's 1024x768 units, from the tokens.
 *
 *   ZI.status(light)          the clock and the battery
 *   ZI.panel(o)               a panel: a tab row or a heading, rows, extras
 *   ZI.rows(list)             menu rows (values, scales, glyphs, ticks, notes)
 *   ZI.pick(o)                an option list, with a picture column if o.pv
 *   ZI.glyph(b, size, lit)    a button as the handheld prints it (ZI.dev)
 *   ZI.chord(list, sign)      buttons together (+) or one after another (,)
 *   ZI.hints(list)            the keys on the foot
 *   ZI.question(o)            the yes/cancel panel
 *   ZI.popup(text, mark)      the pop-up pill at the top
 *   ZI.scene(kind, w, h)      a drawn stand-in game picture
 *   ZI.handheld(lit)          the handheld, small, keys lit
 *   ZI.demo(kind, id, theme)  a playable screen; ZI.kinds holds the kinds
 *   ZI.start(root)            keys, pads, hold-repeat, motion, sounds
 *   ZI.sound(name, level)     the launcher's own WAVs (sounds.js), Wood
 */
(function () {
  "use strict";
  const T = window.ZYPER_TOKENS;
  const esc = s => String(s).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);

  /* ---- the 1.0.6 pieces, as CSS in units --------------------------------- */
  const CSS = `
  .zs .i-abs{position:absolute}
  .zs .i-clock{position:absolute;left:64px;top:52px;font-size:26px;color:var(--text)}
  .zs .i-batt{position:absolute;right:60px;top:52px;width:44px;height:26px;color:var(--text)}
  .zs .i-game{position:absolute;inset:0}.zs .i-game svg{display:block}
  .zs .i-home{position:absolute;inset:0;background:radial-gradient(140px 90px at 120px 600px,color-mix(in srgb,#8a6fc0 60%,transparent),transparent),radial-gradient(160px 100px at 900px 590px,color-mix(in srgb,#c4443a 55%,transparent),transparent),radial-gradient(120px 90px at 512px 640px,color-mix(in srgb,#2e8b3e 50%,transparent),transparent),var(--ground);filter:blur(6px)}
  .zs .i-veil{position:absolute;inset:0}
  .zs .i-pn{position:absolute;border-radius:28px;background:var(--surface);box-shadow:inset 0 0 0 3px var(--edge),var(--panel-lift);transform-origin:50% 50%}
  .zs .i-htitle{position:absolute;left:0;right:0;top:30px;text-align:center;font-size:55px;color:var(--text-soft);white-space:nowrap}
  .zs .i-rule{position:absolute;left:52px;right:52px;top:108px;height:2px;background:var(--edge)}
  .zs .i-tabname{position:absolute;top:30px;font-size:55px;color:var(--text);white-space:nowrap;transform:translateX(-50%)}
  .zs .i-tabn{position:absolute;top:42px;font-size:37px;color:var(--text-dim);white-space:nowrap;transform:translateX(-50%)}
  .zs .i-tabbar{position:absolute;top:106px;height:6px;border-radius:3px;background:var(--accent-ink);transform:translateX(-50%)}
  .zs .i-pill{position:absolute;height:40px;min-width:50px;padding:0 12px;border-radius:20px;background:var(--raised);box-shadow:inset 0 0 0 2px var(--edge);display:grid;place-items:center;font-size:18px;font-weight:600;color:var(--text-soft)}
  .zs .i-body{position:absolute;right:26px}
  .zs .i-fill{position:absolute;left:0;right:0;height:72px;border-radius:14px;background:var(--accent)}
  .zs .i-rw{position:relative;height:72px;display:flex;align-items:center;justify-content:space-between;gap:24px;padding:0 26px;font-size:49px;color:var(--text-soft);white-space:nowrap}
  .zs .i-rw .v{color:var(--text-dim);display:flex;align-items:center;gap:14px}
  .zs .i-rw .v.ink{color:var(--accent-ink)}
  .zs .i-rw.dead,.zs .i-rw.dead .v{color:var(--text-dim)}
  .zs .i-rw.quiet{font-size:37px;color:var(--text-dim);height:56px}
  .zs .i-rw.sel,.zs .i-rw.sel .v{color:var(--on-accent)}
  .zs .i-rw .tick{width:34px;height:34px}
  .zs .i-more{width:0;height:0;border-left:12px solid transparent;border-right:12px solid transparent;margin:4px auto 0}
  .zs .i-more.up{border-bottom:12px solid var(--text-dim)}.zs .i-more.dn{border-top:12px solid var(--text-dim)}
  .zs .i-sep{height:2px;background:var(--edge);margin:14px 26px}
  .zs .i-note{font-size:37px;color:var(--text-soft);line-height:40px;height:40px;padding:0 26px;white-space:nowrap}
  .zs .i-sc{display:flex;align-items:center;gap:20px}
  .zs .i-sc .tr{position:relative;width:300px;height:16px;border-radius:8px;box-shadow:inset 0 0 0 2px var(--edge);background:linear-gradient(90deg,var(--track),color-mix(in srgb,var(--text-dim) 60%,var(--track)) 50%,var(--track))}
  .zs .i-sc .tr i{position:absolute;top:22px;width:2px;height:8px;background:currentColor;opacity:.7}
  .zs .i-sc .tr i.z{top:-6px;height:28px;width:3px}
  .zs .i-sc .tr b{position:absolute;top:-8px;width:32px;height:32px;border-radius:50%;background:var(--raised);box-shadow:inset 0 0 0 3px currentColor;transform:translateX(-50%);transition:left 120ms cubic-bezier(.23,1,.32,1)}
  .zs .i-sc .n{width:70px;text-align:right;font-variant-numeric:tabular-nums}
  .zs .i-pvscr{position:relative;border-radius:14px;overflow:hidden;background:var(--ink);box-shadow:0 0 0 3px var(--edge)}
  .zs .i-pvnote{margin-top:16px;font-size:37px;line-height:40px;color:var(--text-soft)}
  .zs .i-hints{position:absolute;left:64px;right:64px;top:700px;height:40px;display:flex;justify-content:space-between;align-items:center}
  .zs .i-hints.center{justify-content:center;gap:40px}
  .zs .i-hint{display:flex;align-items:center;gap:12px;font-size:28px;color:var(--text-soft);white-space:nowrap}
  .zs .i-gly{display:inline-flex;align-items:center;justify-content:center;flex:none;background:var(--raised);color:var(--text);font-weight:600;line-height:1;white-space:nowrap}
  .zs .i-chord{display:inline-flex;align-items:center;gap:4px}
  .zs .i-chord .sign{font-size:37px;color:var(--text-dim);margin:0 4px}
  .zs .i-rw.sel .i-gly{background:color-mix(in srgb,var(--on-accent) 10%,transparent);box-shadow:inset 0 0 0 2px var(--on-accent)!important;color:var(--on-accent)}
  .zs .i-rw.sel .i-gly i{background:var(--on-accent)!important}
  .zs .i-rw.sel .i-chord .sign{color:var(--on-accent)}
  .zs .i-qbtn{min-width:240px;height:72px;padding:0 32px;border-radius:14px;display:grid;place-items:center;font-size:49px;color:var(--text-soft);background:var(--raised)}
  .zs .i-qbtn.sel{background:var(--accent);color:var(--on-accent)}
  .zs .i-pop{position:absolute;left:50%;top:22px;transform:translateX(-50%);min-height:64px;padding:12px 32px 12px 22px;border-radius:32px;background:color-mix(in srgb,var(--surface) 93%,transparent);box-shadow:inset 0 0 0 2px var(--edge),0 4px 8px rgba(17,19,16,.3);display:flex;align-items:center;gap:16px;font-size:37px;color:var(--text);white-space:nowrap}
  .zs .i-tile{position:absolute;border-radius:22px;background:var(--surface);box-shadow:inset 0 0 0 3px var(--edge);padding:32px;color:var(--text)}
  .zs .i-tile.sel{background:var(--accent);color:var(--on-accent);box-shadow:none}
  .zs .i-tile .tn{font-size:49px}.zs .i-tile .ts{font-size:37px;line-height:40px;color:var(--text-soft);margin-top:16px}
  .zs .i-tile.sel .ts{color:var(--on-accent)}.zs .i-tile svg{width:72px;height:72px;display:block;margin-bottom:24px}
  .zs .i-cover{position:absolute;border-radius:14px;overflow:hidden;box-shadow:0 8px 20px color-mix(in srgb,var(--ink) 25%,transparent)}
  .zs .i-cover .ct{position:absolute;left:10%;right:10%;bottom:10%;font-size:26px;font-weight:600;color:#fff;text-shadow:0 2px 6px rgba(0,0,0,.6);line-height:1.1}
  .zs .i-ring{position:absolute;border-radius:22px;box-shadow:0 0 0 4px var(--accent-ink)}
  .zs .i-notice{position:absolute;border-radius:28px;background:color-mix(in srgb,var(--surface) 94%,transparent);box-shadow:inset 0 0 0 3px var(--edge),var(--panel-lift);padding:26px 34px}
  .zs .i-notice .nt{font-size:49px;color:var(--text)}.zs .i-notice .nd{font-size:37px;color:var(--text-soft);margin-top:14px;line-height:40px}
  .zs .bars{position:absolute;display:flex;align-items:flex-end}
  .zs .bars>span{position:relative;flex:1;height:100%}
  .zs .bars .b{position:absolute;left:0;right:0;bottom:0;height:0;border-radius:3px;background:var(--accent-ink)}
  .zs .bars .p{position:absolute;left:0;right:0;bottom:0;height:5px;border-radius:2px;background:var(--text-dim)}
  .zs .bars.refl{opacity:.22;transform:scaleY(-1);transform-origin:top;-webkit-mask-image:linear-gradient(transparent,#000);mask-image:linear-gradient(transparent,#000)}
  .zs .bars.refl .p{display:none}
  .zi-demo{display:grid;gap:8px}
  .zi-demo .zk-frame{cursor:pointer;outline:2px solid transparent;outline-offset:2px;transition:outline-color .12s}
  .zi-demo.active .zk-frame{outline-color:var(--zk-hi)}
  .zi-cap{font:12px var(--zk-f-mono);color:var(--zk-ink-3)}
  .zi-pad{display:flex;flex-wrap:wrap;gap:6px}
  .zi-pad button{font:500 13px var(--zk-f-mono);color:var(--zk-ink);background:transparent;min-width:40px;height:34px;border:1px solid var(--zk-line);border-radius:999px;padding:0 12px;cursor:pointer;touch-action:manipulation;user-select:none;-webkit-user-select:none}
  .zi-pad button:hover{border-color:var(--zk-ink-3)}
  .zi-pad button:focus-visible,.zi-demo .zk-frame:focus-visible{outline:2px solid var(--zk-hi);outline-offset:2px}
  .zi-pad button.down,.zi-pad button.latched{background:var(--zk-line)}
  `;
  const st = document.createElement("style"); st.textContent = CSS; document.head.appendChild(st);

  /* ---- device: the Pixel 2 or the Brick, for glyphs ------------------------ */
  let dev = "px2";

  /* ---- small pieces ------------------------------------------------------ */
  const BATT = `<svg viewBox="0 0 44 26"><rect x="2" y="3" width="36" height="20" rx="6" fill="none" stroke="currentColor" stroke-width="3"/><rect x="7" y="8" width="26" height="10" rx="3" fill="currentColor"/><rect x="40" y="9" width="3" height="8" rx="1.5" fill="currentColor"/></svg>`;
  const TICK = `<svg class="tick" viewBox="0 0 24 24"><path d="M4 12.5l5 5L20 6.5" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/></svg>`;
  function status(light) {
    const c = light ? ' style="color:#e4e2d8"' : "";
    return `<div class="i-clock"${c}>10:08</div><div class="i-batt"${c}>${BATT}</div>`;
  }
  const textW = (s, size) => Math.round(String(s).length * size * 0.56);

  /* A button as this handheld prints it (DESIGN.md 11.9, src/ds/button.h). */
  function glyph(b, s, lit) {
    s = s || 40;
    const ring = Math.max(2, Math.round(s / 24));
    const look = lit ? `box-shadow:inset 0 0 0 ${ring}px var(--accent),0 0 ${s / 3}px color-mix(in srgb,var(--accent) 75%,transparent);transform:scale(.92);` : `box-shadow:inset 0 0 0 ${ring}px var(--edge);`;
    const circle = (inner, fs) => `<span class="i-gly" style="width:${s}px;height:${s}px;border-radius:50%;${look}font-size:${fs || s * .5}px">${inner}</span>`;
    if (/^[ABXY]$/.test(b)) return circle(b);
    if (b === "SELECT" || b === "START") {
      if (dev === "brick") return circle(b === "SELECT"
        ? `<i style="display:block;width:${s * .3}px;height:${s * .3}px;background:var(--text)"></i>`
        : `<svg width="${s * .4}" height="${s * .4}" viewBox="0 0 10 10"><path d="M2.5 1l7 4-7 4z" fill="var(--text)"/></svg>`);
      return circle(b === "SELECT" ? "SE" : "ST", s * .39);
    }
    if (b === "MENU" && dev !== "brick") {
      const d = Math.max(2, s * .07);
      return `<span class="i-gly" style="width:${s}px;height:${s}px;border-radius:50%;${look}flex-direction:column;gap:${d}px">${`<i style="width:${d * 1.5}px;height:${d * 1.5}px;border-radius:50%;background:var(--text);display:block"></i>`.repeat(3)}</span>`;
    }
    if (b === "DPAD") return `<span class="i-gly" style="width:${s}px;height:${s}px;background:none;box-shadow:none"><svg viewBox="0 0 100 100" width="${s}" height="${s}"><path d="M36 4h28v32h32v28H64v32H36V64H4V36h32z" fill="var(--raised)" stroke="var(--edge)" stroke-width="4" stroke-linejoin="round"/><circle cx="50" cy="50" r="5" fill="var(--edge)"/></svg></span>`;
    const w = b === "VOL" ? "VOL− │ VOL+" : b === "LR" ? "< >" : b;
    return `<span class="i-gly" style="height:${s}px;padding:0 ${s * .45}px;border-radius:${s / 2}px;${look}font-size:${s * .42}px;font-weight:400">${w}</span>`;
  }
  function chord(list, sign, s) {
    return `<span class="i-chord">${list.map(b => glyph(b, s)).join(`<span class="sign">${sign || "+"}</span>`)}</span>`;
  }

  function tabRow(w, tabs, on, each) {
    const cx = w / 2, name = tabs[on], nw = textW(name, 55), n = tabs.length, c0 = 116;
    let h = `<div class="i-pill" style="left:52px;top:34px">L1</div><div class="i-pill" style="right:52px;top:34px">R1</div>`;
    h += `<div class="i-tabname" style="left:${cx}px">${esc(name)}</div><div class="i-tabbar" style="left:${cx}px;width:${nw}px"></div>`;
    let lx = cx - nw / 2 - 40, rx = cx + nw / 2 + 40, nb = "";
    for (let k = 1; k <= each; k++) {
      const l = tabs[(on - k + n * 4) % n], r = tabs[(on + k) % n], lw = textW(l, 37), rw = textW(r, 37);
      nb += `<div class="i-tabn" style="left:${lx - lw / 2 - c0}px">${esc(l)}</div><div class="i-tabn" style="left:${rx + rw / 2 - c0}px">${esc(r)}</div>`;
      lx -= lw + 40; rx += rw + 40;
    }
    const mask = "linear-gradient(90deg,transparent,#000 64px,#000 calc(100% - 64px),transparent)";
    return h + `<div class="i-abs" style="left:${c0}px;right:${c0}px;top:0;height:100px;overflow:hidden;-webkit-mask-image:${mask};mask-image:${mask}">${nb}</div><div class="i-rule"></div>`;
  }

  /* Rows; the focus fill is one element under them, so it can glide (quick). */
  const ROW_H = r => r === "sep" ? 30 : r.note !== undefined ? 40 : (r.up || r.dn) ? 20 : r.quiet ? 56 : 72;
  function rows(list) {
    let y = 0, fillY = -1;
    const html = list.map(r => {
      const h = ROW_H(r);
      if (r !== "sep" && r.sel) fillY = y;
      y += h;
      if (r === "sep") return `<div class="i-sep"></div>`;
      if (r.note !== undefined) return `<div class="i-note" style="${r.style || ""}">${r.note}</div>`;
      if (r.up) return `<div style="height:20px"><div class="i-more up"></div></div>`;
      if (r.dn) return `<div style="height:20px"><div class="i-more dn"></div></div>`;
      const cls = ["i-rw"]; if (r.sel) cls.push("sel"); if (r.dead) cls.push("dead"); if (r.quiet) cls.push("quiet");
      let v = "";
      if (r.scale) {
        const s = r.scale, pct = (s.v - s.min) / (s.max - s.min) * 100, n = s.v > 0 ? `+${s.v}` : s.v < 0 ? `−${-s.v}` : "0";
        v = `<span class="i-sc"><span class="tr"><i style="left:0"></i><i style="left:25%"></i><i class="z" style="left:calc(50% - 1px)"></i><i style="left:75%"></i><i style="left:calc(100% - 2px)"></i><b style="left:${pct}%"></b></span><span class="n">${n}</span></span>`;
      } else if (r.vh !== undefined) v = r.vh;
      else if (r.v !== undefined) v = esc(r.v);
      if (r.tick) v += TICK;
      if (r.glyph) v += glyph(r.glyph, 40);
      return `<div class="${cls.join(" ")}"${r.style ? ` style="${r.style}"` : ""}><span style="display:flex;align-items:center;gap:18px">${r.lh !== undefined ? r.lh : esc(r.l)}</span><span class="v${r.ink ? " ink" : ""}">${v}</span></div>`;
    }).join("");
    return (fillY >= 0 ? `<i class="i-fill" style="top:${fillY}px"></i>` : "") + html;
  }

  /* o: {x, y, w, h, tabs, on, each, title, rows, bodyTop, bodyLeft, extra, cls} */
  function panel(o) {
    const head = o.tabs ? tabRow(o.w, o.tabs, o.on || 0, o.each || 1) : o.title ? `<div class="i-htitle">${esc(o.title)}</div><div class="i-rule"></div>` : "";
    const bl = o.bodyLeft !== undefined ? o.bodyLeft : 26;
    return `<div class="i-pn ${o.cls || ""}" style="left:${o.x}px;top:${o.y}px;width:${o.w}px;height:${o.h}px">${head}<div class="i-body" style="top:${o.bodyTop || 124}px;left:${bl}px">${rows(o.rows || [])}</div>${o.extra || ""}</div>`;
  }
  /* An option list; o.pv = {w, h, html, note} draws the picture column. */
  function pick(o) {
    const pvw = o.pv ? o.pv.w : 0, left = o.pv ? 56 + pvw + 32 - 26 : 26;
    const list = [{ up: 1 }].concat(o.items.map(it => Object.assign({ tick: it.kept }, it)), o.more ? [{ dn: 1 }] : []);
    if (o.notes) list.push("sep", ...o.notes.map(n => ({ note: n })));
    const pv = o.pv ? `<div class="i-abs" style="left:56px;top:150px;width:${pvw}px"><div class="i-pvscr" style="width:${pvw}px;height:${o.pv.h}px">${o.pv.html}</div>${o.pv.note ? `<div class="i-pvnote">${o.pv.note}</div>` : ""}</div>` : "";
    return panel({ x: o.x, y: o.y, w: o.w, h: o.h, title: o.title, rows: list, bodyTop: 120, bodyLeft: left, extra: pv + (o.extra || ""), cls: o.cls });
  }
  function hints(list, center) {
    return `<div class="i-hints${center ? " center" : ""}">${list.map(([g, w]) => `<span class="i-hint">${glyph(g, 32)}${esc(w)}</span>`).join("")}</div>`;
  }
  function question(o) {
    return `<div class="i-pn ${o.cls || ""}" style="left:${o.x || 112}px;top:${o.y || 190}px;width:${o.w || 800}px;padding:44px 56px 40px">
      <div style="font-size:49px;color:var(--text);text-align:center;line-height:60px">${o.title}</div>
      ${o.body ? `<div style="font-size:37px;color:var(--text-soft);text-align:center;line-height:44px;margin-top:18px">${o.body}</div>` : ""}
      <div style="display:flex;gap:24px;justify-content:center;margin-top:36px">${o.buttons.map((b, i) => `<div class="i-qbtn${i === o.sel ? " sel" : ""}">${b}</div>`).join("")}</div></div>`;
  }
  function popup(t, mark, cls) { return `<div class="i-pop ${cls || ""}">${mark || ""}${t}</div>`; }
  const homeBg = () => `<div class="i-home"></div><div class="scrim"></div>${status(false)}`;
  const gameBg = (th, kind) => `<div class="i-game">${scene(kind || "snes", 1024, 768)}</div><div class="i-veil" style="background:color-mix(in srgb, ${th === "light" ? "var(--ground) 52%" : "var(--ink) 58%"}, transparent)"></div>${status(th === "dark")}`;

  /* ---- stand-in pictures -------------------------------------------------- */
  const PAL = {
    snes: { sky: "#5c94fc", cloud: "#fcfcfc", hill: "#3cb83c", hill2: "#00a800", ground: "#c84c0c", brick: "#fc9838", coin: "#fcbc3c", text: "#fcfcfc", dog: "#a8a8a8", dogd: "#585858", beard: "#f8f8f8", block: "#e09040" },
    gb: { sky: "#9bbc0f", cloud: "#8bac0f", hill: "#8bac0f", hill2: "#306230", ground: "#306230", brick: "#0f380f", coin: "#306230", text: "#0f380f", dog: "#306230", dogd: "#0f380f", beard: "#8bac0f", block: "#306230" },
    pocket: { sky: "#c8c3b4", cloud: "#a7a194", hill: "#a7a194", hill2: "#6f6b63", ground: "#6f6b63", brick: "#2f2e2a", coin: "#6f6b63", text: "#2f2e2a", dog: "#6f6b63", dogd: "#2f2e2a", beard: "#a7a194", block: "#6f6b63" },
    light: { sky: "#46d6b4", cloud: "#2fb995", hill: "#2fb995", hill2: "#108a6c", ground: "#108a6c", brick: "#04503e", coin: "#108a6c", text: "#04503e", dog: "#108a6c", dogd: "#04503e", beard: "#2fb995", block: "#108a6c" },
    gray: { sky: "#d8d8d8", cloud: "#b0b0b0", hill: "#b0b0b0", hill2: "#686868", ground: "#686868", brick: "#202020", coin: "#686868", text: "#202020", dog: "#686868", dogd: "#202020", beard: "#b0b0b0", block: "#686868" },
    gbc: { sky: "#f8e8a8", cloud: "#ffffff", hill: "#70c070", hill2: "#408040", ground: "#a05030", brick: "#e0a060", coin: "#f0c030", text: "#402010", dog: "#909090", dogd: "#404040", beard: "#ffffff", block: "#d08040" },
    night: { sky: "#1b2050", cloud: "#3a4278", hill: "#26306a", hill2: "#182050", ground: "#4a3a6a", brick: "#7a6aa0", coin: "#f8d860", text: "#f0f0ff", dog: "#9898a8", dogd: "#484858", beard: "#f0f0f8", block: "#6a5a90" },
    city: { sky: "#f0a060", cloud: "#f8d0a0", hill: "#7a5a7a", hill2: "#4a3a5a", ground: "#303040", brick: "#505068", coin: "#f8f070", text: "#ffffff", dog: "#a0a0a0", dogd: "#505050", beard: "#ffffff", block: "#606080" },
  };
  function scene(kind, w, h, x, y) {
    const p = PAL[kind] || PAL.snes, gb = ["gb", "pocket", "light", "gray"].includes(kind);
    const vw = gb ? 160 : 256, vh = gb ? 144 : 224, g = vh - 40;
    let s = `<rect width="${vw}" height="${vh}" fill="${p.sky}"/>`;
    s += `<rect x="${vw * .1}" y="22" width="34" height="8" fill="${p.cloud}"/><rect x="${vw * .1 + 6}" y="16" width="20" height="8" fill="${p.cloud}"/><rect x="${vw * .62}" y="34" width="40" height="8" fill="${p.cloud}"/><rect x="${vw * .62 + 8}" y="28" width="22" height="8" fill="${p.cloud}"/>`;
    s += `<path d="M0 ${g} L${vw * .18} ${g - 34} L${vw * .36} ${g} Z" fill="${p.hill}"/><path d="M${vw * .5} ${g} L${vw * .72} ${g - 50} L${vw * .94} ${g} Z" fill="${p.hill2}"/><rect y="${g}" width="${vw}" height="${vh - g}" fill="${p.ground}"/>`;
    for (let xx = 0; xx < vw; xx += 16) for (let yy = g; yy < vh; yy += 8) s += `<rect x="${xx + ((yy - g) % 16 ? 8 : 0)}" y="${yy}" width="15" height="7" fill="${p.brick}" opacity=".5"/>`;
    for (let i = 0; i < 4; i++) s += `<rect x="${vw * .55 + i * 14}" y="${g - 54}" width="13" height="13" fill="${p.block}" stroke="${p.brick}" stroke-width="1"/>`;
    for (let i = 0; i < 5; i++) s += `<rect x="${vw * .3 + i * 12}" y="${g - 70}" width="5" height="8" rx="2" fill="${p.coin}"/>`;
    const dx = vw * .22, dy = g - 18;
    s += `<rect x="${dx}" y="${dy + 4}" width="20" height="10" fill="${p.dog}"/><rect x="${dx + 16}" y="${dy - 4}" width="10" height="10" fill="${p.dog}"/><rect x="${dx + 22}" y="${dy + 2}" width="6" height="6" fill="${p.beard}"/><rect x="${dx + 17}" y="${dy - 7}" width="4" height="4" fill="${p.dogd}"/><rect x="${dx + 2}" y="${dy + 14}" width="3" height="4" fill="${p.dogd}"/><rect x="${dx + 14}" y="${dy + 14}" width="3" height="4" fill="${p.dogd}"/><rect x="${dx - 3}" y="${dy + 2}" width="4" height="3" fill="${p.dogd}"/>`;
    s += `<text x="8" y="14" font-family="ui-monospace,Menlo,monospace" font-size="${gb ? 8 : 10}" fill="${p.text}">SCORE 4200</text>`;
    const pos = x !== undefined ? ` x="${x}" y="${y}"` : "";
    return `<svg${pos} viewBox="0 0 ${vw} ${vh}" width="${w}" height="${h}" preserveAspectRatio="none" shape-rendering="crispEdges" style="display:block">${s}</svg>`;
  }
  const cover = hue => `background:linear-gradient(160deg, hsl(${hue} 55% 46%), hsl(${(hue + 40) % 360} 45% 18%))`;

  /* The handheld at the Controls tab's pose, simplified; lit: keys to light. */
  function handheld(lit) {
    lit = [].concat(lit || []);
    const L = (k, s) => lit.includes(k) ? `${s} stroke="#bef23c" stroke-width="6"` : s;
    if (dev === "brick") return `<svg viewBox="0 0 220 200" width="220" height="200">
      <rect x="6" y="6" width="208" height="188" rx="22" fill="var(--surface)" stroke="var(--edge)" stroke-width="5"/>
      <rect x="40" y="20" width="140" height="105" rx="4" fill="var(--text)"/><rect x="46" y="26" width="128" height="93" fill="var(--raised)"/>
      <path d="M22 146h14v-14h12v14h14v12h-14v14h-12v-14h-14z" ${L("dpad", 'fill="var(--raised)"')}/>
      <circle cx="180" cy="136" r="9" ${L("x", 'fill="var(--raised)"')}/><circle cx="196" cy="152" r="9" ${L("a", 'fill="var(--raised)"')}/><circle cx="164" cy="152" r="9" ${L("y", 'fill="var(--raised)"')}/><circle cx="180" cy="168" r="9" ${L("b", 'fill="var(--raised)"')}/>
      <rect x="86" y="150" width="20" height="9" rx="4" ${L("select", 'fill="var(--raised)"')}/><rect x="114" y="150" width="20" height="9" rx="4" ${L("start", 'fill="var(--raised)"')}/></svg>`;
    return `<svg viewBox="0 0 200 260" width="200" height="260">
      <rect x="6" y="6" width="188" height="248" rx="18" fill="var(--surface)" stroke="var(--edge)" stroke-width="5"/>
      <rect x="150" y="0" width="34" height="10" rx="4" ${L("r2", 'fill="var(--raised)"')}/><rect x="16" y="0" width="34" height="10" rx="4" ${L("l2", 'fill="var(--raised)"')}/>
      <rect x="24" y="22" width="152" height="112" rx="6" fill="var(--text)"/><rect x="32" y="30" width="136" height="96" rx="2" fill="var(--raised)"/>
      <path d="M40 170h18v-18h16v18h18v16h-18v18h-16v-18h-18z" ${L("dpad", 'fill="var(--raised)"')}/>
      <circle cx="150" cy="160" r="11" ${L("x", 'fill="var(--raised)"')}/><circle cx="170" cy="178" r="11" ${L("a", 'fill="var(--raised)"')}/>
      <circle cx="130" cy="178" r="11" ${L("y", 'fill="var(--raised)"')}/><circle cx="150" cy="196" r="11" ${L("b", 'fill="var(--raised)"')}/>
      <circle cx="88" cy="230" r="7" ${L("select", 'fill="var(--raised)"')}/><circle cx="112" cy="230" r="7" ${L("start", 'fill="var(--raised)"')}/></svg>`;
  }

  /* ---- sound: the launcher's own WAVs (sounds.js), in the Wood voice ------- */
  let ac = null, gain = 1;
  const bufs = {};
  function wav(uri) {
    const bin = atob(uri.slice(uri.indexOf(",") + 1)), u8 = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
    const dv = new DataView(u8.buffer);
    let p = 12, rate = 48000, data = null;
    while (p + 8 <= u8.length) {
      const id = String.fromCharCode(u8[p], u8[p + 1], u8[p + 2], u8[p + 3]), size = dv.getUint32(p + 4, true);
      if (id === "fmt ") rate = dv.getUint32(p + 12, true);
      if (id === "data") { data = [p + 8, size]; break; }
      p += 8 + size + (size & 1);
    }
    const n = data[1] / 2, b = ac.createBuffer(1, n, rate), ch = b.getChannelData(0);
    for (let i = 0; i < n; i++) ch[i] = dv.getInt16(data[0] + i * 2, true) / 32768;
    return b;
  }
  function audio() {
    if (!ac) ac = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: "interactive" });
    if (ac.state === "suspended") ac.resume();
    return ac;
  }
  function sound(name, level) {
    if (!name || ZI.mute) return;
    const L = window.ZK_SOUNDLAB;
    if (!L) { window.ZK && ZK.sound(name); return; }
    const c = audio();
    if (!bufs[name]) { const uri = L.voices.wood && L.voices.wood[name]; if (!uri) return; bufs[name] = wav(uri); }
    const src = c.createBufferSource(), g = c.createGain();
    src.buffer = bufs[name]; g.gain.value = (level || 1) * gain;
    src.connect(g).connect(c.destination); src.start();
  }

  /* ---- playable screens -------------------------------------------------- */
  const kinds = {}, state = {}; const ZIx = {};
  const LABEL = { up: "▲", down: "▼", left: "◀", right: "▶", SELECT: "SELECT", START: "START" };
  function demo(kind, id, theme, opts) {
    const k = kinds[kind];
    if (!state[id]) state[id] = Object.assign({ id, theme, opts: opts || {} }, k.init(opts || {}, theme));
    const s = state[id];
    const cap = (opts && opts.caption) || k.caption || "";
    const pad = (opts && opts.pad) || k.pad || [];
    return `<div class="zi-demo" data-kind="${kind}" data-id="${id}" data-theme-name="${theme}" data-dev="${(opts && opts.dev) || "px2"}">
      <div tabindex="0" role="application" aria-label="Playable screen: ${esc(cap)}">${ZK.screen(theme, draw(kind, s, theme, opts))}</div>
      ${cap ? `<span class="zi-cap">${cap}</span>` : ""}
      <div class="zi-pad" role="group" aria-label="Buttons">${pad.map(b => `<button type="button" data-btn="${b}" aria-label="${b}">${LABEL[b] || b}</button>`).join("")}</div></div>`;
  }
  function draw(kind, s, theme, opts) { const was = dev; dev = (opts && opts.dev) || "px2"; const h = kinds[kind].draw(s, theme); dev = was; return h; }
  function redraw(el) {
    const id = el.dataset.id, s = state[id], k = kinds[el.dataset.kind], zs = el.querySelector(".zs");
    const oldPns = zs.querySelectorAll(".i-pn"), oldFill = oldPns.length ? oldPns[oldPns.length - 1].querySelector(".i-fill") : null, oldTop = oldFill ? parseFloat(oldFill.style.top) : null;
    const tkOld = s._tk, tkNew = (s.stack ? s.stack.length : 0) + (s.pick ? "p" : "") + (s.cap ? "c" : "") + (s.ask || s.askQ ? "q" : ""); s._tk = tkNew;
    zs.innerHTML = draw(el.dataset.kind, s, el.dataset.themeName, s.opts);
    /* Motion, on DESIGN.md's tokens: the fill glides between rows in quick;
     * a panel that opened comes in from 96% in move; a tab's rows come in
     * from 64 units the press's way in move; a pop-up arrives in quick. */
    const fx = s._fx || {}; s._fx = null;
    const pns = zs.querySelectorAll(".i-pn"), top = pns[pns.length - 1];
    if (fx.open && top) ZK.animate(top, "move", [{ transform: "scale(.96)", opacity: 0 }, { transform: "scale(1)", opacity: 1 }]);
    else if (fx.tab && top) { const b = top.querySelector(".i-body"); if (b) ZK.animate(b, "move", [{ transform: `translateX(${fx.tab * 64}px)`, opacity: 0 }, { transform: "none", opacity: 1 }]); }
    else if (top && oldTop !== null && !fx.close && tkOld === tkNew) {
      const f = top.querySelector(".i-fill");
      if (f && parseFloat(f.style.top) !== oldTop) ZK.animate(f, "quick", [{ top: oldTop + "px" }, { top: f.style.top }]);
    }
    if (fx.pop) zs.querySelectorAll(".i-pop").forEach(p => ZK.animate(p, "quick", [{ transform: "translate(-50%,-24px)", opacity: 0 }, { transform: "translate(-50%,0)", opacity: 1 }]));
    if (k.after) k.after(s, zs);
  }
  function press(el, b, repeat) {
    const id = el.dataset.id, k = kinds[el.dataset.kind], s = state[id];
    const was = dev; dev = el.dataset.dev; const r = k.key(s, b, repeat); dev = was;
    if (r === false) return;
    redraw(el);
    if (typeof r === "string") sound(r, repeat && r === "move" ? 0.5 : 1);
    else if (Array.isArray(r)) sound(r[0], r[1]);
  }
  /* Held buttons repeat as src/platform.c's in_repeat does: once, then
   * after 300 ms every 90 ms. SELECT on the pad latches, for a mouse. */
  let hold = null; const held = new Set();
  function startHold(el, b) {
    stopHold(); press(el, b, false);
    if (!["up", "down", "left", "right"].includes(b)) return;
    hold = { t: setTimeout(function tick() { press(el, b, true); hold.t = setTimeout(tick, 90); }, 300), b };
  }
  function stopHold() { if (hold) clearTimeout(hold.t); hold = null; document.querySelectorAll(".zi-pad .down").forEach(x => x.classList.remove("down")); }
  let active = null;
  function activate(el) { document.querySelectorAll(".zi-demo.active").forEach(c => c.classList.remove("active")); active = el ? el.dataset.id : null; if (el) el.classList.add("active"); }
  const activeEl = () => active && document.querySelector(`.zi-demo[data-id="${active}"]`);
  const KEYS = { ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right", Enter: "A", a: "A", Backspace: "B", Escape: "B", b: "B", x: "X", y: "Y", q: "L1", e: "R1", z: "L2", c: "R2", s: "SELECT", " ": "START", m: "MENU" };
  function start(root) {
    root = root || document;
    root.addEventListener("pointerdown", e => {
      const d = e.target.closest(".zi-demo"); if (d) activate(d);
      const b = e.target.closest("[data-btn]");
      if (b && d) {
        e.preventDefault();
        if (b.dataset.btn === "SELECT") { const on = !held.has("SELECT"); on ? held.add("SELECT") : held.delete("SELECT"); document.querySelectorAll('[data-btn="SELECT"]').forEach(x => x.classList.toggle("latched", on)); press(d, on ? "SELECT" : "SELECT-up", false); return; }
        b.classList.add("down"); startHold(d, b.dataset.btn);
      }
    });
    root.addEventListener("click", e => { const b = e.target.closest("[data-btn]"), d = e.target.closest(".zi-demo"); if (b && d && e.detail === 0) press(d, b.dataset.btn, false); });
    ["pointerup", "pointercancel", "blur"].forEach(t => window.addEventListener(t, stopHold));
    root.addEventListener("focusin", e => { const d = e.target.closest(".zi-demo"); if (d) activate(d); });
    window.addEventListener("keydown", e => {
      if (e.target.closest && e.target.closest("textarea, select, input")) return;
      const b = KEYS[e.key] || KEYS[(e.key || "").toLowerCase()], el = activeEl();
      if (!b || !el || e.metaKey || e.ctrlKey || e.altKey) return;
      if (e.target.closest && e.target.closest("button") && (b === "A" || b === "START")) return;
      e.preventDefault();
      if (e.repeat) return;
      if (b === "SELECT") { held.add("SELECT"); press(el, "SELECT", false); return; }
      startHold(el, b);
    });
    window.addEventListener("keyup", e => {
      const b = KEYS[e.key] || KEYS[(e.key || "").toLowerCase()];
      if (b === "SELECT") { held.delete("SELECT"); const el = activeEl(); if (el) press(el, "SELECT-up", false); }
      else if (hold && b === hold.b) stopHold();
      const el = activeEl(); if (el && b) { const k = kinds[el.dataset.kind]; if (k.release) { k.release(state[el.dataset.id], b); redraw(el); } }
    });
    /* Animated kinds draw every frame they are on screen. */
    const seen = new Set();
    const io = typeof IntersectionObserver !== "undefined" ? new IntersectionObserver(es => es.forEach(x => x.isIntersecting ? seen.add(x.target) : seen.delete(x.target))) : null;
    const watch = () => document.querySelectorAll(".zi-demo").forEach(d => { if (kinds[d.dataset.kind].frame && !d._w) { d._w = 1; io ? io.observe(d) : seen.add(d); } });
    let last = performance.now();
    (function loop(now) {
      const dt = Math.min(64, now - last); last = now; watch();
      seen.forEach(d => { const s = state[d.dataset.id]; if (s) { const was = dev; dev = d.dataset.dev; kinds[d.dataset.kind].frame(s, d.querySelector(".zs"), dt, now, d); dev = was; } });
      if (ZI.tick) ZI.tick(dt, now);
      requestAnimationFrame(loop);
    })(last);
  }

  /* ---- the generic menu: a stack of panels that plays like the launcher ---- *
   * A view: {title | tabs+on, rows: [row], at: {x,y,w,h}, bg(th), extra(s,th)}.
   * A row: {l, values, i, list, scale, pv(value), notes(value), page(view),
   * a(s, row) -> sound|false, quiet, dead, ink(s,row), vh(s,row), lh}. */
  kinds.menu = {
    pad: ["up", "down", "left", "right", "A", "B", "L1", "R1"],
    init: o => ({ stack: [o.view], sel: [o.sel !== undefined ? o.sel : firstLive(o.view.rows, 0)], pick: null }),
    draw(s, th) {
      const base = s.stack[0], bg = base.bg ? base.bg(th, s) : homeBg();
      let h = bg;
      s.stack.forEach((v, d) => { if (d === s.stack.length - 1 || !v.hideUnder) h += viewHtml(s, v, d, th); });
      if (s.pick) h += pickHtml(s, th);
      if (s.overlay) { const o = s.overlay(th, s); if (o) h += `<div class="scrim"></div>` + o; }
      return h;
    },
    key(s, b, repeat) {
      if (s.onKey) { const r = s.onKey(s, b, repeat); if (r !== undefined) return r; }
      if (s.pick) return pickKey(s, b);
      const d = s.stack.length - 1, v = s.stack[d], rowsL = v.rows, sel = s.sel[d], r = rowsL[sel];
      if (b === "up" || b === "down") {
        const to = stepLive(rowsL, sel, b === "up" ? -1 : 1, repeat);
        if (to === sel) return repeat ? false : "edge";
        s.sel[d] = to; return "move";
      }
      if ((b === "L1" || b === "R1") && v.tabs && v.tabSet) { const n = v.tabs.length, dir = b === "R1" ? 1 : -1; v.tabSet(s, (v.on + dir + n) % n); s._fx = { tab: dir }; return "move"; }
      if ((b === "left" || b === "right") && r) {
        if (r.scale) { const nv = Math.max(r.scale.min, Math.min(r.scale.max, r.scale.v + (b === "left" ? -1 : 1))); if (nv === r.scale.v) return repeat ? false : "edge"; r.scale.v = nv; if (r.change) r.change(s, r); return "move"; }
        if (r.values) { const n = r.values.length; r.i = (r.i + (b === "left" ? -1 : 1) + n) % n; if (r.change) r.change(s, r); return "move"; }
        return repeat ? false : "edge";
      }
      if (b === "A" && r) {
        if (r.dead) return "deny";
        if (r.a) return r.a(s, r);
        if (r.list && r.values) { s.pick = { row: r, sel: r.i, d }; s._fx = { open: true }; if (r.preview) r.preview(s, r, r.i); return "accept"; }
        if (r.page) { const pv = typeof r.page === "function" ? r.page(s) : r.page; s.stack.push(pv); s.sel.push(firstLive(pv.rows, 0)); s._fx = { open: true }; return "accept"; }
        return "accept";
      }
      if (b === "B" || b === "MENU") {
        if (s.stack.length > 1) { s.stack.pop(); s.sel.pop(); s._fx = { close: true }; return "back"; }
        return "back";
      }
      return false;
    },
  };
  const live = r => r && r !== "sep" && r.note === undefined && !r.up && !r.dn && !r.quiet;
  function firstLive(list, from) { for (let i = from; i < list.length; i++) if (live(list[i]) && !list[i].skip) return i; return 0; }
  /* Lists go round (11.3): a press past the end goes to the start; a held
   * direction stops at the end. */
  function stepLive(list, sel, dir, repeat) {
    const n = list.length;
    for (let k = 1; k <= n; k++) {
      const i = sel + dir * k;
      if (i < 0 || i >= n) { if (repeat) return sel; const j = ((i % n) + n) % n; if (live(list[j]) && !list[j].skip) return j; continue; }
      if (live(list[i]) && !list[i].skip) return i;
    }
    return sel;
  }
  function rowOut(s, r, i, selIdx) {
    if (r === "sep" || r.note !== undefined || r.up || r.dn) return r;
    const o = { l: r.l, lh: r.lh, quiet: r.quiet, dead: r.dead || r.skip, sel: i === selIdx, tick: r.tickf ? r.tickf(s, r) : r.tick, glyph: r.glyph, style: r.style };
    if (r.scale) o.scale = r.scale;
    else if (r.vh) o.vh = typeof r.vh === "function" ? r.vh(s, r) : r.vh;
    else if (r.values) o.v = r.show ? r.show(r.values[r.i], s, r) : r.values[r.i];
    else if (r.v !== undefined) o.v = typeof r.v === "function" ? r.v(s, r) : r.v;
    o.ink = typeof r.ink === "function" ? r.ink(s, r) : r.ink;
    return o;
  }
  function viewHtml(s, v, d, th) {
    const at = v.at || { x: 66, y: 96, w: 892, h: 624 };
    const selIdx = d === s.stack.length - 1 && !s.pick ? s.sel[d] : -1;
    const list = v.rows.map((r, i) => rowOut(s, r, i, selIdx));
    const extra = v.extra ? v.extra(th, s) : "";
    return panel({ x: at.x, y: at.y, w: at.w, h: at.h, title: v.title, tabs: v.tabs, on: v.on, each: v.each || 1, rows: list, bodyTop: v.bodyTop, bodyLeft: v.bodyLeft, extra });
  }
  function pickHtml(s, th) {
    const p = s.pick, r = p.row, at = r.pickAt || { x: 112, y: 96, w: 800, h: 560 };
    const items = r.values.map((val, i) => ({ l: r.label ? r.label(val) : val, kept: i === r.i, sel: i === p.sel, v: r.itemV ? r.itemV(val, i, s) : undefined, vh: r.itemVh ? r.itemVh(val, i, s) : undefined }));
    const val = r.values[p.sel];
    return pick({ x: at.x, y: at.y, w: at.w, h: at.h, title: r.pickTitle || r.l, items, more: false,
      pv: r.pv ? { w: r.pvW || 300, h: r.pvH || 225, html: r.pv(val, s, th), note: r.pvNote ? r.pvNote(val) : "" } : null,
      notes: r.notes ? r.notes(val, s) : null, extra: r.pickExtra ? r.pickExtra(val, s, th) : "" });
  }
  function pickKey(s, b) {
    const p = s.pick, r = p.row, n = r.values.length;
    if (b === "up" || b === "down" || b === "left" || b === "right") { p.sel = (p.sel + (b === "up" || b === "left" ? -1 : 1) + n) % n; if (r.preview) r.preview(s, r, p.sel); return "move"; }
    if (b === "A" || (b === "Y" && r.y)) {
      if (r.choose) { const res = r.choose(s, r, p.sel, b); if (res !== undefined) { if (!r.keepOpen) s.pick = null; return res; } }
      r.i = p.sel; if (r.change) r.change(s, r); s.pick = null; s._fx = { close: true }; return "accept";
    }
    if (b === "B" || b === "MENU") { if (r.preview) r.preview(s, r, r.i); s.pick = null; s._fx = { close: true }; return "back"; }
    return false;
  }

  /* MENU's six tabs and the in-game menu's four, with 1.0.6's rows (DESIGN.md
   * 11.3, 11.28); a page's own rows replace one tab's. L1 and R1 turn them. */
  const MENU_TABS = ["Games", "Display", "Sound", "Connections", "System", "About"];
  const MENU_ROWS = {
    Games: [["Search Games"], ["Play Time"], ["Random Game"], ["Achievements", "Signed in"], ["Scraper"]],
    Display: [["Appearance", "Light"], ["Theme"], ["Home Screen", "Spotlight"], ["System Order", "A to Z"], ["Home Systems", "All"], ["Battery Icon", "Bars"]],
    Sound: [["Audio Output", "Auto"], ["Audio Channels", "Auto"], ["UI Sounds", "Wood"], ["Menu Music", "Shuffle"]],
    Connections: [["Wi-Fi", "Off"], ["Bluetooth", "Off"], ["File Transfer"], ["USB Accessories"], ["Developer Mode", "Off"]],
    System: [["Date & Time"], ["Clock", "24-hour"], ["Language", "English"], ["Power Button", "Press"], ["Auto Off", "5 min"], ["Charge Limit", "Off"]],
    About: [["About ZyperOS"], ["Controls"], ["Show Tutorial"]],
  };
  const GAME_TABS = ["Game", "Display", "Controls", "Achievements"];
  const GAME_ROWS = {
    Game: [["Continue"], ["Save"], ["Load"], ["Quick Slot", "1"], ["Reset"], ["Quit", "", "X"]],
    Display: [["Display", "Original Shape"], ["Shader", "Off"], ["On-Screen Stats", "Off"]],
    Controls: [["Applies To", "Nintendo 64"], ["D-pad as Stick", "On"], ["A", "A"], ["B", "B"], ["X", "C up"], ["Hotkeys"]],
    Achievements: [],
  };
  function tabbed(names, table, on, own, base) {
    const rowsFor = n => [{ up: 1 }].concat(own[names[n]] || (table[names[n]] || []).map(r => ({ l: r[0], v: r[1], glyph: r[2] })));
    const v = Object.assign({ tabs: names, on, rows: rowsFor(on), tabSet(s, n) { v.on = n; v.rows = rowsFor(n); s.sel[s.sel.length - 1] = firstLive(v.rows, 0); } }, base || {});
    if (!table[names[on]] && !own[names[on]]) v.rows = rowsFor(on);
    return v;
  }
  const menuView = (on, own, base) => tabbed(MENU_TABS, MENU_ROWS, on, own || {}, Object.assign({ each: 2, bodyTop: 124 }, base || {}));
  const gameView = (on, own, base) => tabbed(GAME_TABS, GAME_ROWS, on, own || {}, Object.assign({ each: 1, at: { x: 24, y: 96, w: 976, h: 608 }, bg: (th) => gameBg(th) }, base || {}));
  ZIx.menuView = menuView; ZIx.gameView = gameView; ZIx.firstLive = firstLive;

  window.ZI = { T, esc, CSS, status, panel, rows, pick, hints, glyph, chord, question, popup, homeBg, gameBg, scene, cover, handheld, sound, kinds, state, demo, redraw, start, audio,
    set dev(d) { dev = d; }, get dev() { return dev; }, set gain(g) { gain = g; }, mute: false, tick: null, held, menuView, gameView, firstLive };
})();
