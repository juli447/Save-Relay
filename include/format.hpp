// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
namespace sr {
constexpr uint64_t MAX_BYTES = 256ULL * 1024 * 1024;
constexpr uint32_t MAX_ENTRIES = 4096;
struct Entry {
  std::u16string path;
  bool dir;
  uint64_t size, offset;
};
inline uint32_t crc(uint32_t c, const void *p, size_t n) {
  auto b = (const uint8_t *)p;
  while (n--) {
    c ^= *b++;
    for (int i = 0; i < 8; i++)
      c = (c >> 1) ^ (0xedb88320U & -(c & 1));
  }
  return c;
}
inline bool safePath(const std::u16string &p) {
  if (p.size() < 2 || p.size() > 240 || p[0] != u'/' || p.back() == u'/')
    return false;
  size_t start = 1;
  unsigned depth = 0;
  for (size_t i = 1; i <= p.size(); i++) {
    if (i == p.size() || p[i] == u'/') {
      auto c = p.substr(start, i - start);
      if (c.empty() || c == u"." || c == u".." || ++depth > 16)
        return false;
      start = i + 1;
    } else if (p[i] < 32 || p[i] == u'\\' || p[i] == u':' || p[i] == 127)
      return false;
    else if (p[i] >= 0xd800 && p[i] <= 0xdbff) {
      if (i + 1 >= p.size() || p[i + 1] < 0xdc00 || p[i + 1] > 0xdfff)
        return false;
      i++;
    } else if (p[i] >= 0xdc00 && p[i] <= 0xdfff)
      return false;
  }
  return true;
}
inline std::u16string key(std::u16string p) {
  for (auto &c : p)
    if (c >= u'A' && c <= u'Z')
      c += 32;
  return p;
}
inline bool inspect(FILE *f, uint64_t title, std::vector<Entry> &entries) {
  entries.clear();
  if (fseek(f, 0, SEEK_END))
    return false;
  long total = ftell(f);
  if (total < 28 || (uint64_t)total > MAX_BYTES || fseek(f, 0, SEEK_SET))
    return false;
  uint32_t c = ~0U;
  auto read = [&](void *p, size_t n) {
    if (fread(p, 1, n, f) != n)
      return false;
    c = crc(c, p, n);
    return true;
  };
  char magic[8];
  uint64_t id;
  uint32_t version, count;
  if (!read(magic, 8) || memcmp(magic, "SVRLY001", 8) || !read(&version, 4) ||
      version != 1 || !read(&id, 8) || id != title || !read(&count, 4) ||
      count > MAX_ENTRIES)
    return false;
  std::set<std::u16string> all, dirs;
  dirs.insert(u"");
  uint8_t buf[8192];
  for (uint32_t i = 0; i < count; i++) {
    uint16_t len;
    uint8_t type, reserved;
    uint64_t size;
    if (!read(&type, 1) || type > 1 || !read(&reserved, 1) || reserved ||
        !read(&len, 2) || len > 240 || !read(&size, 8) || size > MAX_BYTES ||
        (type == 1 && size))
      return false;
    std::u16string p(len, 0);
    if (!read(p.data(), len * 2) || !safePath(p))
      return false;
    auto k = key(p);
    auto parent = k.substr(0, k.find_last_of(u'/'));
    if (!all.insert(k).second || !dirs.count(parent))
      return false;
    if (type)
      dirs.insert(k);
    long off = ftell(f);
    if (off < 0 || (uint64_t)off + size > (uint64_t)total - 4)
      return false;
    entries.push_back({p, type == 1, size, (uint64_t)off});
    for (uint64_t left = size; left;) {
      size_t n = std::min<uint64_t>(left, sizeof(buf));
      if (!read(buf, n))
        return false;
      left -= n;
    }
  }
  uint32_t stored;
  return fread(&stored, 1, 4, f) == 4 && stored == ~c && ftell(f) == total;
}
} // namespace sr
