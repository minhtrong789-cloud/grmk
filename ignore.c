

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ignore.h"
#include "utils.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// ============================================================================
// 0. Negation:       !pattern     (unignore)
// 1. Anywhere exact: name         |  dir/
// 2. Anywhere wild:  *.c, temp*   |  test*/
// 3. Root exact:     /name        |  /dir/
// 4. Root wild:      /*.c, /path* |  /test*/
// (trailing '/' means folder only)
// ============================================================================
enum RuleFlags {
  RULE_NONE = 0,
  RULE_FOLDER = 1 << 0,  // can match folder
  RULE_FILE = 1 << 1,    // can match file
  RULE_ROOT = 1 << 2,    // any '/' between
  RULE_DYNAMIC = 1 << 3, // have *
  RULE_NEGATED = 1 << 4  // have !
};

static pathList *ruleList = NULL;

int checkRules(char *outPath, const char *inPath) {
  if (!inPath || !outPath)
    return -1;
  uint8_t flags = 0;
  int countSnow = 0;

  flags |= (RULE_FILE | RULE_FOLDER); // get default beahvior
  if (inPath[0] == '!') {
    flags |= RULE_NEGATED;
    inPath++;
  }

  uint32_t len = strlen(inPath);
  if (len == 0)
    return -1;

  for (int i = 0; i < len; i++) {
    if (inPath[i] == '/' && inPath[i + 1] != '\0')
      flags |= RULE_ROOT;
    if (inPath[i] == '/' && inPath[i + 1] == '\0')
      flags &= ~RULE_FILE;
    if (inPath[i] == '*') {
      flags |= RULE_DYNAMIC;
      countSnow++;
      if (countSnow > 1)
        return -1;
    }
    // rejection rules:
    if (inPath[i] == '/' && countSnow > 0 && inPath[i + 1] != '\0')
      return -1; // count snow exist, but there is not /\0
    if (inPath[i] == '\\')
      return -1;
  }

  // trim '/' first and last "/src/bin/" ;
  const char *relPattern = (inPath[0] == '/') ? inPath + 1 : inPath;
  size_t relLen = strlen(relPattern);
  if (relLen > 0 && relPattern[relLen - 1] == '/') {
    relLen--;
  }

  // Draft outstr: pack flags into byte 0, clean string starts at byte 1
  outPath[0] = (char)flags; // assign frist byte is flag
  memcpy(outPath + 1, relPattern, relLen);
  outPath[1 + relLen] = '\0';

  return 0;
}

static pathList *parseGitignore(void) {
  if (ruleList)
    return ruleList;

  if (findGitRepo() == NULL)
    return NULL;
  char igPath[MAX_PATH];
  snprintf(igPath, MAX_PATH, ".gitignore");
  FILE *fIgnore = NULL;
  fIgnore = fopen(igPath, "r");
  if (fIgnore == NULL)
    return NULL;

  pathList *rList = (pathList *)malloc(sizeof(pathList));
  if (!rList) {
    fclose(fIgnore);
    return NULL;
  }
  pathList_init(rList);

  char line[MAX_PATH];
  char outRule[MAX_PATH];

  while (fgets(line, sizeof(line), fIgnore)) {
    line[strcspn(line, "\r\n")] = '\0'; // get index and trim
    char *p = line;
    while (*p == ' ' || *p == '\t') { // avoid space and tab
      p++;
    }

    if (*p == '\0' || *p == '#') // skipline
      continue;

    if (checkRules(outRule, p) == 0)
      pathList_add(rList, outRule);
  }
  fclose(fIgnore);
  ruleList = rList;
  return ruleList;
}

