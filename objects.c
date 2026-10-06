
#include <Windows.h>
#include <bcrypt.h>
#include <direct.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>

#include "branch.h"
#include "objects.h"
#include "stage.h"
#include "utils.h"

// ---------------------------------------------------------------
// OBJECT EXAMPLES
// ---------------------------------------------------------------
/*
TEST COMMANDS:
git cat-file -p ea22855c0432055cc04a382f62067b313fc7873f (commit)
git cat-file -p 60121ea717d6fa5c540d64c9185723b511fe0d1b (tree)
git cat-file -p dc67c65bb47b56519da77d7476acd86f7eb13ff5 (blob)

Original Git idea - Git Repo SHA1: d6d3f9d0125a7215f3cdc2600b2307ca55b69536

OBJECT BINARY FORMATS:

1. Blob:
blob <size_in_ascii>\0<raw_content_bytes>

2. Tree:
tree <size_in_ascii>\0
<mode_octal_ascii> <filename>\0<binary_sha1_20_bytes>
<mode_octal_ascii> <filename>\0<binary_sha1_20_bytes>
...

3. Commit:
commit <size_in_ascii>\0
tree <root_tree_sha1_40_hex>\n
parent <parent_commit_sha1_40_hex>\n
author <Author Name> <<author_email>> <timestamp_unix> <timezone>\n
committer <Committer Name> <<committer_email>> <timestamp_unix> <timezone>\n
\n
<commit message>\n
*/

// ---------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------
//

// ---------------------------------------------------------------
// APIS
// ---------------------------------------------------------------

// NOTE: param1: 41byte, param2: 20byte
char *getSHA1_40Bytes(char *SHA1_40, unsigned char *SHA1_20) {
  if (!SHA1_40 || !SHA1_20)
    return NULL;

  for (int i = 0; i < 20; i++) {
    sprintf(SHA1_40 + i * 2, "%02x", SHA1_20[i]);
  }
  SHA1_40[40] = '\0';
  return SHA1_40;
}

// NOTE:  param1: 20byte , param2: 41byte,
unsigned char *getSHA1_20Bytes(unsigned char *SHA1_20, char *SHA1_40) {
  if (!SHA1_40 || !SHA1_20)
    return NULL;

  unsigned int byteSHA1;
  for (int i = 0; i < 20; i++) {
    sscanf(SHA1_40 + i * 2, "%2x", &byteSHA1);
    SHA1_20[i] = (unsigned char)byteSHA1;
  }
  return SHA1_20;
}

// WRITE ANY TO GIT OBJCETS FOLDER
int writeObject(const unsigned char *data, uint32_t size,
                const unsigned char *SHA1_20) {
  unsigned char *outzData = NULL;
  FILE *f = NULL;
  uLong outzSize;
  char path[MAX_PATH];
  char SHA1_40[41];

  char *gitRepo = findGitRepo(); // BLOCK FOLDER
  if (!gitRepo)
    return -1;
  if (getSHA1_40Bytes(SHA1_40, (unsigned char *)SHA1_20) < 0)
    return -1;
  snprintf(path, MAX_PATH, "%s/objects/%.2s", repoName, SHA1_40);
  if (_mkdir(path) < 0 && errno != EEXIST)
    return -1;

  snprintf(path, MAX_PATH, "%s/objects/%.2s/%s", repoName, SHA1_40,
           SHA1_40 + 2);
  if (_access(path, 00) == 0)
    goto success;

  outzSize = compressBound(size); // COMPRESS
  outzData = malloc(outzSize);
  if (!outzData)
    goto err;
  if (compress(outzData, &outzSize, data, size) < 0)
    goto err;

  f = fopen(path, "wb"); // WRITE
  if (!f)
    goto err;
  if (fwrite(outzData, 1, outzSize, f) < outzSize) {
    if (f)
      fclose(f);
    f = NULL;
    remove(path);
    goto err;
  }

  if (f)
    fclose(f);
success:
  free(outzData);
  return 0;
err:
  if (f)
    fclose(f);
  free(outzData);
  return -1;
}

