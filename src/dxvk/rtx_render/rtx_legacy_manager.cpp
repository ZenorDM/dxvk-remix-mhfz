#include "rtx_legacy_manager.h"
#include <filesystem>
#include "rtx_asset_data_manager.h"
#include "dxvk_device.h"
#include "rtx_texture_manager.h"
#include <json/json.hpp>

using json = nlohmann::json;

namespace dxvk {

  static LegacyMaterialLayer defaultLayerMaterial;

  LegacyTexture::LegacyTexture(uint32_t textureHash,
                               std::optional< std::string> albedoTexturePath, 
                               std::optional<std::string>& normalTexturePath, 
                               std::optional<std::string>& roughnessTexturePath, 
                               std::optional<std::string>& metallicTexturePath, 
                               std::optional<std::string>& heightTexturePath,
                               TextureOrigin origin,
                               LegacyMaterialLayer* legacyMaterialLayer)
    : hash(textureHash), 
    albedoTexturePath(albedoTexturePath), 
    normalTexturePath(normalTexturePath),
    roughnessTexturePath(roughnessTexturePath), 
    metallicTexturePath(metallicTexturePath),
    heightTexturePath(heightTexturePath),
    origin(origin),
    legacyMaterialLayer(legacyMaterialLayer){

  }

  void to_json(json& j, const Vector3& p) {
    j = json { {"x", p.x},{"y", p.y},{"z", p.z} };
  }

  void from_json(const json& j, Vector3& p) {
    j.at("x").get_to(p.x);
    j.at("y").get_to(p.y);
    j.at("z").get_to(p.z);
  }

  void to_json(json& j, const LegacyMaterialLayer& p) {
    j = json { { "RoughnessBias", p.roughnessBias },
              {"MetallicBias", p.metallicBias} ,
              {"EmissiveIntensity", p.emissiveIntensity} ,
              {"NormalStrenght", p.normalStrenght},

              {"DisplacementFactor", p.displacementFactor},

              {"SubsurfaceTransmittanceColor", p.subsurfaceTransmittanceColor},
              {"SubsurfaceMeasurementDistance", p.subsurfaceMeasurementDistance},
              {"SubsurfaceSingleScatteringAlbedo", p.subsurfaceSingleScatteringAlbedo},
              {"SubsurfaceVolumetricAnisotropy", p.subsurfaceVolumetricAnisotropy},
              {"IsSubsurfaceDiffusionProfile", p.isSubsurfaceDiffusionProfile},
              {"SubsurfaceRadius", p.subsurfaceRadius},
              {"SubsurfaceRadiusScale", p.subsurfaceRadiusScale},
              {"SubsurfaceMaxSampleRadius", p.subsurfaceMaxSampleRadius},

              {"ThinFilmEnable", p.thinFilmEnable},
              {"AlphaIsThinFilmThickness", p.alphaIsThinFilmThickness},
              {"ThinFilmThicknessConstant", p.thinFilmThicknessConstant},

              {"AlphaTestReferenceValue", p.alphaTestReferenceValue},
              {"SoftBlendFactor", p.softBlendFactor},

              {"Features", p.features} };
  }

