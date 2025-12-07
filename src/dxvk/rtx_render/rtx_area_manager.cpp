#include "rtx_area_manager.h"
#include <array>


#include "../../lssusd/usd_include_begin.h"
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usd/primRange.h>
#include "../../lssusd/usd_include_end.h"

#include "imgui/imgui.h"
#include "rtx_light_manager.h"

using namespace pxr;

namespace dxvk {

    void AreaManager::setArea(uint32_t area)
    {
      if (m_currentArea != area && ( area != 200 || m_currentArea != 500))
      {
        m_currentRoughnessFactor = 0.0f;
        m_lightFactor = 1.0f;
        m_currentSkyBrightnessAtt = 1.0f;
        m_currentArea = area;
        auto [it, emplaced] = m_areas.try_emplace(area, AreaData{});
        it->second.lightDirty = true;
      }
    }

    void AreaManager::update() { 
    }

    void AreaManager::setDay() {
      std::unordered_map<uint32_t, uint32_t> nightToDay
      {
        {200, 500 },
        {173, 501 },
        {174, 501 },
        {175, 501 },
        {256, 502 }
      };
      auto it = nightToDay.find(getCurrentAreaID());
      if(it != nightToDay.end())
        setArea(it->second);
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

    void AreaManager::load() {
        wchar_t file_prefix[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
        std::filesystem::path path = file_prefix;
        path = path.parent_path();

        for (uint32_t areaId = 0; areaId <= 502; ++areaId){
          std::filesystem::path usdPath = path / "Area" / std::string(std::to_string(areaId) + ".usda");

          if (std::filesystem::exists(usdPath)){
            AreaData area;
            UsdStageRefPtr stage = UsdStage::Open(usdPath.u8string());

            {
              UsdPrim generic = stage->GetPrimAtPath(SdfPath("/Generic"));

              auto skyBrightnessAttr = generic.GetAttribute(TfToken("skyBrightness"));
              skyBrightnessAttr.Get(&area.skyBrightness);
            }

            {
              UsdPrim volumetric = stage->GetPrimAtPath(SdfPath("/Volumetric"));

              auto transmittanceColorAttr = volumetric.GetAttribute(TfToken("transmittanceColor"));
              GfVec3f transmittanceColor;
              transmittanceColorAttr.Get(&transmittanceColor);
              area.transmittanceColor = Vector3(transmittanceColor[0], transmittanceColor[1], transmittanceColor[2]);

              auto singleScatteringAlbedoAttr = volumetric.GetAttribute(TfToken("singleScatteringAlbedo"));
              GfVec3f singleScatteringAlbedo;
              singleScatteringAlbedoAttr.Get(&singleScatteringAlbedo);
              area.singleScatteringAlbedo = Vector3(singleScatteringAlbedo[0], singleScatteringAlbedo[1], singleScatteringAlbedo[2]);

              auto transmittanceMeasurementDistanceMetersAttr = volumetric.GetAttribute(TfToken("transmittanceMeasurementDistanceMeters"));
              transmittanceMeasurementDistanceMetersAttr.Get(&area.transmittanceMeasurementDistanceMeters);
            }
            {
              UsdPrim prism = stage->GetPrimAtPath(SdfPath("/DistantLights"));
   
              for (const UsdPrim& light : UsdPrimRange(prism)) {
                AreaLightDataDir dirLight;
                auto colorAttr = light.GetAttribute(TfToken("inputs:color"));
                GfVec3f color;
                colorAttr.Get(&color);
                dirLight.lightRadiance = Vector3(color[0], color[1], color[2]);

                auto directionAttr = light.GetAttribute(TfToken("direction"));
                GfVec3f lightDirection;
                directionAttr.Get(&lightDirection);
                dirLight.lightDirection = Vector3(lightDirection[0], lightDirection[1], lightDirection[2]);

                area.dirLightsData.emplace_back(std::move(dirLight));
              }
            }

            {
              UsdPrim prism = stage->GetPrimAtPath(SdfPath("/SphereLights"));

              for (const UsdPrim& light : UsdPrimRange(prism)) {
                AreaLightDataPoint pointLight;
                auto colorAttr = light.GetAttribute(TfToken("inputs:color"));
                GfVec3f color;
                colorAttr.Get(&color);
                pointLight.lightRadiance = Vector3(color[0], color[1], color[2]);

                auto radiusAttr = light.GetAttribute(TfToken("inputs:radius"));
                radiusAttr.Get(&pointLight.lightRadius);

                UsdGeomXformable xformable(light);

                bool resetsXformStack;
                std::vector<UsdGeomXformOp> xformOps = xformable.GetOrderedXformOps(&resetsXformStack);

                for (const auto& op : xformOps) {
                  if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
                    GfVec3d translate;
                    op.Get(&translate);
                    pointLight.lightPosition = Vector3(translate[0], translate[1], translate[2]);
                  }
                }
                area.pointLightsData.emplace_back(std::move(pointLight));
              }
            }

            {
              UsdPrim prism = stage->GetPrimAtPath(SdfPath("/RectLights"));

              for (const UsdPrim& light : UsdPrimRange(prism)) {
                AreaLightDataRect rectLight;
                auto colorAttr = light.GetAttribute(TfToken("inputs:color"));
                GfVec3f color;
                colorAttr.Get(&color);
                rectLight.lightRadiance = Vector3(color[0], color[1], color[2]);

                auto widthAttr = light.GetAttribute(TfToken("inputs:width"));
                widthAttr.Get(&rectLight.dimensions.x);

                auto heightAttr = light.GetAttribute(TfToken("inputs:height"));
                heightAttr.Get(&rectLight.dimensions.y);

                UsdGeomXformable xformable(light);

                bool resetsXformStack;
                std::vector<UsdGeomXformOp> xformOps = xformable.GetOrderedXformOps(&resetsXformStack);

                for (const auto& op : xformOps) {
                  if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
                    GfVec3d translate;
                    op.Get(&translate);
                    rectLight.lightPosition = Vector3(translate[0], translate[1], translate[2]);
                  }

                  if (op.GetOpType() == UsdGeomXformOp::TypeRotateXYZ) {
                    GfVec3f rotate;
                    op.Get(&rotate);
                    rectLight.rotation = Vector3(rotate[0], rotate[1], rotate[2]);
                  }
                }

                area.rectLightsData.emplace_back(std::move(rectLight));
              }
            }

            m_areas.try_emplace(areaId, area);
          }
        }
    }

