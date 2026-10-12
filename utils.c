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

//
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

// CREATE NESTED FODLERS
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

// TURN PATH TO LINUX STYLE from .\mygit to  /.mygit
void standardizePath(char *path) {
  for (; *path; path++) {
    if (*path == '\\')
      *path = '/';
  }
}

// REMOVE \r\n
uint32_t normalizeLf(unsigned char *data, uint32_t size) {
  uint32_t newSize = 0;
  for (uint32_t i = 0; i < size; i++) {
    if (data[i] == '\r' && i + 1 < size && data[i + 1] == '\n')
      continue;
    data[newSize++] = data[i];
  }
  return newSize;
}

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

// ---------------------------------------------------------------
// CURRENT REPO FUNCTIONS
// ---------------------------------------------------------------

// Populate provided files list with repo files. Caller must initialize files
// beforehand.
pathList *parseFilePath(pathList *files, const char *startPath, bool isFilter) {
  if (!files)
    return NULL;

  if (!gitRepoPath) { // get path
    if (findGitRepo() == NULL)
      return NULL;
  }

  const char *root = (startPath && startPath[0] != '\0') ? startPath : ".";
  if (parseFilePath_Recursive(files, root, isFilter) < 0)
    return NULL;

  pathList_sort(files);
  return files;
}

// GET RELPATH
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
