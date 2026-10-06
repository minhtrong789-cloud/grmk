
// ---------------------------------------------------------------
// CORE DELCAREATION
// ---------------------------------------------------------------
#include "pathList.h"

#include <io.h> //Handling Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// ---------------------------------------------------------------
// API
// ---------------------------------------------------------------

// dont declare it as ptr
int pathList_init(pathList *list) {
  if (list == NULL)
    return -1;
  list->count = 0;
  list->capacity = 128;
  list->arrPaths = malloc(list->capacity * sizeof(void *));
  return 0;
}

int pathList_add(pathList *list, const char *path) {

  if (list == NULL || path == NULL)
    return -1;

  if (list->count >= list->capacity) { // increase size
    list->capacity *= 2;
    list->arrPaths = realloc(list->arrPaths, list->capacity * sizeof(void *));
  }

  list->arrPaths[list->count++] = strdup(path); // copy path
  return 0;
}

int pathList_free(pathList *list) {
  for (int i = 0; i < list->count; i++) {
    free(list->arrPaths[i]);
  }
  free(list->arrPaths);

  list->arrPaths = NULL;
  list->count = 0;
  list->capacity = 0;
  return 0;
}

// ---------------------------------------------------------------
// SPECIAL
// ---------------------------------------------------------------

// helper
static int comparePaths(const void *a, const void *b) {
  const char *path1 = *(const char **)a;
  const char *path2 = *(const char **)b;
  // a and b already (**) type in qsort.
  // if not turn them into correect format,
  // we cannot get address of string
  // <0 : a,b | ==0: a=b | >0  : b,a
  return strcmp(path1, path2);
}
// Sort Path A-Z
void pathList_sort(pathList *list) {
  if (!list || !list->arrPaths || list->count <= 1)
    return;

  qsort(list->arrPaths, list->count, sizeof(void *), comparePaths);
}

int pathList_findIndex(const pathList *list, const char *path) {
  if (!list || !list->arrPaths || !path || list->count <= 0)
    return -1;

  int left = 0;
  int right = list->count - 1;

  while (left <= right) {
    int mid = left + (right - left) / 2;
    int cmp = strcmp(path, list->arrPaths[mid]);

    if (cmp == 0)
      return mid;
    else if (cmp < 0)
      right = mid - 1;
    else
      left = mid + 1;
  }

  return -1;
}

void pathList_print(const pathList *list, const char *format) {
  if (!list || !list->arrPaths)
    return;
  if (!format)
    format = "%s\n";
  for (int i = 0; i < list->count; i++) {
    printf(format, list->arrPaths[i]);
  }
}

// TEST
#ifdef TEST_PATHLIST

#include <assert.h>
#include <stdio.h>

int main() {
  pathList list;
  pathList_init(&list);

  // ADD FILES.\test_pathlist.exe
  pathList_add(&list, "src/utils/math.c");
  pathList_add(&list, "README.md");
  pathList_add(&list, "src/main.c");
  pathList_add(&list, "Makefile");
  printf("\n\n=== RUNNING UNIT TEST: PATHLIST ===\n");
  printf("=== BEFORE QSORT ===\n");
  pathList_print(&list, "  %s\n");

  // SORTING TEST
  pathList_sort(&list);

  printf("=== AFTER SORT ===\n");
  pathList_print(&list, "  %s\n");
  assert(strcmp(list.arrPaths[0], "Makefile") == 0);
  assert(strcmp(list.arrPaths[1], "README.md") == 0);
  assert(strcmp(list.arrPaths[2], "src/main.c") == 0);
  assert(strcmp(list.arrPaths[3], "src/utils/math.c") == 0);

  // BINARY SEARCH TEST
  assert(pathList_findIndex(&list, "Makefile") == 0);
  assert(pathList_findIndex(&list, "README.md") == 1);
  assert(pathList_findIndex(&list, "src/main.c") == 2);
  assert(pathList_findIndex(&list, "src/utils/math.c") == 3);
  assert(pathList_findIndex(&list, "nonexistent.c") == -1);

  printf("=== PATHLIST: ALL TESTS PASSED! ===\n\n");
  pathList_free(&list);

  return 0;
}
#endif