#include "app.hpp"
#include "display.hpp"
#include "ui.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
namespace {
std::vector<u16> buffers[2][2], displayed[2];
int front[2] = {0, 0};
bool doubleBuffer[2] = {false, false};
enum { Idle, Flushed, Pending } stage = Idle;
unsigned frames = 0, flushes = 0;
u64 now = 1000;
u32 keys = 0;
std::vector<char> events;
u8 font[2048] = {};
PrintConsole console{{font}};
void unchanged() {
  for (int screen = 0; screen < 2; screen++)
    assert(buffers[screen][front[screen]] == displayed[screen]);
}
void settle(unsigned milliseconds) {
  for (unsigned i = 0; i < milliseconds; i++)
    assert(tick(false));
}
} // namespace
u64 osGetTime() { return now; }
void gfxInit(GSPGPU_FramebufferFormat top, GSPGPU_FramebufferFormat bottom,
             bool vram) {
  assert(top == GSP_RGB565_OES && bottom == GSP_RGB565_OES && !vram);
  for (int s = 0; s < 2; s++) {
    for (int b = 0; b < 2; b++)
      buffers[s][b].assign((s ? 320 : 400) * 240, 0x1234);
    displayed[s] = buffers[s][0];
  }
}
void gfxSetDoubleBuffering(gfxScreen_t s, bool enabled) {
  assert(enabled);
  doubleBuffer[s] = enabled;
}
u8 *gfxGetFramebuffer(gfxScreen_t s, gfx3dSide_t, u16 *, u16 *) {
  assert(stage == Idle && doubleBuffer[s]);
  unchanged();
  return (u8 *)buffers[s][1 - front[s]].data();
}
void gfxFlushBuffers() {
  assert(stage == Idle);
  unchanged();
  stage = Flushed;
  flushes++;
  events.push_back('F');
}
void gfxSwapBuffers() {
  assert(stage == Flushed);
  unchanged();
  stage = Pending;
  events.push_back('S');
}
void gspWaitForVBlank() {
  assert(stage == Pending);
  unchanged();
  for (int s = 0; s < 2; s++) {
    front[s] = 1 - front[s];
    displayed[s] = buffers[s][front[s]];
  }
  now += 17;
  frames++;
  stage = Idle;
  events.push_back('V');
}
PrintConsole *consoleGetDefault() { return &console; }
bool aptMainLoop() { return true; }
void hidScanInput() {}
u32 hidKeysDown() { return keys; }
void hidTouchRead(touchPosition *t) { t->px = t->py = 0; }
Result svcSleepThread(s64 ns) {
  now += ns / 1000000;
  return 0;
}
int main() {
  display::init();
  Game game{0x4000000123400, MEDIATYPE_SD, "Presentation regression"};
  beginOperation(&game, "Senden");
  status("Spielstand wird gesichert");
  assert(frames == 1 && stage == Idle);
  // Rapid forced updates must each finish their swap before drawing the next
  // frame.
  status("Sicherheitskopie wird geprueft");
  status("Sender bereit");
  assert(frames == 3);
  setPhase(Phase::Transfer);
  status("Spielstand wird gesendet");
  progress(1024, 2048);
  unsigned before = frames;
  settle(500);
  assert(frames > before);
  unchanged();
  // The video shows strip flicker on this exact idle completion screen.
  operationResult(true, "");
  before = frames;
  const auto completedTop = displayed[0], completedBottom = displayed[1];
  settle(2000);
  assert(frames == before);
  assert(displayed[0] == completedTop && displayed[1] == completedBottom);
  operationResult(false, "Testfehler");
  before = frames;
  settle(1000);
  assert(frames == before);
  showPeers({"Alex", "Sam"}, 0);
  before = frames;
  settle(1000);
  assert(frames == before);
  showPeers({"Alex", "Sam"}, 1);
  assert(frames == before + 1);
  // A static confirmation frame must not animate/repaint either.
  ui::Operation op;
  op.phase = Phase::Confirm;
  assert(!ui::animated(op));
  op.phase = Phase::Transfer;
  assert(ui::animated(op));
  for (size_t i = 0; i < events.size(); i += 3) {
    assert(events[i] == 'F' && events[i + 1] == 'S' && events[i + 2] == 'V');
  }
  assert(frames == flushes && stage == Idle);
  unchanged();
  puts("PASS: RGB565 double buffering, render only to hidden buffers, "
       "flush/swap/VBlank ordering, rapid transitions, animated transfers and "
       "stable completion/error/peer screens");
}
