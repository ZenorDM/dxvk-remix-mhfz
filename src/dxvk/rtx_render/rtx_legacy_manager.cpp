#include "rtx_legacy_manager.h"
#include <filesystem>
#include "rtx_asset_data_manager.h"
#include "dxvk_device.h"
#include "rtx_texture_manager.h"

namespace dxvk {
  
  LegacyTexture::LegacyTexture(uint32_t textureHash, std::string albedoTexturePath, std::optional<std::string>& normalTexturePath, std::optional<std::string>& roughnessTexturePath, std::optional<std::string>& metallicTexturePath, std::optional<std::string>& heightTexturePath)
    : hash(textureHash), albedoTexturePath(albedoTexturePath), normalTexturePath(normalTexturePath), roughnessTexturePath(roughnessTexturePath), metallicTexturePath(metallicTexturePath), heightTexturePath(heightTexturePath){

  }

  void LegacyManager::pushToLoad(uint32_t texturehash, IDirect3DBaseTexture9* texture){

    unsigned int nthreads = std::min(std::thread::hardware_concurrency(), TextureLoadThreadCount);

    // TODO
    /*size_t minThread = 0xFFffFFff;
    uint32_t selectedThread = 0;
    for (uint32_t threadID = 0; threadID < nthreads; ++threadID) {
      if (minThread > m_textures[threadID].size()) {
        minThread = m_textures[threadID].size();
        selectedThread = threadID;
      }
    }*/

    wchar_t file_prefix[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
    bool found = false;
    std::filesystem::path folderPath = file_prefix;
    folderPath = folderPath.parent_path();
    folderPath = folderPath.parent_path();
    folderPath /= "PBRData";
    HRESULT result = FALSE;
    if (std::filesystem::exists(folderPath)) {
      wchar_t hash_string[11];
      swprintf_s(hash_string, L"%x", texturehash);
      std::wstring strHash = hash_string;
      for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        std::filesystem::path texturePath = entry;
        texturePath /= strHash;
        texturePath += ".a.rtex.dds";

        if (std::filesystem::exists(texturePath)) {
    
          std::optional<std::string> normalPath;
          std::optional<std::string> roughnessPath;
          std::optional<std::string> metallicPath;
          std::optional<std::string> heightPath;

        for (const auto& entryPbr : std::filesystem::directory_iterator(folderPath)) {
              std::filesystem::path normalTexturePath = entryPbr;
              normalTexturePath /= strHash;
              normalTexturePath += "_normal_dx_OTH_Normal.n.rtex.dds";
              if (std::filesystem::exists(normalTexturePath)) {
                normalPath = normalTexturePath.string();
              } 
              std::filesystem::path roughnessTexturePath = entryPbr;
              roughnessTexturePath /= strHash;
              roughnessTexturePath += "_roughness.r.rtex.dds";
              if (std::filesystem::exists(roughnessTexturePath)) {
                roughnessPath = roughnessTexturePath.string();
              }
              std::filesystem::path metallicTexturePath = entryPbr;
              metallicTexturePath /= strHash;
              metallicTexturePath += "_metallic.m.rtex.dds";
              if (std::filesystem::exists(metallicTexturePath)) {
                metallicPath = metallicTexturePath.string();
              }
              std::filesystem::path heightTexturePath = entryPbr;
              heightTexturePath /= strHash;
              heightTexturePath += "_height.h.rtex.dds";
              if (std::filesystem::exists(heightTexturePath)) {
                heightPath = heightTexturePath.string();
              }
          }
          m_textures[0].emplace(texture, LegacyTexture { texturehash,texturePath.string(), normalPath, roughnessPath, metallicPath, heightPath });
          return;
        }
      }
    }
  }

  void LegacyManager::loadFile(LegacyTexture& texture, RtxTextureManager& textureManager){

    auto loadAssetData = [&](const std::string& texturePath, Rc<ManagedTexture>& texture) {
      auto assetData = AssetDataManager::get().findAsset(texturePath);
      if (assetData != nullptr) {
        texture = textureManager.preloadTextureAsset(assetData, ColorSpace::AUTO, true);
      }
    };

    // Check if a replacement file for this texture hash exists and if so, overwrite the texture
    texture.state = TextureState::Loaded;
    loadAssetData(texture.albedoTexturePath, texture.managedTextures.managedAlbedoTexture);

    if (texture.normalTexturePath.has_value()) {
      loadAssetData(texture.normalTexturePath.value(), texture.managedTextures.managedNormalTexture);
    }
    if (texture.roughnessTexturePath.has_value()) {
      loadAssetData(texture.roughnessTexturePath.value(), texture.managedTextures.managedRoughnessTexture);
    }
    if (texture.metallicTexturePath.has_value()) {
      loadAssetData(texture.metallicTexturePath.value(), texture.managedTextures.managedMetallicTexture);
    }
    if (texture.heightTexturePath.has_value()) {
      loadAssetData(texture.heightTexturePath.value(), texture.managedTextures.managedHeightTexture);
    }
    
  }

  void LegacyManager::destroyTexture(IDirect3DBaseTexture9* texture) {
    auto it = m_textures[0].find(texture);
    if (it != m_textures[0].end()) {
      if(it->second.managedTextures.managedAlbedoTexture != nullptr)
        it->second.managedTextures.managedAlbedoTexture->requestMips(0);
      if (it->second.managedTextures.managedNormalTexture != nullptr)
        it->second.managedTextures.managedNormalTexture->requestMips(0);
      if (it->second.managedTextures.managedRoughnessTexture != nullptr)
        it->second.managedTextures.managedRoughnessTexture->requestMips(0);
      if (it->second.managedTextures.managedMetallicTexture != nullptr)
        it->second.managedTextures.managedMetallicTexture->requestMips(0);
      if (it->second.managedTextures.managedHeightTexture != nullptr)
        it->second.managedTextures.managedHeightTexture->requestMips(0);
      m_textures[0].erase(texture);
    }
  }

  void LegacyManager::updateLoadingThread(uint32_t threadID, RtxTextureManager& textureManager){
    for (auto& [hdl, texture] : m_textures[threadID]) {
      if (texture.state == TextureState::None) {
        loadFile(texture, textureManager);
      }
    }
  }
  LegacyManagedTextures LegacyManager::getTextures(IDirect3DBaseTexture9* texture)
  {
    auto it = m_textures[0].find(texture);
    if (it != m_textures[0].end()) {
      return it->second.managedTextures;
    }
    else{
      return LegacyManagedTextures {};
    }
  }

}