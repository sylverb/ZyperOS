/* Resolve official release assets without sending visitors through GitHub UI.
 * Verified links in the HTML remain useful without scripts or API access.
 * Each device with a card image in the release, ZyperOS-v<version>-<id>.img.xz,
 * gets its own card image button, made from the one in the HTML (the GKD
 * Pixel 2's): a device with none in the release has none. */
(() => {
  const panel = document.querySelector('[data-downloads]');
  if (!panel) return;
  const versionLabel = panel.querySelector('[data-download-version]');
  const status = panel.querySelector('[data-download-status]');
  const retry = panel.querySelector('[data-download-retry]');
  const endpoint = 'https://api.github.com/repos/tarikbc/ZyperOS/releases/latest';
  // A device's name on its button; an id not here shows as it is
  const deviceNames = { pixel2: 'GKD Pixel 2', brick: 'TrimUI Brick', brickpro: 'TrimUI Brick Pro' };
  let pending = false;

  function releaseFiles(release) {
    if (!release || release.draft || release.prerelease || !Array.isArray(release.assets)
      || !/^v?\d+\.\d+\.\d+(?:[-+][\w.-]+)?$/.test(release.tag_name)) throw new Error('Invalid release');
    const version = release.tag_name.replace(/^v/, '');
    const names = {
      macos: `ZyperOS-Installer-${version}-macOS.zip`,
      windows: `ZyperOS-Installer-${version}-Windows.exe`,
      deb: `ZyperOS-Installer-${version}-amd64.deb`,
      rpm: `ZyperOS-Installer-${version}-x86_64.rpm`,
      checksums: 'SHA256SUMS'
    };
    // Every device's card image in the release, keyed image-<id>
    const imagePrefix = `ZyperOS-v${version}-`;
    for (const asset of release.assets) {
      const name = asset && typeof asset.name === 'string' ? asset.name : '';
      if (!name.startsWith(imagePrefix) || !name.endsWith('.img.xz')) continue;
      const id = name.slice(imagePrefix.length, -'.img.xz'.length);
      if (/^[a-z0-9]+$/.test(id)) names[`image-${id}`] = name;
    }
    const files = new Map();
    for (const [key, name] of Object.entries(names)) {
      const asset = release.assets.find(item => item.name === name);
      if (!asset || typeof asset.browser_download_url !== 'string') continue;
      const url = new URL(asset.browser_download_url);
      const expectedPath = `/tarikbc/ZyperOS/releases/download/${release.tag_name}/${name}`;
      if (url.origin !== 'https://github.com' || url.username || url.password || url.search || url.hash
        || decodeURIComponent(url.pathname) !== expectedPath) continue;
      files.set(key, { url: url.href, name, size: asset.size });
    }
    if (![...files.keys()].some(key => key !== 'checksums')) throw new Error('No recognized downloads');
    return files;
  }
  // A button for each device's card image the release has and the page
  // doesn't, after the last one
  function addImageLinks(files) {
    const images = () => [...panel.querySelectorAll('[data-download^="image-"]')];
    const template = images()[0];
    if (!template) return;
    for (const key of [...files.keys()].sort()) {
      if (!key.startsWith('image-') || panel.querySelector(`[data-download="${key}"]`)) continue;
      const id = key.slice('image-'.length);
      const link = template.cloneNode(true);
      link.dataset.download = key;
      link.querySelector('strong').textContent = `${deviceNames[id] || id} card image`;
      const all = images();
      all[all.length - 1].after(link);
    }
  }
  function applyRelease(release, files) {
    addImageLinks(files);
    let missing = false;
    for (const link of panel.querySelectorAll('[data-download]')) {
      const file = files.get(link.dataset.download);
      const size = link.querySelector('[data-download-size]');
      link.classList.toggle('download-unavailable', !file);
      if (file) {
        link.href = file.url;
        link.setAttribute('download', file.name);
        link.removeAttribute('aria-disabled');
        if (size) size.textContent = Number.isFinite(file.size) && file.size > 0
          ? `${(file.size / 1048576).toFixed(1)} MB` : '';
      } else {
        missing = true;
        link.removeAttribute('href');
        link.removeAttribute('download');
        link.setAttribute('aria-disabled', 'true');
        if (size) size.textContent = 'Not available in this release yet';
      }
    }
    versionLabel.textContent = release.tag_name;
    status.textContent = missing
      ? 'Some files are not available in this release yet. Check again shortly.'
      : 'Latest release checked. Choose your computer above.';
    retry.hidden = !missing;
  }
  async function checkRelease() {
    if (pending) return;
    pending = true;
    retry.disabled = true;
    panel.setAttribute('aria-busy', 'true');
    status.textContent = 'Checking the latest release…';
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 8000);
    try {
      const response = await fetch(endpoint, {
        headers: { Accept: 'application/vnd.github+json' },
        cache: 'no-cache', signal: controller.signal
      });
      if (!response.ok) throw new Error('Release request failed');
      const release = await response.json();
      applyRelease(release, releaseFiles(release));
    } catch {
      status.textContent = `Could not check for a newer release. These buttons still download ${versionLabel.textContent}.`;
      retry.hidden = false;
    } finally {
      clearTimeout(timeout);
      panel.removeAttribute('aria-busy');
      pending = false;
      retry.disabled = false;
    }
  }
  retry.addEventListener('click', checkRelease);
  checkRelease();
})();
