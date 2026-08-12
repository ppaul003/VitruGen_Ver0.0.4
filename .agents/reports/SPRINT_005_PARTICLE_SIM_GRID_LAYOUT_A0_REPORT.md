# Sprint 005 — PARTICLE_SIM Grid Layout Runtime A0

## Result

Implementation complete on:

`feature/particle-sim-grid-layout-a0`

The runtime presentation path is now:

`ParticleSimDraftConfig::gridLayout`
→ `EuclidEngine::applyPSGridLayoutToWorkspace()`
→ `Tesseract::PSGridVisualState`
→ `EuclidRenderer` boundary/major/minor/axes visibility

The branch has not been merged.

## 1. Files modified

Sprint 005 modified:

- `VitruGen_ver004_prt_a0/VitruGen_ver004/TheArbiter.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/EuclidEngine.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/EuclidEngine.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/TheTesseract.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/TheTesseract.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/renderer_Euclid.h`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/renderer_Euclid.cpp`
- `VitruGen_ver004_prt_a0/VitruGen_ver004/ViewPort.cpp`
- `.agents/reports/SPRINT_005_PARTICLE_SIM_GRID_LAYOUT_A0_REPORT.md`

The repository already contained unrelated dirty files outside the prototype
and uncommitted Sprint 004 radius work. Those files were preserved.

## 2. Renderer boundary/grid refactor

The renderer now exposes explicit workspace-grid visibility:

```text
drawBoundary
drawMajor
drawMinor
drawAxes
```

The misleading `drawWorkspaceBox()` helper was removed.

Its responsibilities were split into:

- `drawWorkspaceBoundary()` — exactly the twelve outer cube edges;
- `drawWorkspaceMajorGrid()` — the internal sparse major grid.

SINGLE_PARTICLE_MCAD calls both split helpers, preserving its previous visual
composition.

The general 3D `displayGrid()` path now conditionally draws:

1. axes;
2. boundary;
3. major lattice;
4. minor lattice.

Particle rendering remains a separate call and is unaffected by the flags.

## 3. FULL implementation

FULL is now the default draft and applied Tesseract mode.

```text
boundary = true
major = true
minor = true
axes = true
dynamicPlaceholder = false
majorStride = 8
```

## 4. MINIMAL implementation

```text
boundary = true
major = true
minor = false
axes = true
dynamicPlaceholder = false
majorStride = 8
```

## 5. NONE implementation

```text
boundary = true
major = false
minor = false
axes = true
dynamicPlaceholder = false
majorStride = 8
```

The PARTICLE_SIM preview suppresses the global moving diagnostic slice, so
NONE cannot be polluted by a center-plane or face grid.

## 6. DYNAMIC placeholder implementation

```text
boundary = true
major = true
minor = true
axes = true
dynamicPlaceholder = true
majorStride = 8
```

DYNAMIC intentionally uses FULL rendering and displays:

```text
HASH DEBUG: RESERVED
VISUAL FALLBACK: FULL
```

No hash data, CUDA hash-buffer readback, occupied cells, heat maps, or neighbor
overlays were fabricated.

## 7. Immediate Layer 1 preview behavior

During the PARTICLE_SIM Layer 1 panel, `onDisplay()` reads the current Arbiter
draft and applies it to the Tesseract before drawing the preview.

A/D changes therefore become visible on the next redraw without requiring E,
Layer 2 entry, simulation start, or reset.

The Layer 1 status line reports:

`PARTICLE_SIM grid preview: <MODE>`

DYNAMIC also identifies its FULL fallback and reserved hash-debug status.

## 8. Layer 2 and Layer 3 persistence

The Arbiter draft remains the authoritative selection.

The same bridge is used while:

- viewing Layer 1;
- viewing PARTICLE_SIM Layer 2;
- starting Layer 3;
- rendering active Layer 3;
- returning from Layer 3.

Pause, step, and workspace re-entry do not reset the draft selection.

Non-PARTICLE_SIM global rendering reapplies the normal universal grid state,
preventing a PARTICLE_SIM NONE or DYNAMIC selection from leaking into other
workspaces.

## 9. Layer 3 overlay changes

The runtime overlay now shows:

`GRID LAYOUT: FULL | MINIMAL | NONE | DYNAMIC`

It preserves:

- color mode;
- active/capacity count;
- reset mode;
- radius mode/value/range;
- running/paused state;
- existing controls.

DYNAMIC adds:

`HASH DEBUG: RESERVED     VISUAL FALLBACK: FULL`

## 10. Removal of the `simulationBoxSize` / `majorStride` bug

Removed:

```text
m_workspaceGridVisual.majorStride = config.simulationBoxSize;
```

The values are now independent:

- physical simulation box size: `4.0f`;
- collision cells per axis: `64`;
- visual major stride: `8`;
- visible major divisions per axis: `8`.

No float-to-integer grid-stride conversion remains in `applyPSConfig()`.

## 11. Build commands and results

```text
MSBuild VitruGen_ver004.sln /t:Build /p:Configuration=Debug /p:Platform=x64
MSBuild VitruGen_ver004.sln /t:Build /p:Configuration=Release /p:Platform=x64
```

Results:

- Debug x64: PASS, 0 warnings, 0 errors
- Release x64: PASS, 1 existing `LNK4098` warning, 0 errors

`git diff --check`: PASS

## 12. Manual tests performed

No visual mode-by-mode result is claimed in this report.

The project’s legacy OpenGL window cannot be captured reliably by the
available desktop automation interface. FULL, MINIMAL, NONE, and DYNAMIC must
therefore receive hands-on visual acceptance in the running application.

## 13. Regression tests performed

Static source audit confirmed Sprint 005 introduced no changes to:

- `particleSystem.h/.cpp`;
- CUDA kernel files;
- `TextEntry.h/.cpp`;
- Camera files;
- Marching Cubes files;
- volume-rendering files.

The Sprint 005 build compiled all affected renderer, Arbiter, Tesseract,
ViewPort, and engine translation units successfully.

SINGLE_PARTICLE_MCAD continues calling both its original boundary and internal
major-grid geometry, now through the two separated helpers.

Hands-on SINGLE_PARTICLE and physics-invariance checks remain part of runtime
acceptance.

## 14. Known limitations

- DYNAMIC is presentation-only and intentionally falls back to FULL.
- Layout selection is not serialized across application restarts.
- Visual acceptance depends on a user-run OpenGL session.
- The existing Release `LIBCMT` warning remains outside this sprint.

## 15. Work intentionally deferred

- Collision-hash visualization
- Occupied-cell highlighting
- Neighbor-cell overlays
- CUDA hash-buffer readback
- PARTICLE_SIM sub-layers

## Hands-on acceptance checklist

- FULL: boundary, major, minor, axes
- MINIMAL: boundary, major, axes; no minor grid
- NONE: exactly twelve boundary edges plus axes and particles
- DYNAMIC: FULL fallback plus reserved status
- Immediate A/D preview through all four values
- NONE persistence through Layers 1 → 2 → 3 → 2 → 1
- Physics invariance across all layouts
- SINGLE_PARTICLE_MCAD grid regression
- Zero active particle grid rendering
- Full-capacity rendering