static int matchWildCard(const char *str, size_t strLen, const char *pattern) {

  const char *star = strchr(pattern, '*'); // try to get suffix and prefix
  if (!star)
    return 0;
  size_t preLen = star - pattern;
  size_t sufLen = strlen(star + 1); // star+1 is suffix

  if (strLen < (preLen + sufLen))
    return 0;
  // strncpm + move ptr
  int preMatch = (preLen == 0) || (strncmp(str, pattern, preLen) == 0);
  int sufMatch = (sufLen == 0) ||
                 (strncmp(str + (strLen - sufLen), star + 1, sufLen) == 0);
  return preMatch && sufMatch;
}
// RETURN BOOLEAN IF FILES IS VALID OR NOT
int isIgnore(const char *path) {
  pathList *rules = parseGitignore();
  if (!rules || rules->count == 0)
    return 0;

  for (int i = rules->count - 1; i >= 0; i--) {
    char *aRule = rules->arrPaths[i];
    uint8_t flags = (uint8_t)aRule[0];
    const char *pattern = aRule + 1;

    int matched = 0;

    // CASE 1: /src/find* or /src/*.c
    // Pattern: src/find* or src/*.c
    if ((flags & RULE_FILE) &&
        (flags & RULE_FOLDER) && // src/find.c      -> OK (file)
        (flags & RULE_ROOT) &&
        (flags & RULE_DYNAMIC)) { // src/find_dir/x -> OK (folder)
      const char *snow = strchr(pattern, '*');

      if (strlen(path) < (size_t)(snow - pattern))
        continue;
      const char *slash = strchr(path + (snow - pattern), '/');

      size_t len = slash ? (size_t)(slash - path) : strlen(path);
      if (matchWildCard(path, len, pattern))
        matched = 1;

      // CASE 2: *.c or temp*
      // Pattern: *.c or temp*
    } else if ((flags & RULE_FILE) &&
               (flags & RULE_FOLDER) &&  // a/b/main.c   -> OK (file)
               (flags & RULE_DYNAMIC)) { // a/temp_dir/x -> OK (folder)
      const char *cur = path;
      while (cur) {
        const char *nextSlash = strchr(cur, '/');
        size_t segLen = nextSlash ? (size_t)(nextSlash - cur) : strlen(cur);

        if (matchWildCard(cur, segLen, pattern)) {
          matched = 1;
          break;
        }
        cur = nextSlash ? nextSlash + 1 : NULL;
      }

      // CASE 3: /main.c or /build
      // Pattern: main.c or build
    } else if ((flags & RULE_FILE) &&
               (flags & RULE_FOLDER) && // main.c   -> OK (file)
               (flags & RULE_ROOT)) {   // build/x  -> OK (folder)
      size_t pLen = strlen(pattern);
      if (strncmp(path, pattern, pLen) == 0 &&
          (path[pLen] == '\0' || path[pLen] == '/'))
        matched = 1;

      // CASE 4: /src/find*/
      // Pattern: src/find*
    } else if ((flags & RULE_FOLDER) &&
               (flags & RULE_ROOT) &&    // src/find_a/x -> OK (folder)
               (flags & RULE_DYNAMIC)) { // src/find.c   -> NO (file)
      const char *snow = strchr(pattern, '*');
      if (strlen(path) < (size_t)(snow - pattern))
        continue;

      const char *prePath_toSlash = strchr(path + (snow - pattern), '/');
      if (prePath_toSlash &&
          matchWildCard(path, prePath_toSlash - path, pattern))
        matched = 1;

      // CASE 5: main.c or build
      // Pattern: main.c or build
    } else if ((flags & RULE_FILE) &&
               (flags & RULE_FOLDER)) { // a/b/main.c -> OK (file)
      size_t pLen = strlen(pattern);
      const char *cur = path;
      while (cur) {
        const char *nextSlash = strchr(cur, '/');
        size_t segLen = nextSlash ? (size_t)(nextSlash - cur) : strlen(cur);

        if (segLen == pLen && strncmp(cur, pattern, pLen) == 0) {
          matched = 1;
          break;
        }
        cur = nextSlash ? nextSlash + 1 : NULL;
      }

      // CASE 6: test*/ or build_*/
      // Pattern: test* or build_*
    } else if ((flags & RULE_FOLDER) &&
               (flags & RULE_DYNAMIC)) { // a/test_b/x -> OK (folder)
      const char *curFolder = path;      // a/test_b.c -> NO (file)
      while (curFolder) {
        const char *nextSlash = strchr(curFolder, '/');
        if (!nextSlash)
          break;
        if (matchWildCard(curFolder, nextSlash - curFolder, pattern)) {
          matched = 1;
          break;
        }
        curFolder = nextSlash + 1;
      }

      // CASE 7: /src/bin/
      // Pattern: src/bin
    } else if ((flags & RULE_FOLDER) &&
               (flags & RULE_ROOT)) { // src/bin/x -> OK (folder)
      size_t pLen = strlen(pattern);  // src/bin.c -> NO (file)
      if (strncmp(path, pattern, pLen) == 0 && path[pLen] == '/')
        matched = 1;

      // CASE 8: bin/
      // Pattern: bin
    } else if (flags & RULE_FOLDER) { // a/bin/x -> OK (folder)
      size_t pLen = strlen(pattern);  // a/bin.c -> NO (file)
      const char *cur = path;
      while (cur) {
        const char *nextSlash = strchr(cur, '/');
        if (!nextSlash)
          break;

        if ((size_t)(nextSlash - cur) == pLen &&
            strncmp(cur, pattern, pLen) == 0) {
          matched = 1;
          break;
        }
        cur = nextSlash + 1;
      }
    }

    if (matched) {
      return (flags & RULE_NEGATED) ? 0 : 1;
    }
  }

  return 0;
}

#ifdef TEST_IGNORE
#include <assert.h>
#include <stdio.h>

static void test_add_rule(const char *pattern) {
  char outRule[MAX_PATH];
  if (checkRules(outRule, pattern) == 0) {
    pathList_add(ruleList, outRule);
  }
}

