#include <cstdio>
#include <cstring>
#include "engine/app/engine.hpp"
#include "engine/app/headless.hpp"

int main(int argc, char** argv) {
  using namespace aztec::engine;
  if (argc > 1 && std::strcmp(argv[1], "--headless") == 0) return run_headless(argc - 1, argv + 1);
  Options o;
  std::string err;
  bool usage = false;
  if (!parse_options(argc, argv, o, err, usage)) { std::fprintf(stderr, "%s\n%s", err.c_str(), usage_text()); return 2; }
  if (usage) { std::fputs(usage_text(), stdout); return 0; }
  return run_engine(o);
}
