/* Optional sharing aid. The tip itself stays in the existing Stripe checkout. */
(() => {
  const button = document.querySelector('.tip-copy');
  const status = document.querySelector('.tip-copy-status');
  if (!button || !status) return;
  button.hidden = false;
  button.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText('https://zyperos.com/');
      status.textContent = 'Copied. The link is ready to share.';
    } catch {
      status.textContent = 'Copy this link: https://zyperos.com/';
    }
    status.hidden = false;
  });
})();
