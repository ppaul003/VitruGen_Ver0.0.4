# Sprint 003 — PARTICLE_SIM Runtime Configuration A0

## Scope

Implemented the Layer 2 draft-to-runtime bridge for:

`SIMCAD_4D -> PARTICLE_SIM`

Prototype workspace:

`C:\Users\richa\Anaheim Systems Dynamics - Software Product Dev\VitruGen_ver004_prt_a0`

Development branch:

`feature/particle-sim-runtime-config-a0`

No merge, commit, or push was performed.

## 1. Files Modified

- `VitruGen_ver004_prt_a0/VitruGen_ver004/particleSystem.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/particleSystem.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/EuclidEngine.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/EuclidEngine.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/TheTesseract.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/TheTesseract.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/ViewPort.cpp`

Files added:

- `VitruGen_ver004_prt_a0/VitruGen_ver004/ParticleSimRuntimeConfig.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/ParticleSimRuntimeConfigTests.cpp`
- `.agents/reports/SPRINT_003_PARTICLE_SIM_RUNTIME_CONFIG_A0_REPORT.md`

## 2. Capacity/Active-Count Implementation

`ParticleSystem` retains `m_numParticles` as its fixed allocated capacity and
adds `m_activeParticleCount` as the runtime work count.

Added:

- `getCapacity()`
- `getActiveParticleCount()`
- `setActiveParticleCount()`

The constructor initializes active count to capacity, preserving baseline
behavior before a Layer 2 configuration is applied. Setting active count:

- accepts `0..capacity`
- rejects values above capacity
- updates `m_params.numBodies`
- refreshes CUDA simulation parameters
- never reallocates host, CUDA, or OpenGL resources

`ParticleSimRuntimeConfig::resolve()` is the shared pure validation path used
by EuclidEngine and focused tests.

## 3. CUDA Launch-Count Changes

`ParticleSystem::update()` returns before mapping the OpenGL position VBO when
active count is zero.

The following calls now receive `m_activeParticleCount`:

- `integrateSystem`
- `forcesKernel`
- `calcHash`
- `sortParticles`
- `reorderDataAndFindCellStart`
- `collide`

Allocated buffers remain capacity-sized.

## 4. Reset-Mode Mapping

EuclidEngine now reads:

`TheArbiter::ParticleSimDraftConfig::resetMode`

Mapping:

- `Default` -> `CNFG_DEFAULT_RESTART`
- `Random` -> `CNFG_RANDOM_RESTART`

The active count is validated and applied before reset.

DEFAULT reset builds a compact lattice containing exactly the active count.
RANDOM reset initializes exactly the active count inside the configured
simulation domain, keeping particle centers inside the boundary by their
radius. Only the active position, velocity, and acceleration ranges upload.

Zero-count reset returns safely without an upload or CUDA launch.

## 5. DEFAULT Color-Ramp Implementation

`setDefaultColorRamp()` now applies the existing seven-color ramp only across
the active range. It safely handles:

- zero particles
- one particle
- any active count through capacity

No later uniform-color call overwrites the PARTICLE_SIM ramp.

## 6. RGB Range Implementation

Added:

`setRGBParticleCounts(redCount, greenCount, blueCount)`

It validates that:

- channel total equals active count
- channel total does not exceed capacity

It assigns contiguous active ranges:

- RED: `{1.00, 0.05, 0.00, 1.00}`
- GREEN: `{0.00, 1.00, 0.25, 1.00}`
- BLUE: `{0.00, 0.25, 1.00, 1.00}`

The method updates the color VBO and the CPU particle-class array only for
active entries. It does not apply the DEFAULT ramp in RGB mode.

## 7. Renderer Active-Count Synchronization

`Tesseract::syncPSRendering()` now binds:

`ParticleSystem::getActiveParticleCount()`

instead of capacity.

The common particle binding path now accepts a zero draw count, continues to
bind the diagnostic grid and particle resources, and sets the renderer draw
count to zero. `dumpRadii(destination, count)` copies only the requested
active range and validates `count <= capacity`.

