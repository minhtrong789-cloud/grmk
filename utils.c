// ---------------------------------------------------------------
// DECALARATION
// ---------------------------------------------------------------
#include <Windows.h>
#include <direct.h>
#include <io.h> //WINOS Handling Files
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// PROJECT FILES
#include "ignore.h"
#include "pathList.h"
#include "utils.h"

const char *repoName = ".git";
static char *gitRepoPath = NULL;

// ---------------------------------------------------------------
// APIS
// ---------------------------------------------------------------

// TURN PATH TO LINUX STYLE from .\mygit to  /.mygit
void standardizePath(char *path) {
  for (; *path; path++) {
    if (*path == '\\')
      *path = '/';
  }
}

// WARN: Malloc on **outbuf
int readFileData(unsigned char **outbufData, uint32_t *outfSize,
                 const char *path) {

  FILE *fHandler = fopen(path, "rb"); // BLOCK OPEN
  if (!fHandler) {
    return -1;
  }

  fseek(fHandler, 0, SEEK_END); // BLOCK GET FILE SIZE
  uint32_t fSize = ftell(fHandler);
  rewind(fHandler);
  unsigned char *bufData = malloc(fSize + 1); // buf
  if (!bufData)                               // BLOCK READ DATA
    goto err;

  if (fSize > 0) {
    fread(bufData, 1, fSize, fHandler);
  }
  bufData[fSize] = '\0';

  *outbufData = bufData; // BLOCK: ASSIGN
  *outfSize = fSize;
  fclose(fHandler);
  return 0;
err:
  fclose(fHandler);
  return -1;
}

// REMARK: PARAM1 must be 20 bytes buffer

// RETURN: path to current repo;
char *findGitRepo(void) {

  if (gitRepoPath != NULL)
    return gitRepoPath; // if already have path

  char curPath[MAX_PATH];
  char *ptrCurPath = curPath;

  if (_getcwd(ptrCurPath, MAX_PATH) == NULL)
    return NULL;
  standardizePath(ptrCurPath);
  int len = strlen(ptrCurPath);
  snprintf(ptrCurPath + len, MAX_PATH - len, "/%s", repoName);

  // CHECK PARENT FOR .mygit
  struct _stat64i32 st;
  int len1;

  while (1) {

    ptrCurPath = curPath;
    if (_stat(ptrCurPath, &st) == 0) {
      ptrCurPath = strrchr(ptrCurPath, '/');
      *ptrCurPath = '\0';
      gitRepoPath = _strdup(curPath); // CORE VALIDATION BLOCK
      _chdir(gitRepoPath);
      return gitRepoPath;
      break;
    }
    if ((ptrCurPath = strrchr(ptrCurPath, '/')) == NULL ||
        curPath == ptrCurPath)
      break;

    *ptrCurPath = '\0';
    ptrCurPath = curPath;
    if ((ptrCurPath = strrchr(ptrCurPath, '/')) == NULL ||
        curPath == ptrCurPath)
      break;
    len1 = ptrCurPath - curPath;
    snprintf(ptrCurPath, MAX_PATH - len1, "/%s", repoName);
  }

  return NULL;
}

static int parseFilePath_Recursive(pathList *list, const char *path,
                                   bool isFilter) {

  // note: add list of excluding files
  if (!list || !path)
    return -1;

  struct __finddata64_t fData;

  // find first file
  char searchPattern[260]; // Macro: MAX_PATH
  snprintf(searchPattern, MAX_PATH, "%s/*", path);
  intptr_t findHandle = _findfirst64(searchPattern, &fData);
  if (findHandle < 0)
    return -1;

  // get full path and add
  char relPath[MAX_PATH];
  do {
    if (strcmp(fData.name, repoName) == 0) // SKIP CONDITIONS IF GIT REPO
      continue;
    if (strcmp(fData.name, ".") == 0 || strcmp(fData.name, "..") == 0)
      continue;

    if (strcmp(path, ".") == 0) // SAFE DRAFT FILE NAME
      snprintf(relPath, MAX_PATH, "%s", fData.name);
    else
      snprintf(relPath, MAX_PATH, "%s/%s", path, fData.name);

    if (isFilter && isIgnore(relPath)) // CONTINUE IF ISIGNORE
      continue;

    if (fData.attrib & _A_SUBDIR) {
      if (parseFilePath_Recursive(list, relPath, isFilter) < 0)
        goto Err;
    } else {
      if (pathList_add(list, relPath) < 0)
        goto Err;
    }

  } while (0 == _findnext64(findHandle, &fData));

  _findclose(findHandle);

  return 0;
Err:
  _findclose(findHandle);
  return -1;
}

