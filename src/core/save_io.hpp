// Internal checked replacement for original profile and save files.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace aztec::detail {
inline bool write_save_bytes(const std::string& path, const std::vector<uint8_t>& v) {
  if (v.empty()) return false;
  // Exclusive creation avoids collisions and following a pre-existing temporary symlink.
  static std::atomic<unsigned> sequence{0};
  std::filesystem::path target(path), temporary;
  int fd = -1;
  for (int attempt = 0; attempt < 100; ++attempt) {
#ifdef _WIN32
    auto pid = _getpid();
#else
    auto pid = getpid();
#endif
    temporary = target;
    temporary += ".tmp." + std::to_string(pid) + "." + std::to_string(sequence++);
#ifdef _WIN32
    fd = _wopen(temporary.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
    if (fd >= 0) break;
    if (errno != EEXIST) return false;
  }
  if (fd < 0) return false;
#ifdef _WIN32
  FILE* f = _fdopen(fd, "wb");
#else
  FILE* f = fdopen(fd, "wb");
#endif
  bool ok = false;
  if (f) {
    ok = std::fwrite(v.data(), 1, v.size(), f) == v.size() && std::fflush(f) == 0;
#ifdef _WIN32
    if (ok) ok = _commit(fd) == 0;
#else
    if (ok) ok = fsync(fd) == 0;
#endif
    if (std::fclose(f) != 0) ok = false;
  } else {
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
  }
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename(temporary.c_str(), target.c_str()) == 0;
#endif
  }
  std::error_code ec;
  if (!ok) std::filesystem::remove(temporary, ec);
  return ok;
}
} // namespace aztec::detail
