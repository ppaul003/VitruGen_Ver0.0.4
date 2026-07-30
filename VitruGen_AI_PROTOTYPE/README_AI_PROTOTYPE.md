# VitruGen AI Prototype

This folder is the isolated rapid-prototyping copy of `VitruGen_ver004`. The original sibling project is not used by the prototype build.

## Build and test

- Open `VitruGen_ver004.sln` and build `Debug | x64`.
- The executable is written to `x64/Debug/VitruGen_ver004.exe`.
- Run `scripts/run_mvp_pipeline_tests.bat` for the portable asset-pipeline and navigation regression suites.

## MVP workflow

1. In `SINGLE_PARTICLE_MCAD`, create a volume, run Marching Cubes, and use **SAVE STATIC**. Sequential parts receive the useful prototype names Hull, Turret, Barrel, Left Track, Right Track, then Detail N.
2. Use **OPEN TEXTURE**. Paint by dragging the left mouse button; select brush, eraser, fill, color, size, clear, image import, save, or UV overlay with `W/S`, adjust with `A/D`, and activate with `E`.
3. Image import looks for `VITRUGEN_PROJECT_DATA/import.ppm`, then `import.bmp`. PPM P6 and uncompressed 24/32-bit BMP are supported.
4. Apply the texture to return to the textured 3D static-particle preview.
5. Open `LINKED_PARTICLES_MCAD`. Create an assembly from saved parts, or add red mesh nodes, blue fixed/revolute joint nodes, and green interaction nodes.
6. The lower linked-editor actions select a node and parent and edit local XYZ position/rotation, joint axis, and revolute limits. Parent changes that create a cycle are rejected.
7. Add a named joint animation, then **VALIDATE / BAKE**. Invalid references, multiple/no roots, cycles, bad materials/textures, invalid joints, and bad animation tracks block the bake with a status message.
8. Save or reopen `VITRUGEN_PROJECT_DATA/VitruGen_MVP.vitru`, then open `SANDBOX_SIM`.

## Sandbox controls

- `W/S`: drive or reverse
- `A/D`: turn
- `E` or `F`: fire from a Weapon Muzzle interaction node
- `Space`: play a named joint animation
- `Z/X`: directly decrease/increase the first revolute joint
- `V`: toggle orbit-compatible follow camera
- `Enter`: spawn or respawn the latest baked kinematic asset (or a saved static asset)
- `R`: reset the scene, targets, hit count, and spawned asset
- Mouse drag/wheel: orbit and zoom

The sandbox includes a ground plane, obstacles, targets, projectiles, collision/hit feedback, and resettable runtime state. Editable assemblies and baked runtime particles remain separate assets in the project file.

## Prototype storage

Generated MVP data stays under `VitruGen_AI_PROTOTYPE/VITRUGEN_PROJECT_DATA` when the executable is launched with this folder as its working directory. The main project file uses a versioned binary format and rejects invalid or corrupt input instead of partially loading it.