// NOTE: if files is NULL, a new pathList is allocated on heap and returned
// (caller must free both) If files is provided, it must be initialized
// beforehand, and it will be populated, sorted, and returned.
pathList *parseFilePath(pathList *files, bool isFilter) {
  if (!gitRepoPath) { // get path
    if (findGitRepo() == NULL)
      return NULL;
  }

  pathList *fList = NULL;
  fList = files;
  int isForeign = 0;
  if (fList == NULL) {
    fList = malloc(sizeof(pathList));
    if (!fList)
      goto err;
    pathList_init(fList);
    isForeign = 1;
  }

  if (parseFilePath_Recursive(fList, ".", isFilter) < 0)
    goto err;

  pathList_sort(fList);
  return fList;
err:
  if (isForeign) {
    pathList_free(fList);
    free(fList);
    fList = NULL;
  }
  return NULL;
}

// VALIDATE PATH :
char *sanitizePath(char *outRelPath, const char *inPath, char *momFolder,
                   char *fileName, char *extension) {
  if (!inPath)
    return NULL;

  char *gitRepo = findGitRepo();
  char fulPath[MAX_PATH];
  char relPath[MAX_PATH];

  if (_fullpath(fulPath, inPath, MAX_PATH) == NULL)
    return NULL;
  standardizePath(fulPath);
  if (strchr(fulPath, ':')) { // VALIDATE IF OUR REPO
    if (strncmp(gitRepo, fulPath, strlen(gitRepo)) != 0)
      return NULL;
  }

  snprintf(relPath, MAX_PATH, "%s", fulPath + strlen(gitRepo) + 1);
  size_t repoNameLen = strlen(repoName); // IF NAME IS GIT FOLDER THEN EXIT
  if (strncmp(relPath, repoName, repoNameLen) == 0 &&
      (relPath[repoNameLen] == '/' || relPath[repoNameLen] == '\0'))
    return NULL;

  if (outRelPath)
    snprintf(outRelPath, MAX_PATH, "%s", relPath);

  _splitpath(fulPath, NULL, momFolder, fileName, extension);

  return outRelPath;
}

int createFolder(const char *dirPath) {
  if (!dirPath || dirPath[0] == '\0')
    return -1;

  char tmp[MAX_PATH];
  snprintf(tmp, sizeof(tmp), "%s", dirPath);
  standardizePath(tmp);

  char *start = tmp;
  if (tmp[0] != '\0' && tmp[1] == ':') {
    start = tmp + 2;
  }
  if (*start == '/')
    start++;

  for (char *p = start; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      _mkdir(tmp);
      *p = '/';
    }
  }
  _mkdir(tmp);
  return (_access(tmp, 00) == 0) ? 0 : -1;
}

int isBinary(unsigned char *bufData, uint32_t fSize) {
  if (!bufData)
    return -1;
  if (fSize == 0)
    return 0;

  uint32_t upperBound = (fSize > 8000) ? 8000 : fSize;
  for (uint32_t i = 0; i < upperBound; i++) {
    if (bufData[i] == '\0')
      return 1;
  }
  return 0;
}

uint32_t normalizeLf(unsigned char *data, uint32_t size) {
  uint32_t newSize = 0;
  for (uint32_t i = 0; i < size; i++) {
    if (data[i] == '\r' && i + 1 < size && data[i + 1] == '\n')
      continue;
    data[newSize++] = data[i];
  }
  return newSize;
}

