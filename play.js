const game = document.getElementById("game");
const error = document.getElementById("error");

if (typeof window.Dos !== "function") {
  error.textContent = "The DOS emulator could not load. Check your connection and reload.";
  error.hidden = false;
} else {
  window.Dos(game, {
    url: new URL(game.dataset.bundle, window.location.href).href,
    backend: "dosbox",
    theme: "dark",
    imageRendering: "pixelated",
    renderAspect: "4/3",
    autoSave: false,
  });
}
