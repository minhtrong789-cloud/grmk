#include <Windows.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "branch.h"
#include "diff.h"
#include "flags.h"
#include "objects.h"
#include "stage.h"
#include "utils.h"

/*
GIT INFO:

HEAD format: ref: refs/heads/master

git branch                        (list branches, mark current)
git branch <name>                 (create branch from HEAD)
git branch <name> <start-point>   (create branch from commit/tag)
git branch -d <name>              (safe delete branch)
git branch -D <name>              (force delete branch)
git branch -m [<old>] <new>       (rename branch)
git checkout <branch>             (switch to branch)
git checkout -b <name>            (create and switch)
git checkout <commit>             (detached HEAD at commit)

GRMK IMPLEMENTED FEATURES:
[x] grmk branch                             (list branches, mark current)
[x] grmk branch [<name>] / -sw <name>       (switch to branch)
[x] grmk branch -cr <name> [<SHA1>]         (create branch from HEAD or commit)
[x] grmk branch -cr -sw <name> [<SHA1>]     (create and switch to branch)
[x] grmk branch -rn [<old>] <new>           (rename branch)
[x] grmk branch --remove / -d <name>        (delete branch, cannot del current)
[x] grmk checkout <SHA1 | branch>           (switch to branch or detached HEAD)
[x] grmk reset [--soft|--mixed|--hard] [<target>] (move HEAD, reset
stage/working tree)


*/

// NOTE:
// - Auto parse input and add prefix sign(grmk-) when creating new branch
// - Lock interacting with git branch for data protection

const char *sign = "grmk-";
const char *branchFolder = ".git/refs/heads";
const char *head = ".git/POINTER";

// ---------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------
// VALIDATE: prevent commit to the wrong branch
int isGRMKBranch(char *brelPath) {
  if (!brelPath)
    return -1;

  standardizePath(brelPath);
  char *name = strrchr(brelPath, '/');
  name = name ? name + 1 : brelPath;

  return (strncmp(name, sign, strlen(sign)) == 0) ? 1 : 0;
}

// Restore working tree and stage from currStage to targetList
int restoreRepo(const entryList *currStage, entryList *targetList) {
  if (!findGitRepo() || !currStage || !targetList)
    return -1;

  // DIFF CURRENT STAGE VS TARGET TREE
  pathList toAdd, toModify, toDelete;
  pathList_init(&toAdd);
  pathList_init(&toModify);
  pathList_init(&toDelete);

  // SAFETY CHECK: Ensure working tree has no uncommitted modifications
  const pathList *mod = diff_getModified();
  const pathList *del = diff_getDeleted();
  if ((mod && mod->count > 0) || (del && del->count > 0)) {
    fprintf(stderr,
            "error: Your local changes would be overwritten by checkout.\n");
    fprintf(stderr, "Please commit your changes before you switch branches.\n");
    goto err;
  }

  diffEntryLists(&toAdd, &toModify, &toDelete, currStage, targetList);

  // SAFETY CHECK: Prevent overwriting untracked working tree files
  const pathList *unt = diff_getUntracked();
  if (unt) {
    for (int i = 0; i < toAdd.count; i++) {
      if (pathList_findIndex(unt, toAdd.arrPaths[i]) >= 0) {
        fprintf(stderr,
                "error: untracked file '%s' would be overwritten by checkout\n",
                toAdd.arrPaths[i]);
        fprintf(stderr,
                "Please move or remove it before you switch branches.\n");
        goto err;
      }
    }
  }

  // APPLY DELETES
  for (int i = 0; i < toDelete.count; i++) {
    const char *delPath = toDelete.arrPaths[i];
    remove(delPath);

    char dirBuf[MAX_PATH];
    snprintf(dirBuf, sizeof(dirBuf), "%s", delPath);
    char *slash = strrchr(dirBuf, '/');
    while (slash) {
      *slash = '\0';
      if (_rmdir(dirBuf) != 0)
        break;
      slash = strrchr(dirBuf, '/');
    }
  }

  // APPLY WRITES (ADD & MODIFY)
  for (uint32_t i = 0; i < targetList->numFiles; i++) {
    stageEntry *entry = &targetList->entries[i];
    int isAdd = (pathList_findIndex(&toAdd, entry->name) >= 0);
    int isMod = (pathList_findIndex(&toModify, entry->name) >= 0);

    if (!isAdd && !isMod)
      continue;

    char blobSHA40[41];
    getSHA1_40Bytes(blobSHA40, entry->SHA1);
    unsigned char *blobData = NULL;
    uint32_t blobSize = 0;

    if (readBlob(&blobData, &blobSize, (const unsigned char *)blobSHA40) < 0 ||
        !blobData) {
      fprintf(stderr, "fatal: unable to read blob object: %s\n", blobSHA40);
      goto err;
    }

    if (writeFileData(entry->name, blobData, blobSize) < 0) {
      fprintf(stderr, "fatal: unable to write file: %s\n", entry->name);
      free(blobData);
      goto err;
    }
    free(blobData);

    // Sync stat so working tree is clean
    struct _stat st;
    if (_stat(entry->name, &st) == 0) {
      entry->st_mtime = (uint32_t)st.st_mtime;
      entry->st_ctime = (uint32_t)st.st_ctime;
      entry->st_size = (uint32_t)st.st_size;
      entry->st_mode = (st.st_mode & 0111) ? 0100755 : 0100644;
    }
  }

  // UPDATE STAGE
  if (writeStage(targetList) < 0) {
    fprintf(stderr, "fatal: failed to update stage\n");
    goto err;
  }

  pathList_free(&toAdd);
  pathList_free(&toModify);
  pathList_free(&toDelete);
  return 0;

err:
  pathList_free(&toAdd);
  pathList_free(&toModify);
  pathList_free(&toDelete);
  return -1;
}

