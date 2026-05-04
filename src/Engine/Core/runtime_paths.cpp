#include "Engine/Core/runtime_paths.hpp"

#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace lve {
  namespace {
    namespace fs = std::filesystem;

    std::string normalize(const fs::path &path) {
      return path.lexically_normal().generic_string();
    }

    bool isEngineResourcePath(const std::string &path) {
      return path.rfind("Assets/", 0) == 0 ||
        path.rfind("Assets\\", 0) == 0 ||
        path.rfind("Shaders/", 0) == 0 ||
        path.rfind("Shaders\\", 0) == 0;
    }
  } // namespace

  std::string RuntimePaths::executableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length > 0 && length < sizeof(buffer)) {
      return normalize(fs::path(buffer).parent_path());
    }
#elif defined(__linux__)
    char buffer[4096]{};
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length > 0) {
      buffer[length] = '\0';
      return normalize(fs::path(buffer).parent_path());
    }
#endif
    return normalize(fs::current_path());
  }

  std::string RuntimePaths::resolveResourcePath(const std::string &path) {
    if (path.empty()) {
      return {};
    }

    fs::path inputPath{path};
    if (inputPath.is_absolute()) {
      return normalize(inputPath);
    }

    std::error_code ec;
    if (fs::exists(inputPath, ec)) {
      return normalize(inputPath);
    }

    if (!isEngineResourcePath(path)) {
      return normalize(inputPath);
    }

    const fs::path executableCandidate = fs::path(executableDirectory()) / inputPath;
    if (fs::exists(executableCandidate, ec)) {
      return normalize(executableCandidate);
    }

    const fs::path cwdCandidate = fs::current_path(ec) / inputPath;
    if (!ec && fs::exists(cwdCandidate, ec)) {
      return normalize(cwdCandidate);
    }

    if (fs::exists(executableCandidate.parent_path(), ec)) {
      return normalize(executableCandidate);
    }

    return normalize(inputPath);
  }

} // namespace lve