// OUTPUT: 20 bytes
unsigned char *HashContent_SHA1(unsigned char *bufSHA1, unsigned char *Content,
                                uint32_t contentSize) {

  BCRYPT_ALG_HANDLE hAlgo_SHA1;
  int provider_Opened =
      BCryptOpenAlgorithmProvider(&hAlgo_SHA1, BCRYPT_SHA1_ALGORITHM, 0, 0);
  if (provider_Opened < 0)
    return NULL;

  unsigned char *ptrSHA1 = NULL;
  if (BCryptHash(hAlgo_SHA1, 0, 0, Content, contentSize, bufSHA1, 20) >= 0)
    ptrSHA1 = bufSHA1;

  BCryptCloseAlgorithmProvider(hAlgo_SHA1, 0);
  return ptrSHA1;
}
// read... aka catfile
int objData(char *outType, uint32_t *outSize, const char *inSHA1_40) {
  if (!findGitRepo())
    return -1;

  unsigned char buf[50];
  unsigned char *comprData = NULL;
  uint32_t comprSize = 0;

  char SHA1Path[MAX_PATH];
  snprintf(SHA1Path, sizeof(SHA1Path), "%s/objects/%.2s/%s", repoName,
           inSHA1_40, inSHA1_40 + 2);
  if (readFileData(&comprData, &comprSize, SHA1Path) < 0 || !comprData)
    goto err;

  int errMes;
  z_stream d_stream;
  d_stream.zalloc = (alloc_func)0;
  d_stream.zfree = (free_func)0;
  d_stream.opaque = (voidpf)0;

  d_stream.next_in = comprData;
  d_stream.avail_in = comprSize;
  errMes = inflateInit(&d_stream);
  if (errMes != Z_OK)
    goto err;
  d_stream.next_out = buf;
  d_stream.avail_out = sizeof(buf);

  errMes = inflate(&d_stream, Z_NO_FLUSH);
  inflateEnd(&d_stream);
  if (errMes < 0)
    goto err;
  buf[sizeof(buf) - 1] = '\0';

  char typeBuf[16];
  uint32_t sizeVal;
  if (sscanf((char *)buf, "%15s %u", typeBuf, &sizeVal) != 2)
    goto err;

  if (outType)
    strcpy(outType, typeBuf);
  if (outSize)
    *outSize = sizeVal;

  free(comprData);
  return 0;
err:
  free(comprData);
  return -1;
}
// WARN: FREE OUTDATA AFTER USE
int readObject(const char *inSHA1_40, char *outType, uint32_t *outSize,
               unsigned char **outData) {
  if (!findGitRepo() || !inSHA1_40 || !outData)
    return -1;

  char typeBuf[16];
  uint32_t objSize = 0;
  if (objData(typeBuf, &objSize, inSHA1_40) < 0)
    return -1;

  char SHA1Path[MAX_PATH];
  snprintf(SHA1Path, sizeof(SHA1Path), "%s/objects/%.2s/%s", repoName,
           inSHA1_40, inSHA1_40 + 2);

  unsigned char *comprData = NULL;
  unsigned char *fullBuf = NULL;
  uint32_t comprSize = 0;

  if (readFileData(&comprData, &comprSize, SHA1Path) < 0 || !comprData)
    goto err;

  uLongf destLen = objSize + 64;
  fullBuf = (unsigned char *)malloc(destLen);
  if (!fullBuf)
    goto err;

  if (uncompress(fullBuf, &destLen, comprData, comprSize) != Z_OK)
    goto err;

  unsigned char *nullByte = (unsigned char *)memchr(fullBuf, '\0', destLen);
  if (!nullByte)
    goto err;

  unsigned char *data = (unsigned char *)malloc(objSize + 1);
  if (!data)
    goto err;

  memcpy(data, nullByte + 1, objSize);
  data[objSize] = '\0';

  *outData = data;
  if (outSize)
    *outSize = objSize;
  if (outType)
    strcpy(outType, typeBuf);

  free(fullBuf);
  free(comprData);
  return 0;
err:
  free(fullBuf);
  free(comprData);
  return -1;
}

// WARN: FREE OUTBUF AFTER USE
int readBlob(unsigned char **outBuf, uint32_t *outSize,
             const unsigned char *SHA1_40) {
  char type[16];
  if (readObject((const char *)SHA1_40, type, outSize, outBuf) < 0)
    return -1;

  if (strcmp(type, "blob") != 0) {
    if (outBuf && *outBuf) {
      free(*outBuf);
      *outBuf = NULL;
    }
    return -1;
  }
  return 0;
}

