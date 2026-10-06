#pragma once

enum CommandFlags {
  FLAG_NONE    = 0,
  FLAG_UPDATE  = 1 << 0, // -u
  FLAG_ALL     = 1 << 1, // -A, .
  FLAG_VERBOSE = 1 << 2, // -v
  FLAG_FORCE   = 1 << 3, // -f
  FLAG_WRITE   = 1 << 4, // -w
  FLAG_SHORT   = 1 << 5, // -s
  FLAG_INDEX   = 1 << 6, // -idx
  FLAG_STAGE   = 1 << 7, // -stg
  FLAG_TREE    = 1 << 8, // --tree
  FLAG_BLOB    = 1 << 9, // --blob
  FLAG_COMMIT  = 1 << 10, // --commit
  FLAG_RECURSIVE = 1 << 11, // -r
  FLAG_TYPE      = 1 << 12, // -t
  FLAG_PRETTY    = 1 << 13, // -p
  FLAG_EXISTS    = 1 << 14, // -e
  FLAG_ALL2      = 1 << 15, // -a
  FLAG_AMEND     = 1 << 16, // --amend
  FLAG_CREATE    = 1 << 17, // -cr, --create
  FLAG_SWITCH    = 1 << 18, // -sw, --switch
  FLAG_RENAME    = 1 << 19, // -rn, --rename
  FLAG_REMOVE    = 1 << 20, // --remove, -rm, -d
  FLAG_SOFT      = 1 << 21, // --soft
  FLAG_MIXED     = 1 << 22, // --mixed
  FLAG_HARD      = 1 << 23, // --hard
  FLAG_BRANCH    = 1 << 24, // -b
  FLAG_HEAD      = 1 << 25  // -H
};

#define FLAG_SIZE FLAG_SHORT

#define STAGE_ADD_NONE    FLAG_NONE
#define STAGE_ADD_UPDATE  FLAG_UPDATE
#define STAGE_ADD_ALL     FLAG_ALL
#define STAGE_ADD_VERBOSE FLAG_VERBOSE
#define STAGE_RM_NONE     FLAG_NONE
#define STAGE_RM_FORCE    FLAG_FORCE
#define STAGE_RM_VERBOSE  FLAG_VERBOSE

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"

