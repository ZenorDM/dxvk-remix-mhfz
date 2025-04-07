#pragma once

#include <unordered_map>
#include <string>
#include "rtx_texture.h"
#include "d3d9.h"
#include "rtx_option.h"

namespace dxvk {
  enum class TextureState {
    None = 0,
    Loaded,
    Unloaded
  };

#define TextureLoadThreadCount 8u

  class RtxTextureManager;

  enum class TextureOrigin {
    None = 0,
    Stage,
    Emmodel,
    Extend,
  };

  enum class LegacyMaterialFeature : uint16_t {
    None = 0,
    Albedo = 1 << 0,
    Normal = 1 << 1,
    Roughness = 1 << 2,
    Metallic = 1 << 3,
    Height = 1 << 4,
    Emissive = 1 << 5,
    Sky = 1 << 7,
    RejectDecal = 1 << 8,
    BackFaceCulling = 1 << 9,
    NoFade = 1 << 10,
    Default = Albedo | Normal | Roughness | Metallic,
    All = Albedo | Normal | Roughness | Metallic | Height,
  };

  constexpr LegacyMaterialFeature operator~(LegacyMaterialFeature a) {
    return static_cast<LegacyMaterialFeature>(~static_cast<uint16_t>(a));
  }
  inline LegacyMaterialFeature& operator&=(LegacyMaterialFeature& a, LegacyMaterialFeature b) {
    return reinterpret_cast<LegacyMaterialFeature&>(reinterpret_cast<uint16_t&>(a) &= static_cast<uint16_t>(b));
  }
  constexpr LegacyMaterialFeature operator&(LegacyMaterialFeature a, LegacyMaterialFeature b) {
    return static_cast<LegacyMaterialFeature>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
  }
  inline LegacyMaterialFeature& operator|=(LegacyMaterialFeature& a, LegacyMaterialFeature b) {
    return reinterpret_cast<LegacyMaterialFeature&>(reinterpret_cast<uint16_t&>(a) |= static_cast<uint16_t>(b));
  }
  constexpr LegacyMaterialFeature operator|(LegacyMaterialFeature a, LegacyMaterialFeature b) {
    return static_cast<LegacyMaterialFeature>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
  }
  inline LegacyMaterialFeature& operator^=(LegacyMaterialFeature& a, LegacyMaterialFeature b) {
    return reinterpret_cast<LegacyMaterialFeature&>(reinterpret_cast<uint16_t&>(a) ^= static_cast<uint16_t>(b));
  }
  constexpr LegacyMaterialFeature operator^(LegacyMaterialFeature a, LegacyMaterialFeature b) {
    return static_cast<LegacyMaterialFeature>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
  }

  struct LegacyMaterialLayer {

    float roughnessBias = 0.0f;
    float metallicBias = 0.0f;
    float normalStrenght = 1.0f;

    float displacementFactor = 1.0f;

    float emissiveIntensity = 1.0f;

    Vector3 subsurfaceTransmittanceColor;
    float subsurfaceMeasurementDistance;
    Vector3 subsurfaceSingleScatteringAlbedo; // scatteringCoefficient / attenuationCoefficient
    float subsurfaceVolumetricAnisotropy;

    bool isSubsurfaceDiffusionProfile;

    // Cache Volumetric Properties
    Vector3 subsurfaceVolumetricAttenuationCoefficient; // scatteringCoefficient + absorptionCoefficient
    // Currently no need to cache scattering and absorption coefficient for single scattering simulation

    // SSS properties using Diffusion Profile
    Vector3 subsurfaceRadius;
    float subsurfaceRadiusScale;
    float subsurfaceMaxSampleRadius;

    bool thinFilmEnable;
    bool alphaIsThinFilmThickness;
    float thinFilmThicknessConstant;

    int alphaTestReferenceValue = 0;

    float softBlendFactor = 1.0;
    float alphaBias = 0.0;

    LegacyMaterialFeature features = LegacyMaterialFeature::Default;
    bool testFeatures(LegacyMaterialFeature feature) const {
      return (features & feature) != LegacyMaterialFeature::None;
    }
    void resetLayerMaterial();
  };

  enum class LegacyMeshFeature : uint16_t {
    None = 0,
    CustomBlend = 1 << 0,
    FillHole = 1 << 1,
    HideMesh = 1 << 2,
    NormalizeVertexColor = 1 << 3,
  };

