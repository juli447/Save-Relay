#include "format.hpp"
#include <cassert>
#include <iostream>
using Bytes = std::vector<uint8_t>;
template <class T> void add(Bytes &b, T v) {
  auto p = (uint8_t *)&v;
  b.insert(b.end(), p, p + sizeof(v));
}
struct Record {
  std::u16string path;
  bool dir;
  std::string data;
};
Bytes pack(const std::vector<Record> &rs, uint64_t title = 0x4000000123400ULL) {
  Bytes b;
  for (char c : std::string("SVRLY001"))
    b.push_back(c);
  add<uint32_t>(b, 1);
  add(b, title);
  add<uint32_t>(b, rs.size());
  for (auto &r : rs) {
    add<uint8_t>(b, r.dir ? 1 : 0);
    add<uint8_t>(b, 0);
    add<uint16_t>(b, r.path.size());
    add<uint64_t>(b, r.data.size());
    for (char16_t c : r.path)
      add<uint16_t>(b, c);
    b.insert(b.end(), r.data.begin(), r.data.end());
  }
  add(b, ~sr::crc(~0U, b.data(), b.size()));
  return b;
}
bool valid(const Bytes &b, uint64_t title = 0x4000000123400ULL) {
  FILE *f = tmpfile();
  assert(f);
  assert(fwrite(b.data(), 1, b.size(), f) == b.size());
  std::vector<sr::Entry> e;
  bool ok = sr::inspect(f, title, e);
  fclose(f);
  return ok;
}
int main() {
  assert(~sr::crc(~0U, "123456789", 9) == 0xcbf43926);
  auto b = pack({{u"/folder", true, ""},
                 {u"/folder/save.bin", false,
                  std::string("\0\xff"
                              "data",
                              6)},
                 {u"/empty", false, ""},
                 {u"/\u00e4\U0001f31f", false, "unicode"}});
  assert(valid(b));
  assert(valid(pack({})));
  assert(!valid(b, 42));
  for (size_t i = 0; i < b.size(); i++) {
    auto broken = b;
    broken[i] ^= 1;
    assert(!valid(broken));
  }
  for (size_t i = 0; i < b.size(); i++) {
    auto truncated = b;
    truncated.resize(i);
    assert(!valid(truncated));
  }
  auto extra = b;
  extra.push_back(0);
  assert(!valid(extra));
  for (auto p : {u"/../save", u"/a/../../save", u"/./save", u"//save", u"save",
                 u"/save/", u"/a\\b", u"/sdmc:save"})
    assert(!valid(pack({{p, false, "x"}})));
  assert(!valid(pack({{u"/missing/child", false, "x"}})));
  assert(!valid(pack({{u"/save", false, "x"}, {u"/SAVE", false, "y"}})));
  assert(!valid(pack({{u"/file", false, "x"}, {u"/file/child", false, "y"}})));
  assert(!valid(pack({{u"/dir", true, "not empty"}})));
  assert(!valid(pack({{std::u16string(u"/x\0y", 4), false, "x"}})));
  assert(!valid(pack({{std::u16string({u'/', 0xd800}), false, "x"}})));
  auto huge = pack({{u"/x", false, ""}});
  uint64_t size = sr::MAX_BYTES + 1;
  memcpy(huge.data() + 28, &size, 8);
  uint32_t crc = ~sr::crc(~0U, huge.data(), huge.size() - 4);
  memcpy(huge.data() + huge.size() - 4, &crc, 4);
  assert(!valid(huge));
  std::vector<Record> many(sr::MAX_ENTRIES + 1, {u"/x", false, ""});
  assert(!valid(pack(many)));
  std::cout
      << "PASS: CRC, valid archives, Unicode, empty files, every single-byte "
         "corruption and truncation, title mismatch, traversal, duplicates, "
         "parent types, oversized files and entry limits\n";
}
