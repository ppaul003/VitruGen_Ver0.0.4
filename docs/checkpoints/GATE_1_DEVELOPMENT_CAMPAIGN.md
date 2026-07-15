# Gate 1 Development Campaign — TheArbiter Navigation Foundation

Baseline tag: `gate-0`

Development branch: `codex/gate-1-arbiter-foundation`

Gate decision target: behavior-preserving architectural migration

## Objective

Reconstitute the top-level navigation portion of `TheArbiter` so it directly
models the VitruGen hierarchy:

```text
Layer 0 — Global shell
Layer 1 — Workspace domain
Layer 2 — Workspace selection/configuration
Layer 3 — Active workspace
```

The authoritative state must distinguish application layer, workspace domain,
and workspace identity. Gate 1 must preserve the workflows accepted at Gate 0.

## Vocabulary freeze

- `GRID_2D`, `GRID_3D`, and `SIMCAD_4D` are workspace domains.
- `SINGLE_PARTICLE_MCAD` and `LINK_PARTICLES_MCAD` use MCAD to mean
  Mesh-CAD.
- `GRAPH_3D` remains the architectural workspace ID. Presentation code may
  later display a more descriptive label such as `GRAPH_3D SURFACE`.
- The existing CUDA particle system maps to
  `SIMCAD_4D / PARTICLE_SIMULATION`; it does not map to
  `LINK_PARTICLES_MCAD`.

## Responsibility boundary

`TheArbiter` may:

- Interpret input intent.
- Own behavioral and navigation state.
- Enforce valid state transitions.
- Produce commands and transition results.
- Provide read-only state queries and presentation names.

`TheArbiter` must not:

- Allocate CUDA or OpenGL resources.
- Render UI or scene content.
- Execute `EuclidEngine` effects.
- Own particle buffers, volume textures, or generated meshes.
- Enter, exit, initialize, reset, or destroy Tesseract workspaces directly.

## Checkpoint 1A — Canonical hierarchy types and workspace catalog

Deliverables:

- Finalize strong types for application layer, workspace domain, workspace ID,
  and workspace availability.
- Add a `WorkspaceDescriptor` representation.
- Add one canonical catalog mapping every workspace ID to exactly one domain,
  availability state, and stable name.
- Add pure query functions for domain, availability, name, and domain
  membership.
- Do not change active navigation storage or visible behavior.

Exit requirements:

- Debug x64 rebuild succeeds.
- Release x64 rebuild succeeds.
- No new warnings are introduced by the checkpoint.
- Existing navigation and runtime behavior remain untouched.

## Checkpoint 1B — Authoritative NavigationState

Deliverables:

- Introduce one `NavigationState` container as the source of truth for the
  current layer, selected domain, and per-domain workspace selection.
- Replace mutable storage in `m_appLayer`, `m_envSelection`, and
  `m_gridSelection`.
- Retain temporary compatibility types and queries where existing consumers
  require them.
- Make compatibility results derive from `NavigationState`; do not mirror or
  synchronize two mutable state models.
- Add state-invariant validation for Debug builds.

Required compatibility mappings:

```text
GRID_GRAPH_3D        -> GRID_3D / GRAPH_3D
GRID_SINGLE_PARTICLE -> GRID_3D / SINGLE_PARTICLE_MCAD
GRID_PARTICLES_3D    -> SIMCAD_4D / PARTICLE_SIMULATION
```

Exit requirements:

- Debug x64 and Release x64 rebuild successfully.
- Existing key paths produce the same layer/workspace results as Gate 0.
- The new state has no invalid domain/workspace combinations.
- `SINGLE_PARTICLE_MCAD` and particle-simulation smoke tests pass.

## Checkpoint 1C — Explicit top-level transition handlers

Deliverables:

- Decompose the top-level portion of `processKeyboard` into layer-specific
  handlers.
- Centralize enter, back, selection, and reset transitions.
- Make valid transition rules readable without inspecting the complete CAD
  editing state machine.
- Preserve all Single Particle sub-layer and volume-edit behavior.
- Keep `ArbiterResult` as the effect boundary consumed by `EuclidEngine`.

Exit requirements:

- Debug x64 and Release x64 rebuild successfully.
- Layer entry, back-navigation, and reset paths match Gate 0 behavior.
- Single Particle's sub-layer loop and guarded `Q` exit remain intact.
- Particle simulation starts, pauses/steps where supported, and exits normally.

## Checkpoint 1D — Canonical consumer queries and compatibility cleanup

Deliverables:

- Move top-level `EuclidEngine` and `ViewPort` decisions to canonical domain,
  workspace, and layer queries where safe.
- Keep narrow compatibility adapters only where they materially reduce risk.
- Remove unused duplicate navigation state and obsolete mappings.
- Do not migrate CAD authoring state or Tesseract resource ownership.

Exit requirements:

- No production consumer depends on parallel legacy navigation storage.
- UI telemetry identifies the same active workflow as the canonical state.
- Debug x64 and Release x64 rebuild successfully.
- Gate 0 runtime workflows continue to pass.

## Checkpoint 1E — Gate verification and freeze

Required verification:

- Clean Debug x64 rebuild.
- Clean Release x64 rebuild.
- Application startup and main-grid rendering.
- `SINGLE_PARTICLE_MCAD` configuration and full sub-layer cycle.
- Marching Cubes mesh generation and OBJ export.
- Original particle simulation startup and animation.
- Back-navigation and repeated workflow entry without a crash.
- No new CUDA, rendering, or console failures.
- No warnings beyond the deferrals recorded at Gate 0 unless separately
  documented and accepted.

Required evidence:

- `docs/checkpoints/GATE_1_ARBITER_FOUNDATION.md`
- A focused diff limited to navigation architecture and necessary adapters.
- Passing commit on `codex/gate-1-arbiter-foundation`.
- Annotated `gate-1` tag after acceptance.

## Explicitly outside Gate 1

- The visible three-domain Layer 1 navigation rollout.
- Domain-specific Layer 2 menus.
- Reserved-workspace UI and entry rejection.
- New `GRAPH_2D`, `GRAPH_3D`, or `LINK_PARTICLES_MCAD` behavior.
- Persistent workspace-resource containers in `TheTesseract`.
- Migration of CAD/volume authoring state out of `TheArbiter`.
- Conversion of every CAD editing enum to `enum class`.
- CUDA solver, renderer, camera, Marching Cubes, or OBJ-format redesign.

## Rollback rule

If any checkpoint cannot preserve the Gate 0 workflows, stop at the last
passing Gate 1 commit. The immutable rollback boundary remains the `gate-0`
tag on `main`.
