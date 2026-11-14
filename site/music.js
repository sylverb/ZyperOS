/* Short, licensed menu-music previews. Nothing loads or plays automatically. */
(() => {
  const section = document.querySelector('#music');
  const audio = document.querySelector('#music-audio');
  const player = section.querySelector('.music-player');
  const covers = [...section.querySelectorAll('[data-track]')];
  const playButton = section.querySelector('.track-play');
  const seek = section.querySelector('#music-seek');
  const status = section.querySelector('.music-status');
  let selected = 0, request = 0, seeking = false;
  const time = seconds => `${Math.floor(seconds / 60)}:${String(Math.floor(seconds % 60)).padStart(2, '0')}`;
  const duration = () => Number.isFinite(audio.duration) ? audio.duration : 30;

  function displayTime(value = audio.currentTime) {
    const total = duration();
    seek.max = total;
    if (!seeking) seek.value = value;
    seek.setAttribute('aria-valuetext', `${time(value)} of ${time(total)}`);
    section.querySelector('[data-track-time]').textContent = time(value);
    section.querySelector('[data-track-duration]').textContent = time(total);
  }
  function displayPlayback() {
    const playing = !audio.paused && !audio.ended;
    player.dataset.playing = String(playing);
    playButton.setAttribute('aria-label', playing ? 'Pause music preview' : 'Play music preview');
    playButton.querySelector('path').setAttribute('d', playing ? 'M6 4h4v16H6zM14 4h4v16h-4z' : 'M8 4v16l12-8z');
  }
  function failure() {
    status.textContent = 'This preview could not play. Try another track.';
    status.hidden = false;
    displayPlayback();
  }
  async function play() {
    const ticket = ++request;
    status.hidden = true;
    try { await audio.play(); }
    catch (error) { if (ticket === request && error.name !== 'AbortError') failure(); }
  }
  function choose(index) {
    if (index === selected) return;
    const resume = !audio.paused;
    ++request;
    audio.pause();
    selected = (index + covers.length) % covers.length;
    const cover = covers[selected];
    audio.src = `music/${cover.dataset.track}.mp3`;
    audio.setAttribute('aria-label', `${cover.dataset.title} music preview`);
    covers.forEach((button, i) => button.setAttribute('aria-pressed', String(i === selected)));
    section.querySelector('[data-track-title]').textContent = cover.dataset.title;
    section.querySelector('[data-track-artist]').textContent = cover.dataset.artist;
    seek.disabled = true; seeking = false; seek.value = 0;
    status.hidden = true;
    displayTime(0); displayPlayback();
    if (resume) play();
  }
  covers.forEach((cover, index) => {
    cover.disabled = false;
    cover.addEventListener('click', () => choose(index));
  });
  section.querySelectorAll('[data-track-step]').forEach(button => button.addEventListener('click', () => choose(selected + Number(button.dataset.trackStep))));
  playButton.addEventListener('click', () => {
    if (audio.paused) play();
    else { ++request; audio.pause(); }
  });
  seek.addEventListener('input', () => {
    seeking = true;
    displayTime(Number(seek.value));
  });
  seek.addEventListener('change', () => {
    if (audio.readyState >= 1) audio.currentTime = Math.min(Number(seek.value), duration());
    seeking = false;
    displayTime();
  });
  audio.addEventListener('loadedmetadata', () => { seek.disabled = false; displayTime(); });
  audio.addEventListener('timeupdate', () => { if (!seeking) displayTime(); });
  for (const event of ['play', 'pause', 'ended']) audio.addEventListener(event, displayPlayback);
  audio.addEventListener('error', failure);
  // Stop when the visitor leaves the player. Returning never starts audio.
  function stop() { ++request; audio.pause(); }
  document.addEventListener('visibilitychange', () => { if (document.hidden) stop(); });
  addEventListener('pagehide', stop);
  new IntersectionObserver(entries => { if (!entries[0].isIntersecting) stop(); }).observe(section);
  audio.volume = .55;
  audio.controls = false;
  audio.hidden = true;
  player.hidden = false;
  displayTime(); displayPlayback();
})();