  constexpr LegacyMeshFeature operator~(LegacyMeshFeature a) {
    return static_cast<LegacyMeshFeature>(~static_cast<uint16_t>(a));
  }
  inline LegacyMeshFeature& operator&=(LegacyMeshFeature& a, LegacyMeshFeature b) {
    return reinterpret_cast<LegacyMeshFeature&>(reinterpret_cast<uint16_t&>(a) &= static_cast<uint16_t>(b));
  }
  constexpr LegacyMeshFeature operator&(LegacyMeshFeature a, LegacyMeshFeature b) {
    return static_cast<LegacyMeshFeature>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
  }
  inline LegacyMeshFeature& operator|=(LegacyMeshFeature& a, LegacyMeshFeature b) {
    return reinterpret_cast<LegacyMeshFeature&>(reinterpret_cast<uint16_t&>(a) |= static_cast<uint16_t>(b));
  }
  constexpr LegacyMeshFeature operator|(LegacyMeshFeature a, LegacyMeshFeature b) {
    return static_cast<LegacyMeshFeature>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
  }
  inline LegacyMeshFeature& operator^=(LegacyMeshFeature& a, LegacyMeshFeature b) {
    return reinterpret_cast<LegacyMeshFeature&>(reinterpret_cast<uint16_t&>(a) ^= static_cast<uint16_t>(b));
  }
  constexpr LegacyMeshFeature operator^(LegacyMeshFeature a, LegacyMeshFeature b) {
    return static_cast<LegacyMeshFeature>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
  }

  struct LegacyMeshLayer {
    LegacyMaterialLayer materialLayer;
    Vector3 offset;

    bool overrideMaterial;

    LegacyMeshFeature features = LegacyMeshFeature::NormalizeVertexColor;

    bool testFeatures(LegacyMeshFeature feature) const {
      return (features & feature) != LegacyMeshFeature::None;
    }
  };

  struct LegacyManagedTextures {
    Rc<ManagedTexture> managedAlbedoTexture;
    Rc<ManagedTexture> managedNormalTexture;
    Rc<ManagedTexture> managedRoughnessTexture;
    Rc<ManagedTexture> managedMetallicTexture;
    Rc<ManagedTexture> managedHeightTexture;
  };

  struct LegacyTexture {
    LegacyTexture() = default;
    LegacyTexture(uint32_t textureHash, 
                  std::optional< std::string> albedoTexturePath, 
                  std::optional<std::string>& normalTexturePath, 
                  std::optional<std::string>& roughnessTexturePath, 
                  std::optional<std::string>& metallicTexturePath, 
                  std::optional<std::string>& heightTexturePath,
                  TextureOrigin origin,
                  LegacyMaterialLayer* legacyMaterialLayer);
    std::optional< std::string> albedoTexturePath;
    std::optional< std::string> normalTexturePath;
    std::optional< std::string> roughnessTexturePath;
    std::optional< std::string> metallicTexturePath;
    std::optional< std::string> heightTexturePath;
    LegacyManagedTextures managedTextures;
    LegacyMaterialLayer* legacyMaterialLayer;
    uint32_t hash;
    TextureState state = TextureState::None;
    TextureOrigin origin = TextureOrigin::None;
  };
  class D3D9CommonTexture;
  class LegacyManager {
  public:
    void load();
    void save();
    void pushMeshMaterial(XXH64_hash_t meshHash);

    void pushToLoad(uint32_t texturehash, D3D9CommonTexture* texture);

    void loadFile(LegacyTexture& texture, RtxTextureManager& textureManager);
    void destroyTexture(D3D9CommonTexture* texture);
    void updateLoadingThread(uint32_t threadID, RtxTextureManager& textureManager);
    LegacyManagedTextures getTextures(D3D9CommonTexture* texture);
    uint32_t getTextureHash(D3D9CommonTexture* texture);
    uint32_t getTextureOrigin(D3D9CommonTexture* texture);

    LegacyMaterialLayer* getLegacyMaterialLayer(D3D9CommonTexture* texture);
    LegacyMeshLayer* getLegacyMeshLayer(XXH64_hash_t meshHash);

  private:

    std::unordered_map<D3D9CommonTexture*, LegacyTexture> m_textures[TextureLoadThreadCount]; // Orihandle to texture
    std::unordered_map<uint32_t, LegacyMaterialLayer> m_legacyMaterialsLayer;
    std::unordered_map<XXH64_hash_t, LegacyMeshLayer> m_legacyMeshesLayer;
  };
}