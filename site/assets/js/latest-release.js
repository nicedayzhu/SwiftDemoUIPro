(() => {
  const releaseUrl = 'https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest';
  const releaseApiUrl = 'https://api.github.com/repos/nicedayzhu/SwiftDemoUIPro/releases/latest';
  const versionPattern = /^v?\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
  const language = document.documentElement.lang.toLowerCase().startsWith('zh') ? 'zh' : 'en';
  const labels = {
    zh: version => `下载 ${version}`,
    en: version => `Download ${version}`,
  };

  let latestVersion = '';

  const normalizeVersion = value => {
    const version = typeof value === 'string' ? value.trim() : '';
    if (!versionPattern.test(version)) return '';
    return version.startsWith('v') ? version : `v${version}`;
  };

  const findLabelNode = link => {
    const iterator = document.createNodeIterator(link, NodeFilter.SHOW_TEXT);
    let node;

    while ((node = iterator.nextNode())) {
      if (node.textContent.trim() && !node.parentElement?.closest('svg')) return node;
    }

    return null;
  };

  const updateReleaseLinks = () => {
    if (!latestVersion) return 0;

    const label = labels[language](latestVersion);
    const links = document.querySelectorAll(`a[href="${releaseUrl}"]`);
    let updatedLinks = 0;

    links.forEach(link => {
      const labelNode = findLabelNode(link);
      if (!labelNode) return;

      const leadingSpace = labelNode.textContent.match(/^\s*/)?.[0] || '';
      const trailingSpace = labelNode.textContent.match(/\s*$/)?.[0] || '';
      labelNode.textContent = `${leadingSpace}${label}${trailingSpace}`;
      link.dataset.releaseVersion = latestVersion;
      link.setAttribute('aria-label', label);
      updatedLinks += 1;
    });

    return updatedLinks;
  };

  const observer = new MutationObserver(() => {
    if (updateReleaseLinks()) observer.disconnect();
  });
  observer.observe(document.body, { childList: true, subtree: true });

  fetch(releaseApiUrl, { cache: 'no-store' })
    .then(response => {
      if (!response.ok) throw new Error(`GitHub latest-release request failed: ${response.status}`);
      return response.json();
    })
    .then(release => {
      latestVersion = normalizeVersion(release.tag_name);
      if (!latestVersion) throw new Error('GitHub latest Release returned an invalid version tag');
      updateReleaseLinks();
    })
    .catch(() => {
      // The version-free source label remains correct when GitHub is unavailable.
    })
    .finally(() => {
      if (!latestVersion) observer.disconnect();
    });
})();
