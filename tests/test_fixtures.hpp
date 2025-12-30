#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Fixture for creating temporary test directories with platformio.ini
class TempProjectFixture {
 public:
  TempProjectFixture() {
    // Create unique temporary directory
    temp_dir = fs::temp_directory_path() /
               ("pio_clangd_test_" + std::to_string(counter++));
    fs::create_directories(temp_dir);
  }

  ~TempProjectFixture() {
    // Clean up on destruction
    if (fs::exists(temp_dir)) {
      fs::remove_all(temp_dir);
    }
  }

  TempProjectFixture(const TempProjectFixture&) = delete;
  TempProjectFixture& operator=(const TempProjectFixture&) = delete;
  TempProjectFixture(TempProjectFixture&&) = delete;
  TempProjectFixture& operator=(TempProjectFixture&&) = delete;

  void create_platformio_ini(const std::vector<std::string>& envs) {
    auto ini_path = temp_dir / "platformio.ini";
    std::ofstream file(ini_path);

    file << "[platformio]\n";
    if (!envs.empty()) {
      file << "default_envs = " << envs[0] << "\n\n";
    }

    for (const auto& env : envs) {
      file << "[env:" << env << "]\n";
      file << "platform = espressif32\n";
      file << "board = esp32dev\n";
      file << "framework = arduino\n\n";
    }
  }

  // Create an empty or invalid platformio.ini (no environments)
  void create_empty_ini() {
    auto ini_path = temp_dir / "platformio.ini";
    std::ofstream file(ini_path);
    file << "[platformio]\n";
    file << "default_envs = none\n";
  }

  fs::path get_path() const { return temp_dir; }
  std::string get_path_string() const { return temp_dir.string(); }

 private:
  fs::path temp_dir;
  static inline int counter = 0;
};
