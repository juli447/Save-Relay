// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui.hpp"
#include <algorithm>
#include <cstring>
namespace ui {
size_t move(size_t s, size_t count, int dx, int dy) {
  if (!count)
    return 0;
  if (dx < 0)
    return s ? s - 1 : count - 1;
  if (dx > 0)
    return s + 1 < count ? s + 1 : 0;
  if (dy < 0)
    return s >= columns ? s - columns : s;
  if (dy > 0)
    return s + columns < count
               ? s + columns
               : std::min(count - 1, (s / columns + 1) * columns);
  return s;
}
size_t page(size_t s, size_t count, int direction) {
  if (!count)
    return 0;
  size_t pages = (count + pageSize - 1) / pageSize;
  size_t next = (s / pageSize + pages + (direction < 0 ? -1 : 1)) % pages;
  return std::min(next * pageSize + s % pageSize, count - 1);
}
int hit(int x, int y, size_t s, size_t count) {
  for (int i = 0; i < (int)pageSize; i++) {
    int left = 14 + (i % 5) * 60, top = 34 + (i / 5) * 58;
    if (x >= left && x < left + 56 && y >= top && y < top + 56) {
      size_t index = s / pageSize * pageSize + i;
      return index < count ? (int)index : -1;
    }
  }
  return -1;
}
bool decodeIcon(const u8 *data, size_t size, std::vector<u16> &out) {
  if (size < 0x36c0 || memcmp(data, "SMDH", 4))
    return false;
  out.resize(48 * 48);
  for (unsigned y = 0; y < 48; y++)
    for (unsigned x = 0; x < 48; x++) {
      unsigned morton = 0;
      for (unsigned bit = 0; bit < 3; bit++) {
        morton |= ((x >> bit) & 1) << (2 * bit);
        morton |= ((y >> bit) & 1) << (2 * bit + 1);
      }
      unsigned pos = 0x24c0 + 2 * ((y / 8 * 6 + x / 8) * 64 + morton);
      out[y * 48 + x] = data[pos] | ((u16)data[pos + 1] << 8);
    }
  return true;
}
namespace {
constexpr u16 rgb(unsigned r, unsigned g, unsigned b) {
  return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
}
constexpr u16 navy = rgb(22, 32, 53), panel = rgb(32, 46, 68),
              cyan = rgb(83, 218, 224), white = rgb(248, 250, 255),
              muted = rgb(170, 187, 207), blue = rgb(36, 116, 222),
              ink = rgb(41, 58, 81), paper = rgb(231, 237, 244);
struct Canvas {
  u16 *p;
  int width;
  const u8 *font;
  void pixel(int x, int y, u16 color) {
    if (x >= 0 && x < width && y >= 0 && y < 240)
      p[x * 240 + 239 - y] = color;
  }
  void rect(int x, int y, int w, int h, u16 color) {
    for (int i = std::max(0, x); i < std::min(width, x + w); i++)
      for (int j = std::max(0, y); j < std::min(240, y + h); j++)
        pixel(i, j, color);
  }
  void text(int x, int y, const std::string &text, u16 color) {
    for (unsigned char ch : text) {
      if (ch < 32 || ch > 126)
        ch = '?';
      for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
          if (font[ch * 8 + row] & (0x80 >> col))
            pixel(x + col, y + row, color);
      x += 8;
    }
  }
  void image(int x, int y, const Game &g, int scale = 1) {
    if (g.icon.size() == 48 * 48) {
      for (int row = 0; row < 48; row++)
        for (int col = 0; col < 48; col++)
          rect(x + col * scale, y + row * scale, scale, scale,
               g.icon[row * 48 + col]);
    } else {
      rect(x, y, 48 * scale, 48 * scale, panel);
      rect(x + 12 * scale, y + 10 * scale, 24 * scale, 30 * scale, muted);
      rect(x + 16 * scale, y + 14 * scale, 16 * scale, 12 * scale, navy);
      rect(x + 18 * scale, y + 31 * scale, 12 * scale, 3 * scale, navy);
    }
  }
};
std::string label(const std::string &in) {
  std::string out;
  for (size_t i = 0; i < in.size();) {
    unsigned char c = in[i++];
    if (c < 128) {
      out += c >= 32 && c != 127 ? char(c) : ' ';
      continue;
    }
    if (c == 0xc3 && i < in.size()) {
      unsigned char next = in[i++];
      switch (next) {
      case 0xa4:
        out += "ae";
        break;
      case 0xb6:
        out += "oe";
        break;
      case 0xbc:
        out += "ue";
        break;
      case 0x84:
        out += "Ae";
        break;
      case 0x96:
        out += "Oe";
        break;
      case 0x9c:
        out += "Ue";
        break;
      case 0x9f:
        out += "ss";
        break;
      case 0xa9:
      case 0xa8:
        out += 'e';
        break;
      default:
        out += '?';
      }
      continue;
    }
    while (i < in.size() && ((unsigned char)in[i] & 0xc0) == 0x80)
      i++;
    out += '?';
  }
  return out;
}
void wrapped(Canvas &c, int x, int y, std::string text, size_t cols, int lines,
             u16 color) {
  for (int line = 0; line < lines && !text.empty(); line++) {
    size_t n = std::min(cols, text.size());
    if (text.size() > cols) {
      auto space = text.rfind(' ', cols);
      if (space != std::string::npos && space > cols / 2)
        n = space;
    }
    std::string part = text.substr(0, n);
    text.erase(0, n);
    while (!text.empty() && text.front() == ' ')
      text.erase(0, 1);
    if (line == lines - 1 && !text.empty() && part.size() >= 3)
      part.replace(part.size() - 3, 3, "...");
    c.text(x, y + line * 12, part, color);
  }
}
} // namespace
void render(u16 *top, u16 *bottom, const u8 *font,
            const std::vector<Game> &list, size_t s) {
  Canvas t{top, 400, font}, b{bottom, 320, font};
  t.rect(0, 0, 400, 240, navy);
  t.rect(0, 0, 400, 34, panel);
  t.rect(16, 12, 7, 10, cyan);
  t.text(32, 13, "SaveRelay", white);
  t.text(328, 13, "1.0.1", muted);
  t.text(18, 48, "SPIELSTAND UEBERTRAGEN", muted);
  if (list.empty()) {
    t.text(18, 83, "Keine erreichbaren 3DS-Saves.", white);
    t.text(18, 104, "Spiel einmal starten und speichern.", muted);
    t.text(18, 128, "SELECT: erneut suchen", cyan);
  } else {
    const auto &g = list[s];
    t.rect(17, 71, 102, 102, panel);
    t.image(20, 74, g, 2);
    t.text(136, 77, g.media == MEDIATYPE_SD ? "SD-KARTE" : "SPIELKARTE", cyan);
    wrapped(t, 136, 96, label(g.name), 30, 4, white);
    char id[40];
    snprintf(id, sizeof(id), "%016llX", (unsigned long long)g.id);
    t.text(136, 152, id, muted);
  }
  t.text(18, 176, "SELECT: Suche     START: Ende", muted);
  t.rect(0, 187, 400, 53, panel);
  t.text(18, 198, "A  Senden", white);
  t.text(210, 198, "Y  Empfangen", cyan);
  t.text(18, 218, "X  Sicherheitskopie zurueckspielen", muted);
  b.rect(0, 0, 320, 240, paper);
  b.rect(0, 0, 320, 27, white);
  b.text(14, 10, "SPIELE", ink);
  char count[64];
  snprintf(count, sizeof(count), "%lu / %lu",
           (unsigned long)(list.empty() ? 0 : s + 1),
           (unsigned long)list.size());
  b.text(216, 10, count, ink);
  const size_t start = s / pageSize * pageSize;
  for (size_t i = start; i < list.size() && i < start + pageSize; i++) {
    int cell = i - start, x = 14 + cell % 5 * 60, y = 34 + cell / 5 * 58;
    b.rect(x + 1, y + 2, 56, 56, rgb(198, 208, 221));
    b.rect(x, y, 56, 56, i == s ? blue : white);
    b.image(x + 4, y + 4, list[i]);
    if (i == s) {
      b.rect(x + 23, y + 53, 10, 2, white);
    }
  }
  b.rect(0, 214, 320, 26, white);
  b.text(14, 223, "< L", blue);
  b.text(282, 223, "R >", blue);
  char pages[64];
  snprintf(pages, sizeof(pages), "Seite %lu / %lu",
           (unsigned long)(list.empty() ? 0 : s / pageSize + 1),
           (unsigned long)((list.size() + pageSize - 1) / pageSize));
  b.text(96, 223, pages, ink);
}

bool animated(const Operation &op) {
  return op.phase != Phase::Complete && op.phase != Phase::Failure &&
         op.phase != Phase::Confirm && op.phase != Phase::Peers;
}
unsigned percent(uint64_t done, uint64_t total) {
  if (!total)
    return 0;
  if (done >= total)
    return 100;
  return (unsigned)((long double)done * 100 / total);
}
int peerHit(int x, int y, size_t selected, size_t count) {
  if (x < 14 || x >= 306)
    return -1;
  for (int row = 0; row < 5; row++)
    if (y >= 38 + row * 27 && y < 62 + row * 27) {
      size_t index = selected / 5 * 5 + row;
      return index < count ? (int)index : -1;
    }
  return -1;
}
int actionHit(const Operation &op, int x, int y) {
  if (y < 202 || y >= 235)
    return 0;
  if (op.phase == Phase::Complete || op.phase == Phase::Failure)
    return x >= 14 && x < 306 ? 1 : 0;
  if (op.phase == Phase::Confirm || op.phase == Phase::Peers) {
    if (x >= 14 && x < 150)
      return -1;
    if (x >= 170 && x < 306)
      return 1;
    return 0;
  }
  return op.cancelable && x >= 14 && x < 306 ? -1 : 0;
}
void renderOperation(u16 *top, u16 *bottom, const u8 *font,
                     const Operation &op) {
  Canvas t{top, 400, font}, b{bottom, 320, font};
  const bool complete = op.phase == Phase::Complete,
             failed = op.phase == Phase::Failure;
  const u16 accent = failed     ? rgb(250, 115, 115)
                     : complete ? rgb(100, 224, 173)
                                : cyan;
  t.rect(0, 0, 400, 240, navy);
  t.rect(0, 0, 400, 34, panel);
  t.rect(16, 12, 7, 10, accent);
  t.text(32, 13, "SaveRelay", white);
  t.text(328, 13, "1.0.1", muted);
  t.rect(16, 47, 368, 58, panel);
  if (op.game) {
    t.image(21, 52, *op.game);
    wrapped(t, 84, 58, label(op.game->name), 35, 2, white);
    t.text(84, 88, op.game->media == MEDIATYPE_SD ? "SD-KARTE" : "SPIELKARTE",
           accent);
  } else {
    t.text(32, 65, "LOKALER SPIELSTAND-TRANSFER", white);
    t.text(32, 85, "Direkt von 3DS zu 3DS", muted);
  }
  wrapped(t, 18, 120, label(op.headline), 45, 2, white);
  const bool known = op.total > 0;
  std::string caption = complete                     ? "ABGESCHLOSSEN"
                        : failed                     ? "DETAILS UNTEN"
                        : op.phase == Phase::Confirm ? "DEINE ENTSCHEIDUNG"
                                                     : label(op.mode);
  t.text(18, 154, caption, accent);
  char text[96];
  if (known) {
    snprintf(text, sizeof(text), "%u%%", percent(op.done, op.total));
    t.text(336, 154, text, white);
  }
  t.rect(18, 173, 364, 10, panel);
  if (complete)
    t.rect(18, 173, 364, 10, accent);
  else if (known)
    t.rect(18, 173, 364 * percent(op.done, op.total) / 100, 10, accent);
  else if (!failed && op.phase != Phase::Confirm && op.phase != Phase::Peers) {
    int pulse = (op.elapsed / 18) % 580;
    int x = pulse <= 290 ? pulse : 580 - pulse;
    t.rect(18 + x, 173, 74, 10, accent);
  }
  if (known) {
    snprintf(text, sizeof(text), "%llu / %llu KiB",
             (unsigned long long)(op.done / 1024),
             (unsigned long long)(op.total / 1024));
    t.text(18, 196, text, muted);
  } else if (op.done) {
    snprintf(text, sizeof(text), "%llu KiB empfangen",
             (unsigned long long)(op.done / 1024));
    t.text(18, 196, text, muted);
  } else
    t.text(18, 196,
           complete ? "Bereit fuer das naechste Spiel."
           : failed ? "Bitte die Meldung unten lesen."
                    : "Direktverbindung ohne Router",
           muted);
  if (op.done && op.elapsed >= 1000 && op.phase == Phase::Transfer) {
    snprintf(
        text, sizeof(text), "%llu KiB/s  |  %llu s",
        (unsigned long long)((long double)op.done * 1000 / op.elapsed / 1024),
        (unsigned long long)(op.elapsed / 1000));
    t.text(18, 216, text, muted);
  } else
    t.text(18, 216,
           op.cancelable        ? "Abbruch mit B oder Touch"
           : complete || failed ? "A oder Touch: zur Spieleauswahl"
                                : "Bitte beide Geraete eingeschaltet lassen.",
           muted);
  b.rect(0, 0, 320, 240, paper);
  if (op.phase == Phase::Peers) {
    b.rect(0, 0, 320, 28, white);
    b.text(14, 11, "SENDER AUSWAEHLEN", ink);
    char pageLabel[64];
    snprintf(pageLabel, sizeof(pageLabel), "%lu/%lu",
             (unsigned long)(op.selectedPeer + 1),
             (unsigned long)op.peers.size());
    b.text(256, 11, pageLabel, ink);
    size_t first = op.selectedPeer / 5 * 5;
    for (size_t i = first; i < op.peers.size() && i < first + 5; i++) {
      int y = 38 + (i - first) * 27;
      bool selected = i == op.selectedPeer;
      b.rect(14, y, 292, 24, selected ? blue : white);
      b.text(24, y + 8, selected ? ">" : " ", selected ? white : ink);
      wrapped(b, 44, y + 8, label(op.peers[i]), 31, 1, selected ? white : ink);
    }
    b.text(14, 181, "Steuerkreuz/Touch: auswaehlen", ink);
  } else if (op.phase == Phase::Confirm || failed) {
    b.rect(0, 0, 320, 29, white);
    b.text(14, 11, failed ? "VORGANG BEENDET" : "SPIELSTAND UEBERNEHMEN?",
           failed ? rgb(185, 45, 55) : ink);
    wrapped(b, 16, 45, label(op.detail), 36, 12, ink);
  } else {
    int step = op.phase == Phase::Transfer                            ? 1
               : op.phase == Phase::Verify                            ? 2
               : op.phase == Phase::Restore                           ? 3
               : complete                                             ? 4
               : op.phase == Phase::Prepare && op.mode == "Empfangen" ? 2
                                                                      : 0;
    const char *steps[] = {"1 Start", "2 Funk", "3 Check", "4 Save"};
    for (int i = 0; i < 4; i++) {
      int x = 14 + i * 76;
      b.rect(x, 12, 68, 23, i <= step ? blue : white);
      b.text(x + 6, 20, steps[i], i <= step ? white : ink);
    }
    // Two consoles with animated data travelling between them.
    auto device = [&](int x) {
      b.rect(x, 54, 68, 50, ink);
      b.rect(x + 5, 59, 58, 19, white);
      b.rect(x + 5, 83, 58, 16, white);
      b.rect(x + 8, 86, 3, 10, blue);
      b.rect(x + 5, 89, 9, 3, blue);
      b.rect(x + 51, 87, 4, 4, blue);
    };
    device(36);
    device(216);
    b.text(40, 112, op.mode == "Senden" ? "DEIN 3DS" : "SENDER", ink);
    b.text(216, 112, op.mode == "Senden" ? "ZIEL" : "DEIN 3DS", ink);
    for (int i = 0; i < 5; i++) {
      int x = 122 + i * 16;
      bool lit = complete || i == (int)(op.elapsed / 160 % 5);
      b.rect(x, 75, 8, 8, lit ? blue : rgb(188, 204, 222));
    }
    std::string detail = op.detail;
    if (detail.empty())
      detail =
          complete ? "Der Vorgang wurde erfolgreich abgeschlossen."
          : op.phase == Phase::Transfer
              ? "Die Daten werden ueber lokalen Funk uebertragen."
          : op.phase == Phase::Prepare
              ? "Der bisherige Spielstand wird auf der SD-Karte gesichert."
          : op.phase == Phase::Verify
              ? "Die Daten werden auf Vollstaendigkeit und Fehler geprueft."
          : op.phase == Phase::Restore
              ? "Der Spielstand wird uebernommen. Bitte nicht ausschalten."
              : "Verbindung wird vorbereitet. Bitte kurz warten.";
    wrapped(b, 16, 136, label(detail), 36, 5, ink);
  }
  b.rect(0, 200, 320, 40, white);
  auto button = [&](int x, int width, const char *text, u16 fill, u16 color) {
    b.rect(x, 205, width, 28, fill);
    b.text(x + (width - (int)strlen(text) * 8) / 2, 216, text, color);
  };
  if (complete || failed)
    button(14, 292, "A  Spieleauswahl", blue, white);
  else if (op.phase == Phase::Confirm) {
    button(14, 136, "B  Zurueck", paper, ink);
    button(170, 136, "A  Uebernehmen", blue, white);
  } else if (op.phase == Phase::Peers) {
    button(14, 136, "B  Zurueck", paper, ink);
    button(170, 136, "A  Verbinden", blue, white);
  } else if (op.cancelable)
    button(14, 292, "B  Abbrechen", paper, ink);
  else
    b.text(44, 216, "Bitte nicht ausschalten", ink);
}
} // namespace ui