// VALIDATE: in name or path -> validate and build fullpath
char *buildPath(char *outPath, char *branch, int isGrmk) {
  if (!outPath || !branch)
    return NULL;

  const char *p = branch; // NAMING ERROR
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    p++;

  if (*p == '\0')
    return NULL;

  standardizePath(branch);
  char path[MAX_PATH];

  if (isGrmk) {
    if (!strchr(branch, '/')) {
      if (strncmp(branch, sign, strlen(sign)) != 0)
        snprintf(path, sizeof(path), "%s/%s%s", branchFolder, sign, branch);
      else
        snprintf(path, sizeof(path), "%s/%s", branchFolder, branch);
    } else {
      strncpy(path, branch, sizeof(path) - 1);
    }
  } else {
    if (!strchr(branch, '/'))
      snprintf(path, sizeof(path), "%s/%s", branchFolder, branch);
    else
      strncpy(path, branch, sizeof(path) - 1);
  }

  // VALIDATE BLOCK
  int prefixLen = strlen(branchFolder);
  if (strncmp(path, branchFolder, prefixLen) != 0)
    return NULL;
  if (path[prefixLen] != '/')
    return NULL;
  if (strchr(path + prefixLen + 1, '/'))
    return NULL;

  strncpy(outPath, path, MAX_PATH);
  return outPath;
}

// COMMIT/RESET FEATURE
int writeBranch(char *branchPath, const char *newSHA1) {
  if (!branchPath || !newSHA1)
    return -1;

  if (!isGRMKBranch(branchPath)) {
    fprintf(stderr,
            "fatal: Cannot modify git branch '%s'. grmk only allows "
            "modifying 'grmk-*' branches.\n",
            branchPath);
    return -1;
  }

  char path[MAX_PATH];
  if (!buildPath(path, branchPath, 1))
    return -1;

  // UPDATE BRANCH SHA1
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;

  fprintf(f, "%.40s\n", newSHA1);
  fclose(f);
  return 0;

  // TODO: WRITE TO RELOG for relog feature
}

