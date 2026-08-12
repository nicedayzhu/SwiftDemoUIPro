(() => {
  const repositoryUrl = 'https://github.com/nicedayzhu/SwiftDemoUIPro';
  const releaseUrl = 'https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest';
  const releasesUrl = `${repositoryUrl}/releases`;
  const repositoryApiUrl = 'https://api.github.com/repos/nicedayzhu/SwiftDemoUIPro';
  const releasesApiUrl = `${repositoryApiUrl}/releases?per_page=100`;
  const versionPattern = /^v?\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
  const windowsPackagePattern = /^SwiftDemoUIPro-v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?-win64\.zip$/i;
  const language = document.documentElement.lang.toLowerCase().startsWith('zh') ? 'zh' : 'en';
  const labels = {
    zh: {
      download: version => `下载 ${version}`,
      group: 'GitHub 项目数据',
      stars: 'GitHub Stars',
      starsTitle: '查看为此项目点 Star 的用户',
      downloads: '次 Windows 下载',
      downloadsTitle: '累计正式 Release 的 Windows x64 完整安装包下载次数，不含更新检查、源码包和校验文件',
    },
    en: {
      download: version => `Download ${version}`,
      group: 'GitHub project statistics',
      stars: 'GitHub Stars',
      starsTitle: 'View the users who starred this project',
      downloads: 'Windows downloads',
      downloadsTitle: 'Complete Windows x64 package downloads across published Releases; update checks, source archives, and checksum files are excluded',
    },
  };

  let latestVersion = '';
  let projectStats = null;

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

    const label = labels[language].download(latestVersion);
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

  const createIcon = pathData => {
    const namespace = 'http://www.w3.org/2000/svg';
    const icon = document.createElementNS(namespace, 'svg');
    icon.setAttribute('viewBox', '0 0 24 24');
    icon.setAttribute('aria-hidden', 'true');
    icon.setAttribute('focusable', 'false');
    const path = document.createElementNS(namespace, 'path');
    path.setAttribute('d', pathData);
    icon.append(path);
    return icon;
  };

  const createStatLink = ({ href, value, label, title, iconPath, statName }) => {
    const link = document.createElement('a');
    link.className = 'swift-project-stat';
    link.href = href;
    link.target = '_blank';
    link.rel = 'noopener';
    link.title = title;
    link.dataset.projectStat = statName;
    link.setAttribute('aria-label', `${value} ${label}. ${title}`);
    link.append(createIcon(iconPath));

    const valueNode = document.createElement('span');
    valueNode.className = 'swift-project-stat__value';
    valueNode.textContent = value;
    link.append(valueNode);

    const labelNode = document.createElement('span');
    labelNode.className = 'swift-project-stat__label';
    labelNode.textContent = label;
    link.append(labelNode);
    return link;
  };

  const findActionRow = hero => {
    const primaryAction = hero.querySelector(`a[href="${releaseUrl}"]`);
    const secondaryAction = hero.querySelector(`a[href="${repositoryUrl}"]`);
    if (!primaryAction || !secondaryAction) return null;

    let candidate = primaryAction.parentElement;
    while (candidate && candidate !== hero) {
      if (candidate.contains(secondaryAction)) return candidate;
      candidate = candidate.parentElement;
    }
    return null;
  };

  const mountProjectStats = () => {
    if (!projectStats) return false;
    if (document.querySelector('.swift-project-stats')) return true;

    const hero = document.querySelector('.swift-hero [data-block-type="hero"]');
    const actionRow = hero && findActionRow(hero);
    if (!actionRow) return false;

    const copy = labels[language];
    const numberFormatter = new Intl.NumberFormat(language === 'zh' ? 'zh-CN' : 'en-US');
    const stats = document.createElement('div');
    stats.className = 'swift-project-stats';
    stats.setAttribute('role', 'group');
    stats.setAttribute('aria-label', copy.group);
    stats.append(createStatLink({
      href: `${repositoryUrl}/stargazers`,
      value: numberFormatter.format(projectStats.stars),
      label: copy.stars,
      title: copy.starsTitle,
      statName: 'stars',
      iconPath: 'M11.48 3.5a.55.55 0 0 1 1.04 0l1.72 5.3h5.57a.55.55 0 0 1 .32.99l-4.5 3.28 1.72 5.3a.55.55 0 0 1-.84.61L12 15.7l-4.51 3.28a.55.55 0 0 1-.84-.61l1.72-5.3-4.5-3.28a.55.55 0 0 1 .32-.99h5.57z',
    }));

    const separator = document.createElement('span');
    separator.className = 'swift-project-stats__separator';
    separator.setAttribute('aria-hidden', 'true');
    stats.append(separator);

    stats.append(createStatLink({
      href: releasesUrl,
      value: numberFormatter.format(projectStats.windowsDownloads),
      label: copy.downloads,
      title: copy.downloadsTitle,
      statName: 'downloads',
      iconPath: 'M12 3.75v10.5m0 0 3.75-3.75M12 14.25 8.25 10.5M4.5 15.75v2.5a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2v-2.5',
    }));

    actionRow.insertAdjacentElement('afterend', stats);
    return true;
  };

  const publishedReleases = releases => releases.filter(release => !release.draft && !release.prerelease);

  const countWindowsDownloads = releases => publishedReleases(releases)
    .flatMap(release => Array.isArray(release.assets) ? release.assets : [])
    .filter(asset => windowsPackagePattern.test(asset.name || ''))
    .reduce((total, asset) => total + Math.max(0, Number(asset.download_count) || 0), 0);

  const updateDynamicContent = () => {
    updateReleaseLinks();
    mountProjectStats();
  };

  const observer = new MutationObserver(updateDynamicContent);
  observer.observe(document.body, { childList: true, subtree: true });

  Promise.all([
    fetch(repositoryApiUrl, { cache: 'no-store' }),
    fetch(releasesApiUrl, { cache: 'no-store' }),
  ])
    .then(async ([repositoryResponse, releasesResponse]) => {
      if (!repositoryResponse.ok || !releasesResponse.ok) {
        throw new Error(`GitHub API request failed: repository ${repositoryResponse.status}, releases ${releasesResponse.status}`);
      }
      return Promise.all([repositoryResponse.json(), releasesResponse.json()]);
    })
    .then(([repository, releases]) => {
      if (!Array.isArray(releases)) throw new Error('GitHub Releases returned an invalid response');
      const latestRelease = publishedReleases(releases)[0];
      latestVersion = normalizeVersion(latestRelease?.tag_name);
      if (!latestVersion) throw new Error('GitHub latest Release returned an invalid version tag');
      projectStats = {
        stars: Math.max(0, Number(repository.stargazers_count) || 0),
        windowsDownloads: countWindowsDownloads(releases),
      };
      updateDynamicContent();
    })
    .catch(() => {
      // Version-free source labels and the existing layout remain correct when GitHub is unavailable.
    })
    .finally(() => {
      window.setTimeout(() => observer.disconnect(), 5000);
    });
})();
