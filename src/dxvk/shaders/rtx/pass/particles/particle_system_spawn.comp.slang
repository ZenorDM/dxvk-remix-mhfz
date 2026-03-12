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
// MHFZ start
//!variant particle_system_spawn.comp

//!variant particle_system_spawn_area.comp
//!>       AREA_SPAWN

//!end-variants
// MHFZ end

#include "rtx/pass/particles/particle_system_bindings.slangh"

#include "rtx/utility/common.slangh"
#include "rtx/utility/noise.slangh"
#include "rtx/utility/procedural_noise.slangh"
#include "rtx/utility/brdf.slangh"
#include "rtx/utility/packing.slangh"
#include "rtx/utility/gbuffer_helpers.slangh"
#include "rtx/utility/debug_view_helpers.slangh"
#include "rtx/concept/camera/camera.slangh"

layout(binding = PARTICLE_SYSTEM_BINDING_SPAWN_CONTEXTS_INPUT)
StructuredBuffer<GpuSpawnContext> SpawnContexts;

// Outputs

layout(binding = PARTICLE_SYSTEM_BINDING_PARTICLES_BUFFER_INPUT_OUTPUT)
RWStructuredBuffer<GpuParticle> Particles;


f16vec3 sampleDirectionInCone(f16vec3 axis, float cosConeHalfAngle, float2 rndUV)
{
  // Sample a direction in a cone about +Z, and get its PDF (we ignore PDF here)
  float solidAnglePdf;
  f16vec3 localDir = calcUniformConeDirectionSample(f16vec2(rndUV), cosConeHalfAngle, solidAnglePdf);

  f16vec3 tangent, bitangent;
  calcOrthonormalBasis(axis, tangent, bitangent);

  // Rotate the local-Z sample into the world-axis frame
  return localDir.x * tangent + localDir.y * bitangent + localDir.z * axis;
}

// MHFZ start
float3 spawnAtTarget(const GpuParticleSystem particleSystem, float3 random01, float randSeed) {
  return float3(particleSystem.desc.targetPos.x, particleSystem.desc.targetPos.y - 85, particleSystem.desc.targetPos.z) + float3(normalize(random01 * 2.0 - 1.0) * randSeed * 50);
}
// MHFZ end

// Generate a new particle in-place with random properties
GpuParticle respawn(uint particleIdx, const GpuParticleSystem particleSystem)
{
  GpuParticle newParticle;
  
  // Get spawn context
  // MHFZ START
  GpuSpawnContext spawnCtx = SpawnContexts[0];
  
  float3 spawnPosition = 0.0;
  float3 initialVelocity = 0.0;
  float4 uvMinMax = float4(0..xx, 1..xx);
  float16_t4 color = 1.h;
  uint randomSeed = particleIdx;
  float3 worldNormal;
  const float randSeed = randomFloat(uint2(randomSeed++, particleCb.frameIdx));
  // MHFZ end
  // Get random triangle
#ifndef AREA_SPAWN
    float3 positions[3];
    float3 prevPositions[3];

    float3 random01 = float3(randomFloat(uint2(randomSeed++, particleCb.frameIdx)), randomFloat(uint2(randomSeed++, particleCb.frameIdx)), randomFloat(uint2(randomSeed++, particleCb.frameIdx)));
    float3 randomBary = saturate(random01 / dot(random01, 1..xxx));

    uint triIdx = randomFloat(uint2(randomSeed++, particleCb.frameIdx)) * spawnCtx.numTriangles;

  for (uint i=0 ; i<3 ; i++)
    {
      uint indexIdx = triIdx * 3 + i;
      uint index = spawnCtx.indices32bit ? BUFFER_ARRAY(indices32, (uint)spawnCtx.spawnMeshIndexIdx, indexIdx) : BUFFER_ARRAY(indices, (uint)spawnCtx.spawnMeshIndexIdx, indexIdx);

      const uint baseSrcPositionOffset = (spawnCtx.spawnMeshPositionsOffset + index * spawnCtx.spawnMeshPositionsStride) / 4;
      positions[i] = float3(BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshPositionsIdx, baseSrcPositionOffset + 0),
        BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshPositionsIdx, baseSrcPositionOffset + 1),
        BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshPositionsIdx, baseSrcPositionOffset + 2));

      if (spawnCtx.spawnMeshPrevPositionsIdx != BINDING_INDEX_INVALID)
      {
        prevPositions[i].x = BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshPrevPositionsIdx, baseSrcPositionOffset + 0);
        prevPositions[i].y = BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshPrevPositionsIdx, baseSrcPositionOffset + 1);
        prevPositions[i].z = BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshPrevPositionsIdx, baseSrcPositionOffset + 2);
      }
      else
      {
        prevPositions[i] = positions[i];
      }

    if(spawnCtx.spawnMeshColorsIdx != BINDING_INDEX_INVALID)
      {
        const uint baseSrcColorOffset = (spawnCtx.spawnMeshColorsOffset + index * spawnCtx.spawnMeshColorsStride) / 4;
      const uint colorBits = floatBitsToUint(BUFFER_ARRAY(geometries, (uint) spawnCtx.spawnMeshColorsIdx, baseSrcColorOffset));
        // Format is B8G8R8A8_UNORM
        color += unorm4x8ToFloat4x16(colorBits).zyxw * randomBary[i];
      }

      const uint baseSrcTexcoordOffset = (spawnCtx.spawnMeshTexcoordsOffset + index * spawnCtx.spawnMeshTexcoordsStride) / 4;
      float2 texcoord = float2(BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshTexcoordsIdx, baseSrcTexcoordOffset + 0),
        BUFFER_ARRAY(geometries, (uint)spawnCtx.spawnMeshTexcoordsIdx, baseSrcTexcoordOffset + 1));

      if (particleSystem.desc.useSpawnTexcoords)
      {
        if (i == 0)
        {
          uvMinMax.xy = texcoord;
          uvMinMax.zw = texcoord;
        }
        else
        {
          uvMinMax.xy = min(uvMinMax.xy, texcoord);
          uvMinMax.zw = max(uvMinMax.zw, texcoord);
        }
      }
    }

    // Sample random point on triangle

    float3 prevWorldPosition = mul(spawnCtx.spawnPrevObjectToWorld, float4(interpolateHitAttribute(prevPositions, randomBary), 1.f)).xyz;
    float3 worldPosition = mul(spawnCtx.spawnObjectToWorld, float4(interpolateHitAttribute(positions, randomBary), 1.f));

    float3 worldSpawnVelocity = (worldPosition - prevWorldPosition) * particleCb.invDeltaTimeSecs;
    float3 localNormal = cross(normalize(positions[1] - positions[0]), normalize(positions[2] - positions[0]));
    worldNormal = mul(spawnCtx.spawnObjectToWorld, float4(localNormal, 0.f));
    // MHFZ use spawn direction context if setup
    worldNormal = length(spawnCtx.spawnDir) > 0 ? normalize(spawnCtx.spawnDir) : worldNormal;
    initialVelocity = worldSpawnVelocity * particleSystem.desc.initialVelocityFromMotion;

    spawnPosition = lerp(worldPosition, prevWorldPosition, randSeed);

  
