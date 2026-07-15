# Gate 0 — Verified Pre-Arbiter Baseline

Date: 2026-07-15

Project: VitruGen SIMCAD ver0.0.4

Decision: **PASS WITH DOCUMENTED DEFERRALS**

Checkpoint tag: `gate-0`

## Gate purpose

Establish a recoverable, buildable, and behaviorally verified ver0.0.4
baseline before the Gate 1 `TheArbiter` architecture campaign begins.

## Repository and rollback evidence

- Repository branch: `main`
- Initial pre-Arbiter baseline: `47d5b3a`
- Remote repository: `https://github.com/ppaul003/VitruGen_Ver0.0.4`
- Gate 0 checkpoint is identified by the annotated `gate-0` tag.
- Visual Studio `.vs` state and `x64` build output are excluded by
  `.gitignore`.

## Build verification

### Debug x64

- Result: **PASS WITH WARNINGS**
- A clean CUDA/C++ rebuild compiled and linked successfully.
- Verified executable:
  `VitruGen_ver004/x64/Debug/VitruGen_ver004.exe`
- Verified executable size: 6,901,760 bytes.

### Release x64

- Result: **PASS WITH WARNINGS**
- Release initially failed to resolve `GL/freeglut.h`.
- The Release `AdditionalIncludeDirectories` setting was aligned with the
  working Debug configuration:
  `C:\Users\richa\cuda-samples-master\Common`.
- Rebuild then compiled and linked successfully.
- Verified executable:
  `VitruGen_ver004/x64/Release/VitruGen_ver004.exe`
- Verified executable size: 1,204,736 bytes.

### Deferred build warnings

The following warnings did not prevent executable production and are deferred
as technical debt:

- CUDA/Thrust/CUB C++14 deprecation; C++17 will be required later.
- Deprecated pre-`sm_75` offline target support.
- Deprecated `cudaGLSetGLDevice` calls in CUDA sample helpers.
- `LNK4098` default-library conflict involving `LIBCMT`.
- Missing `pwsh.exe` during the app-local dependency staging hook.

## Runtime dependency verification

- `freeglut.dll` and `glew64.dll` were manually placed beside the Release
  executable.
- Verified sizes:
  - `freeglut.dll`: 311,296 bytes
  - `glew64.dll`: 229,376 bytes
- Result: **PASS FOR THE VERIFIED RELEASE ARTIFACT**
- Automatic DLL deployment remains deferred.

## Runtime regression verification

User-observed Release x64 test results on 2026-07-15:

| Test | Result |
|---|---|
| Application starts without a missing-DLL failure | Pass |
| Main VitruGen interface and grid render correctly | Pass |
| `SINGLE_PARTICLE_MCAD` workflow opens and behaves normally | Pass |
| Particle simulation starts and animates without crashing | Pass |
| Missing graphics, unusual behavior, or console errors observed | No |

## OBJ export and visual verification

- Representative export inspected:
  `C:\Users\richa\3D Objects\chest_attempt_0.obj`
- Export header and parsed geometry agree:
  - 245,952 vertex records
  - 81,984 triangles
- Structural inspection:
  - 0 degenerate triangles
  - 0 non-manifold edges after coordinate welding at `1e-5`
  - 44 boundary edges after coordinate welding at `1e-5`
- Front, side, top, and isometric projections rendered successfully.
- Result: **PASS FOR EXTERNAL VISUAL EXPORT**
- The small boundary-edge count is retained as future mesh-quality work and
  does not block the verified visual/export workflow.

## Preserved ver0.0.3 baseline material

| Artifact | Status | Evidence |
|---|---|---|
| ver0.0.3 source | Partial | Present at `C:\Users\richa\source\repos\VitruGen_ver003`; no immutable archive yet |
| ver0.0.3 Debug executable | Pass | Preserved executable dated 2026-07-14 |
| Representative ver0.0.3 OBJ | Pass | `p0.obj`: 245,952 vertices and 81,984 faces |

## Explicitly deferred evidence and technical debt

The user accepted Gate 0 for progression with these items still open:

- Create an immutable ver0.0.3 source archive and checksum.
- Capture formal application screenshots and an FPS observation.
- Automate runtime-DLL deployment instead of copying DLLs manually.
- Resolve or intentionally suppress the documented compiler/linker warnings.
- Replace the user-specific CUDA Samples include path with a portable
  dependency configuration in a later build-system checkpoint.

## Gate 1 entry condition

Gate 1 may restructure `TheArbiter`, but it must preserve the behavior verified
above. Each Gate 1 checkpoint must at minimum rebuild Debug x64 and Release x64
and repeat the two runtime workflow regressions before it is accepted.