// SUPPORT SWITCH BRANCH
int readBranch(char *outSHA1, const char *branchName, int flags) {
  if (!outSHA1 || !branchName)
    return -1;

  FILE *f = NULL;
  char path[MAX_PATH];
  int isGrmk = (flags & FLAG_INDEX) ? 0 : 1;
  if (!buildPath(path, (char *)branchName, isGrmk) || _access(path, 00) != 0)
    goto err;

  f = fopen(path, "r");
  if (!f)
    goto err;

  if (!fgets(outSHA1, 41, f))
    goto err;

  fclose(f);
  outSHA1[strcspn(outSHA1, "\r\n")] = '\0';
  return 0;

err:
  if (f)
    fclose(f);
  outSHA1[0] = '\0';
  return -1;
}
// Ref (HEAD / POINTER) Operations
// isHead=1 -> ".git/HEAD", isHead=0 -> ".git/POINTER" (and syncs to .git/HEAD)
int writeRef(const char *target, int isHead) {
  if (!target)
    return -1;

  const char *refFile = isHead ? ".git/HEAD" : head;
  FILE *f = fopen(refFile, "wb");
  if (!f)
    return -1;

  if (strlen(target) == 40 && !strchr(target, '/') && !strchr(target, ' ')) {
    // Detached HEAD (SHA1)
    fprintf(f, "%s\n", target);
  } else {
    // Branch ref
    const char *name = strrchr(target, '/');
    name = name ? name + 1 : target;
    fprintf(f, "ref: refs/heads/%s\n", name);
  }

  fclose(f);

  // Sync git's .git/HEAD whenever POINTER is updated (if not already writing
  // HEAD)
  if (!isHead) {
    writeRef(target, 1);
  }

  return 0;
}

// return 1 if branch (outTarget = ".git/refs/heads/..."), 0 if detached
// (outTarget = SHA1), -1 on error
int readRef(char *outTarget, int isHead) {
  if (!findGitRepo() || !outTarget)
    return -1;

  const char *refFile = isHead ? ".git/HEAD" : head;
  FILE *f = fopen(refFile, "r");
  if (!f) {
    if (errno == ENOENT)
      printf("Corrupted %s file\n", isHead ? "HEAD" : "POINTER");
    return -1;
  }

  char buf[MAX_PATH];
  if (!fgets(buf, sizeof(buf), f)) {
    fclose(f);
    return -1;
  }
  fclose(f);

  buf[strcspn(buf, "\r\n")] = '\0';

  if (strncmp(buf, "ref: ", 5) == 0) {
    snprintf(outTarget, MAX_PATH, ".git/%s", buf + 5);
    return 1;
  } else {
    strcpy(outTarget, buf);
    return 0;
  }
}

// FEATURE: grmk branch - LIST OF BRANCH NAMES
int branch_printList(void) {
  if (!findGitRepo())
    return -1;

  pathList list;
  pathList_init(&list);

  char lookup[MAX_PATH];
  snprintf(lookup, sizeof(lookup), "%s/*", branchFolder);

  struct __finddata64_t fd;
  intptr_t fHandler = _findfirst64(lookup, &fd);
  if (fHandler >= 0) {
    do {
      if (fd.attrib & _A_SUBDIR)
        continue;

      pathList_add(&list, fd.name);
    } while (0 == _findnext64(fHandler, &fd));
    _findclose(fHandler);
    pathList_sort(&list);
  }

  char currentTarget[MAX_PATH] = "";
  int targetType = readPOINTER(currentTarget);

  if (targetType == 0 && currentTarget[0] != '\0') {
    // Detached HEAD
    printf("%s* (HEAD detached at %.7s)%s\n", COLOR_GREEN, currentTarget,
           COLOR_RESET);
  }

  const char *currentBranchName = "";
  if (targetType == 1) {
    currentBranchName = strrchr(currentTarget, '/');
    currentBranchName =
        currentBranchName ? currentBranchName + 1 : currentTarget;
  }

  for (int i = 0; i < list.count; i++) {
    const char *bName = list.arrPaths[i];
    if (targetType == 1 && strcmp(bName, currentBranchName) == 0) {
      printf("%s* %s%s\n", COLOR_GREEN, bName, COLOR_RESET);
    } else {
      printf("  %s\n", bName);
    }
  }

  pathList_free(&list);
  return 0;
}

