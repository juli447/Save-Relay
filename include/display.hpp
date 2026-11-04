#pragma once
#include <3ds.h>
namespace display {
inline void init() {
  // consoleInit disables double buffering; only use its static font data.
  gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
  gfxSetDoubleBuffering(GFX_TOP, true);
  gfxSetDoubleBuffering(GFX_BOTTOM, true);
}
inline void present() {
  gfxFlushBuffers();
  gfxSwapBuffers();
  // Do not reuse the old front buffer until the queued swap has taken effect.
  // Forced status updates can occur several times within one display refresh.
  gspWaitForVBlank();
}
} // namespace display
