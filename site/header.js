/* The build renders every link. JavaScript adds only the mobile disclosure. */
(() => {
  const header = document.querySelector('[data-site-header]');
  if (!header) return;
  const toggle = header.querySelector('.zy-site-header__toggle');
  const menu = header.querySelector('.zy-site-header__links');
  const label = toggle.querySelector('[data-site-menu-label]');
  const mobile = matchMedia('(max-width: 900px)');
  let open = false;

  function setOpen(next, restoreFocus = false) {
    open = mobile.matches && next;
    menu.hidden = mobile.matches && !open;
    toggle.setAttribute('aria-expanded', String(open));
    label.textContent = open ? 'Close' : 'Menu';
    if (restoreFocus && mobile.matches) toggle.focus();
  }
  function resize() {
    const focusedLink = menu.contains(document.activeElement);
    const focusedToggle = document.activeElement === toggle;
    toggle.hidden = !mobile.matches;
    setOpen(false, mobile.matches && focusedLink);
    if (!mobile.matches && focusedToggle) menu.querySelector('a').focus();
  }
  toggle.addEventListener('click', () => setOpen(!open));
  header.addEventListener('keydown', event => {
    if (event.key === 'Escape' && open) {
      event.preventDefault();
      setOpen(false, true);
    }
  });
  menu.addEventListener('click', event => {
    if (event.target.closest('a') && open) setOpen(false, true);
  });
  document.addEventListener('click', event => {
    if (open && !header.contains(event.target)) setOpen(false, menu.contains(document.activeElement));
  });
  header.addEventListener('focusout', () => {
    setTimeout(() => {
      if (open && !header.contains(document.activeElement)) setOpen(false);
    }, 0);
  });
  mobile.addEventListener('change', resize);
  resize();
})();
