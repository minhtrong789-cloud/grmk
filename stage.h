#pragma once
#include <Windows.h>
#include <stdint.h>

// ---------------------------------------------------------------
// DIFF ENGINE OF GIT
//  Lightweight metadata cache to track file
//  state (mtime, size, SHA1)
//  without full Git index complexity.
// //
// struct cache_header {
//     uint32_t hdr_signature; // 4 bytes: Luôn là chữ "DIRC" (0x44495243)
//     uint32_t hdr_version;   // 4 bytes: Số phiên bản (thường là 2)
//     uint32_t hdr_entries;   // 4 bytes: Tổng số file (entries) đang lưu trong
//     index
// };
// struct cache_entry {
//     struct cache_time ce_ctime; // Thời gian tạo (sec, nsec) 8 byte 0
//     struct cache_time ce_mtime; // Thời gian sửa (sec, nsec) 8 byte 8
//     uint32_t ce_dev;            // Device  , ổ đĩa           4 byte 16
//     uint32_t ce_ino;            // Inode (not importa)       4 byte 20
//     uint32_t ce_mode;           // (mode: 100644) no importa 4 byte 24
//     uint32_t ce_uid;            // User ID                   4 byte 28
//     uint32_t ce_gid;            // Group ID                  4 byte 32
//     uint32_t ce_size;           // Dung lượng file (size)    4 byte 36
//     unsigned char sha1[20];     // 20 bytes SHA-1 của file   20 byte 40
//     uint16_t ce_flags;          // Cờ flags (file size)      2 byte 60
//     char name[0];               // Tên đường dẫn file        ... byte 62
// };

// ---------------------------------------------------------------
typedef struct {
  uint32_t st_mtime;
  uint32_t st_ctime;
  uint32_t st_mode; // info
  uint32_t st_size;
  unsigned char SHA1[20]; // #2 Diff
  uint16_t nameLen;
  char name[MAX_PATH];
} stageEntry;

typedef struct {
  uint32_t Signature;
  uint32_t Version;
  uint32_t NumberOfFiles;
} stageHeader;

typedef struct entryList {
  uint32_t capacity;
  uint32_t numFiles;
  stageEntry *entries;
} entryList;

// ---------------------------------------------------------------
// ENTRY LIST OPERATIONS (In-Memory)
// ---------------------------------------------------------------
int entryList_init(entryList *eList);
void entryList_free(entryList *eList);
int entryList_add(entryList *eList, const stageEntry *entry);
int entryList_remove(entryList *eList, const char *path);
int entryList_findIndex(const entryList *eList, const char *path);
void entryList_sort(entryList *eList);

// ---------------------------------------------------------------
// DISK I/O
// ---------------------------------------------------------------
int writeStage(const entryList *list);
entryList *loadStage(int flags);

// ---------------------------------------------------------------
// COMMANDS
// ---------------------------------------------------------------
int command_stage_add(int count, const char **paths, int flags);
int command_stage_remove(int count, const char **paths, int flags);
int command_stage_status(int flags);
int command_stage_diff(const char *path);
int command_stage_sync(int flags);
int command_stage_lsfiles(int flags);
