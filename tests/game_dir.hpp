#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

// CTest supplies the cache setting through the environment. Its legacy game/
// argument remains the fallback when neither the cache nor environment is set.
inline std::string test_game_dir(int argc, char** argv) {
  if (const char* dir = std::getenv("GAME_DIR")) {
    if (*dir) return dir;
  }
  return argc > 1 ? argv[1] : "";
}

inline bool test_game_dir_exists(const std::string& dir) {
  std::error_code err;
  if (!dir.empty() && std::filesystem::is_directory(dir, err)) return true;
  std::printf("SKIP: game directory absent; set GAME_DIR to your local installation.\n");
  return false;
}
