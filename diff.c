#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "diff.h"
#include "flags.h"
#include "objects.h"
#include "pathList.h"
#include "stage.h"
#include "utils.h"

// DIFF ENGINE CACHE:
static pathList *stg_modified = NULL;
static pathList *stg_deleted = NULL;
static pathList *stg_untracked = NULL;
static pathList *stg_unchanged = NULL;

int runDiff(void) {
  //  return if exists
  if (stg_modified && stg_deleted && stg_untracked && stg_unchanged) {
    return 0;
  }

  pathList repoFiles;
  pathList *mod = NULL;
  pathList *del = NULL;
  pathList *utrck = NULL;
  pathList *ucha = NULL;

  mod = malloc(sizeof(pathList));
  del = malloc(sizeof(pathList));
  utrck = malloc(sizeof(pathList));
  ucha = malloc(sizeof(pathList));

  pathList_init(&repoFiles);
  pathList_init(mod);
  pathList_init(del);
  pathList_init(utrck);
  pathList_init(ucha);

  if (parseFilePath(&repoFiles, true) == NULL)
    goto err;

  entryList *eList = NULL;
  eList = loadStage(FLAG_NONE);
  if (!eList)
    goto untracked;

  unsigned char SHA1_20[20]; // CORE LOGIC
  struct _stat st;
  stageEntry *entry;
  for (int i = 0; i < eList->numFiles; i++) {
    entry = &eList->entries[i];
    if (_stat(entry->name, &st) < 0) {
      pathList_add(del, entry->name);
      continue;
    }
    if (st.st_mtime != entry->st_mtime || st.st_size != entry->st_size) {
      if (hashBlob(SHA1_20, entry->name, FLAG_NONE) &&
          memcmp(SHA1_20, entry->SHA1, 20) != 0) {
        pathList_add(mod, entry->name);
        continue;
      }
    }
    pathList_add(ucha, entry->name);
  }

untracked: // if repo is new, then this is valid
  char *path = NULL;
  for (int i = 0; i < repoFiles.count; i++) {
    path = repoFiles.arrPaths[i];
    if (!eList || entryList_findIndex(eList, path) < 0) {
      pathList_add(utrck, path);
    }
  }

  stg_modified = mod;
  stg_deleted = del;
  stg_untracked = utrck;
  stg_unchanged = ucha;

  pathList_free(&repoFiles);
  return 0;
err:
  pathList_free(&repoFiles);
  pathList_free(mod);
  free(mod);
  pathList_free(del);
  free(del);
  pathList_free(utrck);
  free(utrck);
  pathList_free(ucha);
  free(ucha);
  return -1;
}

const pathList *diff_getModified(void) {
  if (!stg_modified)
    runDiff();
  return stg_modified;
}

const pathList *diff_getDeleted(void) {
  if (!stg_deleted)
    runDiff();
  return stg_deleted;
}

const pathList *diff_getUntracked(void) {
  if (!stg_untracked)
    runDiff();
  return stg_untracked;
}

const pathList *diff_getUnchanged(void) {
  if (!stg_unchanged)
    runDiff();
  return stg_unchanged;
}

// Compare two entryLists (e.g. old stage vs new target commit/tree)
// Outputs: files added in new, files modified in new, files deleted from old
int diffEntryLists(pathList *outAdded, pathList *outModified,
                   pathList *outDeleted, const entryList *oldList,
                   const entryList *newList) {
  // 1. Scan newList to detect Added and Modified files
  if (newList && newList->entries) {
    for (uint32_t i = 0; i < newList->numFiles; i++) {
      const stageEntry *newEntry = &newList->entries[i];
      int oldIdx = oldList ? entryList_findIndex(oldList, newEntry->name) : -1;

      if (oldIdx < 0) {
        if (outAdded)
          pathList_add(outAdded, newEntry->name);
      } else {
        const stageEntry *oldEntry = &oldList->entries[oldIdx];
        if (memcmp(newEntry->SHA1, oldEntry->SHA1, 20) != 0) {
          if (outModified)
            pathList_add(outModified, newEntry->name);
        }
      }
    }
  }

  // 2. Scan oldList to detect Deleted files
  if (oldList && oldList->entries) {
    for (uint32_t i = 0; i < oldList->numFiles; i++) {
      const stageEntry *oldEntry = &oldList->entries[i];
      int newIdx = newList ? entryList_findIndex(newList, oldEntry->name) : -1;

      if (newIdx < 0) {
        if (outDeleted)
          pathList_add(outDeleted, oldEntry->name);
      }
    }
  }

  return 0;
}