  void from_json(const json& j, LegacyMaterialLayer& p) {
    j.at("RoughnessBias").get_to(p.roughnessBias);
    j.at("MetallicBias").get_to(p.metallicBias);
    j.at("EmissiveIntensity").get_to(p.emissiveIntensity);
    j.at("NormalStrenght").get_to(p.normalStrenght);

    j.at("DisplacementFactor").get_to(p.displacementFactor);

    j.at("SubsurfaceTransmittanceColor").get_to(p.subsurfaceTransmittanceColor);
    j.at("SubsurfaceMeasurementDistance").get_to(p.subsurfaceMeasurementDistance);
    j.at("SubsurfaceSingleScatteringAlbedo").get_to(p.subsurfaceSingleScatteringAlbedo);
    j.at("SubsurfaceVolumetricAnisotropy").get_to(p.subsurfaceVolumetricAnisotropy);
    j.at("IsSubsurfaceDiffusionProfile").get_to(p.isSubsurfaceDiffusionProfile);
    j.at("SubsurfaceRadius").get_to(p.subsurfaceRadius);
    j.at("SubsurfaceRadiusScale").get_to(p.subsurfaceRadiusScale);
    j.at("SubsurfaceMaxSampleRadius").get_to(p.subsurfaceMaxSampleRadius);

    j.at("ThinFilmEnable").get_to(p.thinFilmEnable);
    j.at("AlphaIsThinFilmThickness").get_to(p.alphaIsThinFilmThickness);
    j.at("ThinFilmThicknessConstant").get_to(p.thinFilmThicknessConstant);

    j.at("AlphaTestReferenceValue").get_to(p.alphaTestReferenceValue);
    j.at("SoftBlendFactor").get_to(p.softBlendFactor);

    j.at("Features").get_to(p.features);
  }

  void to_json(json& j, const LegacyMeshLayer& p) {
    j = json { { "MaterialLayer", p.materialLayer },
              {"WorldPosOffset", p.offset} ,
              {"OverrideMaterial", p.overrideMaterial},
              {"Features", p.features}};
  }

  void from_json(const json& j, LegacyMeshLayer& p) {
    j.at("MaterialLayer").get_to(p.materialLayer);
    j.at("WorldPosOffset").get_to(p.offset);
    j.at("OverrideMaterial").get_to(p.overrideMaterial);
    j.at("Features").get_to(p.features);
  }


