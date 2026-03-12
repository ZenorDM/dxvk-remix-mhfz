#include "rtx_legacy_manager.h"
#include <filesystem>
#include "rtx_asset_data_manager.h"
#include "dxvk_device.h"
#include "rtx_texture_manager.h"

#include "../../lssusd/usd_include_begin.h"
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include "../../lssusd/usd_include_end.h"

#include "rtx_particle_system.h"

using namespace pxr;

namespace dxvk {
  

  static std::array<UINT, 266> emmodelParticleHash = {
    0x10D12296,
0x1150E52B,
0x1195EFE9,
0x1383C6D5,
0x13C28051,
0x14FD09B6,
0x16005D71,
0x17C81AC0,
0x18349B41,
0x188A1268,
0x197C1230,
0x198808C5,
0x19CD0C0,
0x1A48880B,
0x1A7186B3,
0x1B65DC49,
0x1C066333,
0x1D05A60B,
0x1D2ABBBF,
0x1F208C55,
0x1F49ADD3,
0x20361B66,
0x224BF031,
0x23536722,
0x244BF57E,
0x24923BD8,
0x257BE848,
0x264D0487,
0x274EA5,
0x27D29E1F,
0x28D36B61,
0x29B263C7,
0x2AA9CFE3,
0x2B3675E9,
0x2C2ADA17,
0x2C4DCB43,
0x2DCC6E34,
0x2DF5AB1B,
0x2ED98F0,
0x2FC00683,
0x31EEBF44,
0x3385D992,
0x3543B604,
0x35BA0578,
0x39A95B6D,
0x39DE7D00,
0x3A7DBC4D,
0x3BA421ED,
0x3C43A783,
0x3CBC8E6F,
0x3E2D20AD,
0x3E4D6437,
0x3EE8CB0C,
0x3FEAB969,
0x40514297,
0x40D12E7C,
0x40E1A51C,
0x412F1BEC,
0x4192386D,
0x431ADDA5,
0x431E0D94,
0x43476B6,
0x43EB93A2,
0x446890C,
0x46A5F3E5,
0x47BA9BE9,
0x49C8DDEC,
0x4A06007A,
0x4C080429,
0x4D3FD5C3,
0x4DD29238,
0x4DF89311,
0x4EDF7956,
0x4EFC6876,
0x5070C296,
0x50F82B0F,
0x51E5CBD0,
0x530128A3,
0x5384BECB,
0x55837632,
0x55A288D9,
0x56D790FE,
0x5795BE08,
0x57F9C39A,
0x58A7C810,
0x5A2B1966,
0x5B151B18,
0x5BB39607,
0x60530EB2,
0x605EF7E7,
0x638630AB,
0x6542AD49,
0x671A25D8,
0x68C23D9F,
0x695C93E,
0x6A23C530,
0x6C8091AA,
0x6E6D7291,
0x6F0EB96D,
0x6FD0A870,
0x7050F1D9,
0x706FDCF8,
0x7215B4C0,
0x722FB07D,
0x7482755E,
0x75F6139F,
0x761C682D,
0x78D1CD78,
0x790139C,
0x79241CB9,
0x7C502029,
0x7D92FAAE,
0x7E02F9FC,
0x7E1B5C23,
0x7F0EEBCE,
0x7FFF5E45,
0x820C8AFE,
0x823C41C7,
0x8371383A,
0x84B69C88,
0x84E97519,
0x84FF5946,
0x872005AF,
0x89EE5EC9,
0x8C130DEC,
0x8D348598,
0x8F2558B8,
0x942426DD,
0x94C3A4EA,
0x987DD3A8,
0x997C647,
0x99C72ACD,
0x9AAE4A77,
0x9AECEE9D,
0x9AF5AE36,
0x9BEB81C7,
0x9C540022,
0x9E9A34BF,
0xA069FA0B,
0xA17028C6,
0xA237FFD,
0xA2B4AF26,
0xA43AEF23,
0xA46AB306,
0xA5E20764,
0xA765780E,
0xA7BB42BB,
0xA8BD91,
0xA8E3C662,
0xAA6B50B9,
0xAABE8801,
0xABE82243,
0xACB55ACE,
0xAEABA764,
0xAF6320A,
0xB08D5363,
0xB113C3C4,
0xB1A855AA,
0xB2499A18,
0xB28879C,
0xB2B4A5F8,
0xB2D913AC,
0xB2E8E5D,
0xB53E4C74,
0xB7B1F365,
0xB9607452,
0xBF185652,
0xC02C526,
0xC04254D6,
0xC04831EE,
0xC13CC00D,
0xC14B2B73,
0xC255D18A,
0xC2A77DE3,
0xC4227DD,
0xC514DD3A,
0xC5B856DF,
0xC6ABD028,
0xC76F52AF,
0xC78CF46B,
0xC809BC6F,
0xC840DA59,
0xC8BD5746,
0xC9EE96C,
0xC9F2B9A3,
0xCA58A9A2,
0xCAC539D4,
0xCD15EF4A,
0xCD4C4206,
0xD08D944E,
0xD11E23AF,
0xD1AC8801,
0xD1E8AF0E,
0xD2D4B992,
0xD39D59FE,
0xD3B3D820,
0xD4521585,
0xD5C12917,
0xD5CECBC8,
0xD621714D,
0xD6C7D0BC,
0xD7B4AE61,
0xD8A149F4,
0xDA878CD3,
0xDB7AB1C,
0xDC34C5F5,
0xDD6D192D,
0xDD74C4AA,
0xDDB753E,
0xDDE046D,
0xDE1D0E6E,
0xDF501D87,
0xE04B3018,
0xE0A5349D,
0xE0E862FE,
0xE13C8768,
0xE1691845,
0xE3DDC3E3,
0xE3EC5960,
0xE4DCEC20,
0xE5D0AC3C,
0xE7343F5B,
0xE780B17C,
0xE7ED889A,
0xE8AB3908,
0xEB87007D,
0xEC1CF45C,
0xEC5BD65D,
0xED738F40,
0xEE071DAB,
0xEE284348,
0xEE49A7E1,
0xEFCA4BF2,
0xEFE16560,
0xF1A23A31,
0xF1B2E926,
0xF2FCA383,
0xF3C15F99,
0xF447385B,
0xF58EE96F,
0xF68C27E6,
0xF69A12E7,
0xF88B8814,
0xF8C6AC75,
0xF90823A6,
0xF90C28F8,
0xF95D3C18,
0xF95FEEF8,
0xF99326F,
0xF99E9D3E,
0xF99EC662,
0xFA0BD028,
0xFA0C6FAF,
0xFA19E151,
0xFAA39836,
0xFBB96962,
0xFBE5157B,
0xFC82B48C,
0xFDBD5224,
0xFE8F6427,
0xFECA8FB8,
0xFEEAB71E,
0xFF6D5D7E,
0xFF716DCD,
0x580B2A33
  };

