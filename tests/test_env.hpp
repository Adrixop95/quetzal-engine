// Match the C runtime used by the engine's std::getenv reads. SDL's DLL may
// have a separate environment on Windows.
#pragma once
#include <cstdlib>

inline int set_test_env(const char* name, const char* value) {
#ifdef _WIN32
  return _putenv_s(name, value);
#else
  return setenv(name, value, 1);
#endif
}
