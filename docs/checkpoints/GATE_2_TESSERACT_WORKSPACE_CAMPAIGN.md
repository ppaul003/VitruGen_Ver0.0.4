# Gate 2 Development Campaign — Tesseract Workspace Containers

Baseline tag: `gate-1`

Development branch: `codex/gate-2-tesseract-workspace-containers`

Gate decision target: behavior-preserving workspace-runtime separation

## Objective

Reconstitute `TheTesseract` as the persistent runtime container for VitruGen
workspaces while preserving the accepted Gate 1 navigation and Gate 0 runtime
behavior.

The hierarchy established by `TheArbiter` remains authoritative:

```text
GRID_3D
  ├─ SINGLE_PARTICLE_MCAD (SP)
  └─ LINKED_PARTICLES_MCAD (LP) — RESERVED placeholder

SIMCAD_4D
  └─ PARTICLE_SIMULATION (PS) — existing CUDA baseline
```

`LINED_PARTICLES_MCAD` is treated as a spelling error. The canonical name is
`LINKED_PARTICLES_MCAD`.

## Responsibility boundary

`TheArbiter` owns:

- The selected domain and workspace identity.
- Navigation state and valid user transitions.
- Workspace availability (`AVAILABLE`, `EXPERIMENTAL`, or `RESERVED`).
- Commands describing requested effects.

`TheTesseract` owns:

- Persistent per-workspace runtime instances.
- Workspace initialization, entry, exit, update, and render dispatch.
- Workspace logical state as it is migrated from `EuclidEngine`.
- Non-owning bindings to shared runtime services during Gate 2.

`EuclidEngine` continues to own:

- Creation and destruction of shared `ParticleSystem` and renderer services.
- Construction of per-frame update and render contexts.
- Execution of cross-subsystem effects produced by `TheArbiter`.

Inactive does not mean destroyed. Leaving a workspace must not implicitly reset
its authored or simulation state.

## Gate 2 invariants

1. `TheArbiter::WorkspaceId` is the only workspace-identity type.
2. `PARTICLE_SIMULATION` belongs to `SIMCAD_4D`.
3. `SINGLE_PARTICLE_MCAD` and `LINKED_PARTICLES_MCAD` belong to `GRID_3D`.
4. The original CUDA particle behavior remains the PS baseline.
5. LP remains `RESERVED` and cannot be entered through normal navigation.
6. PS and SP may share engine-owned particle/render resources; a workspace
   instance must not claim exclusive ownership of them.
7. Workspace exit stops dispatch but preserves initialized state and resources.
8. CUDA kernels, particle collision mathematics, Marching Cubes, and OBJ output
   are unchanged unless a separately accepted checkpoint requires a change.

## Audit result at campaign start

| Area | Finding | Gate 2 disposition |
|---|---|---|
| Workspace identity | `TheTesseract` duplicated the canonical Arbiter IDs with `WorkspaceBranch` | Remove the duplicate and consume `TheArbiter::WorkspaceId` |
| PS dispatch | PS update/render algorithms were already isolated as functions, but selected through inline switch logic | Add PS lifecycle wrappers without changing the algorithms |
| PS state | Pause, simulation time, solver settings, and boundary settings are still assembled in `EuclidEngine` | Migrate deliberately in Checkpoint 2C |
| Shared services | `ParticleSystem`, `EuclidRenderer`, and the radius array are consumed by both PS and SP | Keep non-owning shared bindings; do not move them into a PS-exclusive owner |
| SP state | CAD placement, volume, boundary-sensor, and interop state are loose Tesseract members | Group them incrementally in Checkpoint 2E |
| LP runtime | No LP initializer, state, update, render, or resource contract existed | Seed an inert reserved instance and lifecycle insertion point |
| Legacy vocabulary | `PARTICLES_3D` remains in Gate 1 compatibility adapters and the accepted UI route | Retain until the visible domain/workspace migration checkpoint |
| Source encoding | `EuclidEngine.cpp` contained twelve Windows-1252 em-dash bytes in comments | Normalize only those comment bytes to UTF-8 so normal patches can edit the file safely |

## Checkpoint 2A — Audit and ownership freeze

Deliverables:

- Trace workspace selection from `TheArbiter` through `EuclidEngine` into
  `TheTesseract`.
- Inventory PS configuration/session state and shared runtime resources.
- Record the PS/SP shared-resource dependency.
- Confirm that LP has no existing runtime implementation to preserve.

Exit requirements:

- Every affected state field has a current owner and proposed Gate 2 owner.
- No code changes are made before the behavior-preserving seam is identified.

Current result: **COMPLETE**.

## Checkpoint 2B — Canonical identity and lifecycle scaffold

