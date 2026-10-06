#pragma once

#include "pathList.h"

// ---------------------------------------------------------------
// DIFF ENGINE
// ---------------------------------------------------------------
int runDiff(void);

const pathList *diff_getModified(void);
const pathList *diff_getDeleted(void);
const pathList *diff_getUntracked(void);
const pathList *diff_getUnchanged(void);

typedef struct entryList entryList;

int diffEntryLists(pathList *outAdded, pathList *outModified,
                   pathList *outDeleted, const entryList *oldList,
                   const entryList *newList);
