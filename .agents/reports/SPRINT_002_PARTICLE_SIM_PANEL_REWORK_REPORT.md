# Sprint 002 — PARTICLE_SIM Panel Rework

## Scope

Implemented the draft side-panel and navigation foundation for:

`SIMCAD_4D -> PARTICLE_SIM`

Development workspace:

`C:\Users\richa\Anaheim Systems Dynamics - Software Product Dev\VitruGen_ver004_rev_a0`

Development branch:

`feature/particle-sim-panel-rework`

No commit, merge, or push was performed.

## 1. Files Added

- `VitruGen_ver004_rev_a0/VitruGen_ver004/ParticleSimPanelTests.cpp`
  - Standalone Arbiter navigation and draft-state regression tests.
  - Not added to the production Visual Studio project.
- `.agents/reports/SPRINT_002_PARTICLE_SIM_PANEL_REWORK_REPORT.md`
  - This completion report.

## 2. Files Modified

- `VitruGen_ver004_rev_a0/VitruGen_ver004/TheArbiter.h`
- `VitruGen_ver004_rev_a0/VitruGen_ver004/TheArbiter.cpp`
- `VitruGen_ver004_rev_a0/VitruGen_ver004/ViewPort.h`
- `VitruGen_ver004_rev_a0/VitruGen_ver004/ViewPort.cpp`

No protected runtime, renderer, CUDA, Marching Cubes, Camera, Tesseract,
ParticleSystem, TextEntry, or SINGLE_PARTICLE_MCAD source file was modified.

## 3. Enums and Draft-State Structures Introduced

`TheArbiter` now owns the following PARTICLE_SIM-specific draft types:

- `ParticleGridLayout`
  - `None`
  - `Minimal`
  - `Full`
  - `Dynamic`
- `ParticleColorMode`
  - `Default`
  - `RGB`
- `ParticleRadiusMode`
  - `Uniform`
  - `Random`
- `ParticleColorChannel`
  - `Red`
  - `Green`
  - `Blue`
- `ParticleSimResetMode`
  - `Default`
  - `Random`
- `ParticleSimLayer1Item`
  - `Workspace`
  - `GridLayout`
  - `ColorMode`
  - `RadiusMode`
  - `Configure`
- `ParticleSimDraftConfig`
- `ParticleCountEntryRequest`

The draft state defaults are:

- Grid layout: `DYNAMIC`
- Color mode: `DEFAULT`
- Radius mode: `UNIFORM`
- Default particle amount: `4200`
- RGB counts: `0 / 0 / 0`
- Selected RGB channel: `RED`
- Reset mode: `DEFAULT`
- Uniform radius: `0.0120`
- Minimum radius: `0.0098`
- Maximum radius: `0.0156`

The displayed capacity is `16384`.

These values are presentation and navigation state only.

## 4. Layer 1 Navigation Behavior

When the Layer 1 context is the SIMCAD_4D PARTICLE_SIM/SANDBOX_SIM panel:

- `W/S` wraps through all five visible rows.
- `A/D` changes:
  - Row 1: `PARTICLE_SIM <-> SANDBOX_SIM`
  - Row 2: `NONE / MINIMAL / FULL / DYNAMIC`
  - Row 3: `DEFAULT / RGB`
  - Row 4: `UNIFORM / RANDOM`
- Row 5 does not change with `A/D`.
- `E` or `Enter` on rows 1–4 does not advance.
- `E` or `Enter` on row 5 enters Layer 2 only when `PARTICLE_SIM` is selected.
- `SANDBOX_SIM` remains visible but its runtime stays reserved.
- `Q` continues to use the existing global layer-retreat path.

Non-PARTICLE_SIM Layer 1 paths retain their existing navigation.

## 5. Layer 2 Conditional-Layout Behavior

Layer 2 uses a separate PARTICLE_SIM cursor and dynamically reports four or
five rows:

- `DEFAULT + UNIFORM`: four rows
- `DEFAULT + RANDOM`: five rows
- `RGB + UNIFORM`: four rows plus RGB totals
- `RGB + RANDOM`: five rows plus RGB totals

Behavior:

- `W/S` wraps through the visible row count.
- Changing the radius mode clamps a previously selected fifth row to the
  four-row layout’s run row.
- Default-color row 1 changes particle amount by `100` and clamps to
  `0..16384`.
- RGB row 1 cycles `RED -> GREEN -> BLUE`.
- Reset row toggles `DEFAULT/RANDOM`.
- Uniform radius uses the isolated 17-value preset table.
- Random minimum and maximum radius use the same preset table.
- Minimum radius cannot exceed maximum radius.
- Maximum radius cannot fall below minimum radius.
- Radius values render with four decimal places.
- RGB totals are presentation-only.

