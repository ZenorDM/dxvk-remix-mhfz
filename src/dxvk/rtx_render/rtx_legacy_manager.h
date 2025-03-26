#pragma once

#include <unordered_map>
#include <string>
#include "rtx_texture.h"
#include "d3d9.h"

namespace dxvk {
  enum class TextureState {
    None = 0,
    Loaded,
    Unloaded
  };

#define TextureLoadThreadCount 8u

  class RtxTextureManager;

  struct LegacyManagedTextures {
    Rc<ManagedTexture> managedAlbedoTexture;
    Rc<ManagedTexture> managedNormalTexture;
    Rc<ManagedTexture> managedRoughnessTexture;
    Rc<ManagedTexture> managedMetallicTexture;
    Rc<ManagedTexture> managedHeightTexture;
  };

  struct LegacyTexture {
    LegacyTexture() = default;
    LegacyTexture(uint32_t textureHash, std::string albedoTexturePath, std::optional<std::string>& normalTexturePath, std::optional<std::string>& roughnessTexturePath, std::optional<std::string>& metallicTexturePath, std::optional<std::string>& heightTexturePath );
    std::string albedoTexturePath;
    std::optional< std::string> normalTexturePath;
    std::optional< std::string> roughnessTexturePath;
    std::optional< std::string> metallicTexturePath;
    std::optional< std::string> heightTexturePath;
    LegacyManagedTextures managedTextures;
    uint32_t hash;
    TextureState state = TextureState::None;
  };

  class LegacyManager {
  public:

    void pushToLoad(uint32_t texturehash, IDirect3DBaseTexture9* texture);

    void loadFile(LegacyTexture& texture, RtxTextureManager& textureManager);
    void destroyTexture(IDirect3DBaseTexture9* texture);
    void updateLoadingThread(uint32_t threadID, RtxTextureManager& textureManager);
    LegacyManagedTextures getTextures(IDirect3DBaseTexture9* texture);
  private:

    std::unordered_map<IDirect3DBaseTexture9*, LegacyTexture> m_textures[TextureLoadThreadCount]; // Orihandle to texture
  };
}