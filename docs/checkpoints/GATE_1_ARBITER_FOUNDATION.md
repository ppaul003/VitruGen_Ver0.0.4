# Gate 1 — TheArbiter Navigation Foundation

Date: 2026-07-15

Project: VitruGen SIMCAD ver0.0.4

Decision: **PASS**

Baseline tag: `gate-0`

Development branch: `codex/gate-1-arbiter-foundation`

## Gate purpose

Reconstitute the top-level navigation portion of `TheArbiter` around the
VitruGen hierarchy while preserving the Gate 0 workflows:

```text
Layer 0 — Global shell
Layer 1 — Workspace domain
Layer 2 — Workspace selection/configuration
Layer 3 — Active workspace
```

This gate establishes the state model and compatibility foundation. The
visible three-domain navigation rollout remains outside Gate 1.

## Checkpoint evidence

| Checkpoint | Commit | Result | Evidence |
|---|---|---|---|
| 1A — Canonical hierarchy and catalog | `95cbab9` | Pass | Strong layer/domain/workspace/availability types and a complete workspace catalog |
| 1B — Authoritative navigation state | `b7ccfd3` | Pass | One `NavigationState`, derived compatibility adapters, Debug invariants, and navigation regression tests |
| 1C — Explicit layer input dispatch | `792ae24` | Pass | Top-level keyboard routing split into layer-specific handlers without changing commands or active-workspace behavior |
| 1D — Consumer compatibility cleanup | Included in 1B–1C | Pass with narrow adapter retained | No parallel legacy storage; `EuclidEngine` retains derived compatibility snapshots |
| 1E — Gate verification and freeze | Gate acceptance commit | Pass | Debug/Release builds, navigation tests, and user-observed runtime regressions passed |

## Architecture outcomes

- `ApplicationLayer` distinguishes global shell, domain selection, workspace
  configuration, and active workspace.
- `WorkspaceDomain` distinguishes `GRID_2D`, `GRID_3D`, and `SIMCAD_4D`.
- `WorkspaceId` contains the canonical workspace identities, including
  `SINGLE_PARTICLE_MCAD`, `LINKED_PARTICLES_MCAD`, and
  `PARTICLE_SIMULATION`.
- Every workspace ID has exactly one descriptor containing its domain,
  availability, and stable name.
- `NavigationState` is the only mutable top-level navigation state.
- Per-domain workspace selections are preserved independently.
- Legacy `AppLayer`, `EnvironmentSelection`, and `GridSelection` results are
  computed from `NavigationState`; they are not mirrored mutable state.
- Debug invariants reject invalid domain/workspace combinations.
- The global-shell, domain-selection, and workspace-configuration input paths
  have dedicated handlers.
- CUDA, OpenGL, rendering, camera, Tesseract resource ownership, and CAD
  authoring state were not moved by this gate.

## Compatibility mappings verified

```text
GRID_GRAPH_3D        -> GRID_3D / GRAPH_3D
GRID_SINGLE_PARTICLE -> GRID_3D / SINGLE_PARTICLE_MCAD
GRID_PARTICLES_3D    -> SIMCAD_4D / PARTICLE_SIMULATION
```

`EuclidEngine.cpp` currently contains legacy non-UTF-8 source bytes. Its small
set of top-level `AppLayer` and `GridSelection` snapshots remains behind the
derived compatibility API so Gate 1 does not force an unrelated whole-file
encoding conversion. This does not create a second state model.

## Automated verification

### Debug x64

- Result: **PASS WITH GATE 0 WARNINGS**
- Full CUDA/C++ rebuild completed and linked
  `VitruGen_ver004/x64/Debug/VitruGen_ver004.exe`.

### Release x64

- Result: **PASS WITH GATE 0 WARNINGS**
- Full CUDA/C++ rebuild completed and linked
  `VitruGen_ver004/x64/Release/VitruGen_ver004.exe`.

### Navigation regression suite

- Source: `tests/TheArbiterNavigationTests.cpp`
- Result: **PASS**
- Verified catalog coverage and domain membership.
- Verified the legacy Single Particle entry route.
- Verified the legacy particle-simulation entry route.
- Verified independent per-domain selection memory.
- Verified again after the Checkpoint 1C handler extraction.

### Warning comparison

No new warning category was observed. The builds retain the Gate 0 deferrals:

- CUDA/Thrust/CUB C++14 deprecation.
- Deprecated pre-`sm_75` offline target support.
- Deprecated `cudaGLSetGLDevice` calls in CUDA sample helpers.
- `LNK4098` default-library conflict involving `LIBCMT`.
- Missing `pwsh.exe` before the successful Windows PowerShell app-local
  fallback.

## Focused change boundary

Compared with `gate-0`, the Gate 1 branch changes only:

- `VitruGen_ver004/VitruGen_ver004/TheArbiter.h`
- `VitruGen_ver004/VitruGen_ver004/TheArbiter.cpp`
- `tests/TheArbiterNavigationTests.cpp`
- Gate 1 checkpoint documentation

No CUDA kernel, renderer, particle-system, Marching Cubes, camera, Tesseract,
or OBJ-export implementation was changed.

## Interactive acceptance checklist

User-observed runtime regression completed on 2026-07-15 using the Gate 1
branch:

- [x] Application starts and the main grid/interface renders normally.
- [x] Top-level entry and back-navigation behave as they did at Gate 0.
- [x] `SINGLE_PARTICLE_MCAD` configuration opens normally.
- [x] The Single Particle sub-layer cycle remains intact.
- [x] Guarded `Q` exit works from Single Particle sub-layer 0.
- [x] Marching Cubes mesh generation completes.
- [x] OBJ export completes and the resulting model is visually plausible.
- [x] Particle simulation starts and animates normally.
- [x] Re-entering both existing workflows does not crash or reset
  unexpectedly.
- [x] No new CUDA, rendering, console, or UI anomaly is observed.

## Gate acceptance result

Gate 1 satisfies its architecture, build, automated-test, and interactive
runtime requirements. The accepted source is published from
`codex/gate-1-arbiter-foundation` to `main` and identified by the annotated
`gate-1` tag.

If a runtime regression is found, remain on the last passing Gate 1 commit and
use `gate-0` as the immutable rollback boundary.