int branch_switch(const char *branchName, int flags) {
  if (!findGitRepo() || !branchName)
    return -1;

  char branchPath[MAX_PATH];
  // Try direct branch first (e.g. git branch like 'master' or already prefixed
  // 'grmk-*')
  if (!buildPath(branchPath, (char *)branchName, 0) ||
      _access(branchPath, 00) != 0) {
    // If not found, try with grmk- prefix
    if (!buildPath(branchPath, (char *)branchName, 1) ||
        _access(branchPath, 00) != 0) {
      fprintf(stderr,
              "error: pathspec '%s' did not match any file(s) known to git\n",
              branchName);
      return -1;
    }
  }

  char targetSHA1[41];
  if (readBranch(targetSHA1, branchPath, FLAG_INDEX) < 0)
    return -1;

  char targetTreeSHA1[41] = "";
  if (readCommit(targetTreeSHA1, NULL, NULL, NULL, NULL,
                 (const unsigned char *)targetSHA1) < 0) {
    strncpy(targetTreeSHA1, targetSHA1, 40);
    targetTreeSHA1[40] = '\0';
  }

  entryList targetList = {0};
  entryList_init(&targetList);
  if (readTree(&targetList, "", targetTreeSHA1, FLAG_RECURSIVE) < 0) {
    fprintf(stderr, "fatal: failed to read tree object: %s\n", targetTreeSHA1);
    goto cleanup;
  }

  entryList *currStage = loadStage(FLAG_NONE);
  if (restoreRepo(currStage, &targetList) < 0) {
    fprintf(stderr, "fatal: failed to restore repo to commit %s\n", targetSHA1);
    goto cleanup;
  }

  if (writePOINTER(branchPath) < 0)
    goto cleanup;

  entryList_free(&targetList);
  return 0;

cleanup:
  entryList_free(&targetList);
  return -1;
}

int branch_create(const char *newBranch, char *inSHA1_40) {
  if (!findGitRepo() || !newBranch || !inSHA1_40)
    return -1;

  char path[MAX_PATH];
  if (!buildPath(path, (char *)newBranch, 1)) {
    fprintf(stderr, "fatal: '%s' is not a valid branch name.\n", newBranch);
    return -1;
  }

  if (_access(path, 00) == 0) {
    printf("branch exists\n");
    return -1;
  }
  FILE *f = fopen(path, "wb");
  if (!f) {
    return -1;
  }

  fprintf(f, "%.40s\n", inSHA1_40);
  fclose(f);
  return 0;
}

int branch_remove(char *inBranch) {
  if (!findGitRepo() || !inBranch)
    return -1;

  char path[MAX_PATH];
  if (!buildPath(path, inBranch, 1)) {
    fprintf(stderr, "fatal: '%s' is not a valid branch name.\n", inBranch);
    return -1;
  }

  if (_access(path, 00) != 0) {
    fprintf(stderr, "error: branch '%s' not found.\n", inBranch);
    return -1;
  }

  if (!isGRMKBranch(path)) {
    fprintf(stderr, "fatal: '%s' is not a grmk branch. Cannot remove.\n",
            inBranch);
    return -1;
  }

  char curHead[MAX_PATH];
  int targetType = readPOINTER(curHead);
  if (targetType == 1 && strcmp(path, curHead) == 0) {
    fprintf(stderr, "error: Cannot delete branch '%s' checked out at '%s'\n",
            inBranch, path);
    return -1;
  }

  if (remove(path) != 0) {
    fprintf(stderr, "fatal: failed to delete branch '%s'.\n", inBranch);
    return -1;
  }

  printf("Deleted branch %s.\n", inBranch);
  return 0;
}

