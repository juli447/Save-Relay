// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "app.hpp"
namespace ui {
struct Operation {
  const Game *game = nullptr;
  Phase phase = Phase::Prepare;
  std::string mode = "SaveRelay", headline = "Bitte warten", detail;
  uint64_t done = 0, total = 0, elapsed = 0;
  bool cancelable = false;
  std::vector<std::string> peers;
  size_t selectedPeer = 0;
};
bool animated(const Operation &operation);
void renderOperation(u16 *top, u16 *bottom, const u8 *font,
                     const Operation &operation);
int actionHit(const Operation &operation, int x, int y);
int peerHit(int x, int y, size_t selected, size_t count);
unsigned percent(uint64_t done, uint64_t total);
constexpr size_t columns = 5, pageSize = 15;
size_t move(size_t selected, size_t count, int dx, int dy);
size_t page(size_t selected, size_t count, int direction);
int hit(int x, int y, size_t selected, size_t count);
void render(u16 *top, u16 *bottom, const u8 *font,
            const std::vector<Game> &list, size_t selected);
bool decodeIcon(const u8 *smdh, size_t size, std::vector<u16> &pixels);
} // namespace ui