// WARN: FREE OUTBUF AFTER USE
int readTree(entryList *eList, char *prefix, const char *inSHA1_40, int flags) {
  /* TREE BINARY FORMAT:
    "tree <size>"\0
    "<strMode_orgOct> <name>"\0binary20SHA1
    "<strMode_orgOct> <name>"\0binary20SHA1
    ...
  */

  if (!inSHA1_40 || !eList)
    return -1;

  char type[16];
  uint32_t size = 0;
  unsigned char *data = NULL;

  if (readObject(inSHA1_40, type, &size, &data) < 0 || !data)
    goto err;

  if (strcmp(type, "tree") != 0)
    goto err;

  char *ptr = (char *)data;
  char *end = (char *)data + size;

  while (ptr < end) {
    uint32_t mode = 0;
    char *name = NULL;
    unsigned char *SHA1_20 = NULL;

    mode = (uint32_t)strtoul(ptr, &ptr, 8);
    if (*ptr++ != ' ')
      goto err;

    name = ptr;
    ptr += strlen(name) + 1;

    if (ptr + 20 > end)
      goto err;
    SHA1_20 = (unsigned char *)ptr;
    ptr += 20;
    if ((mode == 040000) && (flags & FLAG_RECURSIVE)) {
      char subPrefix[MAX_PATH];
      snprintf(subPrefix, sizeof(subPrefix), "%s%s/", prefix ? prefix : "",
               name);

      char subSHA1_40[41];
      getSHA1_40Bytes(subSHA1_40, SHA1_20);

      if (readTree(eList, subPrefix, subSHA1_40, flags) < 0)
        goto err;
    } else {
      stageEntry entry = {0};
      entry.st_mode = mode;
      snprintf(entry.name, sizeof(entry.name), "%s%s", prefix ? prefix : "",
               name);
      entry.nameLen = (uint16_t)strlen(entry.name);
      memcpy(entry.SHA1, SHA1_20, 20);
      entryList_add(eList, &entry);
    }
  }
  if (!prefix || prefix[0] == '\0')
    entryList_sort(eList);
  free(data);
  return 0;
err:
  if (data)
    free(data);
  return -1;
}

int readCommit(char *outTreeSHA1_40, char *outParentSHA1_40, char *outAuthor,
               char *outCommitter, char *outMessage,
               const unsigned char *inSHA1_40) {
  if (!inSHA1_40)
    return -1;

  char type[16];
  uint32_t size = 0;
  unsigned char *data = NULL;

  if (readObject((const char *)inSHA1_40, type, &size, &data) < 0 || !data)
    goto err;

  if (strcmp(type, "commit") != 0)
    goto err;

  char tmpTree[41] = "";
  char tmpParent[41] = "";
  char tmpAuthor[256] = "";
  char tmpCommitter[256] = "";
  char tmpMessage[1024] = "";

  char *ptr = (char *)data;
  char *eol = NULL;

  // Tree (required)
  if (strncmp(ptr, "tree ", 5) != 0)
    goto err;
  snprintf(tmpTree, sizeof(tmpTree), "%.40s", ptr + 5);
  eol = strchr(ptr, '\n');
  if (!eol)
    goto err;
  ptr = eol + 1;

  // Parents (0, 1 or more parents)
  while (strncmp(ptr, "parent ", 7) == 0) {
    if (tmpParent[0] == '\0')
      snprintf(tmpParent, sizeof(tmpParent), "%.40s", ptr + 7);

    eol = strchr(ptr, '\n');
    if (!eol)
      goto err;
    ptr = eol + 1;
  }

  // Author
  if (strncmp(ptr, "author ", 7) == 0) {
    eol = strchr(ptr, '\n');
    if (!eol)
      goto err;
    int len = (int)(eol - (ptr + 7));
    if (len > 0 && ptr[7 + len - 1] == '\r')
      len--;
    snprintf(tmpAuthor, sizeof(tmpAuthor), "%.*s", len, ptr + 7);
    ptr = eol + 1;
  }

  // 4. Committer
  if (strncmp(ptr, "committer ", 10) == 0) {
    eol = strchr(ptr, '\n');
    if (!eol)
      goto err;
    int len = (int)(eol - (ptr + 10));
    if (len > 0 && ptr[10 + len - 1] == '\r')
      len--;
    snprintf(tmpCommitter, sizeof(tmpCommitter), "%.*s", len, ptr + 10);
    ptr = eol + 1;
  }

  // get message
  while (*ptr == '\r' || *ptr == '\n')
    ptr++;
  snprintf(tmpMessage, sizeof(tmpMessage), "%s", ptr);
  size_t mLen = strlen(tmpMessage);
  while (mLen > 0 &&
         (tmpMessage[mLen - 1] == '\n' || tmpMessage[mLen - 1] == '\r')) {
    tmpMessage[--mLen] = '\0';
  }

  // Copy to out pointers if requested
  if (outTreeSHA1_40)
    strcpy(outTreeSHA1_40, tmpTree);
  if (outParentSHA1_40)
    strcpy(outParentSHA1_40, tmpParent);
  if (outAuthor)
    strcpy(outAuthor, tmpAuthor);
  if (outCommitter)
    strcpy(outCommitter, tmpCommitter);
  if (outMessage)
    strcpy(outMessage, tmpMessage);

  free(data);
  return 0;

err:
  if (data)
    free(data);
  return -1;
}

