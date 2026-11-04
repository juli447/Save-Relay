// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.hpp"
#include "format.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
static FS_Path path(const std::u16string &p) {
  return {PATH_UTF16, (u32)((p.size() + 1) * 2), p.c_str()};
}
std::string basePath(const Game &g) {
  char b[100];
  snprintf(b, sizeof(b), "sdmc:/3ds/SaveRelay/%016llX-%u",
           (unsigned long long)g.id, (unsigned)g.media);
  mkdir(b, 0777);
  return b;
}
static bool walk(FS_Archive a, const std::u16string &root,
                 std::vector<sr::Entry> &out, unsigned depth = 0) {
  if (depth > 16)
    return fail("Save-Verzeichnis zu tief");
  Handle d;
  Result r = FSUSER_OpenDirectory(&d, a, path(root));
  if (R_FAILED(r))
    return fail("Verzeichnis lesen", r);
  std::vector<FS_DirectoryEntry> children;
  FS_DirectoryEntry e;
  u32 n;
  while (true) {
    r = FSDIR_Read(d, &n, 1, &e);
    if (R_FAILED(r) || !n)
      break;
    children.push_back(e);
    if (children.size() > sr::MAX_ENTRIES) {
      FSDIR_Close(d);
      return fail("Zu viele Dateien");
    }
  }
  FSDIR_Close(d);
  if (R_FAILED(r))
    return fail("Verzeichnis lesen", r);
  for (auto &e : children) {
    size_t len = 0;
    while (len < 0x106 && e.name[len])
      len++;
    if (len == 0x106)
      return fail("Ungueltiger Dateiname");
    std::u16string p = root == u"/" ? u"/" : root + u"/";
    p.append((char16_t *)e.name, len);
    if (!sr::safePath(p) || out.size() >= sr::MAX_ENTRIES)
      return fail("Pfad oder Dateianzahl nicht unterstuetzt");
    bool dir = e.attributes & FS_ATTRIBUTE_DIRECTORY;
    out.push_back({p, dir, dir ? 0 : e.fileSize, 0});
    if (dir && !walk(a, p, out, depth + 1))
      return false;
  }
  return true;
}
bool validate(const std::string &p, uint64_t id) {
  FILE *f = fopen(p.c_str(), "rb");
  if (!f)
    return fail("Backup fehlt");
  std::vector<sr::Entry> e;
  bool ok = sr::inspect(f, id, e);
  fclose(f);
  return ok || fail("Backup unvollstaendig, falsches Spiel oder CRC falsch");
}
bool backup(const Game &g, const char *kind, std::string &dest) {
  setPhase(Phase::Prepare);
  status("Sicherheitskopie wird erstellt\nDer aktuelle Spielstand wird auf der "
         "SD-Karte gesichert.");
  FS_Archive a;
  if (!openSave(g, a))
    return false;
  std::vector<sr::Entry> entries;
  bool ok = walk(a, u"/", entries);
  if (!ok) {
    FSUSER_CloseArchive(a);
    return false;
  }
  uint64_t estimated = 28;
  for (const auto &e : entries) {
    if (e.size > sr::MAX_BYTES || estimated > sr::MAX_BYTES - e.size ||
        estimated + e.size > sr::MAX_BYTES - 12 - e.path.size() * 2) {
      FSUSER_CloseArchive(a);
      return fail("Save-Archiv zu gross");
    }
    estimated += 12 + e.path.size() * 2 + e.size;
  }
  progress(0, estimated);
  char suffix[100];
  snprintf(suffix, sizeof(suffix), "/%s-%llu.srb", kind,
           (unsigned long long)osGetTime());
  dest = basePath(g) + suffix;
  std::string part = dest + ".part";
  FILE *f = fopen(part.c_str(), "wb");
  if (!f) {
    FSUSER_CloseArchive(a);
    return fail("SD-Datei anlegen fehlgeschlagen");
  }
  uint32_t c = ~0U;
  uint64_t written = 0;
  auto put = [&](const void *b, size_t n) {
    written += n;
    if (written > sr::MAX_BYTES - 4)
      return false;
    c = sr::crc(c, b, n);
    bool saved = fwrite(b, 1, n, f) == n;
    if (saved)
      progress(written, estimated);
    return saved;
  };
  uint32_t v = 1, count = entries.size();
  ok = put("SVRLY001", 8) && put(&v, 4) && put(&g.id, 8) && put(&count, 4);
  uint8_t buf[8192];
  for (auto &e : entries) {
    if (!ok)
      break;
    uint8_t type = e.dir ? 1 : 0, z = 0;
    uint16_t len = e.path.size();
    ok = put(&type, 1) && put(&z, 1) && put(&len, 2) && put(&e.size, 8) &&
         put(e.path.data(), len * 2);
    if (e.dir || !ok)
      continue;
    Handle file;
    Result r = FSUSER_OpenFile(&file, a, path(e.path), FS_OPEN_READ, 0);
    if (R_FAILED(r)) {
      ok = fail("Save-Datei oeffnen", r);
      break;
    }
    for (uint64_t off = 0; off < e.size && ok;) {
      u32 n = 0, want = std::min<uint64_t>(sizeof(buf), e.size - off);
      r = FSFILE_Read(file, &n, off, buf, want);
      if (R_FAILED(r) || n != want) {
        ok = fail("Save-Datei lesen", r);
        break;
      }
      ok = put(buf, n);
      off += n;
    }
    FSFILE_Close(file);
  }
  c = ~c;
  if (ok)
    ok = fwrite(&c, 1, 4, f) == 4;
  if (fclose(f))
    ok = false;
  FSUSER_CloseArchive(a);
  if (!ok) {
    remove(part.c_str());
    if (error.empty())
      fail("Sichern fehlgeschlagen / SD voll");
    return false;
  }
  setPhase(Phase::Prepare);
  status("Sicherheitskopie wird geprueft");
  if (!validate(part, g.id)) {
    remove(part.c_str());
    return false;
  }
  if (rename(part.c_str(), dest.c_str()))
    return fail("Backup abschliessen fehlgeschlagen");
  return true;
}
static bool commit(FS_Archive a) {
  Result r = FSUSER_ControlArchive(a, ARCHIVE_ACTION_COMMIT_SAVE_DATA, nullptr,
                                   0, nullptr, 0);
  return R_SUCCEEDED(r) || fail("Save-Commit fehlgeschlagen", r);
}
static bool clear(FS_Archive a) {
  std::vector<sr::Entry> entries;
  if (!walk(a, u"/", entries))
    return false;
  for (auto i = entries.rbegin(); i != entries.rend(); ++i) {
    Result r = i->dir ? FSUSER_DeleteDirectory(a, path(i->path))
                      : FSUSER_DeleteFile(a, path(i->path));
    if (R_FAILED(r))
      return fail("Alten Save entfernen", r);
  }
  return true;
}
bool restore(const Game &g, const std::string &source) {
  FILE *f = fopen(source.c_str(), "rb");
  if (!f)
    return fail("Backup fehlt");
  std::vector<sr::Entry> entries;
  if (!sr::inspect(f, g.id, entries)) {
    fclose(f);
    return fail("Backup-Pruefung fehlgeschlagen");
  }
  uint64_t bytes = 0, done = 0;
  for (const auto &e : entries)
    bytes += e.size;
  setPhase(Phase::Restore);
  status("Spielstand wird wiederhergestellt\nDie Sicherheitskopie bleibt auf "
         "der SD-Karte erhalten. Bitte nicht ausschalten.");
  progress(0, bytes);
  FS_Archive a;
  if (!openSave(g, a)) {
    fclose(f);
    return false;
  }
  bool ok = clear(a);
  uint8_t buf[8192];
  for (auto &e : entries) {
    if (!ok)
      break;
    Result r;
    if (e.dir) {
      r = FSUSER_CreateDirectory(a, path(e.path), 0);
      if (R_FAILED(r))
        ok = fail("Verzeichnis erstellen", r);
      continue;
    }
    r = FSUSER_CreateFile(a, path(e.path), 0, e.size);
    if (R_FAILED(r)) {
      ok = fail("Save-Datei erstellen", r);
      break;
    }
    Handle file;
    r = FSUSER_OpenFile(&file, a, path(e.path), FS_OPEN_WRITE, 0);
    if (R_FAILED(r)) {
      ok = fail("Save-Datei schreiben", r);
      break;
    }
    if (fseek(f, (long)e.offset, SEEK_SET))
      ok = fail("Backup lesen");
    for (uint64_t off = 0; off < e.size && ok;) {
      u32 want = std::min<uint64_t>(sizeof(buf), e.size - off), n = 0;
      if (fread(buf, 1, want, f) != want) {
        ok = fail("Backup lesen");
        break;
      }
      r = FSFILE_Write(file, &n, off, buf, want, 0);
      if (R_FAILED(r) || n != want) {
        ok = fail("Save schreiben", r);
        break;
      }
      off += n;
      done += n;
      progress(done, bytes);
    }
    r = FSFILE_Flush(file);
    FSFILE_Close(file);
    if (ok && R_FAILED(r))
      ok = fail("Save flush", r);
  }
  if (ok)
    ok = commit(a);
  if (ok) {
    setPhase(Phase::Restore);
    status("Wiederhergestellter Save wird geprueft\nDie geschriebenen Daten "
           "werden zur Kontrolle zurueckgelesen.");
    done = 0;
    progress(0, bytes);
  }
  // Verify committed bytes against the staged archive before reporting success.
  for (auto &e : entries) {
    if (!ok)
      break;
    if (e.dir)
      continue;
    Handle file;
    Result r = FSUSER_OpenFile(&file, a, path(e.path), FS_OPEN_READ, 0);
    if (R_FAILED(r)) {
      ok = fail("Restore-Pruefung: Datei oeffnen", r);
      break;
    }
    u64 size = 0;
    r = FSFILE_GetSize(file, &size);
    if (R_FAILED(r) || size != e.size)
      ok = fail("Restore-Pruefung: Dateigroesse", r);
    if (fseek(f, (long)e.offset, SEEK_SET))
      ok = fail("Restore-Pruefung: Backup lesen");
    uint8_t actual[8192];
    for (uint64_t off = 0; off < e.size && ok;) {
      u32 want = std::min<uint64_t>(sizeof(buf), e.size - off), n = 0;
      if (fread(buf, 1, want, f) != want) {
        ok = fail("Restore-Pruefung: Backup lesen");
        break;
      }
      r = FSFILE_Read(file, &n, off, actual, want);
      if (R_FAILED(r) || n != want || memcmp(actual, buf, want)) {
        ok = fail("Restore-Pruefung: Inhalt abweichend", r);
        break;
      }
      off += n;
      done += n;
      progress(done, bytes);
    }
    FSFILE_Close(file);
  }
  FSUSER_CloseArchive(a);
  fclose(f);
  if (ok && g.media == MEDIATYPE_SD) {
    u64 value = ((u64)SECUREVALUE_SLOT_SD << 32) | ((u32)g.id & 0x0fffffff);
    u8 existed = 0;
    Result r =
        FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &value,
                                 sizeof(value), &existed, sizeof(existed));
    if (R_FAILED(r))
      ok = fail("Secure Value loeschen fehlgeschlagen", r);
  }
  return ok;
}
static bool recoveryIoError(const char *operation, int err) {
  return fail((std::string(operation) + ": " + strerror(err) + " (errno " +
               std::to_string(err) + ")")
                  .c_str());
}
static bool verifyPointer(const std::string &file,
                          const std::string &expected) {
  FILE *f = fopen(file.c_str(), "rb");
  if (!f)
    return recoveryIoError("Recovery-Verweis lesen", errno);
  char data[512];
  size_t n = fread(data, 1, sizeof(data), f);
  bool ok = !ferror(f) && n == expected.size() &&
            memcmp(data, expected.data(), n) == 0;
  if (fclose(f))
    ok = false;
  return ok || fail("Recovery-Verweis: Ruecklesen stimmt nicht ueberein");
}
static bool saveRecoveryPointer(const std::string &pointer,
                                const std::string &safety) {
  const auto temp = pointer + ".part", previous = pointer + ".previous";
  FILE *f = fopen(temp.c_str(), "wb");
  if (!f)
    return recoveryIoError("Recovery-Verweis anlegen", errno);
  bool ok = fwrite(safety.data(), 1, safety.size(), f) == safety.size();
  int err = ok ? 0 : errno;
  if (ok && fflush(f)) {
    ok = false;
    err = errno;
  }
  if (ok && fsync(fileno(f))) {
    ok = false;
    err = errno;
  }
  if (fclose(f) && ok) {
    ok = false;
    err = errno;
  }
  if (!ok)
    return recoveryIoError("Recovery-Verweis schreiben", err);
  if (!verifyPointer(temp, safety))
    return false;

  // 3DS rename does not replace an existing destination. Rotate the pointer,
  // retaining a fallback across an interruption before the new name is
  // published.
  struct stat st;
  bool hadPointer = stat(pointer.c_str(), &st) == 0;
  if (!hadPointer && errno != ENOENT)
    return recoveryIoError("Recovery-Verweis pruefen", errno);
  if (hadPointer) {
    if (!S_ISREG(st.st_mode))
      return fail("Recovery-Verweis ist keine Datei");
    if (unlink(previous.c_str()) && errno != ENOENT)
      return recoveryIoError("Alten Recovery-Verweis entfernen", errno);
    if (rename(pointer.c_str(), previous.c_str()))
      return recoveryIoError("Recovery-Verweis sichern", errno);
  }
  if (rename(temp.c_str(), pointer.c_str())) {
    int saved = errno;
    if (hadPointer)
      rename(previous.c_str(), pointer.c_str());
    return recoveryIoError("Recovery-Verweis veroeffentlichen", saved);
  }
  return verifyPointer(pointer, safety);
}
static bool loadRecoveryPointer(const Game &g, const std::string &file,
                                std::string &p) {
  FILE *f = fopen(file.c_str(), "rb");
  if (!f)
    return false;
  char b[512];
  size_t n = fread(b, 1, sizeof(b), f);
  bool ok = !ferror(f) && n < sizeof(b);
  if (fclose(f))
    ok = false;
  if (!ok)
    return false;
  p.assign(b, n);
  return p.find(basePath(g) + "/before-receive-") == 0 && validate(p, g.id);
}
bool applyWithBackup(const Game &g, const std::string &source) {
  if (!validate(source, g.id))
    return false;
  std::string safety;
  if (!backup(g, "before-receive", safety))
    return false;
  auto pointer = basePath(g) + "/recovery.txt";
  if (!saveRecoveryPointer(pointer, safety))
    return false;
  status(
      "Backup fertig. Save wird wiederhergestellt.\nBitte nicht ausschalten!");
  if (restore(g, source))
    return true;
  std::string original = error;
  status("Restore fehlgeschlagen. Alter Save wird geladen ...");
  if (restore(g, safety))
    error = original + "\nAlter Save wiederhergestellt.";
  else
    error =
        original + "\nRollback fehlgeschlagen! X im Menue nutzen.\n" + safety;
  return false;
}
bool recovery(const Game &g) {
  std::string p;
  const auto pointer = basePath(g) + "/recovery.txt";
  if (!loadRecoveryPointer(g, pointer, p) &&
      !loadRecoveryPointer(g, pointer + ".previous", p))
    return fail("Keine gueltige Sicherheitskopie vorhanden");
  error.clear();
  if (!confirm("Letzte Sicherheitskopie zurueckspielen?\nDer aktuelle Save "
               "wird vorher gesichert."))
    return false;
  std::string current;
  if (!backup(g, "before-recovery", current))
    return false;
  status("Wiederherstellen ... Bitte nicht ausschalten.");
  if (restore(g, p))
    return true;
  std::string first = error;
  if (restore(g, current))
    error = first + "\nBisherigen Save wiederhergestellt.";
  else
    error = first + "\nAuch Rollback fehlgeschlagen.\n" + current;
  return false;
}