  static LegacyMaterialLayer defaultLayerMaterial;

  LegacyTexture::LegacyTexture(uint32_t textureHash,
                               std::optional< std::string> albedoTexturePath, 
                               std::optional<std::string>& normalTexturePath, 
                               std::optional<std::string>& roughnessTexturePath, 
                               std::optional<std::string>& metallicTexturePath, 
                               std::optional<std::string>& heightTexturePath,
                               std::optional<std::string>& emissiveTexturePath,
                               TextureOrigin origin,
                               LegacyMaterialLayer* legacyMaterialLayer)
    : hash(textureHash), 
    albedoTexturePath(albedoTexturePath), 
    normalTexturePath(normalTexturePath),
    roughnessTexturePath(roughnessTexturePath), 
    metallicTexturePath(metallicTexturePath),
    heightTexturePath(heightTexturePath),
    emissiveTexturePath(emissiveTexturePath),
    origin(origin),
    legacyMaterialLayer(legacyMaterialLayer){

  }

  namespace {
    void saveMaterialLayer(UsdStageRefPtr stage, const LegacyMaterialLayer& materialLayer) {
      UsdPrim material = stage->DefinePrim(SdfPath { "/MaterialLayer" });
      auto roughnessBiasAttr = material.CreateAttribute(
      TfToken("roughnessBias"),
      SdfValueTypeNames->Float,
      true
      );
      roughnessBiasAttr.Set(materialLayer.roughnessBias);

      auto metallicBiasAttr = material.CreateAttribute(
      TfToken("metallicBias"),
      SdfValueTypeNames->Float,
      true
      );
      metallicBiasAttr.Set(materialLayer.metallicBias);

      auto normalStrengthAttr = material.CreateAttribute(
      TfToken("normalStrength"),
      SdfValueTypeNames->Float,
      true
      );
      normalStrengthAttr.Set(materialLayer.normalStrength);

      auto emissiveIntensityAttr = material.CreateAttribute(
      TfToken("emissiveIntensity"),
      SdfValueTypeNames->Float,
      true
      );
      emissiveIntensityAttr.Set(materialLayer.emissiveIntensity);

      auto displacementFactorAttr = material.CreateAttribute(
      TfToken("displacementFactor"),
      SdfValueTypeNames->Float,
      true
      );
      displacementFactorAttr.Set(materialLayer.displacementFactor);

      auto alphaTestReferenceValueAttr = material.CreateAttribute(
      TfToken("alphaTestReferenceValue"),
      SdfValueTypeNames->Int,
      true
      );
      alphaTestReferenceValueAttr.Set(materialLayer.alphaTestReferenceValue);

      auto softBlendFactorAttr = material.CreateAttribute(
      TfToken("softBlendFactor"),
      SdfValueTypeNames->Float,
      true
      );
      softBlendFactorAttr.Set(materialLayer.softBlendFactor);

      auto alphaBiasAttr = material.CreateAttribute(
      TfToken("alphaBias"),
      SdfValueTypeNames->Float,
      true
      );
      alphaBiasAttr.Set(materialLayer.alphaBias);

      auto featuresAttr = material.CreateAttribute(
      TfToken("features"),
      SdfValueTypeNames->UInt,
      true
      );
      featuresAttr.Set(static_cast<uint32_t>(materialLayer.features));
    }