#else
    // MHFZ start : area particle spawn
    uint xForRand = hashJenkins(randomSeed++)*10000;
    uint yForRand = hashJenkins(randomSeed++) * 10000;
    uint zForRand = hashJenkins(randomSeed++) * 10000;
    float3 random01 = randomFloat3(uint3(xForRand, yForRand, zForRand));
    float3 worldPosition;
    if (spawnCtx.spawnType == ParticleSpawnType::Sphere) {
      worldPosition = mul(spawnCtx.spawnObjectToWorld, float4(normalize(random01 * 2.0 - 1.0) * randSeed * spawnCtx.radius,1.0f));
    }
    else {
      worldPosition = mul(spawnCtx.spawnObjectToWorld, float4((random01 * 2.0 - 1.0) * randSeed * spawnCtx.boxDim, 1.0f));
    }

    spawnPosition = worldPosition;

    worldNormal = normalize(spawnCtx.spawnDir);
    // MHFZ end
#endif

  float cosCone = cos(radians(particleSystem.desc.initialVelocityConeAngleDegrees));
  f16vec3 dir = sampleDirectionInCone(worldNormal, cosCone, float2(randomFloat(uint2(randomSeed++, particleCb.frameIdx)), randomFloat(uint2(randomSeed++, particleCb.frameIdx))));

  initialVelocity =  dir * particleSystem.desc.initialVelocityFromNormal;

  // Spawn a particle
  newParticle.reset(particleSystem, spawnPosition, initialVelocity, uvMinMax, color, randSeed);
  
  return newParticle;
}


[shader("compute")]
[numthreads(128, 1, 1)]
void main(uint2 threadIdx : SV_DispatchThreadID)
{
  const GpuParticleSystem particleSystem = particleCb.particleSystem;
  
  if(threadIdx.x >= particleSystem.spawnParticleCount)
  {
    return;
  }
  
  const uint particleIdx = (threadIdx.x + particleSystem.spawnParticleOffset) % particleSystem.desc.maxNumParticles;
  
  const bool shouldRespawn = Particles[particleIdx].isDead() || Particles[particleIdx].isSleeping();
  if (shouldRespawn)
  {
    Particles[particleIdx] = respawn(particleIdx, particleSystem);
  }
}
