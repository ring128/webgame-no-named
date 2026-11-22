const figures = [...document.querySelectorAll('main > figure')];
const links = document.querySelectorAll('nav a');

function showScreen() {
  const selected = figures.find(figure => '#' + figure.id === location.hash) || figures[0];
  for (const figure of figures) figure.hidden = figure !== selected;
  for (const link of links) {
    if (link.hash === '#' + selected.id) link.setAttribute('aria-current', 'true');
    else link.removeAttribute('aria-current');
  }
  if (selected.id !== 'play') document.querySelector('video').pause();
}

for (const link of links) {
  link.addEventListener('click', event => {
    if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
    event.preventDefault();
    if (location.hash !== link.hash) history.pushState(null, '', link.hash);
    showScreen();
  });
}
window.addEventListener('hashchange', showScreen);
showScreen();
