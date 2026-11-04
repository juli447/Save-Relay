// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.hpp"
#include "display.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
std::string error;
bool fail(const char *t, Result r) {
  char b[64];
  snprintf(b, sizeof(b), " [0x%08lX]", (unsigned long)r);
  error = t;
  if (r)
    error += b;
  return false;
}
bool openSave(const Game &g, FS_Archive &a) {
  u32 p[3] = {(u32)g.media, (u32)g.id, (u32)(g.id >> 32)};
  Result r = FSUSER_OpenArchive(&a, ARCHIVE_USER_SAVEDATA,
                                {PATH_BINARY, sizeof(p), p});
  return R_SUCCEEDED(r) || fail("Save-Archiv nicht erreichbar", r);
}
static void loadMetadata(Game &game) {
  const auto id = game.id;
  const auto m = game.media;
  u32 ap[4] = {(u32)id, (u32)(id >> 32), (u32)m, 0},
      fp[5] = {0, 0, 2, 0x6e6f6369, 0};
  Handle f;
  std::string name;
  Result r = FSUSER_OpenFileDirectly(
      &f, ARCHIVE_SAVEDATA_AND_CONTENT, {PATH_BINARY, sizeof(ap), ap},
      {PATH_BINARY, sizeof(fp), fp}, FS_OPEN_READ, 0);
  if (R_SUCCEEDED(r)) {
    std::vector<u8> smdh(0x36c0);
    u32 n = 0;
    r = FSFILE_Read(f, &n, 0, smdh.data(), smdh.size());
    FSFILE_Close(f);
    if (R_SUCCEEDED(r) && n == smdh.size() && !memcmp(smdh.data(), "SMDH", 4)) {
      ui::decodeIcon(smdh.data(), smdh.size(), game.icon);
      char out[257] = {};
      u16 text[65] = {};
      memcpy(text, smdh.data() + 8 + 0x200, 128);
      ssize_t len = utf16_to_utf8((u8 *)out, text, 256);
      if (len > 0 && len <= 256) {
        out[len] = 0;
        name = out;
      }
    }
  }
  if (name.empty()) {
    char b[32];
    snprintf(b, sizeof(b), "%016llX", (unsigned long long)id);
    name = b;
  }
  game.name = name;
}
std::vector<Game> games() {
  std::vector<Game> out;
  for (auto m : {MEDIATYPE_SD, MEDIATYPE_GAME_CARD}) {
    u32 count = 0, got = 0;
    if (R_FAILED(AM_GetTitleCount(m, &count)) || count > 8192)
      continue;
    std::vector<u64> ids(count);
    if (R_FAILED(AM_GetTitleList(&got, m, count, ids.data())))
      continue;
    for (u32 i = 0; i < got; i++) {
      if (ids[i] == 0x000400000F7A1200ULL || (ids[i] >> 32) != 0x00040000)
        continue;
      Game g{ids[i], m, ""};
      FS_Archive a;
      if (openSave(g, a)) {
        FSUSER_CloseArchive(a);
        loadMetadata(g);
        out.push_back(std::move(g));
      }
    }
  }
  error.clear();
  std::sort(out.begin(), out.end(),
            [](const Game &a, const Game &b) { return a.name < b.name; });
  return out;
}
int main() {
  display::init();
  beginOperation(nullptr, "SaveRelay");
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/SaveRelay", 0777);
  Result r = amInit();
  if (R_FAILED(r)) {
    status("AM-Dienst fehlt. Luma/Rosalina benoetigt.");

    while (tick()) {
    }
    gfxExit();
    return 1;
  }
  status("Spiele werden gesucht ...");
  hidSetRepeatParameters(20, 6);
  auto list = games();
  size_t sel = 0;
  bool redraw = true;
  while (aptMainLoop()) {
    if (redraw) {
      ui::render(
          (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr),
          (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, nullptr, nullptr),
          consoleGetDefault()->font.gfx, list, sel);
      display::present();
      redraw = false;
    }
    gspWaitForVBlank();
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_START)
      break;
    if (k & KEY_SELECT) {
      beginOperation(nullptr, "Spiele suchen");
      status("Spiele werden gesucht ...");
      list = games();
      sel = 0;
      redraw = true;
      continue;
    }
    if (list.empty())
      continue;
    size_t next = sel;
    u32 nav = hidKeysDownRepeat();
    if (nav & KEY_LEFT)
      next = ui::move(next, list.size(), -1, 0);
    if (nav & KEY_RIGHT)
      next = ui::move(next, list.size(), 1, 0);
    if (nav & KEY_UP)
      next = ui::move(next, list.size(), 0, -1);
    if (nav & KEY_DOWN)
      next = ui::move(next, list.size(), 0, 1);
    if (k & KEY_L)
      next = ui::page(next, list.size(), -1);
    if (k & KEY_R)
      next = ui::page(next, list.size(), 1);
    if (k & KEY_TOUCH) {
      touchPosition touch;
      hidTouchRead(&touch);
      int tapped = ui::hit(touch.px, touch.py, next, list.size());
      if (tapped >= 0)
        next = tapped;
      else if (touch.py >= 214) {
        if (touch.px < 64)
          next = ui::page(next, list.size(), -1);
        else if (touch.px >= 264)
          next = ui::page(next, list.size(), 1);
      }
    }
    if (next != sel) {
      sel = next;
      redraw = true;
    }
    if (k & (KEY_A | KEY_Y | KEY_X)) {
      error.clear();
      beginOperation(&list[sel], k & KEY_X   ? "Recovery"
                                 : k & KEY_A ? "Senden"
                                             : "Empfangen");
      bool ok = false;
      if (k & KEY_X)
        ok = recovery(list[sel]);
      else
        ok = transfer(list[sel], k & KEY_A);
      operationResult(ok, error);
      while (tick(false)) {
        if (operationAction() == 1)
          break;
      }
      beginOperation(nullptr, "SaveRelay");
      redraw = true;
    }
  }
  amExit();
  gfxExit();
  return 0;
}