// ---------------------------------------------------------------
// BUILD OBJECTS
// ---------------------------------------------------------------
unsigned char *hashTree(unsigned char *outSHA1_20, entryList *inEntryList,
                        const char *inSlashPos, int *inIndex, int *inWritten,
                        int flags) {
  /* TREE BINARY FORMAT:
    "tree <size>"\0
    "<strMode_orgOct> <name>"\0binary20SHA1
    "<strMode_orgOct> <name>"\0binary20SHA1
    ...
  */
  entryList *stgList = inEntryList ? inEntryList : loadStage(FLAG_NONE);
  if (!stgList)
    return NULL;
  int writtenCount = inWritten ? *inWritten : 0;
  int curIndex = (inIndex) ? *inIndex : 0;

  // Alloc memeory
  size_t allocSize = 50 + (size_t)stgList->numFiles * (MAX_PATH + 40);
  unsigned char *bufTree = malloc(allocSize);
  if (!bufTree)
    return NULL;
  bufTree[49] = '\0';
  const char *ptr_name = NULL;
  unsigned char *ptr_start = &bufTree[50];
  unsigned char *ptr_end = NULL;

  // BLOCK: write entry -  "<strMode_orgOct> <name>"\0binary20SHA1
  char folder[MAX_PATH];
  folder[0] = '\0';
  unsigned char subTreeSHA1_20[20];

  do {
    stageEntry entry = stgList->entries[curIndex];

    if (folder[0] != '\0' && strncmp(entry.name, folder, strlen(folder)) != 0)
      break; // check if its parent folder matched

    if (folder[0] == '\0' && inSlashPos != NULL)
      snprintf(folder, MAX_PATH, "%.*s", (int)(inSlashPos - entry.name + 1),
               entry.name);

    // search if folder, search if end of folder
    char *nextSlash = strchr(inSlashPos ? inSlashPos + 1 : entry.name, '/');

    if (nextSlash) {
      // CORE: After Subfolder Success, we Write SHA1_20
      ptr_name = inSlashPos ? (inSlashPos + 1) : entry.name;
      if (!hashTree(subTreeSHA1_20, stgList, nextSlash, &curIndex, inWritten,
                    flags))
        goto err;
      sprintf((char *)ptr_start, "%s %.*s", MODE_DIR_STR,
              (int)(nextSlash - ptr_name), ptr_name);
      int lenPreNull = strlen((char *)ptr_start);
      memcpy(ptr_start + lenPreNull + 1, subTreeSHA1_20, 20);
      ptr_end = ptr_start + lenPreNull + 20 + 1;
      ptr_start = ptr_end;
      continue;
    }
    // CORE: WRITING ENTRY BY ENTRY HERE!!
    ptr_name = entry.name + strlen(folder);
    sprintf((char *)ptr_start, "%o %.*s", entry.st_mode, (int)strlen(ptr_name),
            ptr_name);
    int lenPreNull = strlen((char *)ptr_start);
    memcpy(ptr_start + lenPreNull + 1, entry.SHA1, 20);
    ptr_end = ptr_start + lenPreNull + 20 + 1;
    ptr_start = ptr_end;
    curIndex++;
    writtenCount++;

  } while (writtenCount < stgList->numFiles);

  // BLOCK: Metadata - "tree <size>"\0
  char hdr[32];
  int headerLen = sprintf(hdr, "tree %d", (int)(ptr_end - &bufTree[50]));
  ptr_start = &bufTree[50 - headerLen - 1];
  memcpy(ptr_start, hdr, headerLen + 1);

  // BLOCK: Hash It
  unsigned char sha1_20[20];
  uint32_t totalLen = (uint32_t)(ptr_end - ptr_start);
  if (!HashContent_SHA1(sha1_20, ptr_start, totalLen))
    goto err;

  // if write then run compress
  if (flags & FLAG_WRITE) {
    if (writeObject(ptr_start, totalLen, sha1_20) < 0)
      goto err;
  }

  if (outSHA1_20)
    memcpy(outSHA1_20, sha1_20, 20);
  if (inWritten)
    *inWritten = writtenCount;
  if (inIndex)
    *inIndex = curIndex;
  free(bufTree);
  return outSHA1_20;
err:
  free(bufTree);
  return NULL;
}

