/* Star Trail is an original local game, not emulation or a device connection. */
(() => {
  'use strict';
  const section = document.querySelector('.player-two');
  if (!section) return;
  const phone = section.querySelector('.p2-phone');
  const buttons = [...section.querySelectorAll('[data-p2-input]')];
  const play = section.querySelector('[data-p2-play]');
  const reset = section.querySelector('[data-p2-reset]');
  const status = section.querySelector('[data-p2-status]');
  const progress = section.querySelector('[data-p2-progress]');
  const canvases = [...section.querySelectorAll('[data-p2-screen]')];
  const screens = canvases.map(canvas => canvas.getContext('2d'));
  const handheld = section.querySelector('.p2-handheld');
  const modelCanvas = section.querySelector('[data-p2-model-view]');
  const handheldScreen = section.querySelector('[data-p2-screen="handheld"]');
  let modelView = null, modelFailed = false;
  const frame = document.createElement('canvas');
  frame.width = 320; frame.height = 240;
  const ctx = frame.getContext('2d');
  if (!ctx || screens.some(screen => !screen)) return;
  const reduce = matchMedia('(prefers-reduced-motion: reduce)');
  const held = new Map();
  const pulses = new Set();
  const keys = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right', KeyX: 'a', KeyZ: 'b', KeyR: 'select', Enter: 'start' };
  const starLayout = [[52, 56, 1], [128, 80, 1], [63, 150, 1], [142, 196, 1], [260, 53, 2], [209, 104, 2], [270, 169, 2], [187, 204, 2]];
  let stars, players, score, elapsed, dash, cooldown, direction;
  let running = false, started = false, complete = false, visible = true;
  let motionBlocked = document.documentElement.classList.contains('motion-paused') && !reduce.matches;
  let raf = 0, previous = 0, accumulator = 0, pulseId = 0;
  const step = 1 / 60;

  function inputIs(input) { return [...held.values()].includes(input); }
  function showInputs() {
    for (const button of buttons) button.dataset.pressed = String(inputIs(button.dataset.p2Input));
  }
  function release(source) { if (held.delete(source)) showInputs(); }
  function releaseAll() {
    held.clear();
    for (const timer of pulses) clearTimeout(timer);
    pulses.clear();
    showInputs();
  }
  function showControls(message) {
    play.replaceChildren(document.createTextNode(complete ? 'Play again' : running ? 'Pause demo' : started ? 'Resume demo' : 'Start demo'));
    const symbol = document.createElement('span');
    symbol.setAttribute('aria-hidden', 'true'); symbol.textContent = running ? 'Ⅱ' : '▷'; play.append(symbol);
    play.disabled = motionBlocked;
    section.dataset.p2State = complete ? 'complete' : running ? 'playing' : started ? 'paused' : 'ready';
    if (message) status.textContent = message;
  }
  function showScore() {
    progress.replaceChildren(document.createTextNode(`${score} / 8`));
    const label = document.createElement('span'); label.textContent = 'stars'; progress.append(label);
  }
  function stop(message) {
    running = false;
    cancelAnimationFrame(raf); raf = 0; previous = 0; accumulator = 0;
    dash = 0;
    releaseAll();
    showControls(message);
    draw();
  }
  function fresh() {
    running = false; started = false; complete = false;
    cancelAnimationFrame(raf); raf = 0; previous = 0; accumulator = 0;
    releaseAll(); score = 0; elapsed = 0; dash = 0; cooldown = 0;
    direction = { x: 0, y: -1 };
    players = [{ x: 49, y: 202, moving: false }, { x: 252, y: 207, moving: false }];
    stars = starLayout.map(([x, y, owner]) => ({ x, y, owner, found: false }));
    showScore();
    showControls(motionBlocked ? 'Use Play motion above to start.' : 'Start the demo. You are the green player.');
    draw();
  }
  function start() {
    if (motionBlocked || document.hidden || !visible) return;
    if (complete) fresh();
    running = true; started = true; previous = 0; accumulator = 0;
    showControls('Collect the four green stars.');
    raf = requestAnimationFrame(tick);
  }
  function toggle() { if (running) stop('Paused. Resume when you are ready.'); else start(); }

  function move(player, x, y, speed, dt) {
    const length = Math.hypot(x, y);
    player.moving = length > 0;
    if (!length) return;
    player.x = Math.max(17, Math.min(303, player.x + x / length * speed * dt));
    player.y = Math.max(42, Math.min(220, player.y + y / length * speed * dt));
  }
  function update(dt) {
    elapsed += dt;
    cooldown = Math.max(0, cooldown - dt);
    dash = Math.max(0, dash - dt);
    const one = players[0], two = players[1];
    const target = stars.filter(star => star.owner === 1 && !star.found).sort((a, b) => Math.hypot(a.x - one.x, a.y - one.y) - Math.hypot(b.x - one.x, b.y - one.y))[0];
    move(one, target ? target.x - one.x : 0, target ? target.y - one.y : 0, 29, dt);
    const x = Number(inputIs('right')) - Number(inputIs('left'));
    const y = Number(inputIs('down')) - Number(inputIs('up'));
    if (x || y) direction = { x, y };
    move(two, dash > 0 ? direction.x : x, dash > 0 ? direction.y : y, dash > 0 ? 180 : 67, dt);
    for (const star of stars) {
      const player = players[star.owner - 1];
      if (!star.found && Math.hypot(star.x - player.x, star.y - player.y) < 11) {
        star.found = true; score++; showScore();
      }
    }
    if (score === stars.length) {
      complete = true;
      stop('All eight stars collected. You did it together.');
    }
  }
  function tick(time) {
    raf = 0;
    if (!running) return;
    // A stalled frame never jumps the characters across the arena.
    const delta = previous ? Math.min((time - previous) / 1000, .05) : 0;
    previous = time; accumulator += delta;
    while (accumulator >= step && running) { update(step); accumulator -= step; }
    draw();
    if (running) raf = requestAnimationFrame(tick);
  }

  function pixelRect(x, y, w, h, color) {
    ctx.fillStyle = color; ctx.fillRect(Math.round(x), Math.round(y), w, h);
  }
  function drawStar(star) {
    const color = star.owner === 1 ? '#8fc8f2' : '#bef23c';
    pixelRect(star.x - 1, star.y - 6, 3, 13, color);
    pixelRect(star.x - 6, star.y - 1, 13, 3, color);
    pixelRect(star.x - 3, star.y - 3, 7, 7, color);
    pixelRect(star.x - 1, star.y - 2, 2, 3, '#f5f7d7');
  }
  function drawPlayer(player, index) {
    const x = Math.round(player.x), y = Math.round(player.y);
    const color = index ? '#bef23c' : '#8fc8f2';
    const stride = running && player.moving && Math.floor(elapsed * 9) % 2 ? 1 : 0;
    pixelRect(x - 7, y + 5, 15, 3, '#11201f');
    pixelRect(x - 5, y - 8, 11, 10, color);
    pixelRect(x - 7, y - 5, 15, 5, color);
    pixelRect(x - 4, y + 2, 4, 4 - stride, color);
    pixelRect(x + 2, y + 2, 4, 3 + stride, color);
    pixelRect(x - 3, y - 5, 2, 3, '#182a28');
    pixelRect(x + 3, y - 5, 2, 3, '#182a28');
    ctx.fillStyle = color; ctx.font = '7px monospace'; ctx.textAlign = 'center';
    ctx.fillText(String(index + 1), x, y - 13);
    if (index && dash > 0) {
      pixelRect(x - direction.x * 13 - 2, y - direction.y * 13, 4, 3, '#829d38');
      pixelRect(x - direction.x * 20 - 1, y - direction.y * 20, 3, 2, '#526934');
    }
  }
  function draw() {
    ctx.imageSmoothingEnabled = false;
    pixelRect(0, 0, 320, 240, '#182a28');
    pixelRect(9, 31, 302, 200, '#2b443c');
    pixelRect(12, 34, 296, 194, '#233c35');
    for (let i = 0; i < 41; i++) {
      const x = 20 + (i * 47) % 281, y = 44 + (i * 31) % 174;
      pixelRect(x, y, 2, 3, '#345047');
      pixelRect(x + 3, y - 1, 2, 4, '#345047');
    }
    // Stepping stones are decoration, not hidden collision obstacles.
    for (let i = 0; i < 6; i++) pixelRect(149 + (i % 2) * 4, 50 + i * 28, 12, 6, '#2c463f');
    ctx.fillStyle = '#d4e3cf'; ctx.font = 'bold 10px monospace'; ctx.textAlign = 'left'; ctx.fillText('STAR TRAIL', 12, 19);
    ctx.fillStyle = '#bef23c'; ctx.textAlign = 'right'; ctx.fillText(`${score} / 8`, 307, 19);
    for (const star of stars) if (!star.found) drawStar(star);
    players.forEach(drawPlayer);
    if (!running) {
      const label = complete ? 'TOGETHER!' : started ? 'PAUSED' : 'PRESS START';
      pixelRect(81, 104, 158, 31, '#132925');
      ctx.fillStyle = complete ? '#bef23c' : '#e0eadb'; ctx.font = 'bold 11px monospace'; ctx.textAlign = 'center'; ctx.fillText(label, 160, 124);
    }
    for (const screen of screens) { screen.imageSmoothingEnabled = false; screen.drawImage(frame, 0, 0); }
    if (modelView) {
      try { modelView.updateScreen(); } catch { modelFallback(); }
    }
  }

  function modelFallback() {
    modelFailed = true; modelView = null;
    handheld.dataset.p2Model = 'fallback';
    modelCanvas?.setAttribute('aria-hidden', 'true');
  }
  async function loadModel() {
    if (!modelCanvas || !handheldScreen) return;
    try {
      const { mount } = await import('./pixel2.js');
      const view = await mount(modelCanvas, {
        screenSource: handheldScreen, turntable: false, interactive: false,
        pose: { pitch: 0.035, yaw: -0.28, roll: -0.015 }
      });
      if (modelFailed) return;
      modelView = view;
      modelView.updateScreen();
      handheld.dataset.p2Model = 'ready';
      modelCanvas.removeAttribute('aria-hidden');
    } catch { modelFallback(); }
  }

  function press(source, input) {
    const alreadyHeld = inputIs(input);
    held.set(source, input); showInputs();
    if (alreadyHeld) return;
    if (input === 'start') toggle();
    else if (input === 'select') fresh();
    else if (input === 'b' && started && !complete) toggle();
    else if (input === 'a' && running && cooldown === 0) { dash = .18; cooldown = .65; }
  }
  for (const button of buttons) {
    button.disabled = false;
    button.addEventListener('pointerdown', event => {
      if (event.button !== 0) return;
      event.preventDefault(); button.focus({ preventScroll: true });
      button.setPointerCapture(event.pointerId);
      press(`pointer:${event.pointerId}`, button.dataset.p2Input);
    });
    for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) button.addEventListener(name, event => release(`pointer:${event.pointerId}`));
    button.addEventListener('click', event => {
      if (event.detail !== 0) return;
      const source = `click:${++pulseId}`;
      press(source, button.dataset.p2Input);
      const timer = setTimeout(() => { pulses.delete(timer); release(source); }, 180);
      pulses.add(timer);
    });
  }
  phone.addEventListener('keydown', event => {
    if (event.altKey || event.ctrlKey || event.metaKey) return;
    if (event.code === 'Escape') { event.preventDefault(); if (running) stop('Paused. Resume when you are ready.'); return; }
    const button = event.target.closest('[data-p2-input]');
    const input = button && (event.code === 'Space' || event.code === 'Enter') ? button.dataset.p2Input : keys[event.code];
    if (!input) return;
    event.preventDefault();
    if (!event.repeat) press(`key:${event.code}`, input);
  });
  phone.addEventListener('keyup', event => {
    if (held.has(`key:${event.code}`)) event.preventDefault();
    release(`key:${event.code}`);
  });
  phone.addEventListener('focusout', event => { if (!phone.contains(event.relatedTarget)) releaseAll(); });
  play.addEventListener('click', () => { toggle(); if (running) phone.focus({ preventScroll: true }); });
  reset.addEventListener('click', fresh);
  window.addEventListener('blur', () => { if (running) stop('Paused when you left the page.'); else releaseAll(); });
  document.addEventListener('visibilitychange', () => { if (document.hidden) stop('Paused when you left the page.'); });
  document.addEventListener('zyperos-motion', event => {
    motionBlocked = Boolean(event.detail?.paused) && !reduce.matches;
    if (event.detail?.paused) stop(motionBlocked ? 'Use Play motion above to resume.' : 'Motion is paused. Select Start to play.');
    else showControls('Select Start to play.');
  });
  reduce.addEventListener('change', () => {
    motionBlocked = document.documentElement.classList.contains('motion-paused') && !reduce.matches;
    stop('Motion settings changed. Select Start to play.');
  });
  if ('IntersectionObserver' in window) new IntersectionObserver(entries => {
    visible = entries[0].isIntersecting;
    if (!visible && running) stop('Paused while the demo is out of view.');
  }, { threshold: 0 }).observe(section.querySelector('.p2-stage'));
  phone.tabIndex = 0;
  section.querySelector('.p2-demo-actions').hidden = false;
  section.querySelector('#p2-keyboard-help').hidden = false;
  section.querySelectorAll('.p2-game-fallback').forEach(fallback => { fallback.hidden = true; });
  canvases.forEach(canvas => { canvas.hidden = false; });
  fresh();
  if (modelCanvas) {
    modelCanvas.addEventListener('webglcontextlost', modelFallback);
    if ('IntersectionObserver' in window) {
      const modelObserver = new IntersectionObserver(entries => {
        if (!entries.some(entry => entry.isIntersecting)) return;
        modelObserver.disconnect(); loadModel();
      }, { rootMargin: '300px' });
      modelObserver.observe(handheld);
    } else loadModel();
  }
})();
