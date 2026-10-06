
#pragma once

typedef struct {
  int count;
  int capacity;
  char **arrPaths;
} pathList;

extern int pathList_init(pathList *list);
extern int pathList_add(pathList *list, const char *Path);
extern int pathList_free(pathList *list);
extern void pathList_sort(pathList *list);
extern int pathList_findIndex(const pathList *list, const char *path);
extern void pathList_print(const pathList *list, const char *format);