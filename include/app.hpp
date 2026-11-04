// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <3ds.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
struct Game {
  uint64_t id;
  FS_MediaType media;
  std::string name;
  std::vector<uint16_t> icon{};
};
enum class Phase {
  Prepare,
  Discovery,
  Transfer,
  Verify,
  Restore,
  Complete,
  Failure,
  Confirm,
  Peers
};
extern std::string error;
void beginOperation(const Game *game, const char *mode);
void setPhase(Phase phase);
void progress(uint64_t done, uint64_t total = 0);
void showPeers(const std::vector<std::string> &names, size_t selected);
void operationResult(bool ok, const std::string &detail);
int operationAction();
bool fail(const char *text, Result r = 0);
void status(const char *text);
bool tick(bool cancel = true);
bool confirm(const std::string &text);
std::vector<Game> games();
bool openSave(const Game &g, FS_Archive &a);
bool backup(const Game &g, const char *kind, std::string &path);
bool validate(const std::string &path, uint64_t title);
bool restore(const Game &g, const std::string &path);
bool applyWithBackup(const Game &g, const std::string &path);
bool recovery(const Game &g);
std::string basePath(const Game &g);
bool transfer(const Game &g, bool sending);