    void loadMaterialLayer(UsdStageRefPtr stage, LegacyMaterialLayer& materialLayer) {
      UsdPrim material = stage->GetPrimAtPath(SdfPath("/MaterialLayer"));

      auto roughnessBiasAttr = material.GetAttribute(TfToken("roughnessBias"));
      roughnessBiasAttr.Get(&materialLayer.roughnessBias);

      auto metallicBiasAttr = material.GetAttribute(TfToken("metallicBias"));
      metallicBiasAttr.Get(&materialLayer.metallicBias);

      auto normalStrengthAttr = material.GetAttribute(TfToken("normalStrength"));
      normalStrengthAttr.Get(&materialLayer.normalStrength);

      auto emissiveIntensityAttr = material.GetAttribute(TfToken("emissiveIntensity"));
      emissiveIntensityAttr.Get(&materialLayer.emissiveIntensity);

      auto displacementFactorAttr = material.GetAttribute(TfToken("displacementFactor"));
      displacementFactorAttr.Get(&materialLayer.displacementFactor);

      auto alphaTestReferenceValueAttr = material.GetAttribute(TfToken("alphaTestReferenceValue"));
      alphaTestReferenceValueAttr.Get(&materialLayer.alphaTestReferenceValue);

      auto softBlendFactorAttr = material.GetAttribute(TfToken("softBlendFactor"));
      softBlendFactorAttr.Get(&materialLayer.softBlendFactor);

      auto alphaBiasAttr = material.GetAttribute(TfToken("alphaBias"));
      alphaBiasAttr.Get(&materialLayer.alphaBias);

      auto featuresAttr = material.GetAttribute(TfToken("features"));
      uint32_t features = 0;
      featuresAttr.Get(&features);
      materialLayer.features = static_cast<LegacyMaterialFeature>(features);
    }
  }


