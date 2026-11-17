/* Progressive enhancements. The page and first screenshots need no script. */
(() => {
  const reduce = matchMedia('(prefers-reduced-motion: reduce)');
  const motionButton = document.querySelector('.motion-control');
  let userPaused = false;
  const paused = () => userPaused || reduce.matches;
  function updateMotion() {
    document.documentElement.classList.toggle('motion-paused', paused());
    motionButton.setAttribute('aria-pressed', String(paused()));
    motionButton.querySelector('[data-motion-label]').textContent = reduce.matches
      ? 'Reduced motion' : userPaused ? 'Play motion' : 'Pause motion';
    motionButton.querySelector('.motion-symbol').textContent = paused() ? '▷' : 'Ⅱ';
    motionButton.disabled = reduce.matches;
    if (paused()) document.getAnimations().forEach(animation => animation.cancel());
    document.dispatchEvent(new CustomEvent('zyperos-motion', { detail: { paused: paused() } }));
  }
  motionButton.hidden = false;
  motionButton.addEventListener('click', () => { userPaused = !userPaused; updateMotion(); });
  reduce.addEventListener('change', updateMotion);
  updateMotion();

  // The handheld the page shows: the hero's choice (an inline script there
  // has already restored the last one, or #brick)
  const deviceInputs = [...document.querySelectorAll('input[name="device"]')];
  const device = () => deviceInputs.find(input => input.checked)?.value || 'pixel2';
  // The Brick's own captures (res/readme/brick), by the Pixel 2's names
  const BRICK_SHOTS = new Set(['systems', 'shelf-snes', 'theme', 'mp4', 'menu', 'folders', 'list', 'grid-favorites', 'info',
    'tour-language', 'tour-home', 'home-grid', 'scraper', 'media', 'screensaver', 'gamemenu-shader', 'controls-tab',
    'controls-hotkeys', 'hare', 'update', 'nowplaying', 'topbar', 'storage', 'storage-system', 'storage-music',
    'battery', 'heat', 'cheevos-waiting']);
  const BRICK_ALT = { 'controls-tab': 'The in-game Controls tab with button remapping and a TrimUI Brick drawing that highlights the selected buttons.' };
  const shotPath = (name, dark = false, which = device()) =>
    `img/${which === 'brick' && BRICK_SHOTS.has(name) ? 'brick/' : ''}${dark ? 'dark/' : ''}${name}.png`;

  // Decode first, then replace. A slow previous request cannot win a later click.
  const screens = {
    home: { image: 'systems', dark: true, title: 'A familiar face. A whole new home.', copy: 'Original animated art for every system. A cartridge slides in. A handheld comes to life.', alt: 'ZyperOS home with the new original PlayStation illustration.' },
    library: { image: 'shelf-n64', brick: 'shelf-snes', dark: true, title: 'Your games, cover by cover.', copy: 'Real box art. Video previews. All your favorites, ready for another round.', alt: 'Nintendo 64 library with Super Mario 64, Mario Kart 64, and Ocarina of Time.', brickAlt: 'SNES library with Super Mario World, Donkey Kong Country, and Super Metroid.' },
    theme: { image: 'theme', title: 'A little more you.', copy: 'Choose your accent, background, and corners. See your new look before you leave the menu.', alt: 'ZyperOS Theme settings with accent colors and light and dark previews.' },
    music: { image: 'mp4', title: 'Let the soundtrack follow.', copy: 'Albums, audiobooks, and videos share a shelf of their own. One button opens it from anywhere.', alt: 'MP4 shelf with an album, a film, and an audiobook.' },
    menu: { image: 'menu', dark: true, title: 'Everything in its place.', copy: 'Six clear tabs. A menu that remembers where you left it. Less searching, more playing.', alt: 'ZyperOS settings menu with six tabs.' }
  };
  const image = document.querySelector('#tour-image');
  const tabs = [...document.querySelectorAll('[data-screen]')];
  const themes = [...document.querySelectorAll('[data-theme]')];
  const themeControl = document.querySelector('.screen-theme');
  let selected = 'home', appearance = 'light', requested = 0;
  let wantedScreen = selected, wantedAppearance = appearance;
  const status = document.createElement('p');
  status.className = 'small';
  status.setAttribute('role', 'status');
  status.hidden = true;
  document.querySelector('.screen-description').append(status);
  async function chooseScreen(key, theme) {
    wantedScreen = key; wantedAppearance = theme;
    const ticket = ++requested, screen = screens[key], brick = device() === 'brick';
    const src = shotPath(brick && screen.brick ? screen.brick : screen.image, screen.dark && theme === 'dark');
    const next = new Image(); next.src = src;
    try { await next.decode(); }
    catch {
      if (ticket === requested) {
        status.hidden = false;
        status.textContent = 'This screen could not load. Select it to try again.';
      }
      return;
    }
    if (ticket !== requested) return;
    selected = key; appearance = theme;
    image.getAnimations().forEach(animation => animation.cancel());
    image.src = src; image.alt = brick && screen.brickAlt ? screen.brickAlt : screen.alt;
    tabs.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.screen === key)));
    themes.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.theme === theme)));
    document.querySelector('#screen-title').textContent = screen.title;
    document.querySelector('#screen-copy').textContent = screen.copy;
    document.querySelector('.screen-index').textContent = `0${Object.keys(screens).indexOf(key) + 1} / 05`;
    themeControl.hidden = !screen.dark;
    document.querySelector('#music-link').hidden = key !== 'music';
    status.hidden = true;
    if (!paused()) image.animate([{ opacity: .35, transform: 'translateY(8px)' }, { opacity: 1, transform: 'translateY(0)' }], { duration: 420, easing: 'cubic-bezier(.22,1,.36,1)' });
  }
  document.querySelector('.screen-tabs').hidden = false;
  themeControl.hidden = false;
  tabs.forEach(button => button.addEventListener('click', () => chooseScreen(button.dataset.screen, wantedAppearance)));
  themes.forEach(button => button.addEventListener('click', () => chooseScreen(wantedScreen, button.dataset.theme)));

  // Only the selected illustration animates. Keep the original still visible
  // until its atlas decodes; load no atlases while motion is stopped/offscreen.
  const artButtons = [...document.querySelectorAll('[data-art]')];
  const artSection = document.querySelector('.system-art');
  const artCache = new Map();
  const visibleArt = new Set();
  let catalogue, currentArt = 'PS1', artVisible = false, frameRequest = 0;
  let elapsed = 0, previous = null, lastTile = -1, artTicket = 0;
  function stills() {
    artButtons.forEach(button => {
      button.querySelector('img').hidden = false;
      button.querySelector('canvas').hidden = true;
    });
  }
  function stopArt() {
    cancelAnimationFrame(frameRequest); frameRequest = 0; previous = null;
  }
  function tick(now) {
    if (paused() || !artVisible || !visibleArt.has(currentArt) || document.hidden) { stopArt(); return; }
    const asset = catalogue.find(item => item.key === currentArt);
    const atlas = artCache.get(currentArt);
    if (!atlas) return;
    if (previous !== null) elapsed += Math.min(now - previous, 100);
    previous = now;
    let time = elapsed % asset.duration_ms;
    let tile = asset.frames[0].tile;
    for (const frame of asset.frames) { tile = frame.tile; if (time < frame.ms) break; time -= frame.ms; }
    if (tile !== lastTile) {
      const button = artButtons.find(item => item.dataset.art === currentArt);
      const canvas = button.querySelector('canvas'), context = canvas.getContext('2d');
      context.clearRect(0, 0, 256, 256); context.imageSmoothingEnabled = false;
      context.drawImage(atlas, tile % asset.cols * 256, Math.floor(tile / asset.cols) * 256, 256, 256, 0, 0, 256, 256);
      canvas.hidden = false; button.querySelector('img').hidden = true; lastTile = tile;
    }
    frameRequest = requestAnimationFrame(tick);
  }
  async function playArt() {
    const ticket = ++artTicket;
    stopArt();
    if (paused()) { stills(); return; }
    if (!artVisible || !visibleArt.has(currentArt) || document.hidden) return;
    try {
      if (!catalogue) {
        const response = await fetch('art/catalogue.json');
        if (!response.ok) return;
        catalogue = await response.json();
      }
      if (ticket !== artTicket) return;
      const key = currentArt, asset = catalogue.find(item => item.key === key);
      if (!asset) return;
      if (!artCache.has(key)) {
        const atlas = new Image(); atlas.src = `art/${asset.atlas}`; await atlas.decode();
        if (ticket !== artTicket) return;
        artCache.set(key, atlas);
      }
      if (ticket !== artTicket || paused() || !artVisible || !visibleArt.has(currentArt) || document.hidden) return;
      // The full catalogue has 45 systems. Retain only the three latest atlases.
      const texture = artCache.get(key);
      artCache.delete(key); artCache.set(key, texture);
      while (artCache.size > 3) artCache.delete(artCache.keys().next().value);
      lastTile = -1; frameRequest = requestAnimationFrame(tick);
    } catch { /* The selected still remains useful if an animation fails. */ }
  }
  function chooseArt(key) {
    if (key === currentArt) return;
    currentArt = key; elapsed = 0; lastTile = -1; stills();
    artButtons.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.art === key)));
    playArt();
  }
  artButtons.forEach(button => {
    button.disabled = false;
    button.addEventListener('click', () => chooseArt(button.dataset.art));
    button.addEventListener('focus', () => { if (button.matches(':focus-visible')) chooseArt(button.dataset.art); });
  });
  new IntersectionObserver(entries => { artVisible = entries[0].isIntersecting; playArt(); }, { threshold: .1 }).observe(artSection);
  const visibleButtons = new IntersectionObserver(entries => {
    entries.forEach(entry => {
      if (entry.isIntersecting && entry.intersectionRatio >= .3) visibleArt.add(entry.target.dataset.art);
      else visibleArt.delete(entry.target.dataset.art);
    });
    playArt();
  }, { root: artSection, threshold: [0, .3] });
  artButtons.forEach(button => visibleButtons.observe(button));
  document.addEventListener('visibilitychange', playArt);
  document.addEventListener('zyperos-motion', playArt);

  // Touch uses the browser's own momentum and page scrolling. Mouse dragging
  // starts only after horizontal intent, so a click still selects an icon.
  let drag = null, suppressClick = false;
  artSection.addEventListener('dragstart', event => event.preventDefault());
  artSection.addEventListener('pointerdown', event => {
    suppressClick = false;
    if (event.pointerType !== 'mouse' || event.button !== 0) return;
    drag = { id: event.pointerId, x: event.clientX, y: event.clientY, scroll: artSection.scrollLeft, active: false };
  });
  artSection.addEventListener('pointermove', event => {
    if (!drag || event.pointerId !== drag.id) return;
    const dx = event.clientX - drag.x, dy = event.clientY - drag.y;
    if (!drag.active) {
      if (Math.abs(dx) < 6 || Math.abs(dx) < Math.abs(dy)) return;
      drag.active = true;
      artSection.classList.add('dragging');
      artSection.setPointerCapture(event.pointerId);
    }
    event.preventDefault();
    artSection.scrollLeft = drag.scroll - dx;
  });
  function endDrag(event) {
    if (!drag || event.pointerId !== drag.id) return;
    suppressClick = drag.active;
    drag = null; artSection.classList.remove('dragging');
    if (artSection.hasPointerCapture(event.pointerId)) artSection.releasePointerCapture(event.pointerId);
  }
  artSection.addEventListener('pointerup', endDrag);
  artSection.addEventListener('pointercancel', endDrag);
  artSection.addEventListener('lostpointercapture', endDrag);
  artSection.addEventListener('pointerleave', event => { if (drag && !drag.active) endDrag(event); });
  artSection.addEventListener('click', event => {
    if (suppressClick && event.detail !== 0) {
      suppressClick = false;
      event.preventDefault(); event.stopImmediatePropagation();
    }
  }, true);
  const shelfSteps = [...document.querySelectorAll('[data-shelf-step]')];
  function moveShelf(direction) {
    artSection.scrollBy({ left: direction * Math.max(160, artSection.clientWidth * .75), behavior: paused() ? 'instant' : 'smooth' });
  }
  shelfSteps.forEach(button => button.addEventListener('click', () => moveShelf(Number(button.dataset.shelfStep))));
  artSection.addEventListener('keydown', event => {
    if (event.key !== 'ArrowLeft' && event.key !== 'ArrowRight') return;
    event.preventDefault();
    if (event.target === artSection) moveShelf(event.key === 'ArrowLeft' ? -1 : 1);
    else {
      const index = artButtons.indexOf(event.target);
      const next = Math.max(0, Math.min(artButtons.length - 1, index + (event.key === 'ArrowLeft' ? -1 : 1)));
      artButtons[next].focus();
    }
  });
  function shelfPosition() {
    const end = artSection.scrollWidth - artSection.clientWidth;
    shelfSteps[0].disabled = artSection.scrollLeft <= 2;
    shelfSteps[1].disabled = artSection.scrollLeft >= end - 2;
    const left = artSection.getBoundingClientRect().left;
    const visible = artButtons.map((button, index) => ({ index, rect: button.getBoundingClientRect() }))
      .filter(({ rect }) => rect.right > left + 10 && rect.left < left + artSection.clientWidth - 10);
    if (visible.length) document.querySelector('[data-shelf-count]').textContent = `${visible[0].index + 1}–${visible.at(-1).index + 1} of ${artButtons.length} systems`;
  }
  artSection.addEventListener('scroll', shelfPosition, { passive: true });
  new ResizeObserver(shelfPosition).observe(artSection);
  document.querySelector('.shelf-navigation').hidden = false;
  shelfPosition();

  // Native disclosure provides keyboard and no-script behavior. Motion only
  // answers an opening action; closed sections do not load their pictures.
  document.querySelectorAll('.feature-detail').forEach(detail => {
    detail.addEventListener('toggle', () => {
      if (detail.open && !paused()) detail.querySelector('.feature-detail-body').animate(
        [{ opacity: 0, transform: 'translateY(-8px)' }, { opacity: 1, transform: 'translateY(0)' }],
        { duration: 320, easing: 'cubic-bezier(.22,1,.36,1)' });
    });
  });

  // Old inbound section links still lead to the relevant content. Native
  // details remain accessible without this convenience enhancement.
  function openLinkedDetails() {
    const target = document.getElementById(location.hash.slice(1));
    if (target?.tagName === 'DETAILS') target.open = true;
    if (location.hash === '#themes') chooseScreen('theme', wantedAppearance);
  }
  addEventListener('hashchange', openLinkedDetails); openLinkedDetails();

  // Everything else that shows a screenshot follows the choice: the feature
  // galleries swap to the Brick's capture where there is one.
  function showDevice() {
    const which = device();
    document.querySelectorAll('#features img[src^="img/"], #features [data-feature-src]').forEach(element => {
      // A gallery's picture follows its pressed button, below
      if (element.tagName === 'IMG' && element.closest('[data-feature-gallery]')) return;
      const attr = element.hasAttribute('data-feature-src') ? 'data-feature-src' : 'src';
      // The build versions the Pixel 2's addresses (?v=); keep each one as it was
      if (!element.dataset.pixelSrc) element.dataset.pixelSrc = element.getAttribute(attr);
      const name = element.dataset.pixelSrc.replace(/^img\/(brick\/)?/, '').replace(/\.png(\?.*)?$/, '');
      if (!BRICK_SHOTS.has(name)) return;
      element.setAttribute(attr, which === 'brick' ? shotPath(name, false, which) : element.dataset.pixelSrc);
      const altAttr = attr === 'src' ? 'alt' : 'data-feature-alt';
      if (BRICK_ALT[name]) {
        if (!element.dataset.pixelAlt) element.dataset.pixelAlt = element.getAttribute(altAttr);
        element.setAttribute(altAttr, which === 'brick' ? BRICK_ALT[name] : element.dataset.pixelAlt);
      }
    });
    document.querySelectorAll('#features [data-feature-gallery]').forEach(gallery => {
      const pressed = gallery.querySelector('[data-feature-src][aria-pressed="true"]');
      const image = gallery.querySelector('img');
      if (!pressed || image.getAttribute('src') === pressed.dataset.featureSrc) return;
      image.src = pressed.dataset.featureSrc; image.alt = pressed.dataset.featureAlt;
    });
    picks.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.pick === which)));
    views.forEach((view, model) => view.setTurntable(model === which));
    rest(which);
    chooseScreen(wantedScreen, wantedAppearance);
  }
  const picks = [...document.querySelectorAll('[data-pick]')];
  const views = new Map();
  // The handheld not chosen rests on its poster, the model's own pose and
  // first screen; it comes back to life when chosen, dragged or focused.
  const boxes = [...document.querySelectorAll('.device[data-model]')];
  let restTimer = 0;
  function rest(which) {
    clearTimeout(restTimer);
    boxes.forEach(box => { if (box.dataset.model === which) box.classList.remove('resting'); });
    restTimer = setTimeout(() => boxes.forEach(box => box.classList.toggle('resting', box.dataset.model !== which)), 750);
  }
  boxes.forEach(box => ['pointerdown', 'focusin'].forEach(type =>
    box.addEventListener(type, () => { clearTimeout(restTimer); box.classList.remove('resting'); })));
  function pick(which) {
    const input = deviceInputs.find(item => item.value === which);
    if (!input || input.checked) return;
    input.checked = true;
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }
  deviceInputs.forEach(input => input.addEventListener('change', () => {
    try { localStorage.setItem('zyperos-device', device()); } catch { /* a private window */ }
    showDevice();
  }));
  picks.forEach(button => button.addEventListener('click', () => pick(button.dataset.pick)));
  document.querySelector('.device-pick').hidden = false;
  addEventListener('hashchange', () => { if (location.hash === '#brick') pick('brick'); });
  if (device() === 'brick') showDevice();

  if (!navigator.connection?.saveData) {
    // Both posters frame the same millimetres a pixel (tools/build-*-model.py)
    const frames = { pixel2: 109, brick: 1100 * 109 / 960 };
    import('./pixel2.js').then(module => Promise.all([...document.querySelectorAll('.device[data-model]')].map(box => {
      const canvas = box.querySelector('canvas'), model = box.dataset.model;
      canvas.hidden = false;
      return module.mount(canvas, { model, frameMm: frames[model], turntable: model === device(), motionAllowed: () => !paused() })
        .then(view => {
          views.set(model, view);
          box.classList.toggle('resting', model !== device());
          box.classList.add('ready');
          box.querySelector('.device-poster').setAttribute('aria-hidden', 'true');
        }).catch(() => { canvas.hidden = true; });
    }))).then(() => {
      if (views.size) document.querySelector('.device-hint').textContent = 'Shown to scale. Pick yours, or drag one to take a closer look.';
    }).catch(() => {});
  }
})();