unsigned char *hashCommit(unsigned char *outSHA1_20, const char *inMessage,
                          char *inTreeSHA1_40, char *inPCmitSHA1_40,
                          int flags) {
  /* COMMIT FORMAT:
    "commit <size>"\0
    "tree <treeHex40>\n"
    "parent <parentHex40>\n"
    "author <Name> <<Email>> <timestamp> <timezone>\n"
    "committer <Name> <<Email>> <timestamp> <timezone>\n"
    "\n"
    "<message>\n"
  */

  // COMMITOR INFO:
  char tz[8];
  char authorBlock[250];

  const char *username = getenv("USERNAME");
  char *email = "abc@email.com";
  time_t now = (time_t)time(NULL);
  strftime(tz, sizeof(tz), "%z", localtime(&now));
  snprintf(authorBlock, 250, "%s <%s> %lld %s", username, email, now, tz);

  // PREPARE:

  char bufCommit[4096];
  char *ptrStart = NULL;
  char *ptrWriting = NULL;
  bufCommit[39] = '\0';
  ptrStart = &bufCommit[39];
  ptrWriting = &bufCommit[40];

  ptrWriting += sprintf(ptrWriting, "tree %s\n", inTreeSHA1_40);
  if (inPCmitSHA1_40 && inPCmitSHA1_40[0] != '\0')
    ptrWriting += sprintf(ptrWriting, "parent %s\n", inPCmitSHA1_40);
  ptrWriting += sprintf(ptrWriting, "author %s\n", authorBlock);
  ptrWriting += sprintf(ptrWriting, "committer %s\n", authorBlock);
  ptrWriting += sprintf(ptrWriting, "\n");
  ptrWriting += sprintf(ptrWriting, "%s\n", inMessage);

  uint32_t payloadLen = normalizeLf((unsigned char *)&bufCommit[40],
                                    (uint32_t)(ptrWriting - &bufCommit[40]));
  ptrWriting = &bufCommit[40] + payloadLen;

  char temp[40];
  int commitSize = (int)(ptrWriting - &bufCommit[40]);
  int tempLen = sprintf(temp, "commit %d", commitSize);
  ptrStart -= sprintf(&bufCommit[39] - tempLen, "%s", temp);

  // HASH BLOCK;
  unsigned char SHA1_20[20];
  if (HashContent_SHA1(SHA1_20, (unsigned char *)ptrStart,
                       ptrWriting - ptrStart) == NULL)
    goto err;

  if (flags & FLAG_WRITE) {
    if (writeObject((unsigned char *)ptrStart, ptrWriting - ptrStart, SHA1_20) <
        0)
      goto err;
  }

  if (outSHA1_20)
    memcpy(outSHA1_20, SHA1_20, 20);

  return outSHA1_20;

err:
  return NULL;
}

unsigned char *hashBlob(unsigned char *outSHA1_20, const char *path,
                        int flags) {
  /* BLOB BINARY FORMAT:
    "blob <size>"\0
    "binary data
    ...
  */
  unsigned char SHA1_20[20];
  unsigned char *bufData = NULL;
  uint32_t fSize; // READ DATA
  unsigned char *blobData = NULL;
  uint32_t blobSize;

  if (readFileData(&bufData, &fSize, path) < 0) {
    printf("Unvalid path or err in read permission");
    return NULL;
  }
  if (!isBinary(bufData, fSize))
    fSize = normalizeLf(bufData, fSize);

  char hder[150]; // BUILD HEADER
  int hderLen = sprintf(hder, "blob %u", fSize) + 1;

  blobData = malloc(hderLen + fSize); // BUILD DATA
  if (!blobData)
    goto err;

  memcpy(blobData, hder, hderLen);
  memcpy(blobData + hderLen, bufData, fSize);
  blobSize = hderLen + fSize;

  // write into build object then
  if (HashContent_SHA1(SHA1_20, blobData, blobSize) == NULL)
    goto err;

  if (flags & FLAG_WRITE) {
    if (writeObject(blobData, blobSize, SHA1_20) < 0)
      goto err;
  }
  if (outSHA1_20)
    memcpy(outSHA1_20, SHA1_20, 20);

  free(blobData);
  free(bufData);
  return outSHA1_20;
err:
  free(blobData);
  free(bufData);
  return NULL;
}

int command_object_hashobject(int count, const char **paths, int flags) {
  // 1. TREE OBJECT (--tree)
  if (flags & FLAG_TREE) {
    if (count > 0) {
      printf("fatal: 'hash-object --tree' does not take path arguments\n");
      return -1;
    }

    unsigned char SHA1_20[20];
    char SHA1_40[41];

    if (!hashTree(SHA1_20, NULL, NULL, NULL, NULL, flags)) {
      printf("fatal: failed to hash tree object\n");
      return -1;
    }

    getSHA1_40Bytes(SHA1_40, SHA1_20);

    if (flags & FLAG_VERBOSE) {
      const char *status = (flags & FLAG_WRITE) ? "Written to database"
                                                : "Computed (Preview only)";
      printf("[Tree Object]\n");
      printf("Status:   %s\n", status);
      printf("SHA-1:    %s\n", SHA1_40);
    } else {
      printf("%s\n", SHA1_40);
    }
    return 0;
  }

  // 2. BLOB OBJECT (Default or --blob)
  if (count == 0 || !paths) {
    printf("fatal: No path specified\n");
    return -1;
  }

  for (int i = 0; i < count; i++) {
    unsigned char SHA1_20[20];
    char SHA1_40[41];

    if (hashBlob(SHA1_20, paths[i], flags) == NULL) {
      return -1;
    }

    getSHA1_40Bytes(SHA1_40, SHA1_20);

    if (flags & FLAG_VERBOSE) {
      const char *status = (flags & FLAG_WRITE) ? "Written to database"
                                                : "Computed (Preview only)";
      printf("[Blob Object]  %s\n", paths[i]);
      printf("Status:        %s\n", status);
      printf("SHA-1:         %s\n\n", SHA1_40);
    } else {
      printf("%s\n", SHA1_40);
    }
  }
  return 0;
}

int command_object_writeTree(int flags) {
  flags |= FLAG_WRITE;
  unsigned char SHA1_20[20];
  char SHA1_40[41];

  if (!hashTree(SHA1_20, NULL, NULL, NULL, NULL, flags)) {
    printf("fatal: failed to write tree object\n");
    return -1;
  }

  getSHA1_40Bytes(SHA1_40, SHA1_20);
  printf("%s\n", SHA1_40);
  return 0;
}

static int catFile_type(const char *inSHA1_40) {
  char type[16];
  if (objData(type, NULL, inSHA1_40) < 0) {
    printf("fatal: Not a valid object name %s\n", inSHA1_40);
    return -1;
  }
  printf("%s\n", type);
  return 0;
}

static int catFile_size(const char *inSHA1_40) {
  uint32_t size = 0;
  if (objData(NULL, &size, inSHA1_40) < 0) {
    printf("fatal: Not a valid object name %s\n", inSHA1_40);
    return -1;
  }
  printf("%u\n", size);
  return 0;
}

static int catFile_exists(const char *inSHA1_40) {
  if (objData(NULL, NULL, inSHA1_40) < 0)
    return -1;
  return 0;
}

static int catFile_pretty(const char *inSHA1_40) {
  char type[16];
  uint32_t size = 0;
  unsigned char *data = NULL;

  if (readObject(inSHA1_40, type, &size, &data) < 0 || !data) {
    printf("fatal: Not a valid object name %s\n", inSHA1_40);
    return -1;
  }

  if (strcmp(type, "tree") == 0) {
    char *ptr = (char *)data;
    char *end = (char *)data + size;
    char sha40[41];

    while (ptr < end) {
      uint32_t mode = (uint32_t)strtoul(ptr, &ptr, 8);
      if (*ptr++ != ' ')
        goto err;

      char *name = ptr;
      ptr += strlen(name) + 1;

      if (ptr + 20 > end)
        goto err;

      getSHA1_40Bytes(sha40, (unsigned char *)ptr);
      ptr += 20;

      const char *entryType = (mode == 040000) ? "tree" : "blob";
      printf("%06o %s %s\t%s\n", mode, entryType, sha40, name);
    }
  } else {
    // blob, commit, tag
    fwrite(data, 1, size, stdout);
  }

  free(data);
  return 0;
err:
  free(data);
  return -1;
}

int command_object_catFile(const char *inSHA1_40, int flags) {
  if (!inSHA1_40 || strlen(inSHA1_40) != 40) {
    if (flags & FLAG_EXISTS)
      return -1;
    printf("fatal: Not a valid object name %s\n", inSHA1_40 ? inSHA1_40 : "");
    return -1;
  }

  if (flags & FLAG_TYPE)
    return catFile_type(inSHA1_40);
  if (flags & FLAG_SIZE)
    return catFile_size(inSHA1_40);
  if (flags & FLAG_EXISTS)
    return catFile_exists(inSHA1_40);
  if (flags & FLAG_PRETTY)
    return catFile_pretty(inSHA1_40);

  printf("fatal: no action specified (-p, -t, -s, -e)\n");
  return -1;
}

int command_object_lsTree(const char *inSHA1_40, int flags) {
  char resolvedSHA1[41] = "";

  // If NULL or "HEAD" / "head" / "POINTER": resolve current branch/commit
  if (!inSHA1_40 || inSHA1_40[0] == '\0' || _stricmp(inSHA1_40, "HEAD") == 0 ||
      _stricmp(inSHA1_40, "POINTER") == 0) {
    char curHead[MAX_PATH];
    int headType = readPOINTER(curHead);
    if (headType == 1) {
      if (readBranch(resolvedSHA1, curHead, 0) < 0 || resolvedSHA1[0] == '\0') {
        printf("fatal: not a valid object name: '%s'\n", curHead);
        return -1;
      }
    } else if (headType == 0) {
      strncpy(resolvedSHA1, curHead, 40);
      resolvedSHA1[40] = '\0';
    } else {
      printf("fatal: failed to resolve HEAD\n");
      return -1;
    }
  } else if (strlen(inSHA1_40) == 40) {
    strncpy(resolvedSHA1, inSHA1_40, 40);
    resolvedSHA1[40] = '\0';
  } else {
    // Try resolving as branch name
    char bPath[MAX_PATH];
    if ((buildPath(bPath, (char *)inSHA1_40, 0) && _access(bPath, 00) == 0) ||
        (buildPath(bPath, (char *)inSHA1_40, 1) && _access(bPath, 00) == 0)) {
      if (readBranch(resolvedSHA1, bPath, 0) < 0) {
        printf("fatal: Not a valid object name %s\n", inSHA1_40);
        return -1;
      }
    } else {
      printf("fatal: Not a valid object name %s\n", inSHA1_40);
      return -1;
    }
  }

  char type[16];
  if (objData(type, NULL, resolvedSHA1) < 0) {
    printf("fatal: Not a valid object name %s\n", resolvedSHA1);
    return -1;
  }

  char treeSHA1_40[41];
  if (strcmp(type, "commit") == 0) {
    if (readCommit(treeSHA1_40, NULL, NULL, NULL, NULL,
                   (const unsigned char *)resolvedSHA1) < 0) {
      printf("fatal: failed to read commit %s\n", resolvedSHA1);
      return -1;
    }
  } else if (strcmp(type, "tree") == 0) {
    strcpy(treeSHA1_40, resolvedSHA1);
  } else {
    printf("fatal: not a tree-ish %s\n", resolvedSHA1);
    return -1;
  }

  entryList eList = {0};
  if (readTree(&eList, NULL, treeSHA1_40, flags) < 0) {
    if (eList.entries)
      free(eList.entries);
    printf("fatal: failed to read tree %s\n", treeSHA1_40);
    return -1;
  }

  char sha40[41];
  for (uint32_t i = 0; i < eList.numFiles; i++) {
    stageEntry *e = &eList.entries[i];
    getSHA1_40Bytes(sha40, e->SHA1);
    const char *entryType = (e->st_mode == 040000) ? "tree" : "blob";
    printf("%06o %s %s\t%s\n", e->st_mode, entryType, sha40, e->name);
  }

  if (eList.entries)
    free(eList.entries);
  return 0;
}

int command_object_commit(const char *message, int flags) {
  if (!findGitRepo() || !message || message[0] == '\0') {
    if (!message || message[0] == '\0')
      printf("fatal: empty commit message\n");
    return -1;
  }

  // Auto-stage modified tracked files if -a (FLAG_ALL2) is set
  if (flags & FLAG_ALL2) {
    if (command_stage_add(0, NULL, FLAG_UPDATE) < 0) {
      printf("fatal: failed to stage modified files\n");
      return -1;
    }
  }

  // 1. Write index tree
  unsigned char treeSHA1_20[20];
  char treeSHA1_40[41];
  if (!hashTree(treeSHA1_20, NULL, NULL, NULL, NULL, FLAG_WRITE)) {
    printf("fatal: failed to write tree object\n");
    return -1;
  }
  getSHA1_40Bytes(treeSHA1_40, treeSHA1_20);

  // 2. Read POINTER to get parent and target branch
  char currentTarget[MAX_PATH] = "";
  int targetType = readPOINTER(currentTarget);
  if (targetType < 0) {
    return -1;
  }

  char parentSHA1_40[41] = "";
  if (targetType == 1) {
    if (!isGRMKBranch(currentTarget)) {
      fprintf(stderr,
              "fatal: Cannot commit to git branch '%s'. grmk only allows "
              "committing to 'grmk-*' branches.\n",
              currentTarget);
      return -1;
    }
    if (readBranch(parentSHA1_40, currentTarget, 0) < 0)
      parentSHA1_40[0] = '\0';

  } else if (targetType == 0) {
    strncpy(parentSHA1_40, currentTarget, 40);
    parentSHA1_40[40] = '\0';
  }

  // Handle --amend: replace parent with grandparent commit
  if (flags & FLAG_AMEND) {
    if (parentSHA1_40[0] == '\0') {
      printf("fatal: You have nothing to amend.\n");
      return -1;
    }
    char grandparentSHA1_40[41] = "";
    if (readCommit(NULL, grandparentSHA1_40, NULL, NULL, NULL,
                   (const unsigned char *)parentSHA1_40) < 0) {
      printf("fatal: failed to read commit to amend: %s\n", parentSHA1_40);
      return -1;
    }
    strcpy(parentSHA1_40, grandparentSHA1_40);
  }

  // 3. Prevent duplicate empty commit if tree unchanged (skipped on --amend)
  if (!(flags & FLAG_AMEND) && parentSHA1_40[0] != '\0') {
    char parentTreeSHA1_40[41] = "";
    if (readCommit(parentTreeSHA1_40, NULL, NULL, NULL, NULL,
                   (const unsigned char *)parentSHA1_40) == 0) {
      if (strcmp(treeSHA1_40, parentTreeSHA1_40) == 0 &&
          !(flags & FLAG_FORCE)) {
        const char *bName =
            (targetType == 1)
                ? (strrchr(currentTarget, '/') ? strrchr(currentTarget, '/') + 1
                                               : currentTarget)
                : "detached HEAD";
        printf("On branch %s\nnothing to commit, working tree clean\n", bName);
        return 0;
      }
    }
  }

  // 4. Create commit object
  unsigned char commitSHA1_20[20];
  char commitSHA1_40[41];
  const char *pParent = (parentSHA1_40[0] != '\0') ? parentSHA1_40 : NULL;
  if (!hashCommit(commitSHA1_20, message, treeSHA1_40, (char *)pParent,
                  FLAG_WRITE)) {
    printf("fatal: failed to create commit object\n");
    return -1;
  }
  getSHA1_40Bytes(commitSHA1_40, commitSHA1_20);

  // 5. Update branch reference or POINTER via branch.c
  if (targetType == 1) {
    if (writeBranch(currentTarget, commitSHA1_40) < 0) {
      printf("fatal: failed to update branch ref: %s\n", currentTarget);
      return -1;
    }
  } else {
    if (writePOINTER(commitSHA1_40) < 0) {
      printf("fatal: failed to update POINTER\n");
      return -1;
    }
  }

  // 6. Print formatted summary
  const char *bName = (targetType == 1) ? (strrchr(currentTarget, '/')
                                               ? strrchr(currentTarget, '/') + 1
                                               : currentTarget)
                                        : "detached HEAD";
  char shortSHA[8];
  snprintf(shortSHA, sizeof(shortSHA), "%.7s", commitSHA1_40);
  if (flags & FLAG_AMEND) {
    printf("[%s (amend) %s] %s\n", bName, shortSHA, message);
  } else if (parentSHA1_40[0] == '\0') {
    printf("[%s (root-commit) %s] %s\n", bName, shortSHA, message);
  } else {
    printf("[%s %s] %s\n", bName, shortSHA, message);
  }

  if (flags & FLAG_VERBOSE) {
    printf("Commit: %s\n", commitSHA1_40);
    printf("Tree:   %s\n", treeSHA1_40);
    printf("Parent: %s\n", (pParent ? pParent : "(none)"));
  }

  return 0;
}

int command_object_log(int countLimit, int flags) {
  if (!findGitRepo())
    return -1;

  if (countLimit <= 0)
    countLimit = 10;

  // 1. Get starting commit from POINTER
  char currentTarget[MAX_PATH] = "";
  int targetType = readPOINTER(currentTarget);
  if (targetType < 0) {
    fprintf(stderr, "fatal: failed to read POINTER\n");
    return -1;
  }

  char currCommitSHA1[41] = "";
  if (targetType == 1) { // Branch
    if (readBranch(currCommitSHA1, currentTarget, 0) < 0) {
      fprintf(stderr,
              "fatal: your current branch '%s' does not have any commits yet\n",
              currentTarget);
      return -1;
    }
  } else { // Detached SHA1
    strncpy(currCommitSHA1, currentTarget, 40);
    currCommitSHA1[40] = '\0';
  }

  if (currCommitSHA1[0] == '\0') {
    fprintf(stderr,
            "fatal: your current branch does not have any commits yet\n");
    return -1;
  }

  // 2. Walk commit chain
  int count = 0;
  while (count < countLimit && currCommitSHA1[0] != '\0') {
    char parentSHA1[41] = "";
    char author[256] = "";
    char message[1024] = "";

    if (readCommit(NULL, parentSHA1, author, NULL, message,
                   (const unsigned char *)currCommitSHA1) < 0) {
      fprintf(stderr, "fatal: failed to read commit object: %s\n",
              currCommitSHA1);
      return -1;
    }

    if (flags & FLAG_SHORT) {
      // Oneline mode: "%.7s <message>"
      printf("%s%.7s%s %s\n", COLOR_GREEN, currCommitSHA1, COLOR_RESET,
             message);
    } else {
      // Full mode
      printf("%scommit %s%s\n", COLOR_GREEN, currCommitSHA1, COLOR_RESET);
      if (author[0] != '\0')
        printf("Author: %s\n", author);
      printf("\n    %s\n\n", message);
    }

    count++;
    if (parentSHA1[0] == '\0')
      break;

    strncpy(currCommitSHA1, parentSHA1, 40);
    currCommitSHA1[40] = '\0';
  }

  return 0;
}
