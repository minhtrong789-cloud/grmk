/*
INDEX is the [CHANGE-TRACKING ENGINE] of GIT.

GIT INFO:
git add <path...> / -A / -u       (stage new, modified, or deleted files)
git rm <path...> / -f             (remove file from working tree and index)

git status [-s]                   (show working tree and staging status)
git diff [<path>]                 (show changes between working tree and index)
git ls-files                      (list cached files in index)

GRMK WISHLIST / IMPLEMENTED:
grmk add <path...> [-A] [-u]      (update .mygit stage file)
grmk rm <path...> [-f]            (remove entries from stage)

grmk status [-s]                  (compare stage vs working tree)
grmk diff [<path>]                (file-level change summary)
grmk ls-files                     (list all tracked files in stage)

grmk dump-index [-idx] [-stg]     (debug tool: inspect binary index/stage)
*/
#include <Windows.h>

#include <direct.h> // _getcwd()
#include <fcntl.h>
#include <io.h>       // _open, _read, _close, _filelength
#include <sys/stat.h> //_stat

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "branch.h"
#include "diff.h"
#include "objects.h"
#include "stage.h"
#include "utils.h"

entryList stgEntries = {0, 0, NULL};

// ---------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------
static inline uint32_t readBE32(const unsigned char *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

static int cmpEntries(const void *a, const void *b) {
  return strcmp(((stageEntry *)a)->name, ((stageEntry *)b)->name);
}
int entryList_findIndex(const entryList *eList, const char *path) {
  if (!eList)
    eList = &stgEntries;
  if (!path || !eList->entries)
    return -1;

  int left = 0;
  int right = (int)eList->numFiles - 1;
  while (left <= right) {
    int mid = left + (right - left) / 2;
    int cmp = strcmp(path, eList->entries[mid].name); // core compare
    if (cmp == 0)
      return mid;
    else if (cmp < 0)
      right = mid - 1;
    else
      left = mid + 1;
  }
  return -1;
}
void entryList_sort(entryList *eList) {
  if (!eList)
    eList = &stgEntries;
  qsort(eList->entries, eList->numFiles, sizeof(stageEntry), cmpEntries);
}

int entryList_init(entryList *eList) {
  if (!eList)
    return -1;
  eList->numFiles = 0;
  eList->capacity = 16;
  eList->entries = malloc(eList->capacity * sizeof(stageEntry));
  return 0;
}

void entryList_free(entryList *eList) {
  if (!eList)
    return;
  if (eList->entries) {
    free(eList->entries);
    eList->entries = NULL;
  }
  eList->numFiles = 0;
  eList->capacity = 0;
}

int entryList_add(entryList *eList, const stageEntry *entry) {
  if (entry == NULL)
    return -1;
  if (eList == NULL)
    eList = &stgEntries;
  if (eList->capacity == 0) {
    entryList_init(eList);
  }

  if (eList->numFiles >= eList->capacity) {
    eList->capacity *= 2;
    eList->entries =
        realloc(eList->entries, eList->capacity * sizeof(stageEntry));
  }
  eList->entries[eList->numFiles++] = *entry;
  return 0;
}

int entryList_remove(entryList *eList, const char *path) {
  // flags: -A, -v, -f
  // path : file or dir path
  if (path == NULL)
    return -1;
  if (eList == NULL)
    eList = &stgEntries;

  int i = entryList_findIndex(eList, path);
  if (i < 0)
    return -1;

  for (int j = i; j < eList->numFiles - 1; j++) {
    eList->entries[j] = eList->entries[j + 1];
  }
  eList->numFiles--;
  return 0;
}

// ---------------------------------------------------------------
// APIS
// ---------------------------------------------------------------
// we need something to load index on -> parse it compare with sys
// we need something to parse folder -> compare with index
// we have: changed, untracked, notification
// feature of index:
/*
add new entry into index file: createblob-> add index sha and path
*/

// send NULL to work with curstage
int writeStage(const entryList *list) {
  if (!list)
    list = &stgEntries;
  char *gitRepo = NULL;
  gitRepo = findGitRepo();
  if (!gitRepo)
    return -1;

  // create header:
  stageHeader stagehd;
  memcpy(&stagehd.Signature, "GRMK", 4);
  stagehd.Version = 0;
  stagehd.NumberOfFiles = list->numFiles;

  // write binary
  char repoFolder[MAX_PATH];
  snprintf(repoFolder, MAX_PATH, "%s/stage.lock", repoName);
  FILE *f = fopen(repoFolder, "wb");
  if (f == NULL) {
    fprintf(stderr, "cannot create/write to stage file\n");
    return -1;
  }

  fwrite(&stagehd, sizeof(stagehd), 1, f); // write heaader
  if (list->numFiles > 0 && list->entries != NULL) {
    fwrite(list->entries, sizeof(stageEntry), list->numFiles, f);
  }
  fclose(f);
  f = NULL;

  char repoFolder2[MAX_PATH];
  snprintf(repoFolder2, MAX_PATH, "%s/stage", repoName);
  remove(repoFolder2);
  rename(repoFolder, repoFolder2);
  return 0;
}

int loadIndex(int flags) {

  unsigned char *bufStage = NULL; // Parse Data
  uint32_t fSize;
  int entryNumber; // Weite entries
  stageEntry *entries = NULL;
  char const *mygit = repoName; // get Path
  char *repoPath = NULL;

  // PHASE1: CHECK FILE
  repoPath = findGitRepo(); // get Path
  if (repoPath == NULL) {
    printf("Not a git repo yet. use \"init\" instead");
    return -1;
  }

  char indexPath[MAX_PATH]; // Check if new Repo
  snprintf(indexPath, MAX_PATH, "%s/index", mygit);
  int readIndex = readFileData(&bufStage, &fSize, indexPath);
  if (readIndex < 0) // WARN: FILE NOT EXIST, DOESNT MEAN FAIL
    goto success;

  if (fSize < 12 || memcmp(bufStage, "DIRC", 4) != 0 || // Check file
      (readBE32(bufStage + 4) != 2)) {
    printf("Corrupt Index file, try re-init repo!");
    goto err;
  }

  // PHASE 2: START READ IF LINUS GIT
  entryNumber = readBE32(bufStage + 8); // ReadDataBlock
  entries = malloc(sizeof(stageEntry) * entryNumber);
  if (entryNumber > 0 && entries == NULL) {
    fprintf(stderr, "Out of memory\n");
    goto err;
  }

  int entryByte;
  unsigned char *moveBuf = bufStage + 12;
  for (int i = 0; i < entryNumber; i++) {
    entries[i].st_ctime = readBE32(moveBuf + 0);
    entries[i].st_mtime = readBE32(moveBuf + 8);
    entries[i].st_mode = readBE32(moveBuf + 24);
    entries[i].st_size = readBE32(moveBuf + 36);
    memcpy(entries[i].SHA1, moveBuf + 40, 20);
    strncpy(entries[i].name, (const char *)moveBuf + 62, MAX_PATH - 1);
    entries[i].nameLen = strlen(entries[i].name);

    entryByte = entries[i].nameLen + 1;
    entryByte = ((62 + entryByte + 7) / 8) * 8; // CORE: calculate entry size
    if (moveBuf + entryByte > bufStage + fSize) {
      fprintf(stderr, "Index file truncated/corrupted\n");
      goto err;
    }
    moveBuf += entryByte;

    if (flags & FLAG_VERBOSE) {
      // Print Header
      if (i == 0) {
        printf("\nINDEX  %-40s  PATH\n", "SHA-1");
        printf("---------------------------------------------------------------"
               "-----------------\n");
      }

      char shaHex[41];
      getSHA1_40Bytes(shaHex, entries[i].SHA1);
      printf("[%2d]   %s  %s\n", i, shaHex, entries[i].name);

      // Print Footer
      if (i == entryNumber - 1) {
        printf("---------------------------------------------------------------"
               "-----------------\n");
        printf("Total: %d entries.\n\n", entryNumber);
      }
    }
  }
  // PHASE 3: WRITE TO DISK OUR FILE
  entryList eList;
  eList.entries = entries;
  eList.capacity = entryNumber;
  eList.numFiles = entryNumber;
  entryList_sort(&eList);
  if (!(flags & FLAG_WRITE))
    goto success;

  if (writeStage(&eList) < 0)
    goto err;
success:
  free(bufStage);
  free(entries);
  return 0;
err:
  free(bufStage);
  free(entries);
  return -1;
}

// BEHAVIOR: Load data into 2 MODVAR
entryList *loadStage(int flags) {
  if (stgEntries.entries != NULL)
    return &stgEntries;

  unsigned char *bufStage = NULL;
  uint32_t fSize;
  stageEntry *entries = NULL;

  char stagePath[MAX_PATH]; // BUILD PATH
  char const *mygit = repoName;
  char *repoPath = findGitRepo();
  if (repoPath == NULL) {
    printf("Not a git repo yet. use \"init\" instead");
    return NULL;
  }
  snprintf(stagePath, MAX_PATH, "%s/stage", mygit);

  // READ STAGE OR INDEX
  if (readFileData(&bufStage, &fSize, stagePath) < 0) {
    if (loadIndex(FLAG_WRITE) < 0) // LOAD GIT INDEX INTO STAGE
      goto err;
    if (readFileData(&bufStage, &fSize, stagePath) < 0)
      goto success;
  }
  // AFTER STAGE EXIST READ STAGE
  unsigned char *moveBuf = bufStage; // Block Validate
  stageHeader *stgHeader = (stageHeader *)moveBuf;
  if (fSize < sizeof(stageHeader) ||
      memcmp(&stgHeader->Signature, "GRMK", 4) != 0) {
    fprintf(stderr, "stage file corrupted");
    goto err;
  }

  moveBuf += sizeof(stageHeader); // READ STAGE
  entries = malloc(sizeof(stageEntry) * stgHeader->NumberOfFiles);
  if (stgHeader->NumberOfFiles > 0 && entries == NULL) {
    fprintf(stderr, "Out of memory\n");
    goto err;
  }
  for (int i = 0; i < stgHeader->NumberOfFiles; i++) {
    entries[i] = *(stageEntry *)moveBuf;
    moveBuf += sizeof(stageEntry);

    if (flags & FLAG_VERBOSE) {
      if (i == 0) {
        printf("\nSTAGE  %-40s  PATH\n", "SHA-1");
        printf("---------------------------------------------------------------"
               "-----------------\n");
      }

      char shaHex[41];
      getSHA1_40Bytes(shaHex, entries[i].SHA1);
      printf("[%2d]   %s  %s\n", i, shaHex, entries[i].name);

      if (i == stgHeader->NumberOfFiles - 1) {
        printf("---------------------------------------------------------------"
               "-----------------\n");
        printf("Total: %d entries.\n\n", stgHeader->NumberOfFiles);
      }
    }
  }

  stgEntries.numFiles = stgHeader->NumberOfFiles;
  stgEntries.capacity =
      stgHeader->NumberOfFiles > 0 ? stgHeader->NumberOfFiles : 16;
  stgEntries.entries = entries;
success:
  free(bufStage);
  return &stgEntries;
err:
  free(bufStage);
  free(entries);
  return NULL;
}

int stage_remove(char *path) {
  if (path == NULL)
    return -1;

  findGitRepo();

  char relPath[MAX_PATH];
  if (sanitizePath(relPath, path, NULL, NULL, NULL) == NULL)
    return -1;

  return entryList_remove(&stgEntries, relPath);
}

int stage_add(char *path) {
  if (path == NULL)
    return -1;

  entryList *eList = &stgEntries;
  if (eList->capacity == 0) {
    entryList_init(eList);
  }

  stageEntry entry;
  struct _stat st;
  findGitRepo();
  unsigned char SHA1_20[20];
  char chkPath[MAX_PATH];

  if (sanitizePath(chkPath, path, NULL, NULL, NULL) == NULL) {
    printf("Invalid File");
    goto err;
  }

  const pathList *deleted = diff_getDeleted();
  if (deleted && pathList_findIndex(deleted, chkPath) >= 0) {
    stage_remove(chkPath);
    goto success;
  }

  const pathList *uchanged = diff_getUnchanged();
  if (uchanged && pathList_findIndex(uchanged, chkPath) >= 0)
    goto success;

  // IF NEW OR MODIFIED THEN ADD BLOCK
  if (_stat(chkPath, &st) < 0) { // BUILD ST
    printf("Invalid File");
    goto err;
  }
  if (hashBlob(SHA1_20, chkPath, FLAG_WRITE) == NULL)
    goto err;

  entry.st_ctime = st.st_ctime; // build st
  entry.st_mtime = st.st_mtime;
  entry.st_mode = (st.st_mode & 0111) ? 0100755 : 0100644;
  entry.st_size = st.st_size;
  entry.nameLen = strlen(chkPath);
  strcpy(entry.name, chkPath);
  memcpy(entry.SHA1, SHA1_20, 20);

  int index = entryList_findIndex(eList, chkPath);
  if (index < 0) {
    entryList_add(eList, &entry);
    entryList_sort(eList);
  } else { // if exists
    eList->entries[index] = entry;
  }

success:
  return 0;
err:
  return -1;
}

// ---------------------------------------------------------------
// COMMANDS
// ---------------------------------------------------------------
int command_stage_add(int count, const char **paths, int flags) {
  // flags: -A, -u, -v
  // paths: file or folder path
  char curBranch[MAX_PATH] = "";
  int targetType = readPOINTER(curBranch);
  if (targetType == 1 && !isGRMKBranch(curBranch)) {
    fprintf(stderr,
            "fatal: Cannot stage on git branch '%s'. grmk only allows "
            "staging on 'grmk-*' branches.\n",
            curBranch);
    return -1;
  }

  entryList *eList_tracked = loadStage(FLAG_NONE);
  if (eList_tracked == NULL)
    return -1;

  pathList added;
  pathList_init(&added);

  // Case 1: Require at least one path if neither FLAG_ALL nor FLAG_UPDATE is
  // set
  if (!(flags & (FLAG_ALL | FLAG_UPDATE)) && (count == 0 || !paths)) {
    printf("Nothing specified, nothing added.\n");
    return -1;
  }

  // Case 2: FLAG_UPDATE without paths -> stage only tracked files
  if ((flags & FLAG_UPDATE) && count == 0) {
    for (int i = (int)eList_tracked->numFiles - 1; i >= 0; i--) {
      pathList_add(&added, eList_tracked->entries[i].name);
    }
  }

  // Case 2B: FLAG_UPDATE with paths -> stage only tracked files from specified
  // paths
  if ((flags & FLAG_UPDATE) && count > 0) {
    for (int i = 0; i < count; i++) {
      char relPath[MAX_PATH];
      if (sanitizePath(relPath, (char *)paths[i], NULL, NULL, NULL) == NULL)
        goto err;

      if (entryList_findIndex(NULL, relPath) >= 0)
        pathList_add(&added, relPath);
    }
  }

  // Case 3: FLAG_ALL without paths -> stage tracked files and untracked files
  if ((flags & FLAG_ALL) && count == 0) {
    for (int i = (int)eList_tracked->numFiles - 1; i >= 0; i--) {
      pathList_add(&added, eList_tracked->entries[i].name);
    }

    const pathList *untracked = diff_getUntracked();
    if (untracked) {
      for (int i = 0; i < untracked->count; i++) {
        pathList_add(&added, untracked->arrPaths[i]);
      }
    }
  }

  // Case 3B: FLAG_ALL with paths -> stage specific paths
  if ((flags & FLAG_ALL) && count > 0) {
    for (int i = 0; i < count; i++) {
      char relPath[MAX_PATH];
      if (sanitizePath(relPath, (char *)paths[i], NULL, NULL, NULL) == NULL)
        goto err;

      pathList_add(&added, relPath);
    }
  }

  // Case 4: No FLAG_ALL or FLAG_UPDATE with paths -> stage specific paths
  if (!(flags & (FLAG_ALL | FLAG_UPDATE)) && count > 0) {
    for (int i = 0; i < count; i++) {
      pathList_add(&added, (char *)paths[i]);
    }
  }

  // start repopulate stage and write
  for (int i = 0; i < added.count; i++) {
    if (stage_add(added.arrPaths[i]) < 0) {
      printf("fatal: pathspec '%s' did not match any files\n",
             added.arrPaths[i]);
      goto err;
    }
  }
  entryList_sort(eList_tracked);
  if (writeStage(NULL) < 0)
    goto err;

  if (flags & FLAG_VERBOSE) {
    pathList_sort(&added);
    pathList_print(&added, "add '%s'\n");
  }

  pathList_free(&added);
  return 0;
err:
  pathList_free(&added);
  return -1;
}

int command_stage_remove(int count, const char **paths, int flags) {
  // flags: -A ;-f ;-v
  char curBranch[MAX_PATH] = "";
  int targetType = readPOINTER(curBranch);
  if (targetType == 1 && !isGRMKBranch(curBranch)) {
    fprintf(stderr,
            "fatal: Cannot modify stage on git branch '%s'. grmk only allows "
            "staging on 'grmk-*' branches.\n",
            curBranch);
    return -1;
  }

  entryList *eList = loadStage(FLAG_NONE);
  if (eList == NULL)
    return -1;

  pathList removed;
  pathList_init(&removed);

  // Case 1: Require at least one path if FLAG_ALL is not set
  if (!(flags & FLAG_ALL) && (count == 0 || !paths)) {
    printf("fatal: No path specified\n");
    return -1;
  }

  // Case 2: FLAG_ALL without paths -> stage all files for removal
  if ((flags & FLAG_ALL) && count == 0) {
    for (int i = 0; i < (int)eList->numFiles; i++) {
      pathList_add(&removed, eList->entries[i].name);
    }
  }

  // Case 3: FLAG_ALL with paths -> match exact file or all files in folder
  // prefix
  if ((flags & FLAG_ALL) && count > 0) {
    for (int i = 0; i < count; i++) {
      char relPath[MAX_PATH];
      if (sanitizePath(relPath, (char *)paths[i], NULL, NULL, NULL) == NULL) {
        goto err;
      }

      if (entryList_findIndex(NULL, relPath) >= 0) { // if file
        pathList_add(&removed, relPath);
      } else {
        char prefix[MAX_PATH + 2]; // if folder
        snprintf(prefix, sizeof(prefix), "%s/", relPath);
        size_t prefixLen = strlen(prefix);
        int matched = 0;

        for (int j = 0; j < (int)eList->numFiles; j++) {
          if (strncmp(eList->entries[j].name, prefix, prefixLen) == 0) {
            pathList_add(&removed, eList->entries[j].name);
            matched++;
          }
        }

        if (matched == 0) {
          printf("fatal: pathspec '%s' did not match any files\n", paths[i]);
          goto err;
        }
      }
    }
  }

  // Case 4: No FLAG_ALL with paths -> match exact files only
  if (!(flags & FLAG_ALL) && count > 0) {
    for (int i = 0; i < count; i++) {
      char relPath[MAX_PATH];
      if (sanitizePath(relPath, (char *)paths[i], NULL, NULL, NULL) == NULL) {
        goto err;
      }

      if (entryList_findIndex(NULL, relPath) < 0) {
        printf("fatal: pathspec '%s' did not match any files\n", paths[i]);
        goto err;
      }
      pathList_add(&removed, relPath);
    }
  }

  // EXECUTE: Remove all collected files from stage (and disk if FLAG_FORCE)
  for (int i = 0; i < removed.count; i++) {
    if (stage_remove(removed.arrPaths[i]) < 0)
      goto err;

    if (flags & FLAG_FORCE)
      remove(removed.arrPaths[i]);
  }
  // update stage
  entryList_sort(eList);
  if (writeStage(NULL) < 0)
    goto err;

  if (flags & (FLAG_VERBOSE | FLAG_FORCE)) {
    pathList_sort(&removed);
    pathList_print(&removed, "removed '%s'\n");
  }

  pathList_free(&removed);
  return 0;
err:
  pathList_free(&removed);
  return -1;
}

int command_stage_status(int flags) {
  // CURRENT BRANCH & HEAD COMMIT
  char currentTarget[MAX_PATH] = "";
  int targetType = readPOINTER(currentTarget);
  char headCommitSHA1[41] = "";
  if (targetType == 1) {
    readBranch(headCommitSHA1, currentTarget, 0);
  } else if (targetType == 0) {
    strncpy(headCommitSHA1, currentTarget, 40);
    headCommitSHA1[40] = '\0';
  }

  // LOAD HEAD TREE
  entryList headTree = {0};
  if (headCommitSHA1[0] != '\0') {
    char headTreeSHA1[41] = "";
    if (readCommit(headTreeSHA1, NULL, NULL, NULL, NULL,
                   (const unsigned char *)headCommitSHA1) == 0) {
      entryList_init(&headTree);
      readTree(&headTree, "", headTreeSHA1, FLAG_RECURSIVE);
    }
  }

  // STAGED CHANGES (HEAD TREE VS STAGE)
  entryList *stageList = loadStage(FLAG_NONE);
  pathList stagedAdded, stagedModified, stagedDeleted;
  pathList_init(&stagedAdded);
  pathList_init(&stagedModified);
  pathList_init(&stagedDeleted);

  diffEntryLists(&stagedAdded, &stagedModified, &stagedDeleted,
                 (headTree.entries ? &headTree : NULL), stageList);

  if (headTree.entries)
    free(headTree.entries);

  // UNSTAGED CHANGES (WORKING TREE VS STAGE)
  const pathList *modified = diff_getModified();
  const pathList *deleted = diff_getDeleted();
  const pathList *untracked = diff_getUntracked();

  if (!modified && !deleted && !untracked) {
    printf("fatal: failed to compute diff\n");
    pathList_free(&stagedAdded);
    pathList_free(&stagedModified);
    pathList_free(&stagedDeleted);
    return -1;
  }

  // SHORT FORMAT (-s)
  if (flags & FLAG_SHORT) {
    // Staged items (green in column 1)
    for (int i = 0; i < stagedAdded.count; i++) {
      printf(COLOR_GREEN "A " COLOR_RESET " %s\n", stagedAdded.arrPaths[i]);
    }
    for (int i = 0; i < stagedModified.count; i++) {
      const char *p = stagedModified.arrPaths[i];
      int isWorkModified =
          (modified && pathList_findIndex((pathList *)modified, p) >= 0);
      if (isWorkModified) {
        printf(COLOR_GREEN "M" COLOR_RED "M" COLOR_RESET " %s\n", p);
      } else {
        printf(COLOR_GREEN "M " COLOR_RESET " %s\n", p);
      }
    }
    for (int i = 0; i < stagedDeleted.count; i++) {
      printf(COLOR_GREEN "D " COLOR_RESET " %s\n", stagedDeleted.arrPaths[i]);
    }

    // Unstaged items (not already shown as MM)
    if (modified) {
      for (int i = 0; i < modified->count; i++) {
        const char *p = modified->arrPaths[i];
        if (pathList_findIndex(&stagedModified, p) < 0) {
          printf(COLOR_RED " M" COLOR_RESET " %s\n", p);
        }
      }
    }
    if (deleted) {
      for (int i = 0; i < deleted->count; i++) {
        const char *p = deleted->arrPaths[i];
        if (pathList_findIndex(&stagedDeleted, p) < 0) {
          printf(COLOR_RED " D" COLOR_RESET " %s\n", p);
        }
      }
    }
    if (untracked)
      pathList_print(untracked, COLOR_RED "??" COLOR_RESET " %s\n");

    pathList_free(&stagedAdded);
    pathList_free(&stagedModified);
    pathList_free(&stagedDeleted);
    return 0;
  }

  // FULL FORMAT
  const char *bName = (targetType == 1) ? (strrchr(currentTarget, '/')
                                               ? strrchr(currentTarget, '/') + 1
                                               : currentTarget)
                                        : "detached HEAD";
  if (targetType == 0 && currentTarget[0] != '\0') {
    printf("HEAD detached at %.7s\n", currentTarget);
  } else {
    printf("On branch %s\n", bName);
  }

  if (headCommitSHA1[0] == '\0') {
    printf("No commits yet\n");
  }

  int hasStaged = (stagedAdded.count > 0 || stagedModified.count > 0 ||
                   stagedDeleted.count > 0);
  int hasUnstaged =
      ((modified && modified->count > 0) || (deleted && deleted->count > 0));
  int hasUntracked = (untracked && untracked->count > 0);

  // STAGED CHANGES
  if (hasStaged) {
    printf("\nChanges to be committed:\n");
    printf("  (use \"grmk rm --cached <file>...\" to unstage)\n");
    pathList_print(&stagedAdded,
                   COLOR_GREEN "\tnew file:   %s" COLOR_RESET "\n");
    pathList_print(&stagedModified,
                   COLOR_GREEN "\tmodified:   %s" COLOR_RESET "\n");
    pathList_print(&stagedDeleted,
                   COLOR_GREEN "\tdeleted:    %s" COLOR_RESET "\n");
  }

  // UNSTAGED CHANGES
  if (hasUnstaged) {
    printf("\nChanges not staged for commit:\n");
    printf("  (use \"grmk add <file>...\" to update what will be committed)\n");
    if (modified && modified->count > 0)
      pathList_print(modified, COLOR_RED "\tmodified:   %s" COLOR_RESET "\n");
    if (deleted && deleted->count > 0)
      pathList_print(deleted, COLOR_RED "\tdeleted:    %s" COLOR_RESET "\n");
  }

  // UNTRACKED FILES
  if (hasUntracked) {
    printf("\nUntracked files:\n");
    printf("  (use \"grmk add <file>...\" to include in what will be "
           "committed)\n");
    pathList_print(untracked, COLOR_RED "\t%s" COLOR_RESET "\n");
  }

  // SUMMARY MESSAGE
  if (!hasStaged && !hasUnstaged && !hasUntracked) {
    printf("nothing to commit, working tree clean\n");
  } else if (!hasStaged && (hasUnstaged || hasUntracked)) {
    if (hasUnstaged) {
      printf("\nno changes added to commit (use \"grmk add\" to track)\n");
    } else {
      printf(
          "\nnothing added to commit but untracked files present (use \"grmk "
          "add\" to track)\n");
    }
  }

  pathList_free(&stagedAdded);
  pathList_free(&stagedModified);
  pathList_free(&stagedDeleted);
  return 0;
}

int command_stage_diff(const char *path) {
  const pathList *modified = diff_getModified();
  const pathList *deleted = diff_getDeleted();
  const pathList *untracked = diff_getUntracked();
  const pathList *unchanged = diff_getUnchanged();

  if (!modified && !deleted && !untracked && !unchanged) {
    printf("fatal: failed to compute diff\n");
    return -1;
  }

  if (!path) { // IF PATH SI NULL
    if (modified && modified->count > 0) {
      pathList_print(modified, "%s\n");
    }
    return 0;
  }
  char relPath[MAX_PATH]; // IF PATH AVAILABLE
  if (sanitizePath(relPath, path, NULL, NULL, NULL) == NULL) {
    printf("fatal: invalid path '%s'\n", path);
    return -1;
  }

  if (modified && pathList_findIndex(modified, relPath) >= 0) {
    printf("diff --git a/%s b/%s\n", relPath, relPath);
    printf("--- a/%s\n", relPath);
    printf("+++ b/%s\n", relPath);
    // TODO: CompareLineDiff(oldBlobData, newDiskData)
  } else if (deleted && pathList_findIndex(deleted, relPath) >= 0) {
    printf("diff --git a/%s b/%s\n", relPath, relPath);
    printf("deleted file: %s\n", relPath);
  } else if (untracked && pathList_findIndex(untracked, relPath) >= 0) {
    printf("fatal: '%s' is not tracked by git\n", relPath);
    return -1;
  } else if (unchanged && pathList_findIndex(unchanged, relPath) >= 0) {
    // Unchanged -nochange
  } else {
    printf("fatal: path '%s' not in the working tree or stage\n", relPath);
    return -1;
  }

  return 0;
}

int command_stage_sync(int flags) {
  // If no specific copy flags are provided, DEFAULT is SYNC:
  // Overwrites both stage and POINTER with git index and git HEAD
  int isDefaultSync =
      !(flags & (FLAG_INDEX | FLAG_BRANCH | FLAG_HEAD | FLAG_ALL2));

  // 1. COPY INDEX TO STAGE (run on default sync, -idx, or -a)
  if (isDefaultSync || (flags & (FLAG_INDEX | FLAG_ALL2))) {
    char stagePath[MAX_PATH];
    snprintf(stagePath, sizeof(stagePath), "%s/stage", repoName);

    // If not default sync and no -f, block overwrite
    if (!isDefaultSync && !(flags & FLAG_FORCE) &&
        _access(stagePath, 00) == 0) {
      fprintf(stderr,
              "error: stage file already exists. Use -f to overwrite.\n");
      return -1;
    }
    if (flags & FLAG_VERBOSE)
      printf("Syncing .git/index to stage...\n");

    if (loadIndex(FLAG_WRITE | flags) < 0) {
      fprintf(stderr, "fatal: failed to copy git index to stage\n");
      return -1;
    }
  }

  // 2. COPY HEAD TO POINTER (run on default sync, -H, or -a)
  if (isDefaultSync || (flags & (FLAG_HEAD | FLAG_ALL2))) {
    char pointerPath[MAX_PATH];
    snprintf(pointerPath, sizeof(pointerPath), "%s/POINTER", repoName);

    // If not default sync and no -f, block overwrite
    if (!isDefaultSync && !(flags & FLAG_FORCE) &&
        _access(pointerPath, 00) == 0) {
      fprintf(stderr, "error: POINTER already exists. Use -f to overwrite.\n");
      return -1;
    }

    char headTarget[MAX_PATH];
    int headType = readHEAD(headTarget);
    if (headType < 0) {
      fprintf(stderr, "fatal: failed to read git HEAD\n");
      return -1;
    }

    char target[MAX_PATH];
    if (headType == 1) { // Branch ref
      char *branchLeaf = strrchr(headTarget, '/');
      branchLeaf = branchLeaf ? branchLeaf + 1 : headTarget;
      strncpy(target, branchLeaf, sizeof(target));
      target[sizeof(target) - 1] = '\0';
    } else { // Detached SHA1
      strncpy(target, headTarget, sizeof(target));
      target[sizeof(target) - 1] = '\0';
    }

    if (writePOINTER(target) < 0) {
      fprintf(stderr, "fatal: failed to write POINTER\n");
      return -1;
    }

    if (flags & FLAG_VERBOSE)
      printf("Sync POINTER -> %s\n", target);
  }

  // 3. COPY BRANCH (-b or -a, not part of default sync)
  if (flags & (FLAG_BRANCH | FLAG_ALL2)) {
    char headTarget[MAX_PATH];
    if (readHEAD(headTarget) <= 0) {
      fprintf(
          stderr,
          "fatal: git is in detached HEAD or invalid, cannot copy branch\n");
      return -1;
    }

    char *branchLeaf = strrchr(headTarget, '/');
    branchLeaf = branchLeaf ? branchLeaf + 1 : headTarget;

    char finalBranch[MAX_PATH];
    if (strncmp(branchLeaf, "grmk-", 5) == 0) {
      snprintf(finalBranch, sizeof(finalBranch), "%s", branchLeaf);
    } else {
      snprintf(finalBranch, sizeof(finalBranch), "grmk-%s", branchLeaf);
    }

    char existingSHA1[41];
    if (!(flags & FLAG_FORCE) &&
        readBranch(existingSHA1, finalBranch, FLAG_NONE) == 0) {
      fprintf(stderr,
              "error: branch '%s' already exists. Use -f to overwrite.\n",
              finalBranch);
      return -1;
    }

    char commitSHA1[41] = "";
    if (readBranch(commitSHA1, headTarget, FLAG_INDEX) < 0 ||
        writeBranch(finalBranch, commitSHA1) < 0) {
      fprintf(stderr, "fatal: failed to copy branch '%s'\n", finalBranch);
      return -1;
    }

    if (flags & FLAG_VERBOSE)
      printf("Copied branch '%s' -> %.7s\n", finalBranch, commitSHA1);
  }

  return 0;
}

int command_stage_lsfiles(int flags) {
  // If user requested -idx: load git index with verbose, without writing to
  // stage
  if (flags & FLAG_INDEX) {
    if (loadIndex(FLAG_VERBOSE) < 0)
      return -1;
    return 0;
  }

  // Default (or -stg): load stage with verbose, preview only without modifying
  // disk
  if (loadStage(FLAG_VERBOSE) == NULL)
    return -1;

  return 0;
}

// ---------------------------------------------------------------
// TEST
// ---------------------------------------------------------------
#ifdef TEST_STAGE
#include <assert.h>
// AI GENERETAED
void test_loadIndex(void) {
  printf("\n=== RUNNING TEST: loadIndex ===\n");
  int res = loadIndex(FLAG_WRITE);
  assert(res == 0 && "loadIndex must succeed on valid repo index");
  printf("loadIndex success (code: %d)\n", res);

  printf("\n=== VERIFYING GENERATED STAGE FILE VIA loadStage ===\n");
  entryList *stg = loadStage(FLAG_NONE);
  assert(stg != NULL && "loadStage must successfully load generated stage");
  printf("Total entries in stage: %u\n", stg->numFiles);

  for (uint32_t i = 0; i < stg->numFiles; i++) {
    printf("  [%2u] %-30s (size: %u bytes)\n", i, stg->entries[i].name,
           stg->entries[i].st_size);
  }
  printf("\n=== TEST PASSED: loadIndex and stage verification ===\n");
}

int main(void) {
  test_loadIndex();
  return 0;
}
#endif