int branch_rename(char *inOldPath, char *inNewPath) {
  // check if missing newpath, check if newpath conflict
  // check if is exist inpath. NULL then readPOITER

  // check if is grmk path, yes -> ok to interact
  // get rename fullpath with grmk- then rename it.
  // if curBranch- > update POINTER

  if (!findGitRepo() || !inNewPath)
    return -1;

  char newPath[MAX_PATH];
  if (!buildPath(newPath, inNewPath, 1)) {
    fprintf(stderr, "fatal: '%s' is not a valid branch name.\n", inNewPath);
    return -1;
  }
  if (_access(newPath, 00) == 0) {
    fprintf(stderr, "fatal: a branch named '%s' already exists.\n", inNewPath);
    return -1;
  }
  char curHead[MAX_PATH];
  int headType = readPOINTER(curHead);
  if (headType == 0) {
    fprintf(stderr, "fatal: cannot rename branch in detached HEAD state.\n");
    return -1;
  }
  if (headType < 0) {
    return -1;
  }

  char *targetBranch = inOldPath ? inOldPath : curHead;
  if (!isGRMKBranch(targetBranch)) {
    fprintf(stderr, "fatal: '%s' is not a grmk branch. Cannot rename.\n",
            targetBranch);
    return -1;
  }
  char oldPath[MAX_PATH];
  if (!buildPath(oldPath, targetBranch, 1) || _access(oldPath, 00) != 0) {
    fprintf(stderr, "fatal: branch '%s' does not exist.\n", targetBranch);
    return -1;
  }

  if (rename(oldPath, newPath) != 0) {
    fprintf(stderr, "fatal: failed to rename branch file.\n");
    return -1;
  }

  // Update POINTER if renaming current branch
  if (!inOldPath || strcmp(oldPath, curHead) == 0) {
    writePOINTER(newPath);
  }

  return 0;
}

int command_branch(int count, const char **branches, int flags) {
  if (count == 0 && (flags == FLAG_NONE)) {
    return branch_printList();
  }

  // If user passes 1 branch without flags, treat as implicit switch
  if (count == 1 && flags == FLAG_NONE) {
    flags |= FLAG_SWITCH;
  }

  if (flags & (FLAG_CREATE | FLAG_SWITCH)) {
    // validate
    if (count == 0 || !branches || !branches[0]) {
      fprintf(stderr, "fatal: branch name required\n");
      return -1;
    }
    if ((flags & FLAG_CREATE) && count > 2) {
      fprintf(stderr, "fatal: too many arguments: %s\n", branches[2]);
      return -1;
    }
    if (!(flags & FLAG_CREATE) && count > 1) {
      fprintf(stderr, "fatal: too many arguments: %s\n", branches[1]);
      return -1;
    }

    const char *bName = branches[0];

    // 1. CREATE FIRST (if FLAG_CREATE)
    if (flags & FLAG_CREATE) {
      char startSHA1_40[41] = "";

      // Custom start-point provided (branches[1])
      if (count == 2 && branches[1]) {
        char bPath[MAX_PATH];
        // Try exact branch name first (e.g. master), then grmk- prefixed branch
        if ((buildPath(bPath, (char *)branches[1], 0) &&
             _access(bPath, 00) == 0) ||
            (buildPath(bPath, (char *)branches[1], 1) &&
             _access(bPath, 00) == 0)) {
          if (readBranch(startSHA1_40, bPath, 0) < 0)
            return -1;
        } else { // Fallback to SHA1
          strncpy(startSHA1_40, branches[1], 40);
          startSHA1_40[40] = '\0';
        }
      } else { // INJECT NULL: DEFAULT TO HEAD
        char curHead[MAX_PATH];
        int headType = readPOINTER(curHead);

        if (headType == 1) {
          // HEAD is branch -> read SHA1 from branch file
          if (readBranch(startSHA1_40, curHead, 0) < 0 ||
              startSHA1_40[0] == '\0') {
            fprintf(stderr, "fatal: not a valid object name: '%s'\n", curHead);
            return -1;
          }
        } else if (headType == 0) {
          // HEAD is detached -> curHead is already commit SHA1
          strncpy(startSHA1_40, curHead, 40);
          startSHA1_40[40] = '\0';
        } else {
          fprintf(stderr, "fatal: failed to read HEAD\n");
          return -1;
        }
      }

      if (branch_create(bName, startSHA1_40) < 0)
        return -1;

      if (!(flags & FLAG_SWITCH)) {
        printf("Created branch '%s%s' at %.7s\n", sign, bName, startSHA1_40);
      }
    }

    // 2. SWITCH SECOND (if FLAG_SWITCH)
    if (flags & FLAG_SWITCH) {
      if (branch_switch(bName, flags) < 0)
        return -1;

      if (flags & FLAG_CREATE) {
        printf("Switched to a new branch '%s%s'\n", sign, bName);
      } else {
        printf("Switched to branch '%s'\n", bName);
      }
    }

    return 0;
  }

  // 3. RENAME (-rn [<old>] <new>)
  if (flags & FLAG_RENAME) {
    if (count == 0 || !branches || !branches[0]) {
      fprintf(stderr, "fatal: branch name required for rename\n");
      return -1;
    }
    if (count > 2) {
      fprintf(stderr, "fatal: too many arguments for rename\n");
      return -1;
    }

    char *oldName = (count == 2) ? (char *)branches[0] : NULL;
    char *newName = (count == 2) ? (char *)branches[1] : (char *)branches[0];

    if (branch_rename(oldName, newName) < 0)
      return -1;

    if (oldName)
      printf("Renamed branch '%s' to '%s%s'\n", oldName, sign, newName);
    else
      printf("Renamed current branch to '%s%s'\n", sign, newName);

    return 0;
  }

  // 4. REMOVE (--remove <name> or -d <name>)
  if (flags & FLAG_REMOVE) {
    if (count == 0 || !branches || !branches[0]) {
      fprintf(stderr, "fatal: branch name required for remove\n");
      return -1;
    }
    if (count > 1) {
      fprintf(stderr, "fatal: too many arguments for remove: %s\n",
              branches[1]);
      return -1;
    }

    if (branch_remove((char *)branches[0]) < 0)
      return -1;

    return 0;
  }

  fprintf(stderr, "fatal: unrecognized branch options\n");
  return -1;
}

