// SPDX-License-Identifier: GPL-3.0-or-later
#include "app.hpp"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <unistd.h>
namespace fs = std::filesystem;
extern "C" int __real_rename(const char *, const char *);
static bool failPointerRename = false;
static unsigned refusedOverwrites = 0;
extern "C" int __wrap_rename(const char *from, const char *to) {
  // Model the 3DS filesystem: renaming onto an existing destination fails.
  if (fs::exists(to)) {
    ++refusedOverwrites;
    errno = EEXIST;
    return -1;
  }
  if (failPointerRename &&
      std::string(from).find("recovery.txt.part") != std::string::npos) {
    failPointerRename = false;
    errno = EIO;
    return -1;
  }
  return __real_rename(from, to);
}

std::string error;
static int counter = 1, writes = 0, failWrite = 0, corruptWrite = 0;
static std::map<int, FILE *> files;
struct Dir {
  std::vector<fs::directory_entry> entries;
  size_t pos = 0;
};
static std::map<int, Dir> dirs;
bool fail(const char *s, Result) {
  error = s;
  return false;
}
void status(const char *) {}
static Phase currentPhase = Phase::Prepare;
void setPhase(Phase phase) { currentPhase = phase; }
void progress(uint64_t done, uint64_t total) {
  assert(!total || done <= total);
}
bool confirm(const std::string &) { return true; }
bool tick(bool) { return true; }
bool openSave(const Game &, FS_Archive &a) {
  a = 1;
  return true;
}
u64 osGetTime() {
  static u64 t = 1000;
  return ++t;
}
static fs::path native(FS_Path p) {
  return fs::path("live") /
         fs::path(std::u16string((char16_t *)p.data)).relative_path();
}
Result FSUSER_OpenDirectory(Handle *h, FS_Archive, FS_Path p) {
  if (!fs::is_directory(native(p)))
    return -1;
  *h = counter++;
  Dir d;
  for (auto &e : fs::directory_iterator(native(p)))
    d.entries.push_back(e);
  dirs[*h] = d;
  return 0;
}
Result FSDIR_Read(Handle h, u32 *n, u32, FS_DirectoryEntry *e) {
  auto &d = dirs.at(h);
  *n = 0;
  if (d.pos == d.entries.size())
    return 0;
  auto &v = d.entries[d.pos++];
  memset(e, 0, sizeof(*e));
  auto s = v.path().filename().u16string();
  memcpy(e->name, s.c_str(), s.size() * 2);
  e->attributes = v.is_directory() ? 1 : 0;
  e->fileSize = v.is_directory() ? 0 : v.file_size();
  *n = 1;
  return 0;
}
Result FSDIR_Close(Handle h) {
  dirs.erase(h);
  return 0;
}
Result FSUSER_CloseArchive(FS_Archive) { return 0; }
Result FSUSER_OpenFile(Handle *h, FS_Archive, FS_Path p, u32 mode, u32) {
  FILE *f = fopen(native(p).c_str(), mode == FS_OPEN_READ ? "rb" : "r+b");
  if (!f)
    return -1;
  *h = counter++;
  files[*h] = f;
  return 0;
}
Result FSFILE_Read(Handle h, u32 *n, u64 off, void *b, u32 len) {
  FILE *f = files.at(h);
  fseek(f, off, SEEK_SET);
  *n = fread(b, 1, len, f);
  return ferror(f) ? -1 : 0;
}
Result FSFILE_Write(Handle h, u32 *n, u64 off, const void *b, u32 len, u32) {
  assert(currentPhase == Phase::Restore);
  *n = 0;
  ++writes;
  if (writes == failWrite)
    return -10;
  FILE *f = files.at(h);
  fseek(f, off, SEEK_SET);
  *n = fwrite(b, 1, len, f);
  if (writes == corruptWrite && len) {
    fseek(f, off, SEEK_SET);
    fputc(((const unsigned char *)b)[0] ^ 1, f);
  }
  return ferror(f) ? -1 : 0;
}
Result FSFILE_GetSize(Handle h, u64 *n) {
  FILE *f = files.at(h);
  fseek(f, 0, SEEK_END);
  *n = ftell(f);
  return 0;
}
Result FSFILE_Flush(Handle h) { return fflush(files.at(h)); }
Result FSFILE_Close(Handle h) {
  int r = fclose(files.at(h));
  files.erase(h);
  return r;
}
Result FSUSER_ControlArchive(FS_Archive, u32, void *, u32, void *, u32) {
  return 0;
}
Result FSUSER_DeleteDirectory(FS_Archive, FS_Path p) {
  return fs::remove(native(p)) ? 0 : -1;
}
Result FSUSER_DeleteFile(FS_Archive, FS_Path p) {
  return fs::remove(native(p)) ? 0 : -1;
}
Result FSUSER_CreateDirectory(FS_Archive, FS_Path p, u32) {
  return fs::create_directory(native(p)) ? 0 : -1;
}
Result FSUSER_CreateFile(FS_Archive, FS_Path p, u32, u64 size) {
  FILE *f = fopen(native(p).c_str(), "wb");
  if (!f)
    return -1;
  int r = ftruncate(fileno(f), size);
  fclose(f);
  return r;
}
Result FSUSER_ControlSecureSave(u32, void *, u32, void *, u32) { return 0; }
static void write(const char *p, const std::string &s) {
  std::ofstream f(p, std::ios::binary);
  f << s;
}
static std::string read(const char *p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main() {
  fs::create_directories("sdmc:/3ds/SaveRelay");
  fs::create_directories("live/sub");
  Game g{0x4000000123400, MEDIATYPE_SD, "Test"};
  write("live/main", "source");
  write("live/sub/file", std::string(20000, 'x'));
  write("live/empty", "");
  std::string incoming;
  assert(backup(g, "source", incoming));
  assert(validate(incoming, g.id));
  fs::remove_all("live");
  fs::create_directories("live/old-dir");
  write("live/old", "original");
  write("live/old-dir/only", "previous");
  assert(applyWithBackup(g, incoming));
  assert(read("live/main") == "source");
  assert(read("live/sub/file") == std::string(20000, 'x'));
  assert(!fs::exists("live/old"));
  assert(recovery(g));
  assert(read("live/old") == "original");
  assert(read("live/old-dir/only") == "previous");
  assert(!fs::exists("live/main"));
  // A second transfer must replace the pointer without overwriting by rename.
  assert(applyWithBackup(g, incoming));
  assert(recovery(g));
  assert(read("live/old") == "original");
  assert(read("live/old-dir/only") == "previous");
  const std::string pointer = basePath(g) + "/recovery.txt";
  const auto previousValue = read(pointer.c_str());
  int beforePointerFailure = writes;
  failPointerRename = true;
  assert(!applyWithBackup(g, incoming));
  assert(writes == beforePointerFailure);
  assert(read(pointer.c_str()) == previousValue);
  assert(read("live/old") == "original");
  assert(error.find("Recovery-Verweis veroeffentlichen") != std::string::npos);
  // Simulate power loss after rotating the pointer but before publishing it.
  fs::remove(pointer + ".previous");
  assert(rename(pointer.c_str(), (pointer + ".previous").c_str()) == 0);
  write("live/old", "changed");
  assert(recovery(g));
  assert(read("live/old") == "original");
  // A subsequent reception must also work when only the fallback exists.
  assert(applyWithBackup(g, incoming));
  assert(recovery(g));
  assert(read("live/old") == "original");
  assert(refusedOverwrites == 0);
  // Interrupt the second save write, after deletion has already begun.
  failWrite = writes + 2;
  assert(!applyWithBackup(g, incoming));
  assert(read("live/old") == "original");
  assert(read("live/old-dir/only") == "previous");
  assert(!fs::exists("live/main"));
  assert(error.find("Alter Save wiederhergestellt") != std::string::npos);
  corruptWrite = writes + 1;
  assert(!applyWithBackup(g, incoming));
  assert(read("live/old") == "original");
  assert(error.find("Inhalt abweichend") != std::string::npos);
  write("corrupt.srb", "garbage");
  int before = writes;
  assert(!applyWithBackup(g, "corrupt.srb"));
  assert(writes == before);
  assert(read("live/old") == "original");
  assert(files.empty() && dirs.empty());
  puts("PASS: snapshot, automatic restore, read-back, recovery, injected write "
       "failure with rollback, repeated reception without rename-overwrite, "
       "pointer publication failure, interrupted rotation recovery, corrupted "
       "input causes no save writes; host "
       "filesystem mock only");
}
