#pragma once

#include "flags.h"
#include "pathList.h"

// Helpers
int isGRMKBranch(char *brelPath);
char *buildPath(char *outPath, char *branch, int isGrmk);

// Branch & Ref (HEAD / POINTER) Operations
int writeBranch(char *branchPath, const char *newSHA1);
int readBranch(char *outSHA1, const char *branchName, int flags);

int writeRef(const char *target, int isHead);
int readRef(char *outTarget, int isHead);

#define writePOINTER(target) writeRef((target), 0)
#define writeHEAD(target) writeRef((target), 1)
#define readPOINTER(out) readRef((out), 0)
#define readHEAD(out) readRef((out), 1)

int branch_printList(void);

int branch_switch(const char *branchName, int flags);
int branch_create(const char *newBranch, char *inSHA1_40);
int branch_remove(char *inBranch);
int branch_rename(char *inOldPath, char *inNewPath);

int command_branch(int count, const char **branches, int flags);
int command_branch_checkout(const char *target, int flags);
int command_branch_reset(const char *target, int flags);
