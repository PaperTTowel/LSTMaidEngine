#pragma once

#include <string>

namespace lve {

  class RuntimePaths {
  public:
    static std::string executableDirectory();
    static std::string resolveResourcePath(const std::string &path);
  };

} // namespace lve
