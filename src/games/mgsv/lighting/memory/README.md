# Scene memory

Scene memory is a world-space record of the surfaces the camera has seen: a narrow-band distance field, the surfaces' colors and whether the sun or moon reaches them, in four nested grids that follow the camera. It is step 2 of [stage 2](../../ROADMAP.md#stage-2-raster-lighting); the [design](../../docs/DESIGN.md#42-scene-memory) explains why it exists. Nothing reads it yet except its debug views. The probe grid (step 3) and the reflection chain (step 4) will.

## Where it runs

Scene memory runs inside the reflection round trip (`../reflections/runtime.hpp`), so only where enhanced reflections run: the presented main view, with Global Illumination Enhanced and the RenoDX tone mapper. It moves into the main D3D12 block when step 4 creates one. Until something reads it, the upkeep runs only while one of its debug views is selected or the local probe asks for it.

Each update:

1. On D3D11, right after reflection preparation and with its inputs still bound, `sample_memory` turns one pixel per tile of a 128 × 64 grid over the render into a world-space sample: position relative to the camera, color, whether the engine moved the model this frame (the object-velocity mask), and sun visibility from the cascade atlas. Where the game draws terrain, `memory_window` then copies the heights around the camera out of the engine's terrain textures.
2. On D3D12, after the reflection resolve: `memory_state` counts the samples, keeps the light direction and its step count, records the camera, places the grids and decides whether this update marks; `memory_scroll` forgets the cells that wrapped into a moved grid; `memory_terrain` fills the ground; `memory_carve` empties cells that camera rays see through; `memory_mark` stores the samples; for a debug view, `memory_inspect` draws it. The state is copied back to the CPU for the probe.

## What it holds

| Data | Layout | Content |
|---|---|---|
| Distance field | 128 × 64 × 512 R32_UINT: four levels of 128 × 64 × 128 cells (x, height, z) of 0.25, 0.5, 1 and 2 units, stacked along z | Unknown, known empty, solid (below the terrain), or near a surface with the distance from the cell's center to the nearest sample or the terrain. The values are ordered so marking keeps the nearest with an atomic minimum |
| Color | 64 × 32 × 256 R32_UINT, half the resolution, same grid placement | Square root of linear RGB, blended over the samples, with a sample count |
| Sun visibility | 64 × 32 × 256 R32_UINT, as the color | Visibility from 0 to 255 and the light step count it was taken at |
| State | 64 × 1 R32_UINT | Sample counts, light direction and step count, camera, grid origins, census and diagnostics; layout in `common.hlsli` |

Each grid's window starts at its origin and wraps, so a world cell always lives at the same texel while it is in the window. The windows follow the camera in steps of two cells once it is more than two cells off center; the GPU places them from the frame's own camera, so no read-back value can misplace them.

## Rules

- **Moving objects are never stored.** A sample the object-velocity mask marks is skipped (the mask equals stencil bit 0 of the G-buffer depth), and nothing is stored on an update without the mask. The mask covers characters, the player and vehicles every frame, doors and physics props while they move, and the trees nearest the player; other vegetation sways unmasked.
- **Nothing is stored while game time is stopped** (`../../engine/pause.hpp`: the pause menu, game over, a paused cutscene, the game in the background; not iDroid browsing). The frame drawn can lag the game frame the pause word describes, so marking resumes four updates after a pause ends. If the pause entries do not verify, an update with no moving samples stores nothing instead.
- **Carving runs every update, paused or not.** A cell is emptied when its bounding sphere lies wholly in front of the nearest depth over its screen footprint, capped at 16 pixels around the cell's center and clipped to the screen. Opened doors and moved props leave memory this way. Cells right around the camera cannot be judged and stay unknown.
- **Sun visibility comes from samples that face the light inside a cascade.** Surfaces facing away store none: the engine draws shadow casters with front faces culled, so such a surface compares against its own depth and reads as lit. A stored value keeps the light step count it was taken at and is never dropped for its age. In play the sun turns about 0.06° every 0.7 s, and the switch between sun and moon flips the direction in one frame (over 100°) while the engine fades the light through it; both just age the stored values. A sample replaces a value older than 4 half-degree steps at the finest level (doubled per level) and blends with a newer one. How far to trust an old value is for the readers to decide.
- **Terrain comes from the engine's GPU textures.** Every terrain draw binds a tile map and a 16-bit height atlas, decoded as the terrain vertex shaders do; `CaptureTerrain` keeps them from the terrain depth and shadow draws and copies their constants on the GPU. Only full-resolution pages are used (2 units apart, about 640 units around the player). A cell within the band of the ground gets its distance to it; a cell below the band becomes solid if nothing was known there. Air above the ground is left alone, since things stand on it. Where no terrain is drawn (Mother Base, interior stages) nothing is kept and the fill is off.
- **Clears:** an update more than a second after the previous one (a load, a menu without the scene, upkeep switched off) starts from empty grids. Camera cuts and teleports do not clear: memory is in world space, and a jump to another place scrolls every grid.

## Debug views

Lighting Debug View under Indirect Lighting (Advanced). Each ray is marched from one unit in front of the camera through the finest grid that holds it, across known-empty cells, until it meets a stored surface or unknown space or leaves the grids. The views replace the reflection result on every pixel, sky included; water, glass and particles are drawn after it and appear on top.

| View | Shows |
|---|---|
| Scene Memory | Stored surfaces in their stored color, shaded by the distance field's normal; surfaces without a color yet (terrain not looked at) are tinted by level, white to pink from finest to coarsest. Magenta is unknown space, dark blue known-empty space to the grids' edge |
| Scene Memory Behind Camera | The same with the camera turned around |
| Scene Memory Agreement | Memory against the surface each pixel shows, with gaps measured along that surface's normal. Green: matches; red: memory holds a surface in front of it; blue: memory holds that place as empty; magenta: unknown space first; dark green: empty to the grids' edge |
| Scene Memory Sun Visibility | Stored visibility from black to white; red where none is stored, orange where it is old |

Expected red in the agreement view: object outlines, swaying grass, power lines and distant ruins, where detail is finer than a cell.

## Probe

The local lighting probe (untracked) turns the upkeep on with `control.ps1 -Action SceneMemory -Value 1` and reports it under `scene_memory`: updates and read-back lag, the pause word, sample counts, light direction and step count, camera and its forward direction, exposure, grid origins, stored and known cells per level, carving and terrain counts, and clears. The known-cell census runs only while the probe enables the upkeep. GPU time comes from the probe's `D3D12Timing` action, one span per pass.

## Known limits

- Air the camera has not looked through stays unknown: above the player's head, behind the car while driving, and the sky far away. Carving reaches the coarse grids' sky slowly.
- Thin and see-through things (fences, cables, gaps in ruins) are stored as solid as wide as a cell.
- Masked trees near the player are never stored; unmasked swaying vegetation is stored and rewritten.
- Coarse terrain pages beyond about 640 units are not used.
- A load once froze with scene memory on; the cause was not found, and the stacks taken then showed no thread in the mod.

## Verification

Build the `mgsv` and `mgsv_lighting_probe` targets and check the embedded shader bytes against the compiled `.cso` files. At the title screen, with the probe's upkeep on, the status should show four grid origins centered on the camera, marked samples, a read-back lag of one or two updates and verified engine entries. In game, outdoors in daylight: Scene Memory keeps seen surfaces in place while walking and driving; characters never appear; a pause next to a soldier leaves nothing behind; opened doors clear once seen through; Agreement is mostly green; Sun Visibility follows the shadows on screen and turns orange under a cigar; in Afghanistan or Africa the ground is stored behind and under the player at once.