Deliverables:

- Remove `TheTesseract::WorkspaceBranch` and use
  `TheArbiter::WorkspaceId` directly.
- Add a small `PSWorkspaceInstance` with identity, initialization status, and
  shared-resource binding status.
- Route PS entry/update/render through `initializePSWorkspace`,
  `updatePSWorkspace`, and `renderPSWorkspace`.
- Add an inert `LPWorkspaceInstance` and LP initializer/update/render methods.
- Rename the shared binding method so it does not imply PS-exclusive ownership.

Exit requirements:

- Existing PS update and render functions remain behaviorally unchanged.
- LP methods allocate no CUDA/OpenGL resources and render nothing.
- No duplicate Tesseract workspace enum remains.
- Debug x64 and Release x64 compile and link.

Current result: **IMPLEMENTED; BUILD VERIFICATION PENDING**.

## Checkpoint 2C — PS session-state container

Pair-programming assignment: architectural code typed with the user.

Deliverables:

- Define an explicit PS session/configuration state inside the PS instance.
- Migrate only PS-owned pause, time, iteration, damping, gravity, attraction,
  and boundary settings from engine-wide storage.
- Distinguish persistent configuration from transient frame context.
- Preserve the original CUDA `ParticleSystem` allocation and solver calls.
- Add explicit `resetPSWorkspace` behavior; entry and exit must not reset it.

Exit requirements:

- PS resumes with its prior configuration after an exit/re-entry cycle.
- SP pause/configuration behavior remains unchanged.
- `EuclidEngine` no longer acts as the authoritative owner of PS session state.

## Checkpoint 2D — Shared service and lifecycle contract

Deliverables:

- Represent particle/render bindings as an explicitly non-owning service view or
  equivalent narrow contract.
- Make initialization failure observable without crashing.
- Define separate initialize, enter, exit, reset, and shutdown semantics.
- Keep service creation/destruction in `EuclidEngine` for Gate 2.
- Ensure SP and PS can safely rebind the common renderer when switching.

Exit requirements:

- No workspace deletes an engine-owned service.
- Repeated PS/SP entry does not leak, reset, or retain a wrong render binding.
- Exit disables dispatch without destroying the workspace instance.

## Checkpoint 2E — SP boundary and LP insertion point

Deliverables:

- Group the existing loose SP logical state behind an SP instance boundary in
  small, reviewed blocks; do not rewrite the CAD workflow wholesale.
- Preserve volume buffers and CUDA/OpenGL interop ownership until their
  lifecycle is explicitly understood.
- Finalize LP placeholder metadata and lifecycle contract.
- Keep LP availability `RESERVED`; do not add authoring or simulation behavior.

Exit requirements:

- Tesseract dispatch has distinct SP, PS, and LP branches.
- Existing SP placement, volume editing, Marching Cubes, and OBJ export pass.
- LP initialization is safe and inert when invoked directly by a test or debug
  harness.

## Checkpoint 2F — Verification and freeze

Automated requirements:

- Clean Debug x64 rebuild.
- Clean Release x64 rebuild.
- Gate 1 navigation regression suite passes.
- `git diff --check` passes.
- No new compiler or linker warning category is introduced.

Interactive runtime requirements:

- Application starts and renders the global grid/interface.
- PS starts, animates, pauses/continues where supported, exits, and re-enters.
- PS keeps its configuration/session state across a non-reset exit/re-entry.
- SP configuration and the complete guarded sub-layer cycle remain operational.
- SP Marching Cubes generation and OBJ export remain operational.
- Switching between existing SP and PS workflows does not crash or display the
  previous workspace through a stale renderer binding.
- LP remains unavailable in normal navigation and produces no runtime side
  effect.

Required evidence:

- This development campaign.
- A final `GATE_2_TESSERACT_WORKSPACE_CONTAINERS.md` acceptance record.
- Focused commits for each accepted checkpoint.
- An annotated `gate-2` tag only after build and user runtime acceptance.

## Explicitly outside Gate 2

- Actual `LINKED_PARTICLES_MCAD` construction, linking, joints, or rendering.
- A second copy of the CUDA particle simulation for LP.
- New `GRID_2D`, `GRAPH_3D`, or SIMCAD simulation modes.
- Visible three-domain menu rollout.
- New particle solvers, collision algorithms, or CUDA kernels.
- Renderer, camera, Marching Cubes, volume-field, or OBJ redesign.
- GameEngine_SIM, robotics, or neural-billboard implementation.

## Rollback rule

If a checkpoint changes accepted PS or SP behavior, stop at the last passing
Gate 2 checkpoint. The immutable architectural rollback boundary is `gate-1`.
