/*
* Copyright (c) 2025, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/
#include "rtx_particle_system.h"
#include "dxvk_device.h"
#include "rtx_render/rtx_shader_manager.h"

#include "rtx/pass/common_binding_indices.h" 
#include "dxvk_scoped_annotation.h"
#include "dxvk_context.h"
#include "rtx_context.h"
#include "rtx_imgui.h"

#include "../util/util_globaltime.h"

#include <rtx_shaders/particle_system_spawn.h>
// MHFZ start
#include <rtx_shaders/particle_system_spawn_area.h>
// MHFZ end
#include <rtx_shaders/particle_system_evolve.h>
#include <rtx_shaders/particle_system_generate_geometry.h>
#include "math.h"

// MHFZ start
#include "rtx_asset_data_manager.h"
#include "rtx_texture_manager.h"

#include "../../lssusd/usd_include_begin.h"
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/attribute.h>
#include "../../lssusd/usd_include_end.h"

using namespace pxr;
// MHFZ end

namespace dxvk {

  // Defined within an unnamed namespace to ensure unique definition across binary
  namespace {

    class ParticleSystemSpawn : public ManagedShader {


      SHADER_SOURCE(ParticleSystemSpawn, VK_SHADER_STAGE_COMPUTE_BIT, particle_system_spawn)
        BINDLESS_ENABLED()
        BEGIN_PARAMETER()
        COMMON_RAYTRACING_BINDINGS

        CONSTANT_BUFFER(PARTICLE_SYSTEM_BINDING_CONSTANTS)

        STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_SPAWN_CONTEXTS_INPUT)

        RW_STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT_OUTPUT)
        END_PARAMETER()
    };

    // MHFZ start
    class ParticleSystemSpawnArea : public ManagedShader {


      SHADER_SOURCE(ParticleSystemSpawnArea, VK_SHADER_STAGE_COMPUTE_BIT, particle_system_spawn_area)
        BINDLESS_ENABLED()
        BEGIN_PARAMETER()
        COMMON_RAYTRACING_BINDINGS

        CONSTANT_BUFFER(PARTICLE_SYSTEM_BINDING_CONSTANTS)

        STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_SPAWN_CONTEXTS_INPUT)

        RW_STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT_OUTPUT)
        END_PARAMETER()
    };
    // MHFZ end

    class ParticleSystemEvolve : public ManagedShader {

      SHADER_SOURCE(ParticleSystemEvolve, VK_SHADER_STAGE_COMPUTE_BIT, particle_system_evolve)

        BINDLESS_ENABLED()

        BEGIN_PARAMETER()
        COMMON_RAYTRACING_BINDINGS

        CONSTANT_BUFFER(PARTICLE_SYSTEM_BINDING_CONSTANTS)

        TEXTURE2D(PARTICLE_SYSTEM_BINDING_PREV_WORLD_POSITION_INPUT)
        TEXTURE2D(PARTICLE_SYSTEM_BINDING_PREV_PRIMARY_SCREEN_SPACE_MOTION_INPUT)

        RW_STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT_OUTPUT)
        RW_STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_COUNTER_OUTPUT)
        END_PARAMETER()
    };
  }

  class ParticleSystemGenerateGeometry : public ManagedShader {
    SHADER_SOURCE(ParticleSystemGenerateGeometry, VK_SHADER_STAGE_COMPUTE_BIT, particle_system_generate_geometry)

      BINDLESS_ENABLED()

      BEGIN_PARAMETER()
      COMMON_RAYTRACING_BINDINGS

      CONSTANT_BUFFER(PARTICLE_SYSTEM_BINDING_CONSTANTS)

      STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_SPAWN_CONTEXTS_INPUT)
      STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT)

      RW_STRUCTURED_BUFFER(PARTICLE_SYSTEM_BINDING_VERTEX_BUFFER_OUTPUT)
      END_PARAMETER()
  };

  RtxParticleSystemManager::ConservativeCounter::ConservativeCounter(DxvkContext* ctx, const uint32_t framesInFlight, const uint32_t upperBound)
    : m_framesInFlight(framesInFlight)
    , m_upperBound(upperBound) {
    const Rc<DxvkDevice>& device = ctx->getDevice();
    DxvkBufferCreateInfo info;
    info.size = sizeof(int);
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    m_countGpu = device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - ConservativeCounter Buffer");
    ctx->clearBuffer(m_countGpu, 0, info.size, 0);

    info.size *= framesInFlight;
    info.access = VK_ACCESS_TRANSFER_WRITE_BIT;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    m_countsHost = device->createBuffer(info, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - ConservativeCounter HOST Buffer");
    memset(m_countsHost->mapPtr(0), 0, info.size);
  }

  void RtxParticleSystemManager::ConservativeCounter::postSimulation(DxvkContext* ctx, uint32_t frameIdx) {
    // for this to work correct, we must have performed the steps in preSimulation
    assert(m_frameLastCounted == frameIdx);

    // copy the GPU data into host data - will be ready at some future frame
    const uint32_t currentIdx = frameIdx % m_framesInFlight;
    ctx->copyBuffer(m_countsHost, sizeof(int) * currentIdx, m_countGpu, 0, sizeof(int));
  }

  uint32_t RtxParticleSystemManager::ConservativeCounter::preSimulation(DxvkContext* ctx, uint32_t cpuSpawns, uint32_t frameIdx) {
    // only re-read from GPU once per frame slot
    assert(m_frameLastCounted == kInvalidFrameIndex || m_frameLastCounted < frameIdx);

    // read back the GPU-written counter for this slot
    const int* gpuData = reinterpret_cast<int*>(m_countsHost->mapPtr(0));

    // track all GPU subtractions (deaths) between current and previous frame - just in case frames are skipped for some reason
    const uint32_t end = frameIdx;
    const uint32_t begin = std::max(m_frameLastCounted + 1, frameIdx - m_framesInFlight);
    for (uint32_t i = begin; i <= end; i++) {
      int gpuDeaths = gpuData[i % m_framesInFlight];
      cachedTotal += gpuDeaths;
    }

    // tally the CPU additions (spawns)
    cachedTotal += cpuSpawns;

    // safety checks to ensure things working as expected
    assert(cachedTotal <= m_upperBound);

    // clear out the GPU buffer to ready for current frame of simulation data
    ctx->clearBuffer(m_countGpu, 0, sizeof(int), 0);

    // we use this for safety checks
    m_frameLastCounted = frameIdx;

    return cachedTotal;
  }

  RtxParticleSystemManager::RtxParticleSystemManager(DxvkDevice* device)
    : CommonDeviceObject(device) { }

  void RtxParticleSystemManager::showImguiSettings() {
    if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_CollapsingHeader)) {
      ImGui::PushID("rtx_particles");
      ImGui::Dummy({ 0,2 });
      ImGui::Indent();

      ImGui::Checkbox("Enable", &enableObject());
      ImGui::BeginDisabled(!enable());
      ImGui::Checkbox("Enable Spawning", &enableSpawningObject());
      ImGui::DragFloat("Time Scale", &timeScaleObject(), 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

      if (ImGui::CollapsingHeader("Global Preset", ImGuiTreeNodeFlags_CollapsingHeader)) {
        // MHFZ start
        RtxParticleSystemDesc desc = RtxParticleSystemManager::createGlobalParticleSystemDesc();
        showImguiSettings(desc);
        RtxParticleSystemManager::initialVelocityFromMotionRef() = desc.initialVelocityFromMotion;
        RtxParticleSystemManager::initialVelocityFromNormalRef() = desc.initialVelocityFromNormal;
        RtxParticleSystemManager::initialVelocityConeAngleDegreesRef() = desc.initialVelocityConeAngleDegrees;
        RtxParticleSystemManager::alignParticlesToVelocityRef() = desc.alignParticlesToVelocity;
        RtxParticleSystemManager::gravityForceRef() = desc.gravityForce;
        RtxParticleSystemManager::maxSpeedRef() = desc.maxSpeed;
        RtxParticleSystemManager::useTurbulenceRef() = desc.useTurbulence ? 1 : 0;
        RtxParticleSystemManager::turbulenceFrequencyRef() = desc.turbulenceFrequency;
        RtxParticleSystemManager::turbulenceForceRef() = desc.turbulenceForce;
        RtxParticleSystemManager::minParticleLifeRef() = desc.minTtl;
        RtxParticleSystemManager::maxParticleLifeRef() = desc.maxTtl;
        RtxParticleSystemManager::minSpawnSizeRef() = desc.minSpawnSize;
        RtxParticleSystemManager::maxSpawnSizeRef() = desc.maxSpawnSize;
        RtxParticleSystemManager::numberOfParticlesPerMaterialRef() = desc.maxNumParticles;
        RtxParticleSystemManager::minSpawnColorRef() = Vector4(desc.minSpawnColor.x, desc.minSpawnColor.y, desc.minSpawnColor.z, desc.minSpawnColor.w);
        RtxParticleSystemManager::maxSpawnColorRef() = Vector4(desc.maxSpawnColor.x, desc.maxSpawnColor.y, desc.maxSpawnColor.z, desc.maxSpawnColor.w);
        RtxParticleSystemManager::minSpawnRotationSpeedRef() = desc.minSpawnRotationSpeed;
        RtxParticleSystemManager::maxSpawnRotationSpeedRef() = desc.maxSpawnRotationSpeed;
        RtxParticleSystemManager::useSpawnTexcoordsRef() = desc.useSpawnTexcoords ? 1 : 0;
        RtxParticleSystemManager::enableCollisionDetectionRef() = desc.enableCollisionDetection ? 1 : 0;
        RtxParticleSystemManager::alignParticlesToVelocityRef() = desc.alignParticlesToVelocity ? 1 : 0;
        RtxParticleSystemManager::collisionRestitutionRef() = desc.collisionRestitution;
        RtxParticleSystemManager::collisionThicknessRef() = desc.collisionThickness;
        RtxParticleSystemManager::enableMotionTrailRef() = desc.enableMotionTrail  ? 1 : 0;
        RtxParticleSystemManager::motionTrailMultiplierRef() = desc.motionTrailMultiplier;
        RtxParticleSystemManager::spawnRatePerSecondRef() = (int)desc.spawnRate;
        RtxParticleSystemManager::minTargetSizeRef() = desc.minTargetSize;
        RtxParticleSystemManager::maxTargetSizeRef() = desc.maxTargetSize;
        RtxParticleSystemManager::minTargetRotationSpeedRef() = desc.minTargetRotationSpeed;
        RtxParticleSystemManager::maxTargetRotationSpeedRef() = desc.maxTargetRotationSpeed;
        RtxParticleSystemManager::minTargetColorRef() = Vector4(desc.minTargetColor.x, desc.minTargetColor.y, desc.minTargetColor.z, desc.minTargetColor.w);
        RtxParticleSystemManager::maxTargetColorRef() = Vector4(desc.maxTargetColor.x, desc.maxTargetColor.y, desc.maxTargetColor.z, desc.maxTargetColor.w);
        RtxParticleSystemManager::billboardTypeRef() = desc.billboardType;
        // MHFZ end
      }
      ImGui::Unindent();
      ImGui::EndDisabled();
      ImGui::PopID();
    }
  }

  // MHFZ start : show ParticleDesc, spawn context and material settings
  bool RtxParticleSystemManager::showImguiSettings(ParticleDataSpawnContext& spawnCtx, RtxParticleSystemDesc& particleDesc, ParticleSystemMaterial& material, const Vector3& cameraPosition) {
    bool dirty = false;
    if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_CollapsingHeader)) {
      ImGui::PushID("rtx_particles");
      ImGui::Dummy({ 0,2 });
      ImGui::Indent();

      ImGui::BeginDisabled(!enable());

      if (ImGui::CollapsingHeader("Spawn Context", ImGuiTreeNodeFlags_CollapsingHeader)) {

        static auto spawnTypeCombo = ImGui::ComboWithKey<ParticleSpawnType>(
        "Spawn Type",
        ImGui::ComboWithKey<ParticleSpawnType>::ComboEntries { {
          {ParticleSpawnType::Sphere, "Sphere"},
          {ParticleSpawnType::Box, "Box"},
          }});
        dirty |= spawnTypeCombo.getKey(&spawnCtx.spawnType);

        dirty |= ImGui::SliderFloat("Spawn radius", &spawnCtx.radius, 0.0f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::SliderFloat3("Spawn box dimensions", &spawnCtx.boxDim.x, 0.0f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::SliderFloat3("Spawn direction", &spawnCtx.spawnDir.x, 0.0f, 10000.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

        if (ImGui::Button("Init particle emitter position at camera pos")) {
          spawnCtx.emitterWorldPos = cameraPosition;
          dirty |= true;
        }
      }

      dirty |= showImguiSettings(particleDesc);

      if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_CollapsingHeader)) {
        dirty |= ImGui::DragFloat("Emissive Intesity", &material.emissiveIntensity, 0.01f, 0.001f, 10000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        char path[512];
        strcpy(path, material.m_albedoPath.data());
        ImGui::InputText("Albedo texture path", path, 512);
        if (strcmp(material.m_albedoPath.c_str(), path) != 0) {
          material.m_albedoTexture = nullptr;
          dirty |= true;
        }
        material.m_albedoPath = path;
      }

      ImGui::Unindent();
      ImGui::EndDisabled();
      ImGui::PopID();
    }

    return dirty;
  }
  // MHFZ end

  // MHFZ start : show ParticleDesc settings
  bool RtxParticleSystemManager::showImguiSettings(RtxParticleSystemDesc& particleDesc) {
    bool dirty = false;
    if (ImGui::CollapsingHeader("Particle Desc", ImGuiTreeNodeFlags_CollapsingHeader)) {
      ImGui::TextWrapped("The following settings will be applied to all particle systems created using the texture tagging mechanism.  Particle systems created via USD assets are not affected by these.");
      ImGui::Separator();

      dirty |= ImGui::DragInt("Number of Particles Per Material", &particleDesc.maxNumParticles, 0.1f, 1, 10000000, "%d", ImGuiSliderFlags_AlwaysClamp);
      const auto colourPickerOpts = ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_DisplayRGB;
      if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_CollapsingHeader)) {
        ImGui::PushID("spawn");
        int spawnRate = (int) particleDesc.spawnRate;
        dirty |= ImGui::DragInt("Spawn Rate Per Second", &spawnRate, 0.1f, 1, 100000, "%d", ImGuiSliderFlags_AlwaysClamp);
        particleDesc.spawnRate = (float) spawnRate;

        dirty |= ImGui::DragFloat("Initial Velocity From Motion", &particleDesc.initialVelocityFromMotion, 0.01f, -5000.f, 5000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloat("Initial Velocity From Normal", &particleDesc.initialVelocityFromNormal, 0.01f, -5000.f, 5000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloat("Initial Velocity Cone Angle", &particleDesc.initialVelocityConeAngleDegrees, 0.01f, -5000.f, 5000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Separator();
        dirty |= ImGui::DragFloatRange("Time to Live Range", { &particleDesc.minTtl, &particleDesc.maxTtl }, 0.01f, 0.01f, 100.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Separator();
        dirty |= ImGui::DragFloatRange("Size Range", { &particleDesc.minSpawnSize, &particleDesc.maxSpawnSize }, 0.01f, 0.01f, 5000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloatRange("Rotation Speed Range", { &particleDesc.minSpawnRotationSpeed, &particleDesc.maxSpawnRotationSpeed }, 0.01f, -100.0f, 100.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::ColorEdit4("Minimum Color Tint", &particleDesc.minSpawnColor.x, colourPickerOpts);
        dirty |= ImGui::ColorEdit4("Maximum Color Tint", &particleDesc.maxSpawnColor.x, colourPickerOpts);
        ImGui::PopID();
      }

      if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_CollapsingHeader)) {
        ImGui::PushID("target");
        dirty |= ImGui::DragFloatRange("Size Range", { &particleDesc.minTargetSize, &particleDesc.maxTargetSize }, 0.01f, 0.01f, 5000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloatRange("Rotation Speed Range", { &particleDesc.minTargetRotationSpeed, &particleDesc.maxTargetRotationSpeed }, 0.01f, -100.0f, 100.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::ColorEdit4("Minimum Color Tint", &particleDesc.minTargetColor.x, colourPickerOpts);
        dirty |= ImGui::ColorEdit4("Maximum Color Tint", &particleDesc.maxTargetColor.x, colourPickerOpts);
        ImGui::PopID();
      }

      if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_CollapsingHeader)) {
        dirty |= ImGui::DragFloat("Gravity Force", &particleDesc.gravityForce, 0.01f, -1000.f, 1000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloat("Max Speed", &particleDesc.maxSpeed, 0.01f, 0.f, 100000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        dirty |= ImGui::Checkbox("Enable Particle World Collisions", (bool*) &particleDesc.enableCollisionDetection);
        ImGui::BeginDisabled(!particleDesc.enableCollisionDetection);
        dirty |= ImGui::DragFloat("Collision Restitution", &particleDesc.collisionRestitution, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloat("Collision Thickness", &particleDesc.collisionThickness, 0.01f, 0.f, 10000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();

        dirty |= ImGui::Checkbox("Simulate Turbulence", (bool*) &particleDesc.useTurbulence);
        ImGui::BeginDisabled(!particleDesc.useTurbulence);
        dirty |= ImGui::DragFloat("Turbulence Force", &particleDesc.turbulenceForce, 0.01f, 0.f, 1000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        dirty |= ImGui::DragFloat("Turbulence Frequency", &particleDesc.turbulenceFrequency, 0.01f, 0.f, 10.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();
      }

      if (ImGui::CollapsingHeader("Visual", ImGuiTreeNodeFlags_CollapsingHeader)) {
        dirty |= ImGui::Checkbox("Align Particles with Velocity", (bool*) &particleDesc.alignParticlesToVelocity);
        dirty |= ImGui::Checkbox("Enable Motion Trail", (bool*) &particleDesc.enableMotionTrail);
        ImGui::BeginDisabled(!particleDesc.enableMotionTrail);
        dirty |= ImGui::DragFloat("Motion Trail Length Multiplier", &particleDesc.motionTrailMultiplier, 0.01f, 0.001f, 10000.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();

        static auto billboardTypeCombo = ImGui::ComboWithKey<ParticleBillboardType>(
          "Billboard Type",
          ImGui::ComboWithKey<ParticleBillboardType>::ComboEntries { {
            {ParticleBillboardType::FaceCamera_Spherical, "Classic billboard"},
            {ParticleBillboardType::FaceCamera_UpAxisLocked, "Cylindrical billboard (fix up axis)"},
            {ParticleBillboardType::FaceWorldUp, "Horizontal plane (face up axis)"},
            {ParticleBillboardType::FaceCamera_Position, "Face camera position"},
        } });
        dirty |= billboardTypeCombo.getKey(&particleDesc.billboardType);
      }
    }
    return dirty;
  }
  // MHFZ end
  void RtxParticleSystemManager::setupConstants(RtxContext* ctx, ParticleSystemConstants& constants) {
    ScopedCpuProfileZone();
    const RtCamera& camera = ctx->getSceneManager().getCamera();
    constants.viewToWorld = camera.getViewToWorld();
    constants.prevWorldToProjection = camera.getPreviousViewToProjection() * camera.getPreviousWorldToView();

    constants.renderingWidth = ctx->getSceneManager().getCamera().m_renderResolution[0];
    constants.renderingHeight = ctx->getSceneManager().getCamera().m_renderResolution[1];

    constants.frameIdx = device()->getCurrentFrameId();

    constants.upDirection.x = ctx->getSceneManager().getSceneUp().x;
    constants.upDirection.y = ctx->getSceneManager().getSceneUp().y;
    constants.upDirection.z = ctx->getSceneManager().getSceneUp().z;
    constants.deltaTimeSecs = std::min(kMinimumParticleLife, GlobalTime::get().deltaTime()) * timeScale();
    constants.invDeltaTimeSecs = constants.deltaTimeSecs > 0 ? (1.f / constants.deltaTimeSecs) : 0.f;
    constants.absoluteTimeSecs = GlobalTime::get().absoluteTimeMs() * 0.001f * timeScale();

    constants.resolveTransparencyThreshold = RtxOptions::resolveTransparencyThreshold();
    constants.minParticleSize = 0.4f * RtxOptions::sceneScale(); // 4mm

  }

  static_assert(sizeof(GpuParticle) == 12 * 4, "Unexpected, please check perf");// be careful with performance when increasing this!
  static_assert(sizeof(RtxParticleSystemDesc) % (4 * 4) == 0, "Unexpected, please check perf");

  RtxParticleSystemDesc RtxParticleSystemManager::createGlobalParticleSystemDesc() {
    RtxParticleSystemDesc desc;
    desc.initialVelocityFromMotion = RtxParticleSystemManager::initialVelocityFromMotion();
    desc.initialVelocityFromNormal = RtxParticleSystemManager::initialVelocityFromNormal();
    desc.initialVelocityConeAngleDegrees = RtxParticleSystemManager::initialVelocityConeAngleDegrees();
    desc.alignParticlesToVelocity = RtxParticleSystemManager::alignParticlesToVelocity();
    desc.gravityForce = RtxParticleSystemManager::gravityForce();
    desc.maxSpeed = RtxParticleSystemManager::maxSpeed();
    desc.useTurbulence = RtxParticleSystemManager::useTurbulence() ? 1 : 0;
    desc.turbulenceFrequency = RtxParticleSystemManager::turbulenceFrequency();
    desc.turbulenceForce = RtxParticleSystemManager::turbulenceForce();
    desc.minTtl = RtxParticleSystemManager::minParticleLife();
    desc.maxTtl = RtxParticleSystemManager::maxParticleLife();
    desc.minSpawnSize = RtxParticleSystemManager::minSpawnSize();
    desc.maxSpawnSize = RtxParticleSystemManager::maxSpawnSize();
    desc.maxNumParticles = RtxParticleSystemManager::numberOfParticlesPerMaterial();
    desc.minSpawnColor = RtxParticleSystemManager::minSpawnColor();
    desc.maxSpawnColor = RtxParticleSystemManager::maxSpawnColor();
    desc.minSpawnRotationSpeed = RtxParticleSystemManager::minSpawnRotationSpeed();
    desc.maxSpawnRotationSpeed = RtxParticleSystemManager::maxSpawnRotationSpeed();
    desc.useSpawnTexcoords = RtxParticleSystemManager::useSpawnTexcoords() ? 1 : 0;
    desc.enableCollisionDetection = RtxParticleSystemManager::enableCollisionDetection() ? 1 : 0;
    desc.alignParticlesToVelocity = RtxParticleSystemManager::alignParticlesToVelocity() ? 1 : 0;
    desc.collisionRestitution = RtxParticleSystemManager::collisionRestitution();
    desc.collisionThickness = RtxParticleSystemManager::collisionThickness();
    desc.enableMotionTrail = RtxParticleSystemManager::enableMotionTrail() ? 1 : 0;
    desc.motionTrailMultiplier = RtxParticleSystemManager::motionTrailMultiplier();
    desc.spawnRate = (float) RtxParticleSystemManager::spawnRatePerSecond();
    desc.minTargetSize = RtxParticleSystemManager::minTargetSize();
    desc.maxTargetSize = RtxParticleSystemManager::maxTargetSize();
    desc.minTargetRotationSpeed = RtxParticleSystemManager::minTargetRotationSpeed();
    desc.maxTargetRotationSpeed = RtxParticleSystemManager::maxTargetRotationSpeed();
    desc.minTargetColor = RtxParticleSystemManager::minTargetColor();
    desc.maxTargetColor = RtxParticleSystemManager::maxTargetColor();
    desc.hideEmitter = 0;
    desc.billboardType = RtxParticleSystemManager::billboardType();
    return desc;
  }

  // MHFZ start : create particle system Material
  ParticleSystemMaterial RtxParticleSystemManager::createGlobalParticleSystemMaterial() {
    ParticleSystemMaterial mat;
    mat.emissiveIntensity = 0.0f;
    mat.m_albedoPath.reserve(512);
    return mat;
  }

  ParticleDataSpawnContext RtxParticleSystemManager::createGlobalParticleSystemSpawnContext() {
    ParticleDataSpawnContext spawnCtx;
    spawnCtx.emitterWorldPos = vec3(0.0f, 0.0f, 0.0f);
    spawnCtx.boxDim = vec3(50.0f, 50.0f, 50.0f);
    spawnCtx.radius = 50.0f;
    spawnCtx.spawnDir = vec3(0.0f, 1.0f, 0.0f);
    spawnCtx.spawnType = ParticleSpawnType::Sphere;
    return spawnCtx;
  }
  // MHFZ end

  bool RtxParticleSystemManager::fetchParticleSystem(DxvkContext* ctx, const DrawCallState& drawCallState, const RtxParticleSystemDesc& desc, const ParticleSystemMaterial& particleMaterial, const MaterialData* overrideMaterial, ParticleSystem** materialSystem, ParticleDataSpawnContext* spawnContext, uint32_t areaPaticleSystemIndex) {
    ScopedCpuProfileZone();
    if(desc.maxNumParticles == 0) {
      return false;
    }
    
    // MHFZ start : compute particle hash
    XXH64_hash_t spawnContextHash = spawnContext ? spawnContext->calcHash() : 0;
    XXH64_hash_t geoHash = areaPaticleSystemIndex == 255 ? drawCallState.transformData.calcHash() ^ drawCallState.geometryData.getHashForRule<rules::TopologicalHash>() : 0;

    XXH64_hash_t particleSystemHash = drawCallState.getMaterialData().getHash() ^ desc.calcHash() ^ spawnContextHash ^ geoHash ^ particleMaterial.calcHash();
    // MHFZ end

    auto& materialSystemIt = m_particleSystems.find(particleSystemHash);
    if (materialSystemIt == m_particleSystems.end()) {
      auto pNewParticleSystem = std::make_shared<ParticleSystem>(desc, particleMaterial, (overrideMaterial ? *overrideMaterial : drawCallState.getMaterialData()), drawCallState.getCategoryFlags(), m_particleSystemCounter++, spawnContext, areaPaticleSystemIndex);
      pNewParticleSystem->allocStaticBuffers(ctx);
      m_particleSystems.insert({ particleSystemHash, pNewParticleSystem });
    }

    (*materialSystem) = m_particleSystems[particleSystemHash].get();

    return true;
  }

  uint32_t RtxParticleSystemManager::getNumberOfParticlesToSpawn(ParticleSystem* particleSystem, const DrawCallState& drawCallState) {
    ScopedCpuProfileZone();

    float lambda = particleSystem->context.desc.spawnRate * GlobalTime::get().deltaTime();

    // poisson dist wont work well with these values (inf loop)
    if (isnan(lambda) || lambda < 0.0f) {
      return 0;
    }

    std::poisson_distribution<uint32_t> distribution(lambda);
    uint32_t numParticles = std::min(distribution(particleSystem->generator), particleSystem->context.desc.maxNumParticles);

    if (particleSystem->context.particleCount + particleSystem->context.spawnParticleCount + numParticles >= particleSystem->context.desc.maxNumParticles) {
      return 0;
    }

    // Don't allow wrap around, this is because CPU and GPU are implicitly synchronized - and this would allow the CPU to jump ahead.
    if ((particleSystem->context.particleHeadOffset + numParticles) >= particleSystem->context.desc.maxNumParticles) {
      numParticles = particleSystem->context.desc.maxNumParticles - particleSystem->context.particleHeadOffset;
    }

    return numParticles;
  }

  void RtxParticleSystemManager::spawnParticles(DxvkContext* ctx, const RtxParticleSystemDesc& desc, ParticleSystemMaterial& particleMaterial, const uint32_t instanceId, const DrawCallState& drawCallState, const MaterialData* overrideMaterial, ParticleDataSpawnContext* particleSpawnCtx, uint32_t areaPaticleSystemIndex) {
    ScopedCpuProfileZone();
    if (!enable() || !enableSpawning()) {
      return;
    }

    m_initialized = true;

    // MHFZ start : load particle albedo if set up
    if (particleMaterial.m_albedoTexture == nullptr && particleMaterial.m_albedoPath.empty() == false) {
      wchar_t file_prefix[MAX_PATH] = L"";
      GetModuleFileNameW(nullptr, file_prefix, ARRAYSIZE(file_prefix));
      std::filesystem::path texturePath = file_prefix;
      texturePath = texturePath.parent_path();
      texturePath = texturePath.parent_path();
      texturePath /= "PBRData";
      texturePath /= particleMaterial.m_albedoPath.c_str();
      if (std::filesystem::exists(texturePath)) {
        auto assetData = AssetDataManager::get().findAsset(texturePath.string());
        if (assetData != nullptr) {
          particleMaterial.m_albedoTexture = ctx->getDevice()->getCommon()->getTextureManager().preloadTextureAsset(assetData, ColorSpace::AUTO, true);
        }
      }
    }
    // MHFZ end
    ParticleSystem* particleSystem = nullptr;
    if (!fetchParticleSystem(ctx, drawCallState, desc, particleMaterial, overrideMaterial, &particleSystem, particleSpawnCtx, areaPaticleSystemIndex)) {
      return;
    }

    uint32_t numParticles = 0;

    const bool isNumParticlesConstant = particleSystem->context.desc.spawnRate >= particleSystem->context.desc.maxNumParticles;
    if (isNumParticlesConstant) {
      numParticles = particleSystem->context.desc.maxNumParticles - particleSystem->context.spawnParticleCount;
    } else {
      numParticles = getNumberOfParticlesToSpawn(particleSystem, drawCallState);
    }

    if (numParticles == 0) {
      return;
    }

    assert(isNumParticlesConstant || (particleSystem->context.particleHeadOffset + numParticles) <= particleSystem->context.desc.maxNumParticles);

    // Register the spawn context data
    SpawnContext spawnCtx;
    spawnCtx.numberOfParticles = numParticles;
    spawnCtx.particleOffset = particleSystem->context.particleHeadOffset;
    spawnCtx.instanceId = instanceId;
    spawnCtx.particleSystemHash = particleSystem->getHash();
    // MHFZ start
    if (particleSpawnCtx){
      spawnCtx.context = *particleSpawnCtx;
    }
    // MHFZ end

    // Update material specific counters
    particleSystem->context.particleHeadOffset += numParticles;
    particleSystem->context.spawnParticleCount += numParticles;

    // Map the particles to a context for spawn
    particleSystem->spawnContextParticleMap.insert(particleSystem->spawnContextParticleMap.end(), spawnCtx.numberOfParticles, m_spawnContexts.size());
    assert(particleSystem->spawnContextParticleMap.size() <= particleSystem->context.desc.maxNumParticles);

    // Mark the time 
    particleSystem->lastSpawnTimeMs = GlobalTime::get().absoluteTimeMs();

    // Track this spawn context by copying off
    m_spawnContexts.emplace_back(std::move(spawnCtx));
  }

  void RtxParticleSystemManager::simulate(RtxContext* ctx) {
    if (!enable() || !m_initialized) {
      m_spawnContexts.clear();
      m_particleSystems.clear();
      return;
    }

    ScopedGpuProfileZone(ctx, "Rtx Particle Simulation");

    allocStaticBuffers(ctx);

    // If we have particles to simulate...
    if (m_particleSystems.size()) {
      writeSpawnContextsToGpu(ctx);

      ParticleSystemConstants constants;
      setupConstants(ctx, constants);

      ctx->bindResourceView(BINDING_VALUE_NOISE_SAMPLER, ctx->getResourceManager().getValueNoiseLut(ctx), nullptr);
      Rc<DxvkSampler> valueNoiseSampler = ctx->getResourceManager().getSampler(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
      ctx->bindResourceSampler(BINDING_VALUE_NOISE_SAMPLER, valueNoiseSampler);

      const auto& rtOutput = ctx->getResourceManager().getRaytracingOutput();
      ctx->bindResourceView(PARTICLE_SYSTEM_BINDING_PREV_WORLD_POSITION_INPUT,
                                  rtOutput.getPreviousPrimaryWorldPositionWorldTriangleNormal().view(Resources::AccessType::Read,
                                                                                               rtOutput.getPreviousPrimaryWorldPositionWorldTriangleNormal().matchesWriteFrameIdx(constants.frameIdx - 1)), nullptr);
      const uint32_t frameIdx = ctx->getDevice()->getCurrentFrameId();

      for (auto& system : m_particleSystems) {
        auto& conservativeCount = system.second->getCounter();

        GpuParticleSystem& particleSystem = system.second->context;

        // MHFZ start : use by default player pos as targetPos
        particleSystem.desc.targetPos = ctx->getDevice()->getAreaManager().getPlayerPos();
        // MHFZ end

        const bool isNumParticlesConstant = particleSystem.desc.spawnRate >= particleSystem.desc.maxNumParticles;

        if (isNumParticlesConstant) {
          particleSystem.simulateParticleCount = particleSystem.desc.maxNumParticles;
          particleSystem.particleCount = particleSystem.desc.maxNumParticles;
          particleSystem.spawnParticleCount = particleSystem.desc.maxNumParticles;
          particleSystem.particleHeadOffset = particleSystem.desc.maxNumParticles;
          particleSystem.particleTailOffset = 0;
          particleSystem.spawnParticleOffset = 0;
        } else {
          // Update some constants on the system based on our counters
          particleSystem.particleCount = conservativeCount->preSimulation(ctx, particleSystem.spawnParticleCount, frameIdx);

          const uint32_t max = particleSystem.desc.maxNumParticles;
          const uint32_t head = particleSystem.particleHeadOffset;

          particleSystem.particleTailOffset = (head + max - particleSystem.particleCount) % max;
          particleSystem.simulateParticleCount = (particleSystem.particleCount - particleSystem.spawnParticleCount);
        }

        if (particleSystem.particleCount == 0) {
          continue;
        }

        // Finalize some constants to the GPU data
        constants.particleSystem = particleSystem;
        constants.particleSystem.desc.applySceneScale(RtxOptions::sceneScale());
        
        assert(particleSystem.particleCount >= particleSystem.spawnParticleCount);

        // Update CB
        const DxvkBufferSliceHandle cSlice = m_cb->allocSlice();
        ctx->invalidateBuffer(m_cb, cSlice);
        ctx->writeToBuffer(m_cb, 0, sizeof(ParticleSystemConstants), &constants);
        ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_CONSTANTS, DxvkBufferSlice(m_cb));

        DxvkBarrierControlFlags barrierControl;

        // Disable barriers for write after writes - we ensure particle implementation complies with this optimization, 
        //  since we write to particle buffer from both the spawning and evolve kernels, but only ever to unique slots
        //  of the buffer.
        if (!isNumParticlesConstant) {
          barrierControl.set(DxvkBarrierControl::IgnoreWriteAfterWrite);
        }

        // Handle spawning
        if (particleSystem.spawnParticleCount > 0) {
          const VkExtent3D workgroups = util::computeBlockCount(VkExtent3D { particleSystem.spawnParticleCount, 1, 1 }, VkExtent3D { 128, 1, 1 });

          // MHFZ start
          ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_SPAWN_CONTEXTS_INPUT, DxvkBufferSlice(system.second->getSpawnGpuContextBuffer()));
          // MHFZ end
          ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT_OUTPUT, DxvkBufferSlice(system.second->getParticlesBuffer()));
          
          // MHFZ start : bind correct spawn shader
          if (system.second->areaSystem) {
            ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, ParticleSystemSpawnArea::getShader());
          }
          else {
            ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, ParticleSystemSpawn::getShader());
          }
          // MHFZ end

          ctx->dispatch(workgroups.width, workgroups.height, workgroups.depth);
        }

        // Handle simulation updates
        if (particleSystem.simulateParticleCount > 0) {
          const VkExtent3D workgroups = util::computeBlockCount(VkExtent3D { particleSystem.simulateParticleCount, 1, 1 }, VkExtent3D { 128, 1, 1 });

          ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT_OUTPUT, DxvkBufferSlice(system.second->getParticlesBuffer()));
          ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_COUNTER_OUTPUT, DxvkBufferSlice(system.second->getCounter()->getGpuCountBuffer()));

          ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, ParticleSystemEvolve::getShader());

          ctx->dispatch(workgroups.width, workgroups.height, workgroups.depth);
        }

        // Handle geometry creation - note, should move this into own loop (barrier latency)
        {
          const VkExtent3D workgroups = util::computeBlockCount(VkExtent3D { particleSystem.particleCount, 1, 1 }, VkExtent3D { 128, 1, 1 });

          ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT, DxvkBufferSlice(system.second->getParticlesBuffer()));
          ctx->bindResourceBuffer(PARTICLE_SYSTEM_BINDING_VERTEX_BUFFER_OUTPUT, DxvkBufferSlice(system.second->getVertexBuffer()));

          ctx->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, ParticleSystemGenerateGeometry::getShader());

          ctx->dispatch(workgroups.width, workgroups.height, workgroups.depth);
        }

        ctx->setBarrierControl(DxvkBarrierControlFlags());

        if (!isNumParticlesConstant) {
          conservativeCount->postSimulation(ctx, ctx->getDevice()->getCurrentFrameId());
        }
      }
    }

    prepareForNextFrame();
  }

  void RtxParticleSystemManager::writeSpawnContextsToGpu(RtxContext* ctx) {
    if (!m_spawnContexts.size()) {
      return;
    }

    // Align the data
    for (auto& keyPair : m_particleSystems) {
      ParticleSystem& particleSystem = *(keyPair.second.get());
      if (particleSystem.spawnContextParticleMap.size() == 0)
        continue;
      uint32_t spawnCtxIt = *particleSystem.spawnContextParticleMap.begin();
      if (spawnCtxIt >= m_spawnContexts.size())
        continue;
      const SpawnContext& spawnCtx = m_spawnContexts[spawnCtxIt];

      GpuSpawnContext gpuCtx;

      const RtInstance* pTargetInstance = spawnCtx.instanceId < ctx->getSceneManager().getInstanceTable().size() ? ctx->getSceneManager().getInstanceTable()[spawnCtx.instanceId] : nullptr;

      if (pTargetInstance == nullptr && particleSystem.areaSystem == false) {
        // I dont see this case being hit, but in theory it could happen since we track the 
        //   instance ID at draw time, and the instance list can change over the course of a frame.
        //   In the event it does happen, handle gracefully...dw
        memset(&gpuCtx, 0, sizeof(GpuSpawnContext));
        particleSystem.context.spawnParticleCount = 0;
        continue;
      }
      // MHFZ start :  area particle send cutom spawn settings
      if (particleSystem.areaSystem == false) {
        gpuCtx.spawnObjectToWorld = pTargetInstance->getTransform();
        gpuCtx.spawnPrevObjectToWorld = pTargetInstance->getPrevTransform();

        gpuCtx.indices32bit = pTargetInstance->getBlas()->modifiedGeometryData.indexBuffer.indexType() == VK_INDEX_TYPE_UINT32 ? 1 : 0;
        gpuCtx.numTriangles = pTargetInstance->getBlas()->modifiedGeometryData.indexCount / 3;
        gpuCtx.spawnMeshIndexIdx = pTargetInstance->surface.indexBufferIndex;
        gpuCtx.spawnMeshPositionsIdx = pTargetInstance->surface.positionBufferIndex;
        gpuCtx.spawnMeshPrevPositionsIdx = pTargetInstance->surface.previousPositionBufferIndex;

        gpuCtx.spawnMeshColorsIdx = pTargetInstance->surface.color0BufferIndex;
        gpuCtx.spawnMeshTexcoordsIdx = pTargetInstance->surface.texcoordBufferIndex;
        gpuCtx.spawnMeshPositionsOffset = pTargetInstance->surface.positionOffset;
        gpuCtx.spawnMeshPositionsStride = pTargetInstance->surface.positionStride;

        gpuCtx.spawnMeshColorsOffset = pTargetInstance->surface.color0Offset;
        gpuCtx.spawnMeshColorsStride = pTargetInstance->surface.color0Stride;
        gpuCtx.spawnMeshTexcoordsOffset = pTargetInstance->surface.texcoordOffset;
        gpuCtx.spawnMeshTexcoordsStride = pTargetInstance->surface.texcoordStride;
      }
      else {
        Matrix4 m;
        m[0][3] = spawnCtx.context.emitterWorldPos.x;
        m[1][3] = spawnCtx.context.emitterWorldPos.y;
        m[2][3] = spawnCtx.context.emitterWorldPos.z;
        gpuCtx.spawnObjectToWorld = transpose(dxvk::Matrix4(m));
        gpuCtx.radius = spawnCtx.context.radius;
        gpuCtx.boxDim = spawnCtx.context.boxDim;
        gpuCtx.spawnType = spawnCtx.context.spawnType;
       
      }
      gpuCtx.spawnDir = spawnCtx.context.spawnDir;
      ctx->writeToBuffer(particleSystem.getSpawnGpuContextBuffer(), 0, sizeof(GpuSpawnContext), &gpuCtx);

    }
    // MHFZ end

    // Send data to GPU

    for (const auto& keyPair : m_particleSystems) {
      const ParticleSystem& particleSystem = *(keyPair.second.get());
      if (particleSystem.spawnContextParticleMap.size() == 0) {
        assert(particleSystem.context.spawnParticleCount == 0);
        continue;
      }
    }
  }

  void RtxParticleSystemManager::submitDrawState(RtxContext* ctx) const {
    ScopedCpuProfileZone();
    if (!enable() || !m_initialized) {
      return;
    }

    for (const auto& keyPair : m_particleSystems) {
      const ParticleSystem& particleSystem = *(keyPair.second.get());

      // Here we create a fake draw call, and send it through the regular scene manager pipeline
      //   which has the advantage of supporting replacement materials.

      // This is a conservative estimate, so we must clamp particles to the known maximum
      const uint32_t numParticles = std::min(particleSystem.context.particleCount + particleSystem.context.spawnParticleCount, particleSystem.context.desc.maxNumParticles);
      if (numParticles == 0) {
        continue;
      }

      // This is used to uniquely hash particle system geometry data - we do this because the particle data is hashed differently from regular D3D9 geometry.
      const XXH64_hash_t particleHashConstant = XXH3_64bits_withSeed(&numParticles, sizeof(numParticles), particleSystem.getHash());

      const DxvkBufferSlice& vertexSlice = DxvkBufferSlice(particleSystem.getVertexBuffer());
      const DxvkBufferSlice& indexSlice = DxvkBufferSlice(particleSystem.getIndexBuffer());

      RasterGeometry particleGeometry;
      particleGeometry.indexBuffer = RasterBuffer(indexSlice, 0, sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
      particleGeometry.indexCount = particleSystem.getIndicesPerParticle() * numParticles;
      particleGeometry.vertexCount = particleSystem.getVerticesPerParticle() * numParticles;
      particleGeometry.positionBuffer = RasterBuffer(vertexSlice, offsetof(ParticleVertex, position), sizeof(ParticleVertex), VK_FORMAT_R32G32B32_SFLOAT);
      particleGeometry.color0Buffer = RasterBuffer(vertexSlice, offsetof(ParticleVertex, color), sizeof(ParticleVertex), VK_FORMAT_B8G8R8A8_UNORM);
      particleGeometry.texcoordBuffer = RasterBuffer(vertexSlice, offsetof(ParticleVertex, texcoord), sizeof(ParticleVertex), VK_FORMAT_R32G32_SFLOAT);
      particleGeometry.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      particleGeometry.cullMode = VK_CULL_MODE_NONE;
      particleGeometry.frontFace = VK_FRONT_FACE_CLOCKWISE;
      particleGeometry.hashes[HashComponents::Indices] = particleHashConstant ^ particleSystem.getGeneration();
      particleGeometry.hashes[HashComponents::VertexPosition] = particleHashConstant ^ particleSystem.getGeneration();
      particleGeometry.hashes.precombine();

      const RtCamera& camera = ctx->getSceneManager().getCamera();

      DrawCallState newDrawCallState;
      newDrawCallState.geometryData = particleGeometry; // Note: Geometry Data replaced
      newDrawCallState.categories = particleSystem.categories;
      newDrawCallState.categories.set(InstanceCategories::Particle);
      newDrawCallState.categories.clr(InstanceCategories::ParticleEmitter);
      // MHFZ start
      newDrawCallState.categories.clr(InstanceCategories::CustomBlend);
      // MHFZ end
      newDrawCallState.categories.clr(InstanceCategories::Hidden);
      newDrawCallState.transformData.viewToProjection = camera.getViewToProjection();
      newDrawCallState.transformData.worldToView = camera.getWorldToView();
      newDrawCallState.alphaBlendEnable = true;
      // If we have legacy material data, lets use it, otherwise default.
      if (particleSystem.materialData.getType() == MaterialDataType::Legacy) {
        newDrawCallState.materialData = particleSystem.materialData.getLegacyMaterialData();
        // MHFZ start : use particle material emissive
        newDrawCallState.materialData.emissiveIntensity = particleSystem.particleMaterial.emissiveIntensity;
        // MHFZ end
      }
      newDrawCallState.materialData.alphaBlendEnabled = true; 
      // We want to always have particles support vertex colour for now.
      newDrawCallState.materialData.textureColorArg2Source = RtTextureArgSource::VertexColor0;

      // MHFZ start : use particle material albedo
      TextureRef albedo;

      if (particleSystem.particleMaterial.m_albedoPath.empty() == false) {
        if (particleSystem.particleMaterial.m_albedoTexture != nullptr) {
          albedo = TextureRef(particleSystem.particleMaterial.m_albedoTexture);
          newDrawCallState.materialData.alphaTestEnabled = false;
          newDrawCallState.materialData.alphaTestReferenceValue = 0;
          newDrawCallState.materialData.alphaTestCompareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
          newDrawCallState.materialData.colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
          newDrawCallState.materialData.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA;
          newDrawCallState.materialData.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
          newDrawCallState.materialData.colorTextures[1] = albedo;
          newDrawCallState.materialData.isTextureFactorBlend = true;
          newDrawCallState.materialData.textureAlphaArg1Source = RtTextureArgSource::Texture;
          newDrawCallState.materialData.textureAlphaOperation = DxvkRtTextureOperation::SelectArg1;
          newDrawCallState.materialData.updateCachedHash();
        }

      }
      else {
        albedo = newDrawCallState.materialData.getColorTexture();
      }
      // MHFZ end

      ctx->getSceneManager().submitDrawState(ctx, newDrawCallState, (particleSystem.materialData.getType() == MaterialDataType::Legacy) ? nullptr : &particleSystem.materialData);
    }
  }

  RtxParticleSystemManager::ParticleSystem::ParticleSystem(const RtxParticleSystemDesc& desc, const ParticleSystemMaterial& particleMaterial, const MaterialData& matData, const CategoryFlags& cats, const uint32_t seed, ParticleDataSpawnContext* spawnContext, uint32_t areaPaticleSystemIndex) : context(desc)
    , materialData(matData)
    , particleMaterial(particleMaterial)
    , categories(cats) 
    , areaSystem(areaPaticleSystemIndex != 255)
    , lastSpawnTimeMs(GlobalTime::get().absoluteTimeMs()) {
    // Seed the RNG with a parameter from the manager, so we get unique random values for each particle system
    generator = std::default_random_engine(seed);
    // Store this hash since it cannot change now.
    // NOTE: This material data hash is stable within a run, but since hash depends on VK handles, it is not reliable across runs.
    XXH64_hash_t areaParticleSpawnContextHash = spawnContext ? spawnContext->calcHash() : 0;
    m_cachedHash = materialData.getHash() ^ context.desc.calcHash() ^ areaParticleSpawnContextHash ^ particleMaterial.calcHash();
    
    context.numVerticesPerParticle = getVerticesPerParticle();

    // classic square billboard
    static const float2 offsets[4] = {
        float2(-0.5f,  0.5f),
        float2(0.5f,  0.5f),
        float2(-0.5f, -0.5f),
        float2(0.5f, -0.5f)
    };

    // motion trail - first 4 are "head", last 4 are "tail"
    static const float2 offsetsMotionTrail[8] = {
      // TAIL quad (fixed)
      float2(-0.5f, -0.5f),
      float2(-0.5f,  0.0f),
      float2(0.5f, -0.5f),
      float2(0.5f,  0.0f),

      // HEAD quad (stretched)
      float2(-0.5f, 0.0f),
      float2(-0.5f, 0.5f),
      float2(0.5f, 0.0f),
      float2(0.5f, 0.5f)
    };

    if (desc.enableMotionTrail) {
      memcpy(&context.particleVertexOffsets[0], &offsetsMotionTrail[0], sizeof(offsetsMotionTrail));
    } else {
      memcpy(&context.particleVertexOffsets[0], &offsets[0], sizeof(offsets));
    }
  }

  void RtxParticleSystemManager::ParticleSystem::allocStaticBuffers(DxvkContext* ctx) {
    ScopedCpuProfileZone();

    if (m_count == nullptr) {
      m_count = new ConservativeCounter(ctx, 10, context.desc.maxNumParticles);
    }

    // Handle the reallocation of all GPU and CPU data structures.  

    const Rc<DxvkDevice>& device = ctx->getDevice();
    if (m_particles == nullptr || m_particles->info().size != sizeof(GpuParticle) * context.desc.maxNumParticles) {

      DxvkBufferCreateInfo info;
      info.size = sizeof(GpuParticle) * context.desc.maxNumParticles;
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT
        | VK_ACCESS_TRANSFER_READ_BIT
        | VK_ACCESS_SHADER_WRITE_BIT
        | VK_ACCESS_SHADER_READ_BIT;
      m_particles = device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - State Buffer");

      ctx->clearBuffer(m_particles, 0, info.size, GpuParticle::kBufferClearValue);
    }

    if (m_vb == nullptr || m_vb->info().size != sizeof(ParticleVertex) * getMaxVertexCount()) {

      DxvkBufferCreateInfo info;
      info.size = sizeof(ParticleVertex) * getMaxVertexCount();
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT
        | VK_ACCESS_TRANSFER_READ_BIT
        | VK_ACCESS_SHADER_WRITE_BIT
        | VK_ACCESS_SHADER_READ_BIT;
      m_vb = device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - Vertex Buffer");

      ctx->clearBuffer(m_vb, 0, info.size, 0);
    }

    if (m_ib == nullptr || m_ib->info().size != sizeof(uint32_t) * getMaxIndexCount()) {

      DxvkBufferCreateInfo info;
      info.size = sizeof(uint32_t) * getMaxIndexCount();
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                 | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                 | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                 | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
      m_ib = device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - Index Buffer");

      std::vector<uint32_t> indices(getMaxIndexCount());
      for (int i = 0; i < context.desc.maxNumParticles; i++) {
        indices[i * getIndicesPerParticle() + 0] = i * getVerticesPerParticle() + 0;
        indices[i * getIndicesPerParticle() + 1] = i * getVerticesPerParticle() + 1;
        indices[i * getIndicesPerParticle() + 2] = i * getVerticesPerParticle() + 2;
        indices[i * getIndicesPerParticle() + 3] = i * getVerticesPerParticle() + 2;
        indices[i * getIndicesPerParticle() + 4] = i * getVerticesPerParticle() + 1;
        indices[i * getIndicesPerParticle() + 5] = i * getVerticesPerParticle() + 3;
      }
      if (context.desc.enableMotionTrail) {
        for (int i = 0; i < context.desc.maxNumParticles; i++) {
          indices[i * getIndicesPerParticle() + 6] = i * getVerticesPerParticle() + 1;
          indices[i * getIndicesPerParticle() + 7] = i * getVerticesPerParticle() + 4;
          indices[i * getIndicesPerParticle() + 8] = i * getVerticesPerParticle() + 3;

          indices[i * getIndicesPerParticle() + 9] = i * getVerticesPerParticle() + 3;
          indices[i * getIndicesPerParticle() + 10] = i * getVerticesPerParticle() + 4;
          indices[i * getIndicesPerParticle() + 11] = i * getVerticesPerParticle() + 6;

          indices[i * getIndicesPerParticle() + 12] = i * getVerticesPerParticle() + 4;
          indices[i * getIndicesPerParticle() + 13] = i * getVerticesPerParticle() + 5;
          indices[i * getIndicesPerParticle() + 14] = i * getVerticesPerParticle() + 6;

          indices[i * getIndicesPerParticle() + 15] = i * getVerticesPerParticle() + 6;
          indices[i * getIndicesPerParticle() + 16] = i * getVerticesPerParticle() + 5;
          indices[i * getIndicesPerParticle() + 17] = i * getVerticesPerParticle() + 7;
        }
      }
      ctx->writeToBuffer(m_ib, 0, info.size, indices.data());
    }

    if (m_spawnGpuContext == nullptr) {
      const Rc<DxvkDevice>& device = ctx->getDevice();

      DxvkBufferCreateInfo info;
      info.size = sizeof(GpuSpawnContext);
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT
        | VK_ACCESS_TRANSFER_READ_BIT;
      m_spawnGpuContext = device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - Spawn Context Buffer");
    }
  }

  void RtxParticleSystemManager::allocStaticBuffers(DxvkContext* ctx) {
    ScopedCpuProfileZone();

    if (m_cb.ptr() == nullptr) {
      const Rc<DxvkDevice>& device = ctx->getDevice();
      DxvkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
      info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT;
      info.size = sizeof(ParticleSystemConstants);
      m_cb = device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DxvkMemoryStats::Category::RTXBuffer, "RTX Particles - Constant Buffer");
    }


  }

  void RtxParticleSystemManager::prepareForNextFrame() {
    // Spawn contexts dont persist across frames, this is because we want to support objects with unstable hashes.
    m_spawnContexts.clear();

    // Signals which version of the vertex data we are on due to simulation

    // Update material systems including unregistering systems that have no particles remaining
    for (auto keyPairIt = m_particleSystems.begin(); keyPairIt != m_particleSystems.end();) {
      ParticleSystem& particleSystem = *keyPairIt->second.get();

      auto now = GlobalTime::get().absoluteTimeMs();
      if ((particleSystem.lastSpawnTimeMs + (uint64_t) (particleSystem.context.desc.maxTtl * 1000)) < now) {
        keyPairIt = m_particleSystems.erase(keyPairIt);
        continue;
      }

      ++particleSystem.generationIdx;

      particleSystem.context.spawnParticleOffset = particleSystem.context.particleHeadOffset;
      particleSystem.context.spawnParticleCount = 0;


      // Handle wrap around at a safe point (we have already ensured that spawning cant induce a wrap around early).
      if (particleSystem.context.particleHeadOffset >= particleSystem.context.desc.maxNumParticles) {
        particleSystem.context.particleHeadOffset = 0;
      }

      particleSystem.spawnContextParticleMap.clear();

      ++keyPairIt;
    }
  }

  // MHFZ start : save/load particle system
  void RtxParticleSystemManager::save(std::string path, UsdStageRefPtr stage, const RtxParticleSystemDesc& desc, const ParticleSystemMaterial& particleMaterial, const ParticleDataSpawnContext& spawnContext)  {
    {
      UsdPrim particleSpawnPrim = stage->DefinePrim(SdfPath { path + "/ParticleSpawnContext" });
      auto emitterWorldPosAttr = particleSpawnPrim.CreateAttribute(
      TfToken("emitterWorldPos"),
      SdfValueTypeNames->Color3f,
      true
      );
      emitterWorldPosAttr.Set(GfVec3f(spawnContext.emitterWorldPos.x, spawnContext.emitterWorldPos.y, spawnContext.emitterWorldPos.z));

      auto boxDimAttr = particleSpawnPrim.CreateAttribute(
      TfToken("boxDim"),
      SdfValueTypeNames->Color3f,
      true
      );
      boxDimAttr.Set(GfVec3f(spawnContext.boxDim.x, spawnContext.boxDim.y, spawnContext.boxDim.z));

      auto spawnDirAttr = particleSpawnPrim.CreateAttribute(
      TfToken("spawnDir"),
      SdfValueTypeNames->Color3f,
      true
      );
      spawnDirAttr.Set(GfVec3f(spawnContext.spawnDir.x, spawnContext.spawnDir.y, spawnContext.spawnDir.z));

      auto radiusAttr = particleSpawnPrim.CreateAttribute(
      TfToken("radius"),
      SdfValueTypeNames->Float,
      true
      );
      radiusAttr.Set(spawnContext.radius);

      auto spawnTypeAttr = particleSpawnPrim.CreateAttribute(
      TfToken("spawnType"),
      SdfValueTypeNames->UInt,
      true
      );
      spawnTypeAttr.Set((uint32_t) spawnContext.spawnType);
    }
    {
      UsdPrim particleDesc = stage->DefinePrim(SdfPath { path + "/ParticleDescriptor" });
      auto minSpawnColorAttr = particleDesc.CreateAttribute(
      TfToken("minSpawnColor"),
      SdfValueTypeNames->Color4f,
      true
      );
      minSpawnColorAttr.Set(GfVec4f(desc.minSpawnColor.x, desc.minSpawnColor.y, desc.minSpawnColor.z, desc.minSpawnColor.w));

      auto maxSpawnColorAttr = particleDesc.CreateAttribute(
      TfToken("maxSpawnColor"),
      SdfValueTypeNames->Color4f,
      true
      );
      maxSpawnColorAttr.Set(GfVec4f(desc.maxSpawnColor.x, desc.maxSpawnColor.y, desc.maxSpawnColor.z, desc.maxSpawnColor.w));

      auto minTargetColorAttr = particleDesc.CreateAttribute(
      TfToken("minTargetColor"),
      SdfValueTypeNames->Color4f,
      true
      );
      minTargetColorAttr.Set(GfVec4f(desc.minTargetColor.x, desc.minTargetColor.y, desc.minTargetColor.z, desc.minTargetColor.w));
      
      auto maxTargetColorAttr = particleDesc.CreateAttribute(
      TfToken("maxTargetColor"),
      SdfValueTypeNames->Color4f,
      true
      );
      maxTargetColorAttr.Set(GfVec4f(desc.maxTargetColor.x, desc.maxTargetColor.y, desc.maxTargetColor.z, desc.maxTargetColor.w));

      auto minTargetSizeAttr = particleDesc.CreateAttribute(
      TfToken("minTargetSize"),
      SdfValueTypeNames->Float,
      true
      );
      minTargetSizeAttr.Set(desc.minTargetSize);

      auto maxTargetSizeAttr = particleDesc.CreateAttribute(
      TfToken("maxTargetSize"),
      SdfValueTypeNames->Float,
      true
      );
      maxTargetSizeAttr.Set(desc.maxTargetSize);

      auto minTargetRotationSpeedAttr = particleDesc.CreateAttribute(
      TfToken("minTargetRotationSpeed"),
      SdfValueTypeNames->Float,
      true
      );
      minTargetRotationSpeedAttr.Set(desc.minTargetRotationSpeed);

      auto maxTargetRotationSpeedAttr = particleDesc.CreateAttribute(
      TfToken("maxTargetRotationSpeed"),
      SdfValueTypeNames->Float,
      true
      );
      maxTargetRotationSpeedAttr.Set(desc.maxTargetRotationSpeed);

      auto minTtlAttr = particleDesc.CreateAttribute(
      TfToken("minTtl"),
      SdfValueTypeNames->Float,
      true
      );
      minTtlAttr.Set(desc.minTtl);

      auto maxTtlAttr = particleDesc.CreateAttribute(
      TfToken("maxTtl"),
      SdfValueTypeNames->Float,
      true
      );
      maxTtlAttr.Set(desc.maxTtl);

      auto initialVelocityFromNormalAttr = particleDesc.CreateAttribute(
      TfToken("initialVelocityFromNormal"),
      SdfValueTypeNames->Float,
      true
      );
      initialVelocityFromNormalAttr.Set(desc.initialVelocityFromNormal);

      auto initialVelocityConeAngleDegreesAttr = particleDesc.CreateAttribute(
      TfToken("initialVelocityConeAngleDegrees"),
      SdfValueTypeNames->Float,
      true
      );
      initialVelocityConeAngleDegreesAttr.Set(desc.initialVelocityConeAngleDegrees);

      auto minSpawnSizeAttr = particleDesc.CreateAttribute(
      TfToken("minSpawnSize"),
      SdfValueTypeNames->Float,
      true
      );
      minSpawnSizeAttr.Set(desc.minSpawnSize);

      auto maxSpawnSizeAttr = particleDesc.CreateAttribute(
      TfToken("maxSpawnSize"),
      SdfValueTypeNames->Float,
      true
      );
      maxSpawnSizeAttr.Set(desc.maxSpawnSize);

      auto gravityForceAttr = particleDesc.CreateAttribute(
      TfToken("gravityForce"),
      SdfValueTypeNames->Float,
      true
      );
      gravityForceAttr.Set(desc.gravityForce);

      auto maxSpeedAttr = particleDesc.CreateAttribute(
      TfToken("maxSpeed"),
      SdfValueTypeNames->Float,
      true
      );
      maxSpeedAttr.Set(desc.maxSpeed);

      auto turbulenceFrequencyAttr = particleDesc.CreateAttribute(
      TfToken("turbulenceFrequency"),
      SdfValueTypeNames->Float,
      true
      );
      turbulenceFrequencyAttr.Set(desc.turbulenceFrequency);

      auto turbulenceForceAttr = particleDesc.CreateAttribute(
      TfToken("turbulenceForce"),
      SdfValueTypeNames->Float,
      true
      );
      turbulenceForceAttr.Set(desc.turbulenceForce);

      auto motionTrailMultiplierAttr = particleDesc.CreateAttribute(
      TfToken("motionTrailMultiplier"),
      SdfValueTypeNames->Float,
      true
      );
      motionTrailMultiplierAttr.Set(desc.motionTrailMultiplier);

      auto minSpawnRotationSpeedAttr = particleDesc.CreateAttribute(
      TfToken("minSpawnRotationSpeed"),
      SdfValueTypeNames->Float,
      true
      );
      minSpawnRotationSpeedAttr.Set(desc.minSpawnRotationSpeed);

      auto spawnRateAttr = particleDesc.CreateAttribute(
      TfToken("spawnRate"),
      SdfValueTypeNames->Float,
      true
      );
      spawnRateAttr.Set(desc.spawnRate);

      auto collisionThicknessAttr = particleDesc.CreateAttribute(
      TfToken("collisionThickness"),
      SdfValueTypeNames->Float,
      true
      );
      collisionThicknessAttr.Set(desc.collisionThickness);

      auto collisionRestitutionAttr = particleDesc.CreateAttribute(
      TfToken("collisionRestitution"),
      SdfValueTypeNames->Float,
      true
      );
      collisionRestitutionAttr.Set(desc.collisionRestitution);

      auto initialVelocityFromMotionAttr = particleDesc.CreateAttribute(
      TfToken("initialVelocityFromMotion"),
      SdfValueTypeNames->Float,
      true
      );
      initialVelocityFromMotionAttr.Set(desc.initialVelocityFromMotion);

      auto maxNumParticlesAttr = particleDesc.CreateAttribute(
      TfToken("maxNumParticles"),
      SdfValueTypeNames->Float,
      true
      );
      maxNumParticlesAttr.Set(desc.maxNumParticles);

      auto billboardTypeAttr = particleDesc.CreateAttribute(
      TfToken("billboardType"),
      SdfValueTypeNames->UInt,
      true
      );
      billboardTypeAttr.Set((uint32_t) desc.billboardType);

      auto enableMotionTrailAttr = particleDesc.CreateAttribute(
      TfToken("enableMotionTrail"),
      SdfValueTypeNames->Bool,
      true
      );
      enableMotionTrailAttr.Set(desc.enableMotionTrail);

      auto useTurbulenceAttr = particleDesc.CreateAttribute(
      TfToken("useTurbulence"),
      SdfValueTypeNames->Bool,
      true
      );
      useTurbulenceAttr.Set(desc.useTurbulence);

      auto alignParticlesToVelocityAttr = particleDesc.CreateAttribute(
      TfToken("alignParticlesToVelocity"),
      SdfValueTypeNames->Bool,
      true
      );
      alignParticlesToVelocityAttr.Set(desc.alignParticlesToVelocity);

      auto useSpawnTexcoordsAttr = particleDesc.CreateAttribute(
      TfToken("useSpawnTexcoords"),
      SdfValueTypeNames->Bool,
      true
      );
      useSpawnTexcoordsAttr.Set(desc.useSpawnTexcoords);

      auto enableCollisionDetectionAttr = particleDesc.CreateAttribute(
      TfToken("enableCollisionDetection"),
      SdfValueTypeNames->Bool,
      true
      );
      enableCollisionDetectionAttr.Set(desc.enableCollisionDetection);
    }
    {
      UsdPrim particleMaterialPrim = stage->DefinePrim(SdfPath { path + "/ParticleMaterial" });
      auto albedoPathAttr = particleMaterialPrim.CreateAttribute(
      TfToken("albedoPath"),
      SdfValueTypeNames->Asset,
      true
      );
      albedoPathAttr.Set(SdfAssetPath(particleMaterial.m_albedoPath));

      auto emissiveIntensityAttr = particleMaterialPrim.CreateAttribute(
      TfToken("emissiveIntensity"),
      SdfValueTypeNames->Float,
      true
      );
      emissiveIntensityAttr.Set(particleMaterial.emissiveIntensity);
    }
  }

  void RtxParticleSystemManager::load(std::string path, pxr::UsdStageRefPtr stage, RtxParticleSystemDesc& desc, ParticleSystemMaterial& particleMaterial, ParticleDataSpawnContext& spawnContext) {
    {
      UsdPrim particleSpawnPrim = stage->GetPrimAtPath(SdfPath { path + "/ParticleSpawnContext" });

      auto emitterWorldPosAttr = particleSpawnPrim.GetAttribute(TfToken("emitterWorldPos"));
      GfVec3f emitterWorldPos;
      emitterWorldPosAttr.Get(&emitterWorldPos);
      spawnContext.emitterWorldPos = Vector3(emitterWorldPos[0], emitterWorldPos[1], emitterWorldPos[2]);

      auto boxDimAttr = particleSpawnPrim.GetAttribute(TfToken("boxDim"));
      GfVec3f boxDim;
      boxDimAttr.Get(&boxDim);
      spawnContext.boxDim = Vector3(boxDim[0], boxDim[1], boxDim[2]);

      auto spawnDirAttr = particleSpawnPrim.GetAttribute(TfToken("spawnDir"));
      GfVec3f spawnDir;
      spawnDirAttr.Get(&spawnDir);
      spawnContext.spawnDir = Vector3(spawnDir[0], spawnDir[1], spawnDir[2]);

      auto radiusAttr = particleSpawnPrim.GetAttribute(TfToken("radius"));
      radiusAttr.Get(&spawnContext.radius);

      auto spawnTypeAttr = particleSpawnPrim.GetAttribute(TfToken("spawnType"));
      uint32_t spawnType;
      spawnTypeAttr.Get(&spawnType);
      spawnContext.spawnType = (ParticleSpawnType)spawnType;
    }
    {
      UsdPrim particleDesc = stage->GetPrimAtPath(SdfPath { path + "/ParticleDescriptor" });

      auto minSpawnColorAttr = particleDesc.GetAttribute(TfToken("minSpawnColor"));
      GfVec4f minSpawnColor;
      minSpawnColorAttr.Get(&minSpawnColor);
      desc.minSpawnColor = Vector4(minSpawnColor[0], minSpawnColor[1], minSpawnColor[2], minSpawnColor[3]);

      auto maxSpawnColorAttr = particleDesc.GetAttribute(TfToken("maxSpawnColor"));
      GfVec4f maxSpawnColor;
      maxSpawnColorAttr.Get(&maxSpawnColor);
      desc.maxSpawnColor = Vector4(maxSpawnColor[0], maxSpawnColor[1], maxSpawnColor[2], maxSpawnColor[3]);

      auto minTargetColorAttr = particleDesc.GetAttribute(TfToken("minTargetColor"));
      GfVec4f minTargetColor;
      minTargetColorAttr.Get(&minTargetColor);
      desc.minTargetColor = Vector4(minTargetColor[0], minTargetColor[1], minTargetColor[2], minTargetColor[3]);

      auto maxTargetColorAttr = particleDesc.GetAttribute(TfToken("maxTargetColor"));
      GfVec4f maxTargetColor;
      maxTargetColorAttr.Get(&maxTargetColor);
      desc.maxTargetColor = Vector4(maxTargetColor[0], maxTargetColor[1], maxTargetColor[2], maxTargetColor[3]);

      auto minTargetSizeAttr = particleDesc.GetAttribute(TfToken("minTargetSize"));
      minTargetSizeAttr.Get(&desc.minTargetSize);

      auto maxTargetSizeAttr = particleDesc.GetAttribute(TfToken("maxTargetSize"));
      maxTargetSizeAttr.Get(&desc.maxTargetSize);

      auto minTargetRotationSpeedAttr = particleDesc.GetAttribute(TfToken("minTargetRotationSpeed"));
      minTargetRotationSpeedAttr.Get(&desc.minTargetRotationSpeed);

      auto maxTargetRotationSpeedAttr = particleDesc.GetAttribute(TfToken("maxTargetRotationSpeed"));
      maxTargetRotationSpeedAttr.Get(&desc.maxTargetRotationSpeed);

      auto minTtlAttr = particleDesc.GetAttribute(TfToken("minTtl"));
      minTtlAttr.Get(&desc.minTtl);

      auto maxTtlAttr = particleDesc.GetAttribute(TfToken("maxTtl"));
      maxTtlAttr.Get(&desc.maxTtl);

      auto initialVelocityFromNormalAttr = particleDesc.GetAttribute(TfToken("initialVelocityFromNormal"));
      initialVelocityFromNormalAttr.Get(&desc.initialVelocityFromNormal);

      auto initialVelocityConeAngleDegreesAttr = particleDesc.GetAttribute(TfToken("initialVelocityConeAngleDegrees"));
      initialVelocityConeAngleDegreesAttr.Get(&desc.initialVelocityConeAngleDegrees);

      auto minSpawnSizeAttr = particleDesc.GetAttribute(TfToken("minSpawnSize"));
      minSpawnSizeAttr.Get(&desc.minSpawnSize);

      auto maxSpawnSizeAttr = particleDesc.GetAttribute(TfToken("maxSpawnSize"));
      maxSpawnSizeAttr.Get(&desc.maxSpawnSize);

      auto gravityForceAttr = particleDesc.GetAttribute(TfToken("gravityForce"));
      gravityForceAttr.Get(&desc.gravityForce);

      auto maxSpeedAttr = particleDesc.GetAttribute(TfToken("maxSpeed"));
      maxSpeedAttr.Get(&desc.maxSpeed);

      auto turbulenceFrequencyAttr = particleDesc.GetAttribute(TfToken("turbulenceFrequency"));
      turbulenceFrequencyAttr.Get(&desc.turbulenceFrequency);

      auto turbulenceForceAttr = particleDesc.GetAttribute(TfToken("turbulenceForce"));
      turbulenceForceAttr.Get(&desc.turbulenceForce);

      auto motionTrailMultiplierAttr = particleDesc.GetAttribute(TfToken("motionTrailMultiplier"));
      motionTrailMultiplierAttr.Get(&desc.motionTrailMultiplier);

      auto minSpawnRotationSpeedAttr = particleDesc.GetAttribute(TfToken("minSpawnRotationSpeed"));
      minSpawnRotationSpeedAttr.Get(&desc.minSpawnRotationSpeed);

      auto spawnRateAttr = particleDesc.GetAttribute(TfToken("spawnRate"));
      spawnRateAttr.Get(&desc.spawnRate);

      auto collisionThicknessAttr = particleDesc.GetAttribute(TfToken("collisionThickness"));
      collisionThicknessAttr.Get(&desc.collisionThickness);

      auto collisionRestitutionAttr = particleDesc.GetAttribute(TfToken("collisionRestitution"));
      collisionRestitutionAttr.Get(&desc.collisionRestitution);

      auto initialVelocityFromMotionAttr = particleDesc.GetAttribute(TfToken("initialVelocityFromMotion"));
      initialVelocityFromMotionAttr.Get(&desc.initialVelocityFromMotion);

      auto maxNumParticlesAttr = particleDesc.GetAttribute(TfToken("maxNumParticles"));
      maxNumParticlesAttr.Get(&desc.maxNumParticles);

      auto billboardTypeAttr = particleDesc.GetAttribute(TfToken("billboardType"));
      uint32_t type;
      billboardTypeAttr.Get(&type);
      desc.billboardType = ParticleBillboardType(type);

      auto enableMotionTrailAttr = particleDesc.GetAttribute(TfToken("enableMotionTrail"));
      enableMotionTrailAttr.Get(&desc.enableMotionTrail);

      auto useTurbulenceAttr = particleDesc.GetAttribute(TfToken("useTurbulence"));
      useTurbulenceAttr.Get(&desc.useTurbulence);

      auto alignParticlesToVelocityAttr = particleDesc.GetAttribute(TfToken("alignParticlesToVelocity"));
      alignParticlesToVelocityAttr.Get(&desc.alignParticlesToVelocity);

      auto useSpawnTexcoordsAttr = particleDesc.GetAttribute(TfToken("useSpawnTexcoords"));
      useSpawnTexcoordsAttr.Get(&desc.useSpawnTexcoords);

      auto enableCollisionDetectionAttr = particleDesc.GetAttribute(TfToken("enableCollisionDetection"));
      enableCollisionDetectionAttr.Get(&desc.enableCollisionDetection);
    }
    {
      UsdPrim particleMaterialPrim = stage->GetPrimAtPath(SdfPath { path + "/ParticleMaterial" });
      auto albedoPathAttr = particleMaterialPrim.GetAttribute(TfToken("albedoPath"));
      SdfAssetPath path;
      albedoPathAttr.Get(&path);
      particleMaterial.m_albedoPath = path.GetAssetPath().c_str();

      auto emissiveIntensityAttr = particleMaterialPrim.GetAttribute(TfToken("emissiveIntensity"));
      emissiveIntensityAttr.Get(&particleMaterial.emissiveIntensity);
    }
  }
  // MHFZ end
} 