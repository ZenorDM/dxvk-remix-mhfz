#include "rtx_area_manager.h"
#include <array>
#include "../../util/util_json.h"

namespace dxvk {

    void AreaManager::setArea(uint32_t area)
    {
      if (m_currentArea != area)
      {
        m_currentRoughnessFactor = 0.0f;
        m_lightFactor = 1.0f;
        m_currentSkyBrightnessAtt = 1.0f;
        m_currentArea = area;
        auto [it, emplaced] = m_areas.try_emplace(area, AreaData{});
        it->second.lightDirty = true;
        if (emplaced){
          it->second.lightDirection = m_oriLightDir;
        }
      }
    }

    void AreaManager::update() {

    }

    uint32_t AreaManager::getCurrentAreaID(){
      return m_currentArea;
    }

    AreaData& AreaManager::getAreaData(uint32_t area){
      return m_areas[area];
    }

    AreaData& AreaManager::getCurrentAreaData() {
      return m_areas[m_currentArea];
    }

    void to_json(json& j, const AreaLightDataDir& p) {
      j = json {{"LightRadiance", p.lightRadiance },
                 {"LightDirection", p.lightDirection},};
    }
    void from_json(const json& j, AreaLightDataDir& p) {
      j.at("LightRadiance").get_to(p.lightRadiance);
      j.at("LightDirection").get_to(p.lightDirection);
    }

    void to_json(json& j, const AreaLightDataPoint& p) {
      j = json { {"LightRadiance", p.lightRadiance },
                 {"LightPosition", p.lightPosition },
                 {"LightRadius", p.lightRadius}, };
    }
    void from_json(const json& j, AreaLightDataPoint& p) {
      j.at("LightRadiance").get_to(p.lightRadiance);
      j.at("LightPosition").get_to(p.lightPosition);
      j.at("LightRadius").get_to(p.lightRadius);
    }

    void to_json(json& j, const AreaLightDataRect& p) {
      j = json { {"LightRadiance", p.lightRadiance },
                 {"LightPosition", p.lightPosition },
                 {"LightRotation", p.rotation},
                 {"LightDimensions", p.dimensions}, };
    }
    void from_json(const json& j, AreaLightDataRect& p) {
      j.at("LightRadiance").get_to(p.lightRadiance);
      j.at("LightPosition").get_to(p.lightPosition);
      j.at("LightRotation").get_to(p.rotation);
      j.at("LightDimensions").get_to(p.dimensions);
    }


    void to_json(json& j, const AreaData& p) {
      j = json {
                {"DirLightsData", p.dirLightsData},
                {"PointLightsData", p.pointLightsData},
                {"RectLightsData", p.rectLightsData},
                {"SkyBrightness", p.skyBrightness},
                {"TransmittanceColor", p.transmittanceColor},
                {"SingleScatteringAlbedo", p.singleScatteringAlbedo},
                {"TransmittanceMeasurementDistanceMeters", p.transmittanceMeasurementDistanceMeters},
                {"Anisotropy", p.anisotropy},
                {"EnableHeterogeneousFog", p.enableHeterogeneousFog},
                {"NoiseFieldSpatialFrequency", p.noiseFieldSpatialFrequency},
                {"NoiseFieldOctaves", p.noiseFieldOctaves},
                {"NoiseFieldDensityScale", p.noiseFieldDensityScale}};
    }

    void from_json(const json& j, AreaData& p) {
      //j.at("LightRadiance").get_to(p.lightRadiance);
      //j.at("LightDirection").get_to(p.lightDirection);

      //p.dirLightsData.push_back(AreaLightDataDir { p.lightRadiance, p.lightDirection });

      j.at("DirLightsData").get_to(p.dirLightsData);
      j.at("PointLightsData").get_to(p.pointLightsData);
      j.at("RectLightsData").get_to(p.rectLightsData);
      for (AreaLightDataRect& light : p.rectLightsData) {
        light.buildMatrix();
      }
      j.at("SkyBrightness").get_to(p.skyBrightness);
      j.at("TransmittanceColor").get_to(p.transmittanceColor);
      j.at("SingleScatteringAlbedo").get_to(p.singleScatteringAlbedo);
      j.at("TransmittanceMeasurementDistanceMeters").get_to(p.transmittanceMeasurementDistanceMeters);
      j.at("Anisotropy").get_to(p.anisotropy);
      j.at("EnableHeterogeneousFog").get_to(p.enableHeterogeneousFog);
      j.at("NoiseFieldSpatialFrequency").get_to(p.noiseFieldSpatialFrequency);
      j.at("NoiseFieldOctaves").get_to(p.noiseFieldOctaves);
      j.at("NoiseFieldDensityScale").get_to(p.noiseFieldDensityScale);

    }

    void AreaManager::load() {
      {
        wchar_t file_prefix[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
        std::filesystem::path dump_path = file_prefix;
        dump_path = dump_path.parent_path();
        dump_path /= "Areas.json";

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

        m_areas = j.get<std::unordered_map<uint32_t, AreaData>>();
      }
    }

    void AreaManager::save() {
      {
        json j;

        wchar_t file_prefix[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
        std::filesystem::path dump_path = file_prefix;
        dump_path = dump_path.parent_path();
        dump_path /= "Areas.json";

        j = m_areas;

        std::ofstream o { dump_path.c_str() };
        o << j;
      }
    }

    void AreaLightDataRect::buildMatrix() {
      auto radians = [](float angle) {
        return angle * (3.1415 / 180);
        };
      float xR = radians(rotation.x);
      float yR = radians(rotation.y);
      float zR = radians(rotation.z);
      
      Matrix4 Rx {
        Vector4{ 1.0, 0.0, 0.0, 0.0},
        Vector4{ 0.0, cos(xR), -sin(xR), 0.0},
        Vector4{ 0.0, sin(xR), cos(xR), 0.0},
        Vector4(0.0, 0.0, 0.0 ,1.0)
      };
      Matrix4 Ry {
        Vector4{ cos(yR), 0.0, sin(yR) , 0.0},
        Vector4{ 0.0, 1.0, 0.0 , 0.0},
        Vector4{ -sin(yR), 0, cos(yR) , 0.0},
        Vector4(0.0, 0.0, 0.0 ,1.0)
      };
      Matrix4 Rz {
        Vector4{ cos(zR), -sin(zR), 0.0 , 0.0},
        Vector4{ sin(zR), cos(zR), 0.0 , 0.0},
        Vector4{ 0, 0, 1, 0.0},
        Vector4(0.0, 0.0, 0.0 ,1.0)
      };
      transform = Rx * Ry * Rz;
      transform.data[3] = Vector4(lightPosition,1.0);
    }

}