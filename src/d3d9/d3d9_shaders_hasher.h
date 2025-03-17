#pragma once
#include <vulkan/vulkan_core.h>
#include <unordered_map>
#include <unordered_set>
#include <d3d9.h>
#include "../dxso/dxso_common.h"
#include "d3d9_constant_set.h"

namespace dxvk {

  enum class ShaderFlags {
    None = 0,
    UI_Hook = 1 << 0
  };

  constexpr ShaderFlags operator~(ShaderFlags a) {
    return static_cast<ShaderFlags>(~static_cast<uint32_t>(a));
  }
  inline ShaderFlags& operator&=(ShaderFlags& a, ShaderFlags b) {
    return reinterpret_cast<ShaderFlags&>(reinterpret_cast<uint32_t&>(a) &= static_cast<uint32_t>(b));
  }
  constexpr ShaderFlags operator&(ShaderFlags a, ShaderFlags b) {
    return static_cast<ShaderFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
  }
  inline ShaderFlags& operator|=(ShaderFlags& a, ShaderFlags b) {
    return reinterpret_cast<ShaderFlags&>(reinterpret_cast<uint32_t&>(a) |= static_cast<uint32_t>(b));
  }
  constexpr ShaderFlags operator|(ShaderFlags a, ShaderFlags b) {
    return static_cast<ShaderFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
  }
  inline ShaderFlags& operator^=(ShaderFlags& a, ShaderFlags b) {
    return reinterpret_cast<ShaderFlags&>(reinterpret_cast<uint32_t&>(a) ^= static_cast<uint32_t>(b));
  }
  constexpr ShaderFlags operator^(ShaderFlags a, ShaderFlags b) {
    return static_cast<ShaderFlags>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
  }

  enum class ShaderType {
    Vertex = 0,
    Pixel = 1,
    Count = 2,
  };

  struct ShaderDesc
  {
    ShaderFlags flags = ShaderFlags::None;
    bool activated = true;

    bool bindedWorldMatrix = false;
  };

  struct Constant {
    uint32_t m_startRegister;
    uint32_t m_vector4Count;

    std::vector<float> m_copy;
  };

  using ConstantList = std::unordered_map<uint32_t, std::vector<Constant>>;
  using ShadersHashList = std::unordered_map<uint32_t, ShaderDesc>;
  using ShaderToHash = std::unordered_map<uint64_t, uint32_t>;
  using ShaderBinded = std::unordered_set<uint64_t>;

  class ShadersHasher {

  public:

    ShadersHasher();

    void hashShader(VkShaderStageFlagBits shaderType, const DWORD* pFunction, uint64_t shader);

    void bindShader(VkShaderStageFlagBits shaderType, uint64_t shader);

    bool isPreUIBinded();

    bool isShaderBindedActivated();
    void reset();

    ShadersHashList& geShadersHashList(ShaderType shaderType) {
      return m_shaders[(uint32_t) shaderType];
    }

    void pushConstant(DxsoProgramTypes::DxsoProgramType shaderType, D3D9ConstantType constantType, UINT startRegister, UINT vector4fCount ,const void* pConstantData);

    bool isShaderWorldBinded();

    bool isShaderHashBinded(uint32_t hash, ShaderType shaderType);

    void loadProfil();

    void saveProfil();
    bool isShaderBindedHasFlag(ShaderFlags flags);

    const std::vector<Constant>& getConstants(D3D9ConstantType constantType, uint32_t hash, bool& founded) const;

  private:

    ConstantList m_constants[3];

    ShadersHashList m_shaders[(uint32_t)ShaderType::Count];

    ShaderToHash m_shadersToHash[(uint32_t) ShaderType::Count];

    ShaderBinded m_shadersBinded[(uint32_t) ShaderType::Count];

    uint64_t m_currentBindedShader[(uint32_t) ShaderType::Count];


    //Every draw after fall in rasterization
    bool m_preUIBindedThisFrame = false;
  };

}