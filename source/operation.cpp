// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.hpp"
#include "display.hpp"
#include "ui.hpp"
namespace {
ui::Operation operation;
u64 lastDraw = 0, phaseStarted = 0;
bool dirty = true;
void draw(bool force = false) {
  const auto now = osGetTime();
  if (!force && (!dirty && !ui::animated(operation)))
    return;
  if (!force && now - lastDraw < 120)
    return;
  operation.elapsed = now - phaseStarted;
  ui::renderOperation(
      (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr),
      (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, nullptr, nullptr),
      consoleGetDefault()->font.gfx, operation);
  display::present();
  lastDraw = osGetTime();
  dirty = false;
}
} // namespace
void beginOperation(const Game *game, const char *mode) {
  operation = {};
  operation.game = game;
  operation.mode = mode;
  phaseStarted = osGetTime();
  lastDraw = 0;
  dirty = true;
}
void setPhase(Phase phase) {
  dirty = true;
  operation.phase = phase;
  operation.done = operation.total = 0;
  operation.peers.clear();
  operation.cancelable = phase == Phase::Transfer || phase == Phase::Discovery;
  phaseStarted = osGetTime();
}
void status(const char *message) {
  std::string s = message;
  const auto newline = s.find('\n');
  operation.headline = s.substr(0, newline);
  operation.detail = newline == std::string::npos ? "" : s.substr(newline + 1);
  operation.done = operation.total = 0;
  phaseStarted = osGetTime();
  draw(true);
}
void progress(uint64_t done, uint64_t total) {
  if (operation.done != done || operation.total != total)
    dirty = true;
  operation.done = done;
  operation.total = total;
  draw();
}
int operationAction() {
  const u32 keys = hidKeysDown();
  if (keys & KEY_A)
    return 1;
  if (keys & (KEY_B | KEY_START))
    return -1;
  if (keys & KEY_TOUCH) {
    touchPosition touch;
    hidTouchRead(&touch);
    return ui::actionHit(operation, touch.px, touch.py);
  }
  return 0;
}
bool tick(bool cancel) {
  if (!aptMainLoop())
    return false;
  hidScanInput();
  draw();
  if (cancel && operationAction() == -1)
    return false;
  svcSleepThread(1000000);
  return true;
}
bool confirm(const std::string &text) {
  auto previous = operation;
  setPhase(Phase::Confirm);
  operation.headline = "Uebernahme bestaetigen";
  operation.detail = text;
  draw(true);
  bool accepted = false;
  while (tick(false)) {
    int action = operationAction();
    if (action) {
      accepted = action == 1;
      break;
    }
  }
  operation = previous;
  phaseStarted = osGetTime();
  draw(true);
  return accepted;
}
void showPeers(const std::vector<std::string> &names, size_t selected) {
  setPhase(Phase::Peers);
  operation.peers = names;
  operation.selectedPeer = selected;
  operation.headline = "Sender auswaehlen";
  operation.detail =
      "Waehle den 3DS, von dem du den Spielstand empfangen moechtest.";
  operation.cancelable = true;
  draw(true);
}
void operationResult(bool ok, const std::string &detail) {
  setPhase(ok ? Phase::Complete : Phase::Failure);
  operation.headline = ok ? "Alles erledigt" : "Vorgang beendet";
  operation.detail = detail.empty()
                         ? (ok ? "Der Vorgang wurde erfolgreich abgeschlossen."
                               : "Es wurde kein Erfolg bestaetigt.")
                         : detail;
  draw(true);
}
