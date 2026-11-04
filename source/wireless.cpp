#include "app.hpp"
#include "format.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
namespace {
constexpr u32 COMM = 0x5356524c, MAGIC = 0x31525653;
constexpr u8 CHANNEL = 1;
constexpr u8 NETWORK_VERSION = 2;
enum { DATA = 1, ACK = 2, FIN = 3, READY = 4, DONE = 5, DONEACK = 6 };
struct __attribute__((packed)) Beacon {
  u32 magic, session;
  u64 title;
};
struct __attribute__((packed)) Packet {
  u32 magic, session;
  u64 title;
  u32 seq;
  u16 kind, len;
  u32 crc;
  u8 data[1024];
};
static_assert(sizeof(Packet) <= UDS_DATAFRAME_MAXSIZE, "UDS frame too large");
struct Link {
  udsBindContext bind{};
  u32 session = 0;
  u64 title = 0;
  u16 peer = 0;
  bool host = false, active = false;
  ~Link() {
    if (active) {
      if (host)
        udsDestroyNetwork();
      else
        udsDisconnectNetwork();
      udsUnbind(&bind);
    }
    udsExit();
  }
  bool send(u16 kind, u32 seq, const void *data = nullptr, u16 len = 0) {
    Packet p{};
    p.magic = MAGIC;
    p.session = session;
    p.title = title;
    p.seq = seq;
    p.kind = kind;
    p.len = len;
    if (len)
      memcpy(p.data, data, len);
    size_t size = offsetof(Packet, data) + len;
    p.crc = ~sr::crc(~0U, &p, size);
    Result r = udsSendTo(peer, CHANNEL, UDS_SENDFLAG_Default, &p, size);
    return !(R_FAILED(r) && (u32)r != 0xC86113F0U) || fail("Funk senden", r);
  }
  int poll(Packet &p) {
    size_t n = 0;
    u16 src = 0;
    memset(&p, 0, sizeof(p));
    Result r = udsPullPacket(&bind, &p, sizeof(p), &n, &src);
    if (R_FAILED(r)) {
      fail("Funk empfangen", r);
      return -1;
    }
    if (!n)
      return 0;
    if (n < offsetof(Packet, data) || p.len > sizeof(p.data) ||
        n != offsetof(Packet, data) + p.len || p.magic != MAGIC ||
        p.session != session || p.title != title || (peer && src != peer))
      return 0;
    u32 expected = p.crc;
    p.crc = 0;
    if (expected != ~sr::crc(~0U, &p, n))
      return 0;
    if (!peer)
      peer = src;
    return 1;
  }
  bool reliable(u16 kind, u32 seq, const void *data = nullptr, u16 len = 0) {
    u64 start = osGetTime(), last = 0;
    Packet p;
    while (osGetTime() - start < 60000) {
      if (!tick())
        return fail("Abgebrochen; Empfaengerstatus pruefen");
      if (osGetTime() - last >= 350) {
        if (!send(kind, seq, data, len))
          return false;
        last = osGetTime();
      }
      int r = poll(p);
      if (r < 0)
        return false;
      if (r && p.kind == ACK && p.seq == seq && !p.len)
        return true;
    }
    return fail("Funk-Zeitlimit; Empfaengerstatus pruefen");
  }
};
// Both consoles derive the UDS passphrase from the selected session beacon.
// Sender selection and the restore confirmation authorize the transfer.
std::string pass(u32 session) {
  return std::string("SaveRelay-2-") + std::to_string(session);
}
bool host(Link &l, const Game &g) {
  u64 rand = svcGetSystemTick();
  l.session = (u32)(rand >> 8);
  if (!l.session)
    l.session = 1;
  l.title = g.id;
  l.host = true;
  udsNetworkStruct net;
  udsGenerateDefaultNetworkStruct(&net, COMM, NETWORK_VERSION, 2);
  std::string pw = pass(l.session);
  Result r = udsCreateNetwork(&net, pw.c_str(), pw.size() + 1, &l.bind, CHANNEL,
                              UDS_DEFAULT_RECVBUFSIZE);
  if (R_FAILED(r))
    return fail("Lokalfunk erstellen", r);
  l.active = true;
  Beacon b{MAGIC, l.session, g.id};
  r = udsSetApplicationData(&b, sizeof(b));
  if (R_FAILED(r))
    return fail("Funkkennung setzen", r);
  setPhase(Phase::Discovery);
  status("Sender bereit.\nAuf dem zweiten 3DS dasselbe Spiel\nwaehlen und Y "
         "druecken.");

  Packet p;
  u64 start = osGetTime();
  while (osGetTime() - start < 180000) {
    if (!tick())
      return fail("Abgebrochen");
    int n = l.poll(p);
    if (n < 0)
      return false;
    if (n && p.kind == READY && !p.len) {
      r = udsSetNewConnectionsBlocked(true, true, false);
      if (R_FAILED(r))
        return fail("Verbindung sperren", r);
      return true;
    }
  }
  return fail("Kein Empfaenger verbunden");
}
bool join(Link &l, const Game &g) {
  setPhase(Phase::Discovery);
  status("Sender suchen ...\nStarte auf dem anderen 3DS das Senden fuer "
         "dasselbe Spiel.");
  void *buf = malloc(0x4000);
  if (!buf)
    return fail("Kein Speicher");
  udsNetworkScanInfo *nets = nullptr;
  size_t count = 0;
  Result r = 0;
  u64 start = osGetTime();
  std::vector<udsNetworkScanInfo> matches;
  while (osGetTime() - start < 60000 && matches.empty()) {
    if (!tick()) {
      free(buf);
      return fail("Abgebrochen");
    }
    r = udsScanBeacons(buf, 0x4000, &nets, &count, COMM, NETWORK_VERSION,
                       nullptr, false);
    if (R_FAILED(r)) {
      free(buf);
      return fail("Sendersuche", r);
    }
    for (size_t i = 0; i < count; i++) {
      Beacon b{};
      size_t n = 0;
      if (R_SUCCEEDED(udsGetNetworkStructApplicationData(&nets[i].network, &b,
                                                         sizeof(b), &n)) &&
          n == sizeof(b) && b.magic == MAGIC && b.title == g.id)
        matches.push_back(nets[i]);
    }
    free(nets);
    nets = nullptr;
  }
  free(buf);
  if (matches.empty())
    return fail("Kein Sender fuer diese Titel-ID gefunden");
  std::vector<std::string> names;
  for (const auto &match : matches) {
    char name[41] = {};
    udsGetNodeInfoUsername(&match.nodes[0], name);
    names.push_back(name[0] ? name : "3DS");
  }
  size_t sel = 0;
  bool redraw = true;
  while (true) {
    if (redraw) {
      showPeers(names, sel);
      redraw = false;
    }
    if (!tick())
      return fail("Abgebrochen");
    u32 k = hidKeysDown();
    if (k & KEY_UP) {
      sel = (sel + matches.size() - 1) % matches.size();
      redraw = true;
    }
    if (k & KEY_DOWN) {
      sel = (sel + 1) % matches.size();
      redraw = true;
    }
    if (k & KEY_TOUCH) {
      touchPosition touch;
      hidTouchRead(&touch);
      int tapped = ui::peerHit(touch.px, touch.py, sel, matches.size());
      if (tapped >= 0) {
        sel = tapped;
        redraw = true;
      }
    }
    if (operationAction() == 1)
      break;
  }
  if (!confirm("Empfangenen Save automatisch uebernehmen?\n\nDer aktuelle "
               "Spielstand wird auf SD\ngesichert und danach ersetzt.\nGleiche "
               "Spielversion verwenden."))
    return fail("Abgebrochen");
  Beacon b{};
  size_t n = 0;
  udsGetNetworkStructApplicationData(&matches[sel].network, &b, sizeof(b), &n);
  l.session = b.session;
  l.title = g.id;
  l.peer = UDS_HOST_NETWORKNODEID;
  std::string pw = pass(l.session);
  setPhase(Phase::Discovery);
  status("Verbindung wird hergestellt ...");
  r = udsConnectNetwork(&matches[sel].network, pw.c_str(), pw.size() + 1,
                        &l.bind, UDS_HOST_NETWORKNODEID, UDSCONTYPE_Client,
                        CHANNEL, UDS_DEFAULT_RECVBUFSIZE);
  if (R_FAILED(r))
    return fail("Verbinden fehlgeschlagen. Sender neu starten", r);
  l.active = true;
  return true;
}
bool transmit(Link &l, const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    return fail("Backup oeffnen");
  fseek(f, 0, SEEK_END);
  long total = ftell(f);
  rewind(f);
  if (total < 0) {
    fclose(f);
    return fail("Backup-Groesse lesen");
  }
  setPhase(Phase::Transfer);
  status(
      "Spielstand wird gesendet\nBitte beide Konsolen in Reichweite lassen.");
  progress(0, total);
  u8 data[1024];
  u32 seq = 0;
  long sent = 0;
  bool ok = true;
  while (sent < total) {
    size_t n = fread(data, 1, std::min<long>(sizeof(data), total - sent), f);
    if (!n) {
      ok = fail("Backup lesen");
      break;
    }
    if (!l.reliable(DATA, seq++, data, n)) {
      ok = false;
      break;
    }
    sent += n;
    progress(sent, total);
  }
  fclose(f);
  if (!ok || !l.reliable(FIN, seq))
    return false;
  setPhase(Phase::Restore);
  status("Empfang bestaetigt.\nZweiter 3DS stellt den Save wieder her "
         "...\nNicht ausschalten.");
  u64 start = osGetTime();
  Packet p;
  while (osGetTime() - start < 300000) {
    if (!tick(false))
      return fail("Status unbekannt; Empfaenger pruefen");
    int n = l.poll(p);
    if (n < 0)
      return false;
    if (n && p.kind == DONE && p.len == 1 && p.seq == seq) {
      l.send(DONEACK, seq);
      return p.data[0] == 1 ||
             fail("Restore fehlgeschlagen. Empfaenger pruefen.");
    }
  }
  return fail("Keine Restore-Bestaetigung. Empfaenger pruefen.");
}
bool receive(Link &l, const Game &g) {
  std::string dest = basePath(g) + "/received-" + std::to_string(osGetTime()) +
                     ".srb",
              part = dest + ".part";
  FILE *f = fopen(part.c_str(), "wb");
  if (!f)
    return fail("SD-Datei erstellen");
  setPhase(Phase::Transfer);
  status("Spielstand wird empfangen\nDie Wiederherstellung beginnt nach der "
         "vollstaendigen Uebertragung.");
  u32 expected = 0;
  uint64_t total = 0;
  u64 last = osGetTime(), ready = 0;
  Packet p;
  bool finished = false, ok = true;
  while (osGetTime() - last < 60000) {
    if (!tick()) {
      ok = fail("Abgebrochen");
      break;
    }
    if (expected == 0 && osGetTime() - ready > 350) {
      if (!l.send(READY, 0)) {
        ok = false;
        break;
      }
      ready = osGetTime();
    }
    int n = l.poll(p);
    if (n < 0) {
      ok = false;
      break;
    }
    if (!n)
      continue;
    if (p.kind == DATA && p.seq == expected && p.len) {
      if (total + p.len > sr::MAX_BYTES ||
          fwrite(p.data, 1, p.len, f) != p.len) {
        ok = fail("Save zu gross oder SD voll");
        break;
      }
      total += p.len;
      expected++;
      last = osGetTime();
      if (!l.send(ACK, p.seq)) {
        ok = false;
        break;
      }
      progress(total);
    } else if (p.kind == DATA && expected && p.seq == expected - 1) {
      last = osGetTime();
      if (!l.send(ACK, p.seq)) {
        ok = false;
        break;
      }
    } else if (p.kind == FIN && p.seq == expected && !p.len) {
      finished = true;
      break;
    }
  }
  if (fclose(f))
    ok = fail("SD-Datei abschliessen");
  if (!ok || !finished) {
    remove(part.c_str());
    if (error.empty())
      fail("Empfangs-Zeitlimit");
    return false;
  }
  // No save archive is opened for writing until the entire staged file passes
  // validation.
  setPhase(Phase::Verify);
  status("Empfangene Daten werden geprueft");
  ok = validate(part, g.id);
  if (ok && rename(part.c_str(), dest.c_str()))
    ok = fail("Empfang abschliessen");
  if (!ok) {
    remove(part.c_str());
    return false;
  }
  l.send(ACK, expected);
  ok = applyWithBackup(g, dest);
  std::string savedError = error;
  u8 result = ok ? 1 : 0;
  u64 start = osGetTime(), retry = 0;
  while (osGetTime() - start < 10000) {
    if (!tick(false))
      break;
    if (osGetTime() - retry >= 350) {
      l.send(DONE, expected, &result, 1);
      retry = osGetTime();
    }
    int n = l.poll(p);
    if (n < 0)
      break;
    if (n && p.kind == FIN && p.seq == expected)
      l.send(ACK, expected);
    if (n && p.kind == DONEACK && p.seq == expected)
      break;
  }
  error = savedError;
  return ok;
}
} // namespace
bool transfer(const Game &g, bool sending) {
  std::string file;
  if (sending && !backup(g, "sent", file))
    return false;
  Result r = udsInit(0x3000, nullptr);
  if (R_FAILED(r))
    return fail("Lokalfunk nicht verfuegbar. WLAN aktivieren", r);
  Link l;
  aptSetSleepAllowed(false);
  bool ok;
  if (sending)
    ok = host(l, g) && transmit(l, file);
  else
    ok = join(l, g) && receive(l, g);
  aptSetSleepAllowed(true);
  return ok;
}
