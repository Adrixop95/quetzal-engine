#include <cstdio>
#include "engine/app/engine.hpp"

int main(int argc, char** argv) {
  using namespace aztec::engine;
  Options o;
  std::string err;
  bool usage = false;
  if (!parse_options(argc, argv, o, err, usage)) { std::fprintf(stderr, "%s\n%s", err.c_str(), usage_text()); return 2; }
  if (usage) { std::fputs(usage_text(), stdout); return 0; }
  return run_engine(o);
}
