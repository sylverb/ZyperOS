/* The feature chapters remain complete, native disclosures without script. */
(() => {
  const reduce = matchMedia('(prefers-reduced-motion: reduce)');
  const paused = () => reduce.matches || document.documentElement.classList.contains('motion-paused');
  const preview = document.querySelector('[data-charge-preview]');
  if (preview) {
    preview.querySelector('.charge-choices').hidden = false;
    preview.addEventListener('change', event => {
      if (!event.target.matches('input[name="charge-preview"]')) return;
      const value = Number(event.target.value);
      preview.style.setProperty('--charge', `${value}%`);
      preview.querySelector('[data-charge-value]').textContent = value;
      preview.querySelector('.charge-explanation').textContent = value === 100
        ? 'With the limit off, the battery charges to 100%.'
        : `With a ${value}% limit, charging stops at ${value}% and starts again at ${value - 2}%.`;
    });
  }

  document.querySelectorAll('[data-feature-gallery]').forEach(gallery => {
    const image = gallery.querySelector('img');
    const caption = gallery.querySelector('figcaption');
    const status = gallery.querySelector('[role="status"]');
    const buttons = [...gallery.querySelectorAll('[data-feature-src]')];
    let request = 0;
    gallery.querySelector('.feature-screen-choices').hidden = false;
    buttons.forEach((button, index) => button.addEventListener('click', async () => {
      const ticket = ++request;
      const next = new Image();
      next.src = button.dataset.featureSrc;
      try { await next.decode(); }
      catch {
        if (ticket === request) {
          status.textContent = 'This screenshot could not load. Select it to try again.';
          status.hidden = false;
        }
        return;
      }
      if (ticket !== request) return;
      image.getAnimations().forEach(animation => animation.cancel());
      image.src = next.src;
      image.alt = button.dataset.featureAlt;
      caption.firstChild.textContent = `${button.textContent} `;
      caption.querySelector('span').textContent = `${String(index + 1).padStart(2, '0')} / ${String(buttons.length).padStart(2, '0')}`;
      buttons.forEach(item => item.setAttribute('aria-pressed', String(item === button)));
      status.hidden = true;
      if (!paused()) image.animate(
        [{ opacity: .35, transform: 'translateY(6px)' }, { opacity: 1, transform: 'none' }],
        { duration: 300, easing: 'cubic-bezier(.22,1,.36,1)' });
    }));
  });
})();
