

#include "branch.h"
#include "objects.h"
#include "stage.h"
#include "utils.h"
#include <io.h>
#include <stdio.h>
#include <string.h>

static int parseFlag(int *outFlags, const char *arg) {
  if (!outFlags || !arg)
    return 0;

  if (strcmp(arg, "-A") == 0 || strcmp(arg, ".") == 0) {
    *outFlags |= FLAG_ALL;
    return 1;
  } else if (strcmp(arg, "-a") == 0) {
    *outFlags |= FLAG_ALL2;
    return 1;
  } else if (strcmp(arg, "-u") == 0) {
    *outFlags |= FLAG_UPDATE;
    return 1;
  } else if (strcmp(arg, "-v") == 0) {
    *outFlags |= FLAG_VERBOSE;
    return 1;
  } else if (strcmp(arg, "-f") == 0) {
    *outFlags |= FLAG_FORCE;
    return 1;
  } else if (strcmp(arg, "-s") == 0) {
    *outFlags |= FLAG_SHORT;
    return 1;
  } else if (strcmp(arg, "-w") == 0) {
    *outFlags |= FLAG_WRITE;
    return 1;
  } else if (strcmp(arg, "-idx") == 0) {
    *outFlags |= FLAG_INDEX;
    return 1;
  } else if (strcmp(arg, "-stg") == 0) {
    *outFlags |= FLAG_STAGE;
    return 1;
  } else if (strcmp(arg, "--tree") == 0) {
    *outFlags |= FLAG_TREE;
    return 1;
  } else if (strcmp(arg, "--blob") == 0) {
    *outFlags |= FLAG_BLOB;
    return 1;
  } else if (strcmp(arg, "--commit") == 0) {
    *outFlags |= FLAG_COMMIT;
    return 1;
  } else if (strcmp(arg, "-t") == 0) {
    *outFlags |= FLAG_TYPE;
    return 1;
  } else if (strcmp(arg, "-p") == 0) {
    *outFlags |= FLAG_PRETTY;
    return 1;
  } else if (strcmp(arg, "-e") == 0) {
    *outFlags |= FLAG_EXISTS;
    return 1;
  } else if (strcmp(arg, "-r") == 0) {
    *outFlags |= FLAG_RECURSIVE;
    return 1;
  } else if (strcmp(arg, "--amend") == 0) {
    *outFlags |= FLAG_AMEND;
    return 1;
  } else if (strcmp(arg, "-cr") == 0 || strcmp(arg, "--create") == 0) {
    *outFlags |= FLAG_CREATE;
    return 1;
  } else if (strcmp(arg, "-sw") == 0 || strcmp(arg, "--switch") == 0) {
    *outFlags |= FLAG_SWITCH;
    return 1;
  } else if (strcmp(arg, "-rn") == 0 || strcmp(arg, "--rename") == 0) {
    *outFlags |= FLAG_RENAME;
    return 1;
  } else if (strcmp(arg, "--remove") == 0 || strcmp(arg, "-d") == 0) {
    *outFlags |= FLAG_REMOVE;
    return 1;
  } else if (strcmp(arg, "--soft") == 0) {
    *outFlags |= FLAG_SOFT;
    return 1;
  } else if (strcmp(arg, "--mixed") == 0) {
    *outFlags |= FLAG_MIXED;
    return 1;
  } else if (strcmp(arg, "--hard") == 0) {
    *outFlags |= FLAG_HARD;
    return 1;
  } else if (strcmp(arg, "-b") == 0) {
    *outFlags |= FLAG_BRANCH;
    return 1;
  } else if (strcmp(arg, "-H") == 0) {
    *outFlags |= FLAG_HEAD;
    return 1;
  }

  return 0;
}

