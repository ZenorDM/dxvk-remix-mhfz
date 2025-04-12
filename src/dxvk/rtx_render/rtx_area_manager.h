#pragma once
#include <unordered_map>
#include <string>
#include "../util/util_vector.h"
#include "../dxvk/dxvk_include.h"
#include "rtx_types.h"
namespace dxvk {

  struct AreaData {
    // light values
    Vector3 lightRadiance = Vector3(1.6f, 1.8f, 2.0f);
    Vector3 lightDirection = Vector3(-0.2f, -1.0f, 0.4f);

    // volumetric values
    Vector3 transmittanceColor = Vector3(0.999f, 0.999f, 0.999f);
    Vector3 singleScatteringAlbedo = Vector3(0.999f, 0.999f, 0.999f);
    float transmittanceMeasurementDistanceMeters = 200.0f;

    float anisotropy = 0.0f;
    bool enableHeterogeneousFog = false;
    float noiseFieldSpatialFrequency = 0.01f;
    int noiseFieldOctaves = 3;
    float noiseFieldDensityScale = 1.0f;
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
    void update();
    const char* getCurrentAreaName();
    uint32_t getCurrentAreaID();
    AreaData& getAreaData(uint32_t area);
    AreaData& getCurrentAreaData();
  private:

    std::unordered_map<uint32_t, AreaData> m_areas;
    uint32_t m_currentArea = 0;
    uint32_t m_time = 0;
  };
}