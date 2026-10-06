#pragma once

#include "pathList.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------
// GLOBALS
// ---------------------------------------------------------------
extern const char *repoName;

// ---------------------------------------------------------------
// APIS
// ---------------------------------------------------------------

// Path helpers
void standardizePath(char *path);
char *sanitizePath(char *outRelPath, const char *inPath, char *momFolder,
                   char *fileName, char *extension);

// Buffer utilities
int isBinary(unsigned char *bufData, uint32_t fSize);
uint32_t normalizeLf(unsigned char *data, uint32_t size);

// File & Folder I/O
int readFileData(unsigned char **outbufData, uint32_t *outfSize,
                 const char *path);
int writeFileData(const char *path, const unsigned char *data, uint32_t size);
int createFolder(const char *dirPath);

// Repo & Directory traversal
char *findGitRepo(void);
pathList *parseFilePath(pathList *files, bool isFilter);