  void LegacyManager::load() {     
    wchar_t file_prefix[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
    std::filesystem::path path = file_prefix;
    path = path.parent_path();

    std::vector<std::thread> threads;
    std::mutex mtx;
    std::vector<std::filesystem::path> materialsFile;
    std::vector<std::filesystem::path> meshesFile;

    uint32_t threadCount = std::thread::hardware_concurrency() / 2;

    for (auto& entry : std::filesystem::directory_iterator(path / "MaterialsLayer")) {
      materialsFile.push_back(entry.path());
    }
    for (auto& entry : std::filesystem::directory_iterator(path / "MeshesLayer")) {
      meshesFile.push_back(entry.path());
    }
     

    for (uint32_t threadId = 0; threadId < threadCount; ++threadId) {
      threads.emplace_back([&,threadId] {
        uint32_t perThreadFile = materialsFile.size() / threadCount;
        for (uint32_t id = 0; id < perThreadFile; ++id) {
          std::filesystem::path usdPath = materialsFile[id+ perThreadFile* threadId];
          LegacyMaterialLayer legacyMaterial;
          uint32_t hash = std::stoul(usdPath.filename().stem());

          UsdStageRefPtr stage = UsdStage::Open(usdPath.u8string());

          loadMaterialLayer(stage, legacyMaterial);
          if (legacyMaterial.testFeatures(LegacyMaterialFeature::ParticleEmitter)) {
            RtxParticleSystemManager::load("", stage, legacyMaterial.particleDesc, legacyMaterial.particleMaterial, legacyMaterial.particleSpawnCtx);
          }
          std::unique_lock<std::mutex> lock(mtx);
          m_legacyMaterialsLayer.emplace(hash, std::move(legacyMaterial));
        }
      });
    }

    for (auto& t : threads)
      t.join();

    threads.clear();

    for (uint32_t threadId = 0; threadId < threadCount; ++threadId) {
      threads.emplace_back([&, threadId] {
        uint32_t perThreadFile = meshesFile.size() / threadCount;
        for (uint32_t id = 0; id < perThreadFile; ++id) {
          std::filesystem::path usdPath = meshesFile[id + perThreadFile * threadId];
          LegacyMeshLayer legacyMesh;
          XXH64_hash_t hash = std::stoull(usdPath.filename().stem());

          UsdStageRefPtr stage = UsdStage::Open(usdPath.u8string());
          UsdPrim mesh = stage->GetPrimAtPath(SdfPath("/MeshLayer"));

          UsdGeomXformable xformable(mesh);

          bool resetsXformStack;
          std::vector<UsdGeomXformOp> xformOps = xformable.GetOrderedXformOps(&resetsXformStack);

          for (const auto& op : xformOps) {
            if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
              GfVec3d translate;
              op.Get(&translate);
              legacyMesh.offset = Vector3(translate[0], translate[1], translate[2]);
            }
          }

          auto featuresAttr = mesh.GetAttribute(TfToken("features"));
          uint32_t features = 0;
          featuresAttr.Get(&features);
          legacyMesh.features = static_cast<LegacyMeshFeature>(features);

          auto overrideMaterialsAttr = mesh.GetAttribute(TfToken("overrideMaterial"));
          overrideMaterialsAttr.Get(&legacyMesh.overrideMaterial);

          if (legacyMesh.overrideMaterial) {
            loadMaterialLayer(stage, legacyMesh.materialLayer);
            if (legacyMesh.materialLayer.testFeatures(LegacyMaterialFeature::ParticleEmitter)) {
              RtxParticleSystemManager::load("", stage, legacyMesh.materialLayer.particleDesc, legacyMesh.materialLayer.particleMaterial, legacyMesh.materialLayer.particleSpawnCtx);
            }
          }
          std::unique_lock<std::mutex> lock(mtx);
          m_legacyMeshesLayer.emplace(hash, std::move(legacyMesh));
        }
        });
    }

    for (auto& t : threads)
      t.join();


  }