    void AreaManager::save() {


      wchar_t file_prefix[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
      std::filesystem::path path = file_prefix;
      path = path.parent_path();

      // USD
      for(auto& [id, area] : m_areas)
      {
        std::filesystem::path usdPath = path / "Area" / std::string(std::to_string(id) + ".usda");

        UsdStageRefPtr stage = UsdStage::CreateNew(usdPath.u8string());

        {
          UsdPrim generic = stage->DefinePrim(SdfPath { "/Generic" });
          auto skyBrightnessAttr = generic.CreateAttribute(
          TfToken("skyBrightness"),
          SdfValueTypeNames->Float,
          true
          );
          skyBrightnessAttr.Set(area.skyBrightness);

        }

        {
          UsdPrim volumetric = stage->DefinePrim(SdfPath { "/Volumetric" });
          auto transmittanceColorAttr = volumetric.CreateAttribute(
          TfToken("transmittanceColor"),
          SdfValueTypeNames->Color3f,
          true
          );
          transmittanceColorAttr.Set(GfVec3f(area.transmittanceColor.x, area.transmittanceColor.y, area.transmittanceColor.z));

          auto singleScatteringAlbedoAttr = volumetric.CreateAttribute(
          TfToken("singleScatteringAlbedo"),
          SdfValueTypeNames->Color3f,
          true
          );
          singleScatteringAlbedoAttr.Set(GfVec3f(area.singleScatteringAlbedo.x, area.singleScatteringAlbedo.y, area.singleScatteringAlbedo.z));

          auto transmittanceMeasurementDistanceMetersAttr = volumetric.CreateAttribute(
          TfToken("transmittanceMeasurementDistanceMeters"),
          SdfValueTypeNames->Float,
          true
          );
          transmittanceMeasurementDistanceMetersAttr.Set(area.transmittanceMeasurementDistanceMeters);

        }

        {
          UsdPrim light = stage->DefinePrim(SdfPath { "/DistantLights" });
          uint32_t dirLightIndex = 0;
          for (auto& lightDir : area.dirLightsData) {
            UsdLuxDistantLight light = UsdLuxDistantLight::Define(stage, SdfPath { std::string("/DistantLights/DistantLight_" + std::to_string(dirLightIndex)) });
            light.CreateColorAttr().Set(GfVec3f(lightDir.lightRadiance.x, lightDir.lightRadiance.y, lightDir.lightRadiance.z));

            auto dirAttr = light.GetPrim().CreateAttribute(
              TfToken("direction"),
              SdfValueTypeNames->Vector3f,
              true
            );

            dirAttr.Set(GfVec3f(lightDir.lightDirection.x, lightDir.lightDirection.y, lightDir.lightDirection.z));
            ++dirLightIndex;
          }
        }

        {
          UsdPrim light = stage->DefinePrim(SdfPath { "/SphereLights" });
          uint32_t pointLightIndex = 0;
          for (auto& pointLight : area.pointLightsData) {
            UsdLuxSphereLight light = UsdLuxSphereLight::Define(stage, SdfPath { std::string("/SphereLights/SphereLight_" + std::to_string(pointLightIndex)) });
            light.CreateColorAttr().Set(GfVec3f(pointLight.lightRadiance.x, pointLight.lightRadiance.y, pointLight.lightRadiance.z));
            light.CreateRadiusAttr().Set(pointLight.lightRadius);
            light.AddTranslateOp().Set(GfVec3d(pointLight.lightPosition.x, pointLight.lightPosition.y, pointLight.lightPosition.z));
            ++pointLightIndex;
          }
        }
        {
          UsdPrim light = stage->DefinePrim(SdfPath { "/RectLights" });
          uint32_t rectLightIndex = 0;
          for (auto& rectLight : area.rectLightsData) {
            UsdLuxRectLight  light = UsdLuxRectLight::Define(stage, SdfPath { std::string("/RectLights/RectLight_" + std::to_string(rectLightIndex)) });
            light.CreateColorAttr().Set(GfVec3f(rectLight.lightRadiance.x, rectLight.lightRadiance.y, rectLight.lightRadiance.z));
            light.CreateWidthAttr().Set(rectLight.dimensions.x);
            light.CreateHeightAttr().Set(rectLight.dimensions.y);
            light.AddTranslateOp().Set(GfVec3d(rectLight.lightPosition.x, rectLight.lightPosition.y, rectLight.lightPosition.z));
            light.AddRotateXYZOp().Set(GfVec3f(rectLight.rotation.x, rectLight.rotation.y, rectLight.rotation.z));
            ++rectLightIndex;
          }
        }

        stage->GetRootLayer()->Save();
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

    void AreaData::showImguiSettings(const AreaManager& areaManager, const Vector3& cameraPosition, LightManager& lightManager)
    {
      static constexpr float MaxTransmittanceValue = 1.f - 1.f / 255.f;
      bool areaDirty = false;
      if (ImGui::TreeNode("Light")) {
        areaDirty |= ImGui::SliderFloat("Sky Brighness", &skyBrightness, 0.0f, 100.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::TreeNode("Directional Lights")) {
          if (ImGui::Button("Add Directional Light")) {
            areaDirty = true;
            dirLightsData.push_back(AreaLightDataDir{ });
          }
          uint lightId = 0;
          uint toRemove = -1;
          for (AreaLightDataDir& lightData : dirLightsData) {
            if (ImGui::TreeNode(std::to_string(lightId).c_str())) {
              if (ImGui::Button("Delete")) {
                toRemove = lightId;
                areaDirty = true;
              }

              areaDirty |= ImGui::SliderFloat3("Ligh Direction", lightData.lightDirection.data, -1.0f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::Button("Use game original light direction")) {
                lightData.lightDirection = areaManager.getOriLightDir();
                areaDirty = true;
              }
              areaDirty |= ImGui::SliderFloat3("Ligh Radiance", lightData.lightRadiance.data, 0.0f, 100.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::Button("X2")) {
                lightData.lightRadiance *= 2.0f;
                areaDirty = true;
              }
              ImGui::SameLine();
              if (ImGui::Button("/2")) {
                lightData.lightRadiance *= 0.5f;
                areaDirty = true;
              }
              ImGui::TreePop();
            }
            ++lightId;
          }
          if (toRemove != -1)
            dirLightsData.erase(dirLightsData.begin() + toRemove);
          ImGui::TreePop();
        }
        if (ImGui::TreeNode("Point Lights")) {
          if (ImGui::Button("Add Point Light")) {
            areaDirty = true;
            pointLightsData.push_back(AreaLightDataPoint{ });
          }
          uint lightId = 0;
          uint toRemove = -1;
          for (AreaLightDataPoint& lightData : pointLightsData) {
            if (ImGui::TreeNode(std::to_string(lightId).c_str())) {
              if (ImGui::Button("Delete")) {
                toRemove = lightId;
                areaDirty = true;
              }


              areaDirty |= ImGui::SliderFloat3("Ligh Radiance", lightData.lightRadiance.data, 0.0f, 100.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::Button("Init light position at camera pos")) {
                lightData.lightPosition = cameraPosition;
              }
              areaDirty |= ImGui::SliderFloat3("Ligh Position", lightData.lightPosition.data, -10000.0f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              areaDirty |= ImGui::SliderFloat("Ligh Radius", &lightData.lightRadius, 0.01f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::Button("X2")) {
                lightData.lightRadiance *= 2.0f;
                areaDirty = true;
              }
              ImGui::SameLine();
              if (ImGui::Button("/2")) {
                lightData.lightRadiance *= 0.5f;
                areaDirty = true;
              }
              ImGui::TreePop();
            }
            ++lightId;
          }
          if (toRemove != -1)
            pointLightsData.erase(pointLightsData.begin() + toRemove);
          ImGui::TreePop();
        }

        if (ImGui::TreeNode("Rect Lights")) {
          if (ImGui::Button("Add Rect Light")) {
            areaDirty = true;
            rectLightsData.push_back(AreaLightDataRect{ });
          }
          uint lightId = 0;
          uint toRemove = -1;
          for (AreaLightDataRect& lightData : rectLightsData) {
            if (ImGui::TreeNode(std::to_string(lightId).c_str())) {
              if (ImGui::Button("Delete")) {
                toRemove = lightId;
                areaDirty = true;
              }


              areaDirty |= ImGui::SliderFloat3("Ligh Radiance", lightData.lightRadiance.data, 0.0f, 100.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::Button("Init light position at camera pos")) {
                lightData.lightPosition = cameraPosition;
              }
              areaDirty |= ImGui::SliderFloat3("Ligh Position", lightData.lightPosition.data, -10000.0f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::SliderFloat3("Ligh rotation", lightData.rotation.data, 0.0, 360.0, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                areaDirty = true;
                lightData.buildMatrix();
              }
              areaDirty |= ImGui::SliderFloat2("Ligh dimensions", lightData.dimensions.data, 0.0, 5000.0, "%.3f", ImGuiSliderFlags_AlwaysClamp);
              if (ImGui::Button("X2")) {
                lightData.lightRadiance *= 2.0f;
                areaDirty = true;
              }
              ImGui::SameLine();
              if (ImGui::Button("/2")) {
                lightData.lightRadiance *= 0.5f;
                areaDirty = true;
              }
              ImGui::TreePop();
            }
            ++lightId;
          }
          if (toRemove != -1)
            rectLightsData.erase(rectLightsData.begin() + toRemove);
          ImGui::TreePop();
        }

        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Volumetric")) {
        areaDirty |= ImGui::DragFloat3("Transmittance Color", transmittanceColor.data, 0.01f, 0.0f, MaxTransmittanceValue, "%.3f");
        areaDirty |= ImGui::DragFloat("Transmittance Measurement Distance", &transmittanceMeasurementDistanceMeters, 0.25f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        areaDirty |= ImGui::DragFloat3("Single Scattering Albedo", singleScatteringAlbedo.data, 0.01f, 0.0f, 1.0f, "%.3f");

        areaDirty |= ImGui::DragFloat("Anisotropy", &anisotropy, 0.01f, -.99f, .99f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        areaDirty |= ImGui::Checkbox("Enable Heterogeneous Fog", &enableHeterogeneousFog);

        areaDirty |= ImGui::DragFloat("Noise Field Spatial Frequency", &noiseFieldSpatialFrequency, 0.01f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        areaDirty |= ImGui::DragInt("Noise Field Number of Octaves", &noiseFieldOctaves, 1.f, 0, 10);
        areaDirty |= ImGui::DragFloat("Noise Field Density Scale", &noiseFieldDensityScale, 0.01f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::TreePop();
      }
      if (areaDirty) {
        lightDirty = true;
        lightManager.resetLightFallback();
      }
    }

}