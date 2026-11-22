// Match std::getenv in the executable, including Windows builds using an SDL DLL.
#pragma once
#include <cstdlib>

inline int set_test_env(const char* name, const char* value) {
#ifdef _WIN32
  return _putenv_s(name, value);
#else
  return setenv(name, value, 1);
#endif
}