Independent default-count, RGB-count, uniform-radius, minimum-radius, and
maximum-radius draft values are preserved when modes change.

## 6. Text-Entry Hooks Prepared

Added:

- `CMD_REQUEST_DEFAULT_PARTICLE_COUNT_ENTRY`
- `CMD_REQUEST_RGB_PARTICLE_COUNT_ENTRY`
- `ParticleCountEntryRequest`

`E` or `Enter` on Layer 2 row 1 records the request and selected RGB channel.
The panel displays:

`EXACT PARTICLE ENTRY PENDING TEXT-ENTRY INTEGRATION`

No number-key parsing, temporary numeric buffer, or second text-entry system
was introduced.

## 7. Build Command

Debug:

```powershell
MSBuild.exe VitruGen_ver004.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo
```

Release:

```powershell
MSBuild.exe VitruGen_ver004.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
```

## 8. Build Result

- Debug x64: PASS, 0 errors
- Release x64: PASS, 0 errors

Existing warnings remain:

- `TheTesseract.cpp`: float-to-int conversion warning
- `LNK4098`: default library conflict

The sprint did not modify the protected files responsible for those warnings.

## 9. Tests Performed

Standalone test:

`ParticleSimPanelTests.cpp`

Compiler settings:

```text
/std:c++17 /EHsc /W4 /WX /wd4127
```

Result:

```text
PASS: PARTICLE_SIM panel navigation and draft-state tests
```

Covered behavior:

- SIMCAD_4D/PARTICLE_SIM entry
- Five-row Layer 1 traversal
- No Layer 1 advance from rows 1–4
- PARTICLE_SIM/SANDBOX_SIM cycling
- Grid, color, and radius mode changes
- All four Layer 2 conditional combinations
- Four/five-row cursor clamping
- Default-count lower and upper bounds
- Reset-mode toggle
- Uniform-radius preset advancement
- RGB channel cycling
- RGB count-entry request channel
- Random minimum/maximum ordering
- Future default-count entry request
- Preserved `CMD_START_PARTICLE_SIMULATION`
- SINGLE_PARTICLE_MCAD selection and configuration navigation regression

Static verification:

- All required Layer 1 and Layer 2 labels were located in `ViewPort.cpp`.
- `TODO(PARTICLE_SIM_DRAFT_APPLICATION)` marks the deferred runtime handoff.
- SHA-256 comparison confirmed every protected source file remained unchanged.

Runtime visual acceptance still requires interactive user verification of panel
spacing, colors, and navigation inside the OpenGL application.

## 10. Runtime Behavior Intentionally Left Unchanged

The run row still emits:

`CMD_START_PARTICLE_SIMULATION`

The existing runtime continues to use its established 16,384-particle path and
legacy supported selections. The new draft values do not:

- Allocate particles
- Change active CUDA particle count
- Allocate RGB particle groups
- Apply uniform or random draft radii
- Modify CUDA buffers
- Change simulation parameters

The source contains:

`TODO(PARTICLE_SIM_DRAFT_APPLICATION)`

to make this boundary explicit.

## 11. Known Limitations

- Exact particle-count entry is a request/status hook only.
- RGB counts remain zero until TextEntrySession integration.
- RGB total validation against capacity is deferred.
- Draft particle count does not change the active runtime count.
- Draft radius values do not change ParticleSystem state.
- Random per-particle radii are not implemented.
- SANDBOX_SIM remains reserved.
- No mouse-based menu selection was added.
- Final OpenGL panel appearance must be accepted interactively.

## 12. Recommended TextEntrySession Integration Points

Recommended future flow:

1. `EuclidEngine::onKeyboard` checks whether `TextEntrySession` is active
   before normal `KeyboardInput`/Arbiter navigation.
2. `CMD_REQUEST_DEFAULT_PARTICLE_COUNT_ENTRY` opens an unsigned-integer
   session seeded with `defaultParticleCount`.
3. `CMD_REQUEST_RGB_PARTICLE_COUNT_ENTRY` opens an unsigned-integer session
   seeded with the selected channel’s current count.
4. On commit, EuclidEngine calls a narrow Arbiter setter that validates:
   - `0..16384` for default count
   - RGB combined total `<= 16384`
5. On cancel, the existing draft value remains unchanged.
6. The text-entry component remains independent of ViewPort, ParticleSystem,
   CUDA, and renderer resources.

Runtime application of committed draft values should remain a separate sprint
after text entry and active-count capacity rules are accepted.
