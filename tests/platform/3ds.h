// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using Result = int32_t;
using FS_Archive = uint64_t;
using Handle = int;
enum FS_MediaType { MEDIATYPE_SD = 1, MEDIATYPE_GAME_CARD = 2 };
struct FS_Path {
  u32 type, size;
  const void *data;
};
struct FS_DirectoryEntry {
  u16 name[0x106];
  u32 attributes;
  u64 fileSize;
};
constexpr u32 PATH_UTF16 = 4, FS_ATTRIBUTE_DIRECTORY = 1, FS_OPEN_READ = 1,
              FS_OPEN_WRITE = 2, ARCHIVE_ACTION_COMMIT_SAVE_DATA = 0,
              SECUREVALUE_SLOT_SD = 0x1000, SECURESAVE_ACTION_DELETE = 0;
#define R_SUCCEEDED(r) ((r) >= 0)
#define R_FAILED(r) ((r) < 0)
Result FSUSER_OpenDirectory(Handle *, FS_Archive, FS_Path);
Result FSDIR_Read(Handle, u32 *, u32, FS_DirectoryEntry *);
Result FSDIR_Close(Handle);
Result FSUSER_CloseArchive(FS_Archive);
Result FSUSER_OpenFile(Handle *, FS_Archive, FS_Path, u32, u32);
Result FSFILE_Read(Handle, u32 *, u64, void *, u32);
Result FSFILE_Write(Handle, u32 *, u64, const void *, u32, u32);
Result FSFILE_GetSize(Handle, u64 *);
Result FSFILE_Flush(Handle);
Result FSFILE_Close(Handle);
Result FSUSER_ControlArchive(FS_Archive, u32, void *, u32, void *, u32);
Result FSUSER_DeleteDirectory(FS_Archive, FS_Path);
Result FSUSER_DeleteFile(FS_Archive, FS_Path);
Result FSUSER_CreateDirectory(FS_Archive, FS_Path, u32);
Result FSUSER_CreateFile(FS_Archive, FS_Path, u32, u64);
Result FSUSER_ControlSecureSave(u32, void *, u32, void *, u32);
u64 osGetTime();

// Graphics/input mock API used by the presentation regression test.
using s64 = int64_t;
enum gfxScreen_t { GFX_TOP, GFX_BOTTOM };
enum gfx3dSide_t { GFX_LEFT };
enum GSPGPU_FramebufferFormat { GSP_RGB565_OES };
struct ConsoleFont {
  u8 *gfx;
};
struct PrintConsole {
  ConsoleFont font;
};
struct touchPosition {
  u16 px, py;
};
constexpr u32 KEY_A = 1, KEY_B = 2, KEY_START = 8, KEY_TOUCH = 1 << 20;
void gfxInit(GSPGPU_FramebufferFormat, GSPGPU_FramebufferFormat, bool);
void gfxSetDoubleBuffering(gfxScreen_t, bool);
u8 *gfxGetFramebuffer(gfxScreen_t, gfx3dSide_t, u16 *, u16 *);
void gfxFlushBuffers();
void gfxSwapBuffers();
void gspWaitForVBlank();
PrintConsole *consoleGetDefault();
bool aptMainLoop();
void hidScanInput();
u32 hidKeysDown();
void hidTouchRead(touchPosition *);
Result svcSleepThread(s64);
