#include "ui.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
static void ppm(const char *path, const std::vector<u16> &pixels, int width) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << width << " 240\n255\n";
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < width; x++) {
      u16 c = pixels[x * 240 + 239 - y];
      unsigned char rgb[] = {static_cast<u8>(((c >> 11) & 31) * 255 / 31),
                             static_cast<u8>(((c >> 5) & 63) * 255 / 63),
                             static_cast<u8>((c & 31) * 255 / 31)};
      out.write((char *)rgb, 3);
    }
}
int main(int argc, char **argv) {
  assert(argc >= 2);
  for (size_t count = 0; count < 200; count++)
    for (size_t s = 0; s < (count ? count : 1); s++) {
      for (auto d : {-1, 1}) {
        assert(ui::move(s, count, d, 0) < (count ? count : 1));
        assert(ui::move(s, count, 0, d) < (count ? count : 1));
        assert(ui::page(s, count, d) < (count ? count : 1));
      }
    }
  assert(ui::move(2, 20, 0, 1) == 7);
  assert(ui::move(7, 20, 0, -1) == 2);
  assert(ui::page(0, 20, 1) == 15);
  assert(ui::page(15, 20, -1) == 0);
  assert(ui::hit(18, 38, 0, 20) == 0);
  assert(ui::hit(18, 38, 15, 20) == 15);
  assert(ui::hit(258, 154, 15, 20) == -1);
  assert(ui::hit(4, 4, 0, 20) == -1);
  std::ifstream in(argv[1], std::ios::binary);
  std::vector<u8> smdh{std::istreambuf_iterator<char>(in), {}};
  std::vector<u16> icon;
  assert(ui::decodeIcon(smdh.data(), smdh.size(), icon));
  for (unsigned y = 0; y < 48; y++)
    for (unsigned x = 0; x < 48; x++) {
      // Golden image encoded independently by the official smdhtool.
      u16 r = (x * 5) >> 3, g = (y * 5) >> 2, b = ((x + y) * 3 % 256) >> 3;
      assert(icon[y * 48 + x] == (r << 11 | g << 5 | b));
    }
  assert(!ui::decodeIcon(smdh.data(), smdh.size() - 1, icon));
  auto corrupt = smdh;
  corrupt[0] = 0;
  assert(!ui::decodeIcon(corrupt.data(), corrupt.size(), icon));
  std::vector<u8> font(2048, 0xff);
  if (argc >= 3) {
    std::ifstream f(argv[2], std::ios::binary);
    f.read((char *)font.data(), font.size());
    assert(f.gcount() == 2048);
  }
  std::vector<Game> games;
  const char *names[] = {
      "Sky Garden",      "Ocean Quest",    "Puzzle Planet",   "Star Racer",
      "Paper Adventure", "Island Friends", "Moonlight",       "Forest Stories",
      "Crystal Valley",  "Rainbow Rally",  "Little Explorer", "Music Club",
      "Dream World",     "Space Voyage",   "Garden Party",    "Second Page"};
  for (int i = 0; i < 16; i++) {
    Game game{0x4000000000000ULL + (u64)i * 256,
              i == 7 ? MEDIATYPE_GAME_CARD : MEDIATYPE_SD, names[i]};
    game.icon.resize(2304);
    for (int y = 0; y < 48; y++)
      for (int x = 0; x < 48; x++) {
        unsigned r = (i * 39 + 40) % 200 + 30, g = (i * 63 + 20) % 190 + 30,
                 b = (i * 23 + 90) % 180 + 40;
        if ((x - 24) * (x - 24) + (y - 24) * (y - 24) < 160) {
          r = 245;
          g = 245;
          b = 245;
        }
        if (x > 20 && x < 28 && y > 13 && y < 35) {
          r = 40;
          g = 60;
          b = 80;
        }
        game.icon[y * 48 + x] = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
      }
    games.push_back(game);
  }
  std::vector<u16> top(400 * 240), bottom(320 * 240);
  ui::render(top.data(), bottom.data(), font.data(), games, 7);
  if (argc >= 3) {
    ppm("top.ppm", top, 400);
    ppm("bottom.ppm", bottom, 320);
  }
  games[15].icon.clear();
  ui::render(top.data(), bottom.data(), font.data(), games, 15);
  ui::render(top.data(), bottom.data(), font.data(), {}, 0);

  assert(ui::percent(10, 0) == 0);
  assert(ui::percent(0, 100) == 0);
  assert(ui::percent(50, 100) == 50);
  assert(ui::percent(200, 100) == 100);
  assert(ui::percent((UINT64_MAX - 1) / 2, UINT64_MAX - 1) == 50);
  ui::Operation op;
  op.game = &games[7];
  op.mode = "Senden";
  op.phase = Phase::Transfer;
  op.cancelable = true;
  op.headline = "Spielstand wird gesendet";
  op.detail = "Bitte beide Konsolen in Reichweite lassen.";
  op.done = 524288;
  op.total = 1048576;
  op.elapsed = 6000;
  ui::renderOperation(top.data(), bottom.data(), font.data(), op);
  if (argc >= 3) {
    ppm("send-top.ppm", top, 400);
    ppm("send-bottom.ppm", bottom, 320);
  }
  assert(ui::actionHit(op, 160, 216) == -1);
  op.mode = "Empfangen";
  op.total = 0;
  op.headline = "Spielstand wird empfangen";
  ui::renderOperation(top.data(), bottom.data(), font.data(), op);
  if (argc >= 3) {
    ppm("receive-top.ppm", top, 400);
    ppm("receive-bottom.ppm", bottom, 320);
  }
  op.done = op.total = 0;
  op.phase = Phase::Confirm;
  op.cancelable = false;
  op.headline = "Uebernahme bestaetigen";
  op.detail =
      "Empfangenen Save automatisch uebernehmen? Der aktuelle Spielstand wird "
      "auf SD gesichert und danach ersetzt. Gleiche Spielversion verwenden.";
  assert(ui::actionHit(op, 40, 216) == -1);
  assert(ui::actionHit(op, 200, 216) == 1);
  assert(ui::actionHit(op, 160, 216) == 0);
  ui::renderOperation(top.data(), bottom.data(), font.data(), op);
  if (argc >= 3) {
    ppm("confirm-top.ppm", top, 400);
    ppm("confirm-bottom.ppm", bottom, 320);
  }
  op.peers = {"Alex", "Sam", "Robin", "3DS 4", "3DS 5", "3DS 6"};
  op.selectedPeer = 5;
  op.phase = Phase::Peers;
  op.headline = "Sender auswaehlen";
  assert(ui::peerHit(40, 48, 5, 6) == 5);
  assert(ui::peerHit(40, 75, 5, 6) == -1);
  assert(ui::peerHit(0, 48, 5, 6) == -1);
  ui::renderOperation(top.data(), bottom.data(), font.data(), op);
  if (argc >= 3) {
    ppm("peers-top.ppm", top, 400);
    ppm("peers-bottom.ppm", bottom, 320);
  }
  for (auto phase : {Phase::Prepare, Phase::Discovery, Phase::Verify,
                     Phase::Restore, Phase::Complete, Phase::Failure}) {
    op.phase = phase;
    op.done = op.total = 0;
    op.cancelable = false;
    op.headline = phase == Phase::Failure ? "Vorgang beendet" : "Alles bereit";
    op.detail = phase == Phase::Failure
                    ? std::string(520, 'X')
                    : "Der Spielstand wird auf der SD-Karte gesichert.";
    ui::renderOperation(top.data(), bottom.data(), font.data(), op);
    assert(ui::actionHit(op, 160, 216) ==
           (phase == Phase::Complete || phase == Phase::Failure ? 1 : 0));
  }
  op.game = nullptr;
  op.phase = Phase::Prepare;
  ui::renderOperation(top.data(), bottom.data(), font.data(), op);
  puts("PASS: icon decoding, grid/touch navigation, transfer progress "
       "including unknown/zero size, all operation screens, peer pagination, "
       "confirmation and cancel targets");
}
