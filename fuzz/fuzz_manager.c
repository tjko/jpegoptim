#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fuzz_manager.h"
#include "jpegoptim.h"

fuzz_manager_t fuzz_manager = {0};

void generate_temp_file_name(char *buf, size_t size)
{
  int fd;
  strncpy(buf, "/dev/shm/fuzz_file_XXXXXX", size);
  if (-1 == (fd = mkstemp(buf)))
  {
    perror("mkstemp");
    exit(EXIT_FAILURE);
  }
  close(fd);
}

void fuzz_manager_init()
{
  if (fuzz_manager.is_init)
  {
    return;
  }

  fuzz_set_target_size(-75);

  fuzz_manager.is_init = true;
  fuzz_manager.log_fh = fopen("/dev/null", "a"); // Do not log

  generate_temp_file_name(fuzz_manager.fuzz_file_name, PATH_MAX);
  generate_temp_file_name(fuzz_manager.new_fuzz_file_name, PATH_MAX);

  strncpy(fuzz_manager.tmp_dir, fuzz_manager.fuzz_file_name, PATH_MAX);
  dirname(fuzz_manager.tmp_dir);
  // Ensure the temp path ends with a /
  size_t tmp_len = strnlen(fuzz_manager.tmp_dir, PATH_MAX);

  if (tmp_len < PATH_MAX - 1 && fuzz_manager.tmp_dir[tmp_len - 1] != '/')
  {
    fuzz_manager.tmp_dir[tmp_len] = '/';
    fuzz_manager.tmp_dir[tmp_len + 1] = '\0';
  }
}
