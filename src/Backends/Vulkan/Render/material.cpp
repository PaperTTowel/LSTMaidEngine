#include "Backends/Vulkan/Render/material.hpp"

#include "Engine/IO/image_io.hpp"
#include "Engine/IO/material_io.hpp"

#include <deque>
#include <exception>
#include <utility>

namespace lve {

  namespace {
    std::shared_ptr<LveTexture> loadTexture(
      LveDevice &device,
      const std::string &path,
      std::string *outError,
      const backend::TextureLoadOptions &options) {
      if (path.empty()) return {};
      try {
        ImageData image{};
        if (!loadImageDataFromFile(path, image, outError, options.flipVertically)) {
          return {};
        }
        auto tex = LveTexture::createTextureFromRgba(
          device,
          image.pixels.data(),
          image.width,
          image.height,
          options);
        return std::shared_ptr<LveTexture>(std::move(tex));
      } catch (const std::exception &e) {
        if (outError) {
          *outError = e.what();
        }
      }
      return {};
    }

    void retireTexture(std::shared_ptr<LveTexture> texture) {
      static std::deque<std::shared_ptr<LveTexture>> retiredTextures;
      if (texture) {
        retiredTextures.push_back(std::move(texture));
      }
      constexpr std::size_t kMaxRetired = 16;
      while (retiredTextures.size() > kMaxRetired) {
        retiredTextures.pop_front();
      }
    }
  } // namespace

  LveMaterial::LveMaterial(LveDevice &device)
    : device{device} {}

  std::shared_ptr<LveMaterial> LveMaterial::loadFromFile(
    LveDevice &device,
    const std::string &path,
    std::string *outError,
    const std::function<std::string(const std::string &)> &pathResolver) {
    MaterialData parsed{};
    if (!loadMaterialDataFromFile(path, parsed, outError, pathResolver)) {
      return {};
    }

    auto material = std::make_shared<LveMaterial>(device);
    material->path = path;
    material->applyData(parsed, outError, pathResolver);
    return material;
  }

  bool LveMaterial::applyData(
    const MaterialData &newData,
    std::string *outError,
    const std::function<std::string(const std::string &)> &pathResolver) {
    const MaterialData previous = data;
    data = newData;
    bool ok = true;
    std::string firstError{};

    auto resolveTexturePath = [&](const std::string &texPath) {
      if (texPath.empty()) return texPath;
      if (pathResolver) {
        const std::string resolved = pathResolver(texPath);
        return resolved.empty() ? texPath : resolved;
      }
      return texPath;
    };

    auto updateTexture = [&](const std::string &newPath,
                             const std::string &oldPath,
                             std::shared_ptr<LveTexture> &target,
                             const backend::TextureLoadOptions &options) {
      if (newPath == oldPath) {
        return;
      }
      std::shared_ptr<LveTexture> oldTexture = target;
      target.reset();
      if (!newPath.empty()) {
        std::string localError;
        target = loadTexture(device, resolveTexturePath(newPath), &localError, options);
        if (!target && !localError.empty()) {
          ok = false;
          if (firstError.empty()) {
            firstError = localError;
          }
        }
      }
      if (oldTexture && oldTexture != target) {
        retireTexture(std::move(oldTexture));
      }
    };

    backend::TextureLoadOptions colorTextureOptions{};
    colorTextureOptions.sRGB = true;
    backend::TextureLoadOptions dataTextureOptions{};
    dataTextureOptions.sRGB = false;

    updateTexture(data.textures.baseColor, previous.textures.baseColor, baseColorTexture, colorTextureOptions);
    updateTexture(data.textures.normal, previous.textures.normal, normalTexture, dataTextureOptions);
    updateTexture(
      data.textures.metallicRoughness,
      previous.textures.metallicRoughness,
      metallicRoughnessTexture,
      dataTextureOptions);
    updateTexture(data.textures.occlusion, previous.textures.occlusion, occlusionTexture, dataTextureOptions);
    updateTexture(data.textures.emissive, previous.textures.emissive, emissiveTexture, colorTextureOptions);

    if (!firstError.empty() && outError) {
      *outError = firstError;
    }
    return ok;
  }

} // namespace lve