int writeFileData(const char *path, const unsigned char *data, uint32_t size) {
  if (!path)
    return -1;

  char dirBuf[MAX_PATH];
  snprintf(dirBuf, sizeof(dirBuf), "%s", path);
  standardizePath(dirBuf);
  char *slash = strrchr(dirBuf, '/');
  if (slash) {
    *slash = '\0';
    createFolder(dirBuf);
  }

  // Always use binary mode "wb" so Windows runtime does not inject \r before \n
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;

  if (size > 0 && data) {
    if (fwrite(data, 1, size, f) < size) {
      fclose(f);
      return -1;
    }
  }
  fclose(f);
  return 0;
}

// ---------------------------------------------------------------
// TEST
// ---------------------------------------------------------------
#ifdef TEST_UTILS
#include <assert.h>

void test_parseFilePath(void) {
  printf("\n=== RUNNING UNIT TEST: parseFilePath ===\n");

  pathList *list = parseFilePath(NULL, true);
  assert(list != NULL);

  printf("Total project files found: %d\n", list->count);
  assert(list->count > 0);
  assert(list->arrPaths != NULL);

  for (int i = 0; i < list->count; i++) {
    if (i < 8 || i == list->count - 1) {
      printf("  [%d] %s\n", i, list->arrPaths[i]);
    } else if (i == 8) {
      printf("  ...\n");
    }

    if (i > 0) {
      assert(strcmp(list->arrPaths[i - 1], list->arrPaths[i]) < 0);
    }
  }

  pathList_free(list);
  free(list);

  printf("=== parseFilePath: ALL TESTS PASSED! ===\n\n");
}

void test_findGitRepo(void) {
  printf("\n=== RUNNING UNIT TEST: findGitRepo ===\n");

  char *repo = findGitRepo();
  printf("Found repo path: %s\n", repo ? repo : "NULL");

  // Validate repo path is found and non-null
  assert(repo != NULL);

  // Validate path is standardized without backslashes
  assert(strchr(repo, '\\') == NULL);

  // Validate cache mechanism returns the same pointer
  char *repoSecond = findGitRepo();
  assert(repo == repoSecond);

  printf("=== findGitRepo: ALL TESTS PASSED! ===\n\n");
}

void test_readFileData(void) {
  printf("\n=== RUNNING UNIT TEST: readFileData ===\n");

  char indexPath[MAX_PATH];
  char *repo = findGitRepo();
  if (repo) {
    snprintf(indexPath, sizeof(indexPath), "%s/%s/index", repo, repoName);
  } else {
    snprintf(indexPath, sizeof(indexPath), "%s/index", repoName);
  }
  unsigned char *buf = NULL;
  uint32_t size = 0;

  int res = readFileData(&buf, &size, indexPath);
  assert(res == 0);
  assert(buf != NULL);
  assert(size >= 12);

  printf("File: %s\n", indexPath);
  printf("Total Size: %u bytes\n", size);

  // Dump raw buffer to stdout (printable characters as-is, non-printable as
  // '.')
  printf("\n--- RAW BUFFER DUMP ---\n");
  for (uint32_t i = 0; i < size; i++) {
    unsigned char c = buf[i];
    if (c >= 32 && c <= 126) {
      putchar(c);
    } else if (c == '\n') {
      putchar('\n');
    } else {
      putchar('.');
    }
  }
  printf("\n--- END RAW BUFFER DUMP ---\n\n");

  printf("Signature : %c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);

  // Read version and entry count using Big-Endian bit shift
  uint32_t version = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                     ((uint32_t)buf[6] << 8) | ((uint32_t)buf[7]);
  uint32_t entries = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) |
                     ((uint32_t)buf[10] << 8) | ((uint32_t)buf[11]);

  printf("Version   : %u\n", version);
  printf("Entries   : %u files in index\n", entries);

  assert(memcmp(buf, "DIRC", 4) == 0);
  assert(version == 2);

  free(buf);
  printf("=== readFileData: ALL TESTS PASSED! ===\n\n");
}

