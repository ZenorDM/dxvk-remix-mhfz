#pragma once
#include <unordered_map>
#include <string>
#include "../util/util_vector.h"
#include "../dxvk/dxvk_include.h"
#include "rtx_types.h"
namespace dxvk {


  struct AreaLightDataDir {
    Vector3 lightRadiance = Vector3(1.6f, 1.8f, 2.0f);
    Vector3 lightDirection = Vector3(-0.2f, -1.0f, 0.4f);
  };

  struct AreaLightDataPoint {
    Vector3 lightRadiance = Vector3(1.6f, 1.8f, 2.0f);
    Vector3 lightPosition = Vector3(-0.2f, -1.0f, 0.4f);
    float lightRadius = 1.0f;
  };

  struct AreaLightDataRect {
    Vector3 lightRadiance = Vector3(1.6f, 1.8f, 2.0f);
    Vector3 lightPosition = Vector3(-0.2f, -1.0f, 0.4f);
    Vector3 rotation = Vector3(0.f, 0.f, 0.f);
    Vector2 dimensions = Vector2(1.f, 1.f);

    Matrix4 transform;

    void buildMatrix();
  };

  struct AreaData {
    // light values
    Vector3 lightRadiance = Vector3(1.6f, 1.8f, 2.0f);
    Vector3 lightDirection = Vector3(-0.2f, -1.0f, 0.4f);
    std::vector<AreaLightDataDir > dirLightsData;
    std::vector<AreaLightDataPoint > pointLightsData;
    std::vector<AreaLightDataRect > rectLightsData;

    // volumetric values
    Vector3 transmittanceColor = Vector3(0.999f, 0.999f, 0.999f);
    Vector3 singleScatteringAlbedo = Vector3(0.999f, 0.999f, 0.999f);
    float transmittanceMeasurementDistanceMeters = 200.0f;

    float anisotropy = 0.0f;
    bool enableHeterogeneousFog = false;
    float noiseFieldSpatialFrequency = 0.01f;
    int noiseFieldOctaves = 3;
    float noiseFieldDensityScale = 1.0f;
    float skyBrightness = 1.0f;
    bool lightDirty = true;

  };

  class AreaManager {
  public:
    void setArea(uint32_t area);
    void setTime(uint32_t time) {
      m_time = time;
    }

    uint32_t getTime() {
      return m_time;
    }

    void setQuestID(uint32_t questID) {
      m_questID = questID;
    }

    uint32_t getQuestID() {
      return m_questID;
    }

    void update();

    uint32_t getCurrentAreaID();
    AreaData& getAreaData(uint32_t area);
    AreaData& getCurrentAreaData();
    void save();
    void load();
    void setOriLightDir(const Vector3& lightDir) {
      m_oriLightDir = lightDir;
    }
    const Vector3& getOriLightDir() const {
      return m_oriLightDir;
    }

    float getSkyBrightness() {
      return getCurrentAreaData().skyBrightness * m_currentSkyBrightnessAtt;
    }

    float getRoughnessFactor() {
      return m_currentRoughnessFactor;
    }

    float getLightFactor() {
      return m_lightFactor;
    }

    void rainRegister() {
      m_currentSkyBrightnessAtt = 0.05f;
      m_currentRoughnessFactor = 0.5f;
      if (m_lightFactor == 1) {
        getCurrentAreaData().lightDirty = true;
      }
      m_lightFactor = 0.15f;
    }

    std::unordered_map<uint32_t, AreaData>& getAreas() {
      return m_areas;
    }

  private:

    std::unordered_map<uint32_t, AreaData> m_areas;
    Vector3 m_oriLightDir;
    uint32_t m_currentArea = 0;
    uint32_t m_time = 0;
    uint32_t m_questID = 0;
    float m_currentSkyBrightnessAtt = 1.0f;
    float m_currentRoughnessFactor = 0.0f;
    float m_lightFactor = 1.0f;
  };
}