SINGLE_PARTICLE_MCAD continues binding its one-particle system independently.

## 8. Layer 3 Status Changes

The PARTICLE_SIM Layer 3 overlay now displays:

- `COLOR MODE: DEFAULT` or `COLOR MODE: RGB`
- RGB channel counts when applicable
- `ACTIVE: current / 16384`
- `RESET: DEFAULT` or `RESET: RANDOM`
- `STATUS: RUNNING` or `STATUS: PAUSED`

The existing Q, Space, Enter, RMB, and wheel help remains present.

The legacy `COLOR: RED` presentation is no longer used for PARTICLE_SIM.

## 9. Gravity Configuration

`PSSimulationConfig::gravityMagnitude` now defaults to:

`0.0003f`

The existing collision spring, damping, shear, and attraction values remain
unchanged.

## 10. Build Command and Result

Commands:

```powershell
MSBuild.exe VitruGen_ver004.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo
MSBuild.exe VitruGen_ver004.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
```

Final results:

- Debug x64: PASS, 0 errors, 0 warnings on the final incremental build
- Release x64: PASS, 0 errors, 1 existing `LNK4098` warning

An earlier full Debug build also reported the existing Tesseract
float-to-int conversion warning.

The unavailable `pwsh.exe` application-local hook still falls back
successfully to Windows PowerShell.

## 11. Manual Tests Performed

No interactive OpenGL acceptance test was claimed during this automated
implementation session.

The following require user-visible runtime verification:

- DEFAULT 4200 lattice and seven-color ramp
- RGB 420/240/120 ranges
- RGB 780/0/0 all-red range
- DEFAULT versus RANDOM placement
- zero-particle grid-only workspace
- full 16384 capacity
- 4200-to-780 reconfiguration
- visible gravity, particle collision, and boundary collision
- pause and single-step controls

The executable builds and links successfully for both configurations.

## 12. Regression Tests Performed

Focused test:

`ParticleSimRuntimeConfigTests.cpp`

Result:

```text
PASS: PARTICLE_SIM runtime configuration resolution tests
```

Covered:

- DEFAULT count 4200
- RGB mixed total 780
- RGB all-red total 780
- zero active particles
- full capacity 16384
- rejection above capacity

Static source-contract audit passed for:

- zero-count CUDA guard
- active count in all six CUDA work stages
- active-range reset uploads
- renderer active-count binding
- nonzero gravity
- shared draft resolver
- Layer 3 active/RGB status

SHA-256 comparison confirmed these protected files match the baseline:

- `TheArbiter.h/.cpp`
- `TextEntry.h/.cpp`
- `kernel.cu`
- `kernel_impl.cuh`
- `marchingCubes.h/.cpp`
- `Camera.h/.cpp`

The older `ParticleSimPanelTests.cpp` from the previous prototype snapshot was
also attempted. It compiles against the current dependencies but fails on
older Layer 1 cycling assumptions that no longer match this accepted source
baseline. Arbiter was intentionally not changed to satisfy that stale test.

## 13. Known Limitations

- Final OpenGL visual and physics acceptance remains interactive.
- Runtime failure defensively returns to Layer 2 through the existing public
  Arbiter `Q` transition, but invalid totals should already be prevented by
  TextEntry.
- Layer 3 displays the accepted Arbiter draft, which is the applied snapshot
  for this sprint.
- Existing compiler/linker warnings remain outside sprint scope.
- Pre-existing whitespace exists in legacy source and was not mechanically
  rewritten.

## 14. Work Intentionally Deferred

- Uniform/random radius tuning
- Grid-layout runtime modes
- PARTICLE_SIM sub-layers

Also unchanged:

- CUDA collision algorithms
- buffer allocation capacity
- SINGLE_PARTICLE_MCAD behavior
- LINKED_PARTICLES_MCAD
- SANDBOX_SIM
- volume rendering
- Marching Cubes
- OBJ export
- camera behavior
