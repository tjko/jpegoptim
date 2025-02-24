#include <stdint.h>
#include "fuzz_manager.h"
#include "jpegoptim.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
  int rc = -1;
  struct stat file_stat;

  fuzz_manager_init();

  if (0 != lstat(fuzz_manager.fuzz_file_name, &file_stat)) {
    // Getting file stat failed
    goto end;
  }

  // Write data to fuzz file
  FILE *fuzz_file = fopen(fuzz_manager.fuzz_file_name, "wb+");

  if (!fuzz_file) {
    goto end;
  }

  if (size != fwrite(data, sizeof(uint8_t), size, fuzz_file)) {
    goto end;
  }
  fclose(fuzz_file);

  optimize(
    fuzz_manager.log_fh,
    fuzz_manager.fuzz_file_name,
    fuzz_manager.new_fuzz_file_name,
    fuzz_manager.tmp_dir,
    &file_stat,
    &fuzz_manager.rate,
    &fuzz_manager.saved
  );

  rc = 0;

  end:
  return rc;
}
