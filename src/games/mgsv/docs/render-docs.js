// Shared by render-map.html and render-engine.html. Each page is one explorer:
// a sticky tree sidebar (.ex-tree) and one view at a time (.ex-panel) in the
// main column, which scrolls with the page. Selecting a tree row or following
// a link swaps the view; the back button returns to the previous one.
// Without this script the pages still work: views are hidden="until-found",
// so following a link or finding text in the page reveals them.
(() => {
  'use strict';
  document.documentElement.classList.add('js');
  const explorer = document.querySelector('.explorer');
  if (!explorer) return;
  const tree = explorer.querySelector('.ex-tree');
  const panels = [...explorer.querySelectorAll('.ex-panel')];
  const defaultPanel = explorer.querySelector('.ex-panel.ex-default') ?? panels[0];

  // Where an id leads: a view, a heading inside a view, or a tree row's own id.
  function resolve(id) {
    const element = id && document.getElementById(id);
    if (!element) return null;
    if (element.classList.contains('ex-panel')) return { panel: element, target: null };
    if (element.matches('.ex-link[href^="#"]')) return resolve(decodeURIComponent(element.hash.slice(1)));
    const panel = element.closest('.ex-panel');
    return panel ? { panel, target: element } : null;
  }

  function openAncestors(link) {
    for (let details = link.closest('details'); details; details = details.parentElement.closest('details')) {
      if (!details.querySelector(':scope > summary').contains(link)) details.open = true;
    }
  }

  // Keep the selected row inside the sidebar's visible area.
  function keepVisible(link) {
    const box = link.getBoundingClientRect();
    const view = tree.getBoundingClientRect();
    if (box.top < view.top + 8 || box.bottom > view.bottom - 8) tree.scrollTop += box.top - view.top - view.height / 3;
  }

  function show({ panel, target }, { scroll = true } = {}) {
    for (const other of panels) {
      if (other === panel || other.hasAttribute('hidden')) continue;
      other.setAttribute('hidden', 'until-found');
    }
    panel.removeAttribute('hidden');
    for (const link of tree.querySelectorAll('[aria-current]')) link.removeAttribute('aria-current');
    const link = tree.querySelector(`.ex-link[href="#${CSS.escape(panel.id)}"]`);
    if (link) {
      link.setAttribute('aria-current', 'true');
      openAncestors(link);
      keepVisible(link);
    }
    if (!scroll) return;
    if (target) target.scrollIntoView({ block: 'start' });
    else scrollTo({ top: 0 });
  }

  document.addEventListener('click', event => {
    const link = event.target.closest('a[href^="#"]');
    if (!link || event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey) return;
    const found = resolve(decodeURIComponent(link.hash.slice(1)));
    if (!found) return;
    event.preventDefault();
    show(found);
    const hash = `#${(found.target ?? found.panel).id}`;
    if (location.hash !== hash) history.pushState(null, '', hash);
  });

  addEventListener('popstate', () => show(resolve(decodeURIComponent(location.hash.slice(1))) ?? { panel: defaultPanel, target: null }));

  // Find in page reveals a hidden view; make it the selected one.
  for (const panel of panels) panel.addEventListener('beforematch', () => show({ panel, target: null }, { scroll: false }));

  // Keyboard: arrows move through visible rows and open them, Right and Left
  // expand and collapse, Home and End jump to the ends.
  tree.addEventListener('keydown', event => {
    const link = event.target.closest('.ex-link');
    if (!link) return;
    const links = [...tree.querySelectorAll('.ex-link')].filter(item => item.offsetParent !== null);
    const index = links.indexOf(link);
    const details = link.closest('summary')?.parentElement;
    let next = null;
    if (event.key === 'ArrowDown') next = links[index + 1];
    else if (event.key === 'ArrowUp') next = links[index - 1];
    else if (event.key === 'Home') next = links[0];
    else if (event.key === 'End') next = links.at(-1);
    else if (event.key === 'ArrowRight' && details && !details.open) details.open = true;
    else if (event.key === 'ArrowRight') next = links[index + 1];
    else if (event.key === 'ArrowLeft' && details?.open) details.open = false;
    else if (event.key === 'ArrowLeft') next = link.closest('ul')?.closest('details')?.querySelector(':scope > summary .ex-link');
    else return;
    event.preventDefault();
    if (!next) return;
    next.focus();
    const found = resolve(decodeURIComponent(next.hash.slice(1)));
    if (found) {
      show(found);
      history.replaceState(null, '', `#${found.panel.id}`);
    }
  });

  for (const button of tree.querySelectorAll('[data-ex-expand]')) {
    button.addEventListener('click', () => {
      for (const details of tree.querySelectorAll('details')) details.open = button.dataset.exExpand === 'all';
    });
  }

  // The browser's own jump to the fragment can land after this script runs, so
  // apply the initial view again once loading finishes.
  history.scrollRestoration = 'manual';
  const initial = () => show(resolve(decodeURIComponent(location.hash.slice(1))) ?? { panel: defaultPanel, target: null });
  initial();
  addEventListener('load', () => requestAnimationFrame(initial), { once: true });
})();
