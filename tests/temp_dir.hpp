// Private temporary files using the host's temporary directory, without POSIX APIs.
#pragma once
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

class TempDir {
 public:
  TempDir() {
    std::random_device random;
    for (int attempt = 0; attempt < 100; ++attempt) {
      path_ = std::filesystem::temp_directory_path() /
          ("aztec-test-" + std::to_string(random()) + "-" + std::to_string(random()));
      std::error_code ec;
      if (std::filesystem::create_directory(path_, ec)) return;
      if (ec) throw std::filesystem::filesystem_error("create test directory", path_, ec);
    }
    throw std::runtime_error("cannot create a unique test directory");
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  ~TempDir() { std::error_code ec; std::filesystem::remove_all(path_, ec); }
  const std::filesystem::path& path() const { return path_; }
 private:
  std::filesystem::path path_;
};