int command_branch_checkout(const char *target, int flags) {
  if (!findGitRepo() || !target || target[0] == '\0') {
    fprintf(stderr, "fatal: target required for checkout\n");
    return -1;
  }

  char targetSHA1[41] = "";
  char pointerRef[MAX_PATH] = "";
  int isBranch = 0;

  // 1. Check if target is a branch name
  char branchPath[MAX_PATH];
  if ((buildPath(branchPath, (char *)target, 0) &&
       _access(branchPath, 00) == 0) ||
      (buildPath(branchPath, (char *)target, 1) &&
       _access(branchPath, 00) == 0)) {
    if (readBranch(targetSHA1, branchPath, FLAG_INDEX) == 0) {
      strncpy(pointerRef, branchPath, sizeof(pointerRef) - 1);
      isBranch = 1;
    }
  }

  // 2. If not a branch, check if target is a valid commit object (detached
  // HEAD)
  if (!isBranch) {
    char testTree[41] = "";
    if (readCommit(testTree, NULL, NULL, NULL, NULL,
                   (const unsigned char *)target) == 0) {
      strncpy(targetSHA1, target, 40);
      targetSHA1[40] = '\0';
      strncpy(pointerRef, targetSHA1, sizeof(pointerRef) - 1);
    } else {
      fprintf(stderr,
              "error: pathspec '%s' did not match any branch or commit known "
              "to git\n",
              target);
      return -1;
    }
  }

  // 3. Resolve tree SHA1 from commit
  char targetTreeSHA1[41] = "";
  if (readCommit(targetTreeSHA1, NULL, NULL, NULL, NULL,
                 (const unsigned char *)targetSHA1) < 0) {
    strncpy(targetTreeSHA1, targetSHA1, 40);
    targetTreeSHA1[40] = '\0';
  }

  // 4. Load target tree & current stage, then restore
  entryList targetList = {0};
  entryList_init(&targetList);
  if (readTree(&targetList, "", targetTreeSHA1, FLAG_RECURSIVE) < 0) {
    fprintf(stderr, "fatal: failed to read tree object: %s\n", targetTreeSHA1);
    goto cleanup;
  }

  entryList *currStage = loadStage(FLAG_NONE);
  if (restoreRepo(currStage, &targetList) < 0) {
    fprintf(stderr, "fatal: failed to restore repo to %s\n", targetSHA1);
    goto cleanup;
  }

  // 5. Update POINTER
  if (writePOINTER(pointerRef) < 0) {
    fprintf(stderr, "fatal: failed to update POINTER\n");
    goto cleanup;
  }

  entryList_free(&targetList);

  if (isBranch) {
    printf("Switched to branch '%s'\n", target);
  } else {
    printf("Note: switching to '%.7s' (detached HEAD)\n", targetSHA1);
  }

  return 0;

cleanup:
  entryList_free(&targetList);
  return -1;
}