static void printHelp(void) {
  printf("Usage: grmk <command> [<args>]\n\n");
  printf("Available commands:\n");
  printf("  init [<directory>]  Create an empty Git repository\n");
  printf("  add <file>...       Add file contents to the index (-A, -u, -v)\n");
  printf("  rm <file>...        Remove files from working tree and index (-A, "
         "-f, -v)\n");
  printf("  status [-s]         Show the working tree status\n");
  printf("  diff [<file>]       Show changes between working tree and stage\n");
  printf("  ls-files            List tracked files (-stg, -idx)\n");
  printf("  sync                Sync git index & HEAD to grmk (-idx, -H, -b, "
         "-a, -f, -v)\n");
  printf("  hash-object         Compute object ID (--tree, --blob, --commit, "
         "-w, -v)\n");
  printf("  write-tree          Create a tree object from the current index\n");
  printf("  cat-file            Provide contents or details of repository "
         "objects (-p, -t, -s, -e)\n");
  printf("  ls-tree [-r] <tree> List the contents of a tree object\n");
  printf("  commit [-a] [--amend] -m <msg> Record changes to the repository\n");
  printf("  log [-s] [-n <count>] Show commit logs (default 10 commits)\n");
  printf(
      "  branch              Manage branches (-cr, -sw, -rn, -d, --remove)\n");
  printf("  checkout <target>   Switch branches or restore working tree\n");
  printf("  reset [<target>]    Reset current HEAD to specified state (--soft, "
         "--mixed, --hard)\n");
  printf("  -h, --help          Show this help message\n");
}

