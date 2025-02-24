#ifndef FUZZ_MANAGER_H
#define FUZZ_MANAGER_H
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * Manages necessary state across fuzz iterations, should exist as singleton
 */
typedef struct fuzz_manager
{
  bool is_init;
  double rate, saved;
  FILE *log_fh;
  char fuzz_file_name[PATH_MAX + 1];
  char new_fuzz_file_name[PATH_MAX + 1];
  char tmp_dir[PATH_MAX + 1];
} fuzz_manager_t;

extern fuzz_manager_t fuzz_manager;

/**
 * Initializes the fuzz manager singleton, if it has not already been
 */
void fuzz_manager_init();

#endif //FUZZ_MANAGER_H
