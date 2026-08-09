(() => {
  const focusSpecs = [
    {
      className: 'swift-focus--voice-avatars',
      centerX: 0.105,
      centerY: 0.775,
      zoom: 2.25,
      labels: { zh: '语音头像', en: 'Voice indicators' },
    },
    {
      className: 'swift-focus--menu',
      centerX: 0.795,
      centerY: 0.3,
      zoom: 1.65,
      labels: { zh: 'Demo 语音菜单', en: 'Voice menu' },
    },
  ];

  const largestSource = image => {
    const candidates = (image.srcset || '')
      .split(',')
      .map(candidate => candidate.trim().split(/\s+/))
      .filter(parts => parts[0])
      .map(([url, width]) => ({ url, width: Number.parseInt(width, 10) || 0 }))
      .sort((left, right) => right.width - left.width);

    return candidates[0]?.url || image.currentSrc || image.src;
  };

  const mountFocusViews = () => {
    const hero = document.querySelector('.swift-hero [data-block-type="hero"]');
    const sourceImage = hero?.querySelector('img[loading="eager"]');
    if (!sourceImage) return false;

    const visual = sourceImage.parentElement;
    if (!visual || visual.classList.contains('swift-hero-visual')) return true;

    visual.classList.add('swift-hero-visual');
    sourceImage.classList.add('swift-hero-main');

    const language = document.documentElement.lang.toLowerCase().startsWith('zh') ? 'zh' : 'en';
    const detailSource = largestSource(sourceImage);
    const views = focusSpecs.map(spec => {
      const figure = document.createElement('figure');
      figure.className = `swift-focus ${spec.className}`;
      figure.setAttribute('role', 'img');
      figure.setAttribute('aria-label', spec.labels[language]);

      const detailImage = document.createElement('img');
      detailImage.src = detailSource;
      detailImage.alt = '';
      detailImage.setAttribute('aria-hidden', 'true');
      detailImage.decoding = 'async';

      const caption = document.createElement('figcaption');
      caption.textContent = spec.labels[language];

      figure.append(detailImage, caption);
      visual.append(figure);
      return { figure, detailImage, spec };
    });

    const updateViews = () => {
      const sourceRect = sourceImage.getBoundingClientRect();
      if (!sourceRect.width || !sourceRect.height) return;

      views.forEach(({ figure, detailImage, spec }) => {
        const viewRect = figure.getBoundingClientRect();
        const scaledWidth = sourceRect.width * spec.zoom;
        const scaledHeight = sourceRect.height * spec.zoom;
        detailImage.style.width = `${scaledWidth}px`;
        detailImage.style.height = `${scaledHeight}px`;
        detailImage.style.left = `${viewRect.width / 2 - scaledWidth * spec.centerX}px`;
        detailImage.style.top = `${viewRect.height / 2 - scaledHeight * spec.centerY}px`;
      });
    };

    sourceImage.addEventListener('load', updateViews, { once: true });
    requestAnimationFrame(updateViews);
    new ResizeObserver(updateViews).observe(visual);
    return true;
  };

  const initialize = () => {
    if (mountFocusViews()) return;

    const observer = new MutationObserver(() => {
      if (mountFocusViews()) observer.disconnect();
    });
    observer.observe(document.body, { childList: true, subtree: true });
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initialize, { once: true });
  } else {
    initialize();
  }
})();