void test_standardizePath(void) {
  printf("\n=== RUNNING UNIT TEST: standardizePath ===\n");
  char p1[] = "C:\\Windows\\System32\\cmd.exe";
  standardizePath(p1);
  assert(strcmp(p1, "C:/Windows/System32/cmd.exe") == 0);

  char p2[] = ".\\mygit\\objects";
  standardizePath(p2);
  assert(strcmp(p2, "./mygit/objects") == 0);

  char p3[] = "already/standard/path.c";
  standardizePath(p3);
  assert(strcmp(p3, "already/standard/path.c") == 0);

  printf("=== standardizePath: ALL TESTS PASSED! ===\n\n");
}

void test_sanitizePath(void) {
  printf("\n=== RUNNING UNIT TEST: sanitizePath ===\n");
  char outRel[MAX_PATH];
  char momFolder[MAX_PATH];
  char fileName[MAX_PATH];
  char extension[MAX_PATH];

  // 1. Simple relative file in root
  char *res =
      sanitizePath(outRel, "grmk_utils.c", momFolder, fileName, extension);
  assert(res != NULL);
  assert(strcmp(outRel, "grmk_utils.c") == 0);
  assert(strcmp(fileName, "grmk_utils") == 0);
  assert(strcmp(extension, ".c") == 0);

  // 2. Relative file in subfolder with backslash
  res = sanitizePath(outRel, ".vscode\\settings.json", momFolder, fileName,
                     extension);
  assert(res != NULL);
  assert(strcmp(outRel, ".vscode/settings.json") == 0);
  assert(strcmp(fileName, "settings") == 0);
  assert(strcmp(extension, ".json") == 0);

  // 3. Dot slash prefix ./
  res = sanitizePath(outRel, "./grmk.c", momFolder, fileName, extension);
  assert(res != NULL);
  assert(strcmp(outRel, "grmk.c") == 0);
  assert(strcmp(fileName, "grmk") == 0);
  assert(strcmp(extension, ".c") == 0);

  // 4. Dot-dot navigation
  res = sanitizePath(outRel, "build/../grmk.c", momFolder, fileName, extension);
  assert(res != NULL);
  assert(strcmp(outRel, "grmk.c") == 0);

  // 5. Must reject files inside .git
  res = sanitizePath(outRel, ".git/config", momFolder, fileName, extension);
  assert(res == NULL);

  // 6. Can draft path for nonexistent/new file
  res = sanitizePath(outRel, "this_file_does_not_exist_xyz.txt", momFolder,
                     fileName, extension);
  assert(res != NULL);
  assert(strcmp(outRel, "this_file_does_not_exist_xyz.txt") == 0);

  // 7. Must reject files outside repo
  res = sanitizePath(outRel, "C:\\Windows\\notepad.exe", momFolder, fileName,
                     extension);
  assert(res == NULL);

  printf("=== sanitizePath: ALL TESTS PASSED! ===\n\n");
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    if (strcmp(argv[1], "sanitizePath") == 0) {
      test_sanitizePath();
      return 0;
    }
    if (strcmp(argv[1], "standardizePath") == 0) {
      test_standardizePath();
      return 0;
    }
    if (strcmp(argv[1], "findGitRepo") == 0) {
      test_findGitRepo();
      return 0;
    }
    if (strcmp(argv[1], "parseFilePath") == 0) {
      test_parseFilePath();
      return 0;
    }
    if (strcmp(argv[1], "readFileData") == 0) {
      test_readFileData();
      return 0;
    }
    printf("Unknown test: %s\n", argv[1]);
    return 1;
  }

  test_sanitizePath();
  test_standardizePath();
  test_findGitRepo();
  test_parseFilePath();
  test_readFileData();
  return 0;
}
#endif