# Sprint 004 — PARTICLE_SIM Runtime Radius Configuration A0

## Result

Implementation complete on:

`feature/particle-sim-runtime-radius-a0`

The runtime path is now:

`ParticleSimDraftConfig`
→ validated runtime configuration
→ placement radius
→ active reset
→ active `velocity.w` assignment
→ CUDA collision/boundary use
→ active renderer-radius readback
→ Layer 3 status

The branch has not been merged.

## 1. Files modified

- `VitruGen_ver004_prt_a0/VitruGen_ver004/ParticleSimRuntimeConfig.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/ParticleSimRuntimeConfigTests.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/particleSystem.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/particleSystem.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/EuclidEngine.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/EuclidEngine.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/ViewPort.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/kernel_impl.cuh`
- `.agents/reports/SPRINT_004_PARTICLE_SIM_RUNTIME_RADIUS_A0_REPORT.md`

## 2. Uniform radius implementation

`ParticleSystem::setUniformActiveRadii(float radius)` now:

- rejects non-finite, non-positive, and greater-than-`0.0156f` values;
- updates `m_params.particleRadius`;
- safely handles zero active particles;
- reads the active device velocity range;
- preserves velocity XYZ;
- assigns the requested radius to active velocity W values only;
- uploads only the active velocity range.

## 3. Random radius implementation

`ParticleSystem::setRandomActiveRadii(minimum, maximum, seed)` now:

- validates both finite bounds;
- rejects non-positive, inverted, and unsupported ranges;
- treats equal minimum/maximum bounds safely;
- uses deterministic seed `1973`;
- stores `maximumRadius` as the conservative system reference;
- preserves active velocity XYZ;
- assigns one independently sampled W radius per active particle;
- uploads only the active velocity range.

## 4. Per-particle `velocity.w` synchronization

The authoritative active collision and render radius remains:

`m_hVel[i * 4 + 3]` / `m_dVel[i * 4 + 3]`

No second active-particle radius source of truth was introduced.

`dumpRadii(destination, count)` now rejects counts above either allocated
capacity or active count. The legacy no-count overload now reads the active
range rather than full capacity.

## 5. Reset-placement radius integration

`EuclidEngine::applyParticleSelectionsToSystem()` now performs:

1. count and radius validation;
2. active-count application;
3. conservative placement-radius selection;
4. reset using that placement radius;
5. final active per-particle radius assignment;
6. color assignment;
7. renderer synchronization.

UNIFORM placement uses `uniformRadius`.

RANDOM placement uses `maximumRadius`, which is the accepted lower-risk A0
strategy.

The existing DEFAULT lattice already uses:

- spacing `2.0f * m_params.particleRadius`;
- jitter `0.01f * m_params.particleRadius`;
- exactly `m_activeParticleCount` initialized entries.

## 6. Random-position boundary safety

The existing RANDOM reset derives its minimum and maximum center coordinates
from `m_params.particleRadius`.

Because RANDOM mode sets the placement reference to `maximumRadius` before
reset, every generated particle center is safe for every subsequently assigned
radius in the configured range.

This is conservative: smaller particles initially receive maximum-radius
clearance, then use their own W radius during live boundary collision.

## 7. Renderer radius synchronization

The existing `Tesseract::syncPSRendering()` path was audited and already:

- obtains `getActiveParticleCount()`;
- passes active draw count to the particle renderer;
- calls count-limited `dumpRadii()`;
- sends the same active radius mirror to `renderer->setRadius()`;
- safely uses draw count zero.

No Tesseract change was required.

## 8. Boundary-collision verification

Particle-particle contact already uses:

`velA.w + velB.w`

All six simulation-box boundary checks now use the particle’s W radius.

One confirmed negative-Z sign defect was corrected:

`position.z < -boundary + radius`

The CUDA collision algorithm was otherwise left unchanged.

## 9. Layer 3 status changes

The PARTICLE_SIM Layer 3 overlay now includes:

- `RADIUS MODE: UNIFORM` and the four-decimal radius; or
- `RADIUS MODE: RANDOM` and the four-decimal minimum/maximum range.

The original color, active count, reset, pause/run status, and control help
remain visible.

## 10. Build commands and results

Focused runtime resolver test:

```text
cl /std:c++17 /EHsc /W4 /WX ParticleSimRuntimeConfigTests.cpp
ParticleSimRuntimeConfigTests.exe
```

Result:

```text
PASS: PARTICLE_SIM count, color, and radius configuration resolution tests
```

Full builds:

```text
MSBuild VitruGen_ver004.sln /t:Build /p:Configuration=Debug /p:Platform=x64
MSBuild VitruGen_ver004.sln /t:Build /p:Configuration=Release /p:Platform=x64
```

Results:

- Debug x64: PASS, 0 errors
- Release x64: PASS, 0 errors

Existing warnings remain:

- pre-SM75 CUDA target deprecation;
- deprecated `cudaGLSetGLDevice`;
- existing `TheTesseract.cpp` float-to-int conversion;
- existing Release `LIBCMT` library conflict.

## 11. Manual tests performed

- Staged the project’s existing `freeglut.dll` and `glew64.dll` beside the
  Debug/Release executables.
- Launched the Debug executable successfully.
- Confirmed creation of the VitruGen OpenGL render window and live FPS title.

The desktop capture interface could not inspect the legacy OpenGL window
(`SetIsBorderRequired` unsupported). Therefore, the requested visual
UNIFORM/RANDOM configuration matrix remains pending user-run acceptance.

## 12. Regression tests performed

- Focused count/color/radius resolver tests passed.
- Debug and Release application builds passed.
- `git diff --check` passed.
- Source-contract audit confirmed no changes to:
  - `TheArbiter.h/.cpp`
  - `TextEntry.h/.cpp`
  - `kernel.cu`
  - Marching Cubes files
  - Camera files
- `kernel_impl.cuh` contains only the intended semantic negative-Z boundary
  correction; no collision redesign was introduced.
- SINGLE_PARTICLE_MCAD continues using its existing scalar radius setter and
  was not routed through the PARTICLE_SIM random-radius methods.

Hands-on SINGLE_PARTICLE behavior remains part of user runtime acceptance.

## 13. Known limitations

- Particle mass and density remain independent of radius.
- RANDOM reset uses maximum-radius placement clearance rather than first
  generating individual radii and packing each position against its exact
  radius.
- The random distribution is deterministic for repeatability.
- The legacy application deployment still relies on staging the two OpenGL
  runtime DLLs beside the executable.

## 14. Work intentionally deferred

- Radius-dependent mass
- Radius-dependent density
- PARTICLE_SIM sub-layers
- Grid-layout runtime modes
- New collision algorithms
- Dynamic GPU buffer reallocation
- LINKED_PARTICLES_MCAD changes
- SANDBOX_SIM changes

## Hands-on acceptance checklist

The following still requires visual/runtime confirmation:

- UNIFORM minimum radius
- UNIFORM maximum radius
- RANDOM full range
- RANDOM degenerate range
- DEFAULT reset with UNIFORM radius
- DEFAULT reset with RANDOM radius
- RANDOM reset with UNIFORM radius
- RANDOM reset with RANDOM radius
- zero active particles
- full capacity
- UNIFORM-to-UNIFORM reconfiguration
- RANDOM-to-UNIFORM reconfiguration
- SINGLE_PARTICLE_MCAD one-anchor regression