static void test_reset_rules(void) {
  if (ruleList) {
    pathList_free(ruleList);
    free(ruleList);
  }
  ruleList = (pathList *)malloc(sizeof(pathList));
  pathList_init(ruleList);
}

int main(void) {
  printf("\n=== RUNNING UNIT TEST: IGNORE (8 CASES) ===\n");

  // CASE 1: /src/find* (FILE + FOLDER + ROOT + DYNAMIC)
  test_reset_rules();
  test_add_rule("/src/find*");
  assert(isIgnore("src/find.c") == 1);            // file match
  assert(isIgnore("src/find_dir/file.txt") == 1); // folder match
  assert(isIgnore("src/other.c") == 0);           // no match
  assert(isIgnore("sub/src/find.c") == 0);        // not at root
  printf("  [PASS] Case 1: /src/find* (Root Dynamic File+Folder)\n");

  // CASE 2: *.o and temp* (FILE + FOLDER + DYNAMIC)
  test_reset_rules();
  test_add_rule("*.o");
  test_add_rule("temp*");
  assert(isIgnore("main.o") == 1);
  assert(isIgnore("sub/dir/main.o") == 1);
  assert(isIgnore("temp_folder/data.txt") == 1);
  assert(isIgnore("a/b/temp_dir/file.txt") == 1);
  assert(isIgnore("main.c") == 0);
  printf("  [PASS] Case 2: *.o / temp* (Anywhere Dynamic File+Folder)\n");

  // CASE 3: /main.c and /build (FILE + FOLDER + ROOT)
  test_reset_rules();
  test_add_rule("/main.c");
  test_add_rule("/build");
  assert(isIgnore("main.c") == 1);
  assert(isIgnore("build/output.exe") == 1);
  assert(isIgnore("sub/main.c") == 0);
  assert(isIgnore("sub/build/out.exe") == 0);
  assert(isIgnore("main.cpp") == 0);
  printf("  [PASS] Case 3: /main.c / /build (Root Static File+Folder)\n");

  // CASE 4: /src/find*/ (FOLDER + ROOT + DYNAMIC)
  test_reset_rules();
  test_add_rule("/src/find*/");
  assert(isIgnore("src/find_dir/app.exe") == 1);
  assert(isIgnore("src/find.c") == 0); // must NOT match file
  assert(isIgnore("sub/src/find_dir/app.exe") == 0);
  printf("  [PASS] Case 4: /src/find*/ (Root Dynamic Folder Only)\n");

  // CASE 5: config.json and build (FILE + FOLDER)
  test_reset_rules();
  test_add_rule("config.json");
  test_add_rule("build");
  assert(isIgnore("config.json") == 1);
  assert(isIgnore("sub/config.json") == 1);
  assert(isIgnore("build/app.exe") == 1);
  assert(isIgnore("a/build/app.exe") == 1);
  assert(isIgnore("config.json.bak") == 0);
  printf(
      "  [PASS] Case 5: config.json / build (Anywhere Static File+Folder)\n");

  // CASE 6: test*/ (FOLDER + DYNAMIC)
  test_reset_rules();
  test_add_rule("test*/");
  assert(isIgnore("test_dir/file.txt") == 1);
  assert(isIgnore("sub/test_run/data.csv") == 1);
  assert(isIgnore("test_run.c") == 0); // must NOT match file
  assert(isIgnore("sub/test_run.c") == 0);
  printf("  [PASS] Case 6: test*/ (Anywhere Dynamic Folder Only)\n");

  // CASE 7: /doc/ (FOLDER + ROOT)
  test_reset_rules();
  test_add_rule("/doc/");
  assert(isIgnore("doc/readme.txt") == 1);
  assert(isIgnore("doc") == 0); // must NOT match file
  assert(isIgnore("sub/doc/readme.txt") == 0);
  printf("  [PASS] Case 7: /doc/ (Root Static Folder Only)\n");

  // CASE 8: bin/ (FOLDER)
  test_reset_rules();
  test_add_rule("bin/");
  assert(isIgnore("bin/app.exe") == 1);
  assert(isIgnore("a/bin/app.exe") == 1);
  assert(isIgnore("bin") == 0); // must NOT match file
  assert(isIgnore("a/bin.c") == 0);
  printf("  [PASS] Case 8: bin/ (Anywhere Static Folder Only)\n");

  // NEGATION RULE: !
  test_reset_rules();
  test_add_rule("*.o");
  test_add_rule("!important.o");
  assert(isIgnore("main.o") == 1);
  assert(isIgnore("important.o") == 0); // un-ignored by !
  printf("  [PASS] Negation: !important.o\n");

  // CLEANUP
  test_reset_rules();
  if (ruleList) {
    pathList_free(ruleList);
    free(ruleList);
    ruleList = NULL;
  }

  printf("=== IGNORE: ALL TESTS PASSED! ===\n\n");
  return 0;
}
#endif