int command_init(const char *targetDir) {
  char baseDir[MAX_PATH];
  if (targetDir && targetDir[0] != '\0') {
    snprintf(baseDir, sizeof(baseDir), "%s/%s", targetDir, repoName);
  } else {
    snprintf(baseDir, sizeof(baseDir), "%s", repoName);
  }
  standardizePath(baseDir);

  char objectsDir[MAX_PATH * 2];
  char refsHeadsDir[MAX_PATH * 2];
  snprintf(objectsDir, sizeof(objectsDir), "%s/objects", baseDir);
  snprintf(refsHeadsDir, sizeof(refsHeadsDir), "%s/refs/heads", baseDir);

  if (createFolder(objectsDir) < 0 || createFolder(refsHeadsDir) < 0) {
    fprintf(stderr, "fatal: cannot create directory structure for %s\n",
            baseDir);
    return -1;
  }

  // Create default HEAD and POINTER pointing to refs/heads/grmk-master
  const char *defaultRef = "ref: refs/heads/grmk-master\n";
  char headPath[MAX_PATH * 2];
  char pointerPath[MAX_PATH * 2];
  snprintf(headPath, sizeof(headPath), "%s/HEAD", baseDir);
  snprintf(pointerPath, sizeof(pointerPath), "%s/POINTER", baseDir);

  if (_access(headPath, 00) != 0) {
    writeFileData(headPath, (const unsigned char *)defaultRef,
                  (uint32_t)strlen(defaultRef));
  }
  if (_access(pointerPath, 00) != 0) {
    writeFileData(pointerPath, (const unsigned char *)defaultRef,
                  (uint32_t)strlen(defaultRef));
  }

  char absPath[MAX_PATH];
  if (_fullpath(absPath, baseDir, MAX_PATH)) {
    standardizePath(absPath);
    printf("Initialized empty Git repository in %s/\n", absPath);
  } else {
    printf("Initialized empty Git repository in %s/\n", baseDir);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2 || strcmp(argv[1], "-h") == 0 ||
      strcmp(argv[1], "--help") == 0) {
    printHelp();
    return (argc < 2) ? 1 : 0;
  }

  const char *cmd = argv[1]; // IF  grmk add

  if (strcmp(cmd, "init") == 0) {
    const char *targetDir = (argc >= 3) ? argv[2] : NULL;
    if (command_init(targetDir) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "add") == 0) {
    if (argc < 3) {
      printf("fatal: nothing specified, nothing added.\n");
      return 1;
    }

    int flags = FLAG_NONE;
    const char *paths[MAX_PATH];
    int count = 0;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        paths[count++] = argv[i];
      }
    }

    if (command_stage_add(count, paths, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "rm") == 0) {
    int flags = FLAG_NONE;
    const char *paths[MAX_PATH];
    int count = 0;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        paths[count++] = argv[i];
      }
    }

    if (command_stage_remove(count, paths, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "status") == 0) {
    int flags = FLAG_NONE;
    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        printf("fatal: 'status' does not take path arguments: %s\n", argv[i]);
        return 1;
      }
    }
    if (command_stage_status(flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "diff") == 0) {
    const char *target = (argc >= 3) ? argv[2] : NULL;
    if (command_stage_diff(target) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "sync") == 0) {
    int flags = FLAG_NONE;
    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        printf("fatal: 'sync' does not take path arguments: %s\n", argv[i]);
        return 1;
      }
    }
    if (command_stage_sync(flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "ls-files") == 0) {
    int flags = FLAG_NONE;
    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        printf("fatal: 'ls-files' does not take path arguments: %s\n", argv[i]);
        return 1;
      }
    }
    if (command_stage_lsfiles(flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "hash-object") == 0) {
    int flags = FLAG_VERBOSE;
    const char *paths[MAX_PATH];
    int count = 0;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        paths[count++] = argv[i];
      }
    }

    if (command_object_hashobject(count, paths, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "write-tree") == 0) {
    int flags =
        FLAG_WRITE; // default write like git write-tree, or can parse -v/-w

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        printf("fatal: 'write-tree' does not take path arguments: %s\n",
               argv[i]);
        return 1;
      }
    }

    if (command_object_writeTree(flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "cat-file") == 0) {
    int flags = FLAG_NONE;
    const char *sha = NULL;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        if (!sha) {
          sha = argv[i];
        } else {
          printf("fatal: only one object name allowed: %s\n", argv[i]);
          return 1;
        }
      }
    }

    if (!sha) {
      printf("fatal: object name required\n");
      return 1;
    }

    if (command_object_catFile(sha, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "ls-tree") == 0) {
    int flags = FLAG_NONE;
    const char *sha = NULL;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        if (!sha) {
          sha = argv[i];
        } else {
          printf("fatal: only one object name allowed: %s\n", argv[i]);
          return 1;
        }
      }
    }

    if (command_object_lsTree(sha, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "commit") == 0) {
    int flags = FLAG_NONE;
    const char *message = NULL;

    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "-m") == 0) {
        if (i + 1 < argc) {
          message = argv[++i];
        } else {
          printf("error: switch 'm' requires a value\n");
          return 1;
        }
      } else if (strcmp(argv[i], "-am") == 0 || strcmp(argv[i], "-ma") == 0) {
        flags |= FLAG_ALL2;
        if (i + 1 < argc) {
          message = argv[++i];
        } else {
          printf("error: switch 'm' requires a value\n");
          return 1;
        }
      } else if (!parseFlag(&flags, argv[i])) {
        printf("fatal: unrecognized argument: %s\n", argv[i]);
        return 1;
      }
    }

    if (!message || message[0] == '\0') {
      printf("fatal: commit message required (-m <message>)\n");
      return 1;
    }

    if (command_object_commit(message, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "branch") == 0) {
    int flags = FLAG_NONE;
    const char *branches[MAX_PATH];
    int count = 0;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        branches[count++] = argv[i];
      }
    }

    if (command_branch(count, branches, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "checkout") == 0) {
    int flags = FLAG_NONE;
    const char *target = NULL;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        if (!target) {
          target = argv[i];
        } else {
          printf("fatal: only one target allowed for checkout: %s\n", argv[i]);
          return 1;
        }
      }
    }

    if (!target) {
      printf("fatal: you must specify a branch or commit to checkout\n");
      return 1;
    }

    if (command_branch_checkout(target, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "reset") == 0) {
    int flags = FLAG_NONE;
    const char *target = NULL;

    for (int i = 2; i < argc; i++) {
      if (!parseFlag(&flags, argv[i])) {
        if (!target) {
          target = argv[i];
        } else {
          printf("fatal: only one target allowed for reset: %s\n", argv[i]);
          return 1;
        }
      }
    }

    if (command_branch_reset(target, flags) < 0) {
      return 1;
    }
    return 0;
  }

  if (strcmp(cmd, "log") == 0) {
    int flags = FLAG_NONE;
    int countLimit = 10;

    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
        countLimit = atoi(argv[++i]);
        if (countLimit <= 0)
          countLimit = 10;
      } else if (!parseFlag(&flags, argv[i])) {
        printf("fatal: 'log' does not take path arguments: %s\n", argv[i]);
        return 1;
      }
    }

    if (command_object_log(countLimit, flags) < 0) {
      return 1;
    }
    return 0;
  }

  printf("grmk: '%s' is not a grmk command.\n", cmd);
  return 1;
}