  void LegacyManager::save() {
    for (uint32_t hash : m_legacyMaterialsLayerModified) {
      auto it = m_legacyMaterialsLayer.find(hash);
      if (it != m_legacyMaterialsLayer.end()) {
        const LegacyMaterialLayer& materialLayer = it->second;
        wchar_t file_prefix[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
        std::filesystem::path path = file_prefix;
        path = path.parent_path();
        std::filesystem::path usdPath = path / "MaterialsLayer" / std::string(std::to_string(hash) + ".usda");

        UsdStageRefPtr stage = UsdStage::CreateNew(usdPath.u8string());

        saveMaterialLayer(stage, materialLayer);
        if (materialLayer.testFeatures(LegacyMaterialFeature::ParticleEmitter)) {
          RtxParticleSystemManager::save("", stage, materialLayer.particleDesc, materialLayer.particleMaterial, materialLayer.particleSpawnCtx);
        }
        stage->GetRootLayer()->Save();
      }
    }
    m_legacyMaterialsLayerModified.clear();
    for (XXH64_hash_t hash : m_legacyMeshesLayerModified) {
      auto it = m_legacyMeshesLayer.find(hash);
      if (it != m_legacyMeshesLayer.end()) {
        const LegacyMeshLayer& meshLayer = it->second;
        wchar_t file_prefix[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
        std::filesystem::path path = file_prefix;
        path = path.parent_path();
        std::filesystem::path usdPath = path / "MeshesLayer" / std::string(std::to_string(hash) + ".usda");

        UsdStageRefPtr stage = UsdStage::CreateNew(usdPath.u8string());


        UsdGeomMesh mesh = UsdGeomMesh::Define(stage, SdfPath { "/MeshLayer" });
        mesh.AddTranslateOp().Set(GfVec3d(meshLayer.offset.x, meshLayer.offset.y, meshLayer.offset.z));

        auto featuresAttr = mesh.GetPrim().CreateAttribute(
        TfToken("features"),
        SdfValueTypeNames->UInt,
        true
        );
        featuresAttr.Set(static_cast<uint32_t>(meshLayer.features));

        auto overrideMaterialAttr = mesh.GetPrim().CreateAttribute(
        TfToken("overrideMaterial"),
        SdfValueTypeNames->Bool,
        true
        );
        overrideMaterialAttr.Set(meshLayer.overrideMaterial);

        if (meshLayer.overrideMaterial) {
          saveMaterialLayer(stage, meshLayer.materialLayer);
          if (meshLayer.materialLayer.testFeatures(LegacyMaterialFeature::ParticleEmitter)) {
            RtxParticleSystemManager::save("", stage, meshLayer.materialLayer.particleDesc, meshLayer.materialLayer.particleMaterial, meshLayer.materialLayer.particleSpawnCtx);
          }
        }

        stage->GetRootLayer()->Save();
      }

    }
    m_legacyMeshesLayerModified.clear();

  }

  void LegacyManager::pushMeshMaterial(XXH64_hash_t meshHash) { 
    auto [it, _] = m_legacyMeshesLayer.try_emplace(meshHash, LegacyMeshLayer {});
  }

  void LegacyManager::pushToLoad(uint32_t texturehash, D3D9CommonTexture* texture){

    std::optional<std::string> albedoPath;
    std::optional<std::string> normalPath;
    std::optional<std::string> roughnessPath;
    std::optional<std::string> metallicPath;
    std::optional<std::string> heightPath;
    std::optional<std::string> emissivePath;

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
              std::filesystem::path emissiveTexturePath = entryPbr;
              emissiveTexturePath /= strHash;
              emissiveTexturePath += "_emissive.e.rtex.dds";
              if (std::filesystem::exists(emissiveTexturePath)) {
                emissivePath = emissiveTexturePath.string();
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
      else if (path.find("npc") != std::string::npos) {
        origin = TextureOrigin::NPC;
      }
      else if (path.find("parts") != std::string::npos) {
        origin = TextureOrigin::Parts;
      }
      else if (path.find("effect") != std::string::npos) {
        origin = TextureOrigin::Effect;
      }
    }
    auto [it,_] = m_legacyMaterialsLayer.try_emplace(texturehash, LegacyMaterialLayer {});
    LegacyMaterialLayer& material = it->second;

    if (std::find(emmodelParticleHash.begin(), emmodelParticleHash.end(), texturehash) != emmodelParticleHash.end()) {
      material.features |= LegacyMaterialFeature::Particle;
    }

    if (origin == TextureOrigin::Extend || origin == TextureOrigin::Parts || origin == TextureOrigin::NPC) {
      material.features |= LegacyMaterialFeature::NoFade;
    }
    if (origin == TextureOrigin::Extend || origin == TextureOrigin::Parts || origin == TextureOrigin::NPC || origin == TextureOrigin::Effect) {
      material.features |= LegacyMaterialFeature::RejectDecal;
    }
    if (origin == TextureOrigin::Emmodel) {
      material.normalStrength = -1.0f;
      material.features &= ~LegacyMaterialFeature::BackFaceCulling;
    }

    if (material.testFeatures(LegacyMaterialFeature::Particle)) {
      albedoPath.reset();
      normalPath.reset();
      roughnessPath.reset();
      metallicPath.reset();
      heightPath.reset();
    }

    if (material.testFeatures(LegacyMaterialFeature::Sky)) {
      normalPath.reset();
      roughnessPath.reset();
      metallicPath.reset();
      heightPath.reset();
    }
#ifndef REMIX_DEVELOPMENT
    if (material.testFeatures(LegacyMaterialFeature::Albedo) == false) {
      albedoPath.reset();
    }
    if (material.testFeatures(LegacyMaterialFeature::Normal) == false) {
      normalPath.reset();
    }
    if (material.testFeatures(LegacyMaterialFeature::Roughness) == false) {
      roughnessPath.reset();
    }
    if (material.testFeatures(LegacyMaterialFeature::Metallic) == false) {
      metallicPath.reset();
    }
    if (material.testFeatures(LegacyMaterialFeature::Height) == false) {
      heightPath.reset();
    }
#endif

    m_textures[0].emplace(texture, LegacyTexture { texturehash , albedoPath, normalPath, roughnessPath, metallicPath, heightPath, emissivePath, origin, &(it->second) });

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
    if (texture.emissiveTexturePath.has_value()) {
      loadAssetData(texture.emissiveTexturePath.value(), texture.managedTextures.managedEmissiveTexture);
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
      if (it->second.managedTextures.managedEmissiveTexture != nullptr)
        it->second.managedTextures.managedEmissiveTexture->requestMips(0);
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

  void LegacyManager::pushDirtyMaterialLayer(uint32_t _hash) {
    m_legacyMaterialsLayerModified.emplace(_hash);
  }

  void LegacyManager::pushDirtyMeshLayer(XXH64_hash_t _hash) {
    m_legacyMeshesLayerModified.emplace(_hash);
  }

  LegacyMaterialLayer::LegacyMaterialLayer() { 
    particleDesc = RtxParticleSystemManager::createGlobalParticleSystemDesc();
    particleMaterial = RtxParticleSystemManager::createGlobalParticleSystemMaterial();
    particleSpawnCtx = RtxParticleSystemManager::createGlobalParticleSystemSpawnContext();
  }

  void LegacyMaterialLayer::resetLayerMaterial() {
    *this = defaultLayerMaterial;
  }

  void LegacyMaterialLayer::showImguiSettings(uint32_t hash, LegacyManager& legacyManager){
    bool dirty = false;
    static LegacyMaterialLayer materiaLayerCopy;
    if (ImGui::Button("Reset Layer")) {
      resetLayerMaterial();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
      materiaLayerCopy = *this;
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
      *this = materiaLayerCopy;
    }

    if (ImGui::TreeNode("Features support")) {

      auto drawFeature = [&dirty, this](const char* str, LegacyMaterialFeature feature) {
        bool featureState = testFeatures(feature);
        dirty |= ImGui::Checkbox(str, &featureState);
        if (featureState) {
          features |= feature;
        } else {
          features &= ~feature;
        }
        };

      drawFeature("Albedo", LegacyMaterialFeature::Albedo);
      drawFeature("Normal", LegacyMaterialFeature::Normal);
      drawFeature("Roughness", LegacyMaterialFeature::Roughness);
      drawFeature("Metallic", LegacyMaterialFeature::Metallic);
      drawFeature("Height", LegacyMaterialFeature::Height);
      drawFeature("Emissive", LegacyMaterialFeature::Emissive);
      drawFeature("Sky", LegacyMaterialFeature::Sky);
      drawFeature("RejectDecal", LegacyMaterialFeature::RejectDecal);
      drawFeature("BackFaceCulling", LegacyMaterialFeature::BackFaceCulling);
      drawFeature("NoFade", LegacyMaterialFeature::NoFade);
      drawFeature("Water", LegacyMaterialFeature::Water);
      drawFeature("Particle", LegacyMaterialFeature::Particle);
      drawFeature("ParticleIgnoreLight", LegacyMaterialFeature::ParticleIgnoreLight);
      drawFeature("Decals", LegacyMaterialFeature::Decals);
      drawFeature("IgnoreOriginal", LegacyMaterialFeature::IgnoreOriginal);
      drawFeature("RainTexture", LegacyMaterialFeature::RainTexture);
      drawFeature("ParticleEmitter", LegacyMaterialFeature::ParticleEmitter);

      ImGui::TreePop();
    }

    if(ImGui::TreeNode("Params"))
    { 
      dirty |= ImGui::SliderInt("Alpha test reference value", &alphaTestReferenceValue, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);

      dirty |= ImGui::SliderFloat("Metallic Bias", &metallicBias, -1.0f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      dirty |= ImGui::SliderFloat("Roughness Bias", &roughnessBias, -1.0f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

      dirty |= ImGui::SliderFloat("Emissive Intensity", &emissiveIntensity, 0.0f, 100.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

      dirty |= ImGui::SliderFloat("Normal Strength", &normalStrength, -10.0f, 10.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      dirty |= ImGui::SliderFloat("Soft blend factor", &softBlendFactor, 0.0f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      dirty |= ImGui::SliderFloat("Alpha bias", &alphaBias, -1.0f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      ImGui::TreePop();
    }

    if (testFeatures(LegacyMaterialFeature::Height) && ImGui::TreeNode("Displacement params")) {
      dirty |= ImGui::SliderFloat("Displacement factor", &displacementFactor, -10.0f, 10.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      dirty |= ImGui::SliderFloat("Displacement Noise", &displacementNoise, 0.0f, 1.f, "%.03f", ImGuiSliderFlags_AlwaysClamp);
      ImGui::TreePop();
    }

    if (testFeatures(LegacyMaterialFeature::ParticleEmitter)) {
      dirty |= RtxParticleSystemManager::showImguiSettings(particleSpawnCtx, particleDesc, particleMaterial, Vector3(0.0f, 0.0f, 0.0f));
    }

    if (dirty) {
      legacyManager.pushDirtyMaterialLayer(hash);
    }
  }

  void LegacyMeshLayer::showImguiSettings(uint32_t meshHash, LegacyMaterialLayer* legacyMaterialLayer, uint32_t materialLayerHash, LegacyManager& legacyManager) {
    bool dirty = false;

    if (ImGui::Checkbox("Override material", &overrideMaterial)) {
      dirty = true;
      materialLayer = *legacyMaterialLayer;
    }
    if (overrideMaterial) {
      LegacyMaterialLayer* legacyMaterialLayer = &materialLayer;
      legacyMaterialLayer->showImguiSettings(materialLayerHash, legacyManager);
    }

    dirty |= ImGui::SliderFloat3("World position offset", offset.data, -4000.0f, 4000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

    auto drawFeature = [&dirty, this](const char* str, LegacyMeshFeature feature) {
      bool featureState = testFeatures(feature);
      dirty |= ImGui::Checkbox(str, &featureState);
      if (featureState) {
        features |= feature;
      } else {
        features &= ~feature;
      }
      };

    if (ImGui::TreeNode("Mesh Features")) {
      drawFeature("CustomBlend", LegacyMeshFeature::CustomBlend);
      drawFeature("FillHole", LegacyMeshFeature::FillHole);
      drawFeature("HideMesh", LegacyMeshFeature::HideMesh);
      drawFeature("NormalizeVertexColor", LegacyMeshFeature::NormalizeVertexColor);
      ImGui::TreePop();
    }
    if (dirty) {
      legacyManager.pushDirtyMeshLayer(meshHash);
    }
  }

}