  void LegacyManager::load() { 
    {
      wchar_t file_prefix[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
      std::filesystem::path dump_path = file_prefix;
      dump_path = dump_path.parent_path();
      dump_path /= "LegacyMaterialsLayer.json";

      if (std::filesystem::exists(dump_path) == false)
        return;
      std::ifstream i;
      i.exceptions(std::ifstream::failbit | std::ifstream::badbit);
      try {
        i.open(dump_path.c_str());
      }
      catch (std::system_error& e) {
        Logger::err(e.code().message());
      }

      if (!i.is_open()) {
        return;
      }
      json j;
      try {
        j = json::parse(i);
      }
      catch (json::parse_error& ex) {
        std::string error = str::format("load parse error at byte", ex.byte);
        Logger::err(error.c_str());
      }

      m_legacyMaterialsLayer = j.get<std::unordered_map<uint32_t, LegacyMaterialLayer>>();
    }
    {
      wchar_t file_prefix[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
      std::filesystem::path dump_path = file_prefix;
      dump_path = dump_path.parent_path();
      dump_path /= "LegacyMeshesLayer.json";

      if (std::filesystem::exists(dump_path) == false)
        return;
      std::ifstream i;
      i.exceptions(std::ifstream::failbit | std::ifstream::badbit);
      try {
        i.open(dump_path.c_str());
      }
      catch (std::system_error& e) {
        Logger::err(e.code().message());
      }

      if (!i.is_open()) {
        return;
      }
      json j;
      try {
        j = json::parse(i);
      }
      catch (json::parse_error& ex) {
        std::string error = str::format("load parse error at byte", ex.byte);
        Logger::err(error.c_str());
      }

      m_legacyMeshesLayer = j.get<std::unordered_map<XXH64_hash_t, LegacyMeshLayer>>();
    }
  }

  void LegacyManager::save() {
    {
      json j;

      wchar_t file_prefix[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
      std::filesystem::path dump_path = file_prefix;
      dump_path = dump_path.parent_path();
      dump_path /= "LegacyMaterialsLayer.json";

      j = m_legacyMaterialsLayer;

      std::ofstream o { dump_path.c_str() };
      o << j;
    }
    {
      json jMesh;

      wchar_t file_prefix[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
      std::filesystem::path dump_path = file_prefix;
      dump_path = dump_path.parent_path();
      dump_path /= "LegacyMeshesLayer.json";

      jMesh = m_legacyMeshesLayer;

      std::ofstream oMesh { dump_path.c_str() };
      oMesh << jMesh;
    }
  }

  void LegacyManager::pushMeshMaterial(XXH64_hash_t meshHash) { 
    auto [it, _] = m_legacyMeshesLayer.try_emplace(meshHash, LegacyMeshLayer {});
  }

  void LegacyManager::pushToLoad(uint32_t texturehash, D3D9CommonTexture* texture){

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

    std::optional<std::string> albedoPath;
    std::optional<std::string> normalPath;
    std::optional<std::string> roughnessPath;
    std::optional<std::string> metallicPath;
    std::optional<std::string> heightPath;

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
        std::filesystem::path albedoPathTexturePath = entry;
        albedoPathTexturePath /= strHash;
        albedoPathTexturePath += ".a.rtex.dds";

        if (std::filesystem::exists(albedoPathTexturePath)) {
   
          albedoPath = albedoPathTexturePath.string();
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
        }
      }
    }
    TextureOrigin origin = TextureOrigin::None;
    if (albedoPath.has_value()) {
      const std::string& path = albedoPath.value();
      if (path.find("stage") != std::string::npos) {
        origin = TextureOrigin::Stage;
      }
      else if (path.find("emmodel") != std::string::npos) {
        origin = TextureOrigin::Emmodel;
      }
      else if (path.find("extend") != std::string::npos) {
        origin = TextureOrigin::Extend;
      }
    }
    auto [it,_] = m_legacyMaterialsLayer.try_emplace(texturehash, LegacyMaterialLayer {});
    if (origin == TextureOrigin::Extend) {
      it->second.features |= LegacyMaterialFeature::NoFade;
    }
    if (origin == TextureOrigin::Emmodel) {
      it->second.normalStrenght = -1.0f;
    }
    m_textures[0].emplace(texture, LegacyTexture { texturehash , albedoPath, normalPath, roughnessPath, metallicPath, heightPath, origin, &(it->second) });

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
    if (texture.albedoTexturePath.has_value()) {
      loadAssetData(texture.albedoTexturePath.value(), texture.managedTextures.managedAlbedoTexture);
    }
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

  void LegacyManager::destroyTexture(D3D9CommonTexture* texture) {
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
  LegacyManagedTextures LegacyManager::getTextures(D3D9CommonTexture* texture)
  {
    auto it = m_textures[0].find(texture);
    if (it != m_textures[0].end()) {
      return it->second.managedTextures;
    }
    else{
      return LegacyManagedTextures {};
    }
  }

  uint32_t LegacyManager::getTextureHash(D3D9CommonTexture* texture) {
    auto it = m_textures[0].find(texture);
    if (it != m_textures[0].end()) {
      return it->second.hash;
    } else {
      return 0;
    }
  }

  uint32_t LegacyManager::getTextureOrigin(D3D9CommonTexture* texture) {
    auto it = m_textures[0].find(texture);
    if (it != m_textures[0].end()) {
      return (uint32_t)it->second.origin;
    } else {
      return 0;
    }
  }

  LegacyMaterialLayer* LegacyManager::getLegacyMaterialLayer(D3D9CommonTexture* texture) {
    auto it = m_textures[0].find(texture);
    if (it != m_textures[0].end()) {
      return it->second.legacyMaterialLayer;
    } else {
      return nullptr;
    }
  }

  LegacyMeshLayer* LegacyManager::getLegacyMeshLayer(XXH64_hash_t meshHash) {
    auto it = m_legacyMeshesLayer.find(meshHash);
    if (it != m_legacyMeshesLayer.end()) {
      return &(it->second);
    } else {
      return nullptr;
    }
  }

  void LegacyMaterialLayer::resetLayerMaterial() {
    *this = defaultLayerMaterial;
  }

}