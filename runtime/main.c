#include <stdio.h>
#include "aztec_rt.h"
int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s path/to/Aztec.exe\n", argv[0]); return 2; }
  rt_run_main(argv[1], argc, argv);
  return 0;
}