int command_branch_reset(const char *target, int flags) {
  if (!findGitRepo())
    return -1;

  char curHead[MAX_PATH];
  int headType = readPOINTER(curHead);
  if (headType < 0) {
    fprintf(stderr, "fatal: failed to read HEAD\n");
    return -1;
  }

  if (headType == 1 && !isGRMKBranch(curHead)) {
    fprintf(stderr,
            "fatal: Cannot reset git branch '%s'. grmk only allows "
            "resetting 'grmk-*' branches.\n",
            curHead);
    return -1;
  }

  // CHECK INPUT
  char targetCommitSHA1[41] = "";
  if (target && target[0] != '\0') {
    char bPath[MAX_PATH];
    // Try exact branch name first, then grmk- prefixed branch
    if ((buildPath(bPath, (char *)target, 0) && _access(bPath, 00) == 0) ||
        (buildPath(bPath, (char *)target, 1) && _access(bPath, 00) == 0)) {
      if (readBranch(targetCommitSHA1, bPath, 0) < 0)
        return -1;
    } else { // Fallback to SHA1
      strncpy(targetCommitSHA1, target, 40);
      targetCommitSHA1[40] = '\0';
    }
  } else { // IF NULL INPUT
    // Default to HEAD commit
    if (headType == 1) { // HEAD IS BRANCH
      if (readBranch(targetCommitSHA1, curHead, 0) < 0)
        return -1;
    } else { // HEAD IS SHA1
      strncpy(targetCommitSHA1, curHead, 40);
      targetCommitSHA1[40] = '\0';
    }
  }

  // Verify commit object exists and get tree SHA1
  char targetTreeSHA1[41] = "";
  if (readCommit(targetTreeSHA1, NULL, NULL, NULL, NULL,
                 (const unsigned char *)targetCommitSHA1) < 0) {
    fprintf(stderr, "fatal: could not parse commit: %s\n", targetCommitSHA1);
    return -1;
  }

  // Load TREE WHEN HAVING TREE SHA
  entryList targetList = {0};
  entryList_init(&targetList);
  if (readTree(&targetList, "", targetTreeSHA1, FLAG_RECURSIVE) < 0) {
    fprintf(stderr, "fatal: failed to read tree object: %s\n", targetTreeSHA1);
    goto cleanup;
  }

  // 2. HARD RESET (--hard): Restore working tree & stage
  if (flags & FLAG_HARD) {
    entryList *currStage = loadStage(FLAG_NONE);
    if (restoreRepo(currStage, &targetList) < 0) {
      fprintf(stderr, "fatal: failed to restore working tree\n");
      goto cleanup;
    }
  }
  // 3. MIXED RESET (default if not --soft): Update stage only
  else if (!(flags & FLAG_SOFT)) {
    if (writeStage(&targetList) < 0) {
      fprintf(stderr, "fatal: failed to update stage\n");
      goto cleanup;
    }
  }

  // 4. Update branch ref or POINTER (move pointer)
  if (headType == 1) {
    if (writeBranch(curHead, targetCommitSHA1) < 0) {
      fprintf(stderr, "fatal: failed to update branch ref: %s\n", curHead);
      goto cleanup;
    }
  } else {
    if (writePOINTER(targetCommitSHA1) < 0) {
      fprintf(stderr, "fatal: failed to update POINTER\n");
      goto cleanup;
    }
  }

  entryList_free(&targetList);

  if (headType == 0) {
    printf("HEAD is detached at %.7s\n", targetCommitSHA1);
  }

  if (flags & FLAG_HARD)
    printf("HEAD is now at %.7s (hard)\n", targetCommitSHA1);
  else if (flags & FLAG_SOFT)
    printf("HEAD is now at %.7s (soft)\n", targetCommitSHA1);
  else
    printf("Unstaged changes after reset (mixed):\nHEAD is now at %.7s\n",
           targetCommitSHA1);

  return 0;

cleanup:
  entryList_free(&targetList);
  return -1;
}
