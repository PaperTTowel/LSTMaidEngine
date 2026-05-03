#pragma once

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace test {

  inline void require(bool condition, const std::string &message) {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  inline bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
  }

  inline std::filesystem::path outputDir(const std::string &suiteName) {
    std::filesystem::path dir{TEST_OUTPUT_ROOT};
    dir /= suiteName;
    std::filesystem::create_directories(dir);
    return dir;
  }

  inline void writeTextFile(const std::filesystem::path &path, const std::string &content) {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(file), "failed to open test file for writing: " + path.string());
    file << content;
  }

  inline int runSuite(const char *name, void (*suite)()) {
    try {
      suite();
      std::cout << "[passed] " << name << "\n";
      return 0;
    } catch (const std::exception &e) {
      std::cerr << "[failed] " << name << ": " << e.what() << "\n";
      return 1;
    }
  }

} // namespace test
