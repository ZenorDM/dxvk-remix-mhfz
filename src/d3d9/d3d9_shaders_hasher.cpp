#include "d3d9_shaders_hasher.h"
#include "../util/log/log.h"
#include "../util/util_string.h"
#include <json/json.hpp>
#include <filesystem>

using json = nlohmann::json;

namespace dxvk {

  namespace helpers {
    void directCopy(const void* value, std::vector<float>& dst, size_t size) {
      dst.resize(size);

      memcpy(dst.data(), (float*)value, size);
    }
    inline uint32_t compute_crc32(const uint8_t* data, size_t size) {
      static constexpr uint32_t crc32_table[256] = { // CRC polynomial 0xEDB88320
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
      };

      uint32_t crc = 0xFFFFFFFF;
      for (; size != 0; --size, ++data)
        crc = (crc >> 8) ^ crc32_table[(crc ^ (*data)) & 0xFF];
      return ~crc;
    }
  }

  ShadersHasher::ShadersHasher() { }


  void ShadersHasher::parseShaderAsm(uint32_t shader_hash, ShaderDesc& shaderDesc, ShaderType shaderType) {
    wchar_t file_prefix[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
    std::filesystem::path asmPath = file_prefix;
    asmPath = asmPath.parent_path();
    if (shaderType == ShaderType::Vertex) {
      asmPath /= "DumpShader/Vertex/asm";
      std::string hash = std::to_string(shader_hash);
      asmPath /= hash + ".asm";
      std::ifstream file(asmPath);

      std::string line;
      while (getline(file, line)) {
        if (line.find("gFadeColor") != std::string::npos) {
          shaderDesc.constantVs.emplace(2);

        }
        if (line.find("gMaterialDiffuse") != std::string::npos) {
          shaderDesc.constantVs.emplace(170);

        }
        if (line.find("gMaterialAlbedo") != std::string::npos) {
          shaderDesc.constantVs.emplace(171);

        }
        if (line.find("gMatPower") != std::string::npos) {
          shaderDesc.constantVs.emplace(4);

        }
        if (line.find("gAmbientColor") != std::string::npos) {
          shaderDesc.constantVs.emplace(1);

        }
        if (line.find("gDepthView") != std::string::npos) {
          shaderDesc.constantVs.emplace(186);
        }
      }
      file.close();
    } else {
      asmPath /= "DumpShader/Pixel/asm";
      std::string hash = std::to_string(shader_hash);
      asmPath /= hash + ".asm";
      std::ifstream file(asmPath);

      std::string line;
      while (getline(file, line)) {
        if (line.find("gMaterialDiffuse") != std::string::npos) {
          shaderDesc.constantPs.emplace(170);
        }
        if (line.find("gMaterialAlbedo") != std::string::npos) {
          shaderDesc.constantPs.emplace(171);
        }

      }
      file.close();
    }

  }

  void ShadersHasher::hashShader(VkShaderStageFlagBits shaderType, const DWORD* pFunction, uint64_t shader) {


    uint32_t size = sizeof(DWORD);
    for (int i = 0; pFunction[i] != 0x0000FFFF; ++i)
      size += sizeof(DWORD);

    uint32_t shader_hash = helpers::compute_crc32(reinterpret_cast<const uint8_t*>(pFunction), size);
    ShaderDesc desc;
    switch (shaderType) {
    case VK_SHADER_STAGE_VERTEX_BIT:
    {
      m_shaders[(uint32_t) ShaderType::Vertex].try_emplace(shader_hash, std::move(desc));
      m_shadersToHash[(uint32_t) ShaderType::Vertex].try_emplace(shader, shader_hash);
      break;
    }
    case VK_SHADER_STAGE_FRAGMENT_BIT:
    {
      m_shaders[(uint32_t) ShaderType::Pixel].try_emplace(shader_hash, std::move(desc));
      m_shadersToHash[(uint32_t) ShaderType::Pixel].try_emplace(shader, shader_hash);
      break;
    }
    default:
      Logger::err("Unsupported Shader Type");
      break;
    }
  }

  void ShadersHasher::bindShader(VkShaderStageFlagBits shaderType, uint64_t shader) {
    switch (shaderType) {
    case VK_SHADER_STAGE_VERTEX_BIT:
    {
      m_currentBindedShader[(uint32_t) ShaderType::Vertex] = shader;
      m_shadersBinded[(uint32_t) ShaderType::Vertex].emplace(shader);
      break;
    }
    case VK_SHADER_STAGE_FRAGMENT_BIT:
    {
      m_currentBindedShader[(uint32_t) ShaderType::Pixel] = shader;
      m_shadersBinded[(uint32_t) ShaderType::Pixel].emplace(shader);
      break;
    }
    default:
      Logger::err("Unsupported Shader Type");
      break;
    }

    m_preUIBindedThisFrame |= isShaderBindedHasFlag(ShaderFlags::UI_Hook);
  }

  bool ShadersHasher::isPreUIBinded() {
    return m_preUIBindedThisFrame;
  }

  bool ShadersHasher::isShaderBindedActivated() {

    for (uint32_t i = 0; i < (uint32_t) ShaderType::Count; ++i) {
      auto it = m_shadersToHash[i].find(m_currentBindedShader[i]);
      if (it != m_shadersToHash[i].end()) {
        auto itShader = m_shaders[i].find(it->second);
        if (itShader != m_shaders[i].end() && itShader->second.activated == false) {
          return false;
        }
      }
    }

    return true;
  }

  bool ShadersHasher::isShaderBindedConstant(uint32_t constant, ShaderType shaderType) {

    uint32_t i = (uint32_t) shaderType;

    {
      auto it = m_shadersToHash[i].find(m_currentBindedShader[i]);
      if (it != m_shadersToHash[i].end()) {
        auto itShader = m_shaders[i].find(it->second);
        if (shaderType == ShaderType::Vertex) {
          if (itShader != m_shaders[i].end() && itShader->second.constantVs.find(constant) != itShader->second.constantVs.end()) {
            return true;
          }
        } else {
          if (itShader != m_shaders[i].end() && itShader->second.constantPs.find(constant) != itShader->second.constantPs.end()) {
            return true;
          }
        }
      }
    }

    return false;
  }

  void ShadersHasher::reset() {
    m_preUIBindedThisFrame = false;

    for (uint32_t i = 0; i < (uint32_t) ShaderType::Count; ++i) {
      m_shadersBinded[i].clear();
    }
  }

  void ShadersHasher::pushConstant(DxsoProgramTypes::DxsoProgramType shaderType, D3D9ConstantType constantType, UINT startRegister, UINT vector4fCount, const void* pConstantData) {
    uint32_t hash = 0;

    auto itShader = m_shadersToHash[(uint32_t) shaderType].find(m_currentBindedShader[(uint32_t) shaderType]);
    if (itShader != m_shadersToHash[(uint32_t) shaderType].end()) {
      hash = itShader->second;
    }

    bool bindedWorldMatrix = shaderType == DxsoProgramTypes::DxsoProgramType::VertexShader && startRegister == 195;
    if (bindedWorldMatrix) {
      auto itShaderDesc = m_shaders[(uint32_t) shaderType].find(hash);
      if (itShaderDesc != m_shaders[(uint32_t) shaderType].end()) {
        itShaderDesc->second.bindedWorldMatrix = bindedWorldMatrix;
      }
    }

    auto [it, inserted] = m_constants[(uint32_t) constantType].try_emplace(hash);
    auto bindingIt = std::find_if(it->second.begin(), it->second.end(), [startRegister](const Constant& constant) { return constant.m_startRegister == startRegister; });

    std::vector<float> cpy;

    helpers::directCopy(pConstantData, cpy, vector4fCount * 4 * sizeof(float));

    if (bindingIt == it->second.end()) {
      Constant constant = Constant { startRegister, vector4fCount * sizeof(float), std::move(cpy) };
      it->second.push_back(std::move(constant));
    } else {
      bindingIt->m_copy = std::move(cpy);
    }

  }

  bool ShadersHasher::isShaderWorldBinded() {
    for (uint32_t i = 0; i < (uint32_t) ShaderType::Count; ++i) {
      auto it = m_shadersToHash[i].find(m_currentBindedShader[i]);
      if (it != m_shadersToHash[i].end()) {
        auto itShader = m_shaders[i].find(it->second);
        if (itShader != m_shaders[i].end() && itShader->second.bindedWorldMatrix) {
          return true;
        }
      }
    }
    return false;
  }

  bool ShadersHasher::isShaderHashBinded(uint32_t hash, ShaderType shaderType) {
    auto it = std::find_if(m_shadersToHash[(uint32_t) shaderType].begin(), m_shadersToHash[(uint32_t) shaderType].end(), [hash](const std::pair<uint64_t, XXH64_hash_t>& p) {
      return p.second == hash;
    });
    if (it != m_shadersToHash[(uint32_t) shaderType].end()) {
      return m_shadersBinded[(uint32_t) shaderType].find(it->first) != m_shadersBinded[(uint32_t) shaderType].end();
    }
    return false;
  }

  static constexpr char* ShaderTypeStr[(uint32_t) ShaderType::Count] { "VertexShaders", "PixelShaders" };

  void to_json(json& j, const ShaderDesc& p) {
    j = json { {"Activate", p.activated}, {"ShaderFlags", p.flags} , {"ConstantVs", p.constantVs}, {"ConstantPs", p.constantPs} };
  }

  void from_json(const json& j, ShaderDesc& p) {
    j.at("Activate").get_to(p.activated);
    j.at("ShaderFlags").get_to(p.flags);
    j.at("ConstantVs").get_to(p.constantVs);
    j.at("ConstantPs").get_to(p.constantPs);
  }

  void ShadersHasher::loadProfil() {
    wchar_t file_prefix[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
    std::filesystem::path dump_path = file_prefix;
    dump_path = dump_path.parent_path();
    dump_path /= "ShaderProfil.json";

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
    for (uint32_t i = 0; i < (uint32_t) ShaderType::Count; ++i) {
      m_shaders[i] = j[ShaderTypeStr[i]].get<ShadersHashList>();
#ifdef REMIX_DEVELOPMENT
      for (auto& [hash, desc] : m_shaders[i]) {
        parseShaderAsm(hash, desc, (ShaderType) i);
      }
#endif
    }

  }

  void ShadersHasher::saveProfil() {

    json j;
    auto saveShaders = [&j](const char* _shaderType, ShadersHashList& _list) {
      j[_shaderType] = _list;
      };


    wchar_t file_prefix[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
    std::filesystem::path dump_path = file_prefix;
    dump_path = dump_path.parent_path();
    dump_path /= "ShaderProfil.json";

    for (uint32_t i = 0; i < (uint32_t) ShaderType::Count; ++i) {
      saveShaders(ShaderTypeStr[i], m_shaders[i]);
    }

    std::ofstream o { dump_path.c_str() };
    o << j;
  }

  bool ShadersHasher::isShaderBindedHasFlag(ShaderFlags flags) {
    for (uint32_t i = 0; i < (uint32_t) ShaderType::Count; ++i) {
      auto it = m_shadersToHash[i].find(m_currentBindedShader[i]);
      if (it != m_shadersToHash[i].end()) {
        auto itShader = m_shaders[i].find(it->second);
        if (itShader != m_shaders[i].end() && (itShader->second.flags & flags) != ShaderFlags::None) {
          return true;
        }
      }
    }
    return false;
  }

  const std::vector<Constant>& ShadersHasher::getConstants(D3D9ConstantType constantType, uint32_t hash, bool& founded) const {
    auto it = m_constants[(uint32_t) constantType].find(hash);
    if (it != m_constants[(uint32_t) constantType].end()) {
      founded = true;
      return it->second;
    }
    founded = false;
    return m_constants[(uint32_t) constantType].end()->second;
  }

}
