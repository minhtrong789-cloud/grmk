#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODE_DIR_STR "40000"

typedef struct entryList entryList;

// Utilities
int isBinary(unsigned char *bufData, uint32_t fSize);
uint32_t normalizeLf(unsigned char *data, uint32_t size);
char *getSHA1_40Bytes(char *SHA1_40, unsigned char *SHA1_20);
unsigned char *getSHA1_20Bytes(unsigned char *SHA1_20, char *SHA1_40);
unsigned char *HashContent_SHA1(unsigned char *bufSHA1, unsigned char *Content,
                                uint32_t contentSize);

// Git Object Operations
int writeObject(const unsigned char *data, uint32_t size,
                const unsigned char *SHA1_20);
int readObjectData(char *objType, uint32_t *objSize,
                   unsigned char **outObjData);
int readBlob(unsigned char **outBuf, uint32_t *outSize,
             const unsigned char *SHA1_40);
int readTree(entryList *eList, char *prefix, const char *inSHA1_40, int flags);
int readCommit(char *outTreeSHA1_40, char *outParentSHA1_40, char *outAuthor,
               char *outCommitter, char *outMessage,
               const unsigned char *inSHA1_40);
unsigned char *hashBlob(unsigned char *outSHA1_20, const char *path, int flags);
int command_object_hashobject(int count, const char **paths, int flags);
int command_object_writeTree(int flags);
int command_object_catFile(const char *inSHA1_40, int flags);
int command_object_lsTree(const char *inSHA1_40, int flags);
int command_object_commit(const char *message, int flags);
int command_object_log(int countLimit, int flags);

// Object Builders
unsigned char *hashTree(unsigned char *outSHA1_20, entryList *inEntryList,
                        const char *inSlashPos, int *inIndex, int *inWritten,
                        int flags);
unsigned char *hashCommit(unsigned char *outSHA1_20, const char *inMessage,
                          char *inTreeSHA1_40, char *inPCmitSHA1_40, int flags);
