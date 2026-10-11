#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pathList.h"

/*
TEST CASES:
1. Lifecycle: Initialize and free memory safely
2. Data Insertion & Resizing: Add paths, handle NULL, and verify dynamic realloc
growth
3. Sorting: Sort arbitrary paths into lexicographical order (A-Z)
4. Binary Search: Lookup existing and non-existing paths via findIndex
*/

// -----------------------------------------------------------------
// 1. LIFECYCLE: INITIALIZATION AND CLEANUP
// -----------------------------------------------------------------
static void initfree(void) {
  pathList list;

  // Safety check: NULL pointer handling
  assert(pathList_init(NULL) == -1);
  assert(pathList_free(NULL) == -1);

  // Normal initialization
  assert(pathList_init(&list) == 0);
  assert(list.count == 0);
  assert(list.capacity == 128);
  assert(list.arrPaths != NULL);

  // Memory cleanup
  assert(pathList_free(&list) == 0);
  assert(list.arrPaths == NULL);
  assert(list.count == 0);
  assert(list.capacity == 0);

  // Safe double-free
  assert(pathList_free(&list) == 0);
}

static void addresize(void) {
  pathList list;
  pathList_init(&list);

  // Safety checks: NULL arguments should fail safely
  assert(pathList_add(NULL, "src/main.c") == -1);
  assert(pathList_add(&list, NULL) == -1);
  assert(list.count == 0);

  // Add initial sample elements
  assert(pathList_add(&list, "file_a.txt") == 0);
  assert(pathList_add(&list, "file_b.txt") == 0);
  assert(list.count == 2);
  assert(strcmp(list.arrPaths[0], "file_a.txt") == 0);
  assert(strcmp(list.arrPaths[1], "file_b.txt") == 0);

  // Exceed default capacity (128) to trigger dynamic array realloc
  // Add 200 more elements -> 202 elements total (> 128)
  for (int i = 0; i < 200; i++) {
    char temp[64];
    snprintf(temp, sizeof(temp), "folder/dummy_%03d.c", i);
    assert(pathList_add(&list, temp) == 0);
  }
  assert(list.count == 202);
  assert(list.capacity >= 202); // Capacity doubled (to 256)

  // Verify existing data integrity after realloc
  assert(strcmp(list.arrPaths[0], "file_a.txt") == 0);
  assert(strcmp(list.arrPaths[1], "file_b.txt") == 0);
  assert(strcmp(list.arrPaths[102], "folder/dummy_100.c") == 0);

  pathList_free(&list);
}

static void sort(void) {
  pathList list;

  // Edge cases: Sorting empty list, single element, or duplicates
  pathList_init(&list);
  pathList_sort(&list);

  // Single element sort
  pathList_add(&list, "only_one.txt");
  pathList_sort(&list);
  assert(strcmp(list.arrPaths[0], "only_one.txt") == 0);
  assert(list.count == 1);

  // Duplicate elements sort
  pathList_add(&list, "dup.txt");
  pathList_add(&list, "dup.txt");
  pathList_sort(&list);
  assert(strcmp(list.arrPaths[0], "dup.txt") == 0);
  assert(strcmp(list.arrPaths[1], "dup.txt") == 0);
  assert(strcmp(list.arrPaths[2], "only_one.txt") == 0);

  pathList_free(&list);
  pathList_init(&list);

  // Add elements in arbitrary order
  pathList_add(&list, "src/utils.c");
  pathList_add(&list, "README.md");
  pathList_add(&list, "src/main.c");
  pathList_add(&list, "Makefile");
  pathList_add(&list, "docs/manual.txt");

  pathList_sort(&list);

  // Verify lexicographical order
  assert(strcmp(list.arrPaths[0], "Makefile") == 0);
  assert(strcmp(list.arrPaths[1], "README.md") == 0);
  assert(strcmp(list.arrPaths[2], "docs/manual.txt") == 0);
  assert(strcmp(list.arrPaths[3], "src/main.c") == 0);
  assert(strcmp(list.arrPaths[4], "src/utils.c") == 0);

  pathList_free(&list);
}

static void findindex(void) {
  pathList list;
  pathList_init(&list);

  // Edge cases: Search in empty list or with NULL inputs
  assert(pathList_findIndex(&list, "anything.txt") == -1);
  assert(pathList_findIndex(&list, NULL) == -1);
  assert(pathList_findIndex(NULL, "anything.txt") == -1);

  // Single element binary search
  pathList_add(&list, "solo.txt");
  assert(pathList_findIndex(&list, "solo.txt") == 0);
  assert(pathList_findIndex(&list, "missing.txt") == -1);
  pathList_free(&list);
  pathList_init(&list);

  // Populate data
  pathList_add(&list, "src/utils.c");
  pathList_add(&list, "README.md");
  pathList_add(&list, "src/main.c");
  pathList_add(&list, "Makefile");
  pathList_add(&list, "docs/manual.txt");

  // NOTE: Binary search requires sorted array
  pathList_sort(&list);

  // Search existing items (indices 0 to 4 after sort)
  assert(pathList_findIndex(&list, "Makefile") == 0);
  assert(pathList_findIndex(&list, "README.md") == 1);
  assert(pathList_findIndex(&list, "docs/manual.txt") == 2);
  assert(pathList_findIndex(&list, "src/main.c") == 3);
  assert(pathList_findIndex(&list, "src/utils.c") == 4);

  // Search non-existing items -> must return -1
  assert(pathList_findIndex(&list, "AAAAA.txt") == -1);
  assert(pathList_findIndex(&list, "src/other.c") == -1);
  assert(pathList_findIndex(&list, "ZZZZZ.txt") == -1);

  pathList_free(&list);
}

int main(void) {
  initfree();
  addresize();
  sort();
  findindex();

  return 0;
}