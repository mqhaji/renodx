# AGENTS: MGSV contributions

Scope: `src/games/mgsv/**`. This file adds MGSV-specific rules to the [root guidance](../../../AGENTS.md) and [game-mod guidance](../AGENTS.md). General C++ style, helper design, dependencies, and change-scope rules are inherited, not repeated here. See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for file ownership and known limitations, and the [reconstruction README](reconstruction/README.md) for temporal contracts and verification.

## Local conventions

- Use plain American English in comments, documentation, UI, logs, and proposed commit messages: `color`, `behavior`, `initialize`, and `organization`. Preserve external names, saved keys, quotations, and attribution.
- Keep each Markdown paragraph and list item on one source line. Do not introduce MGSV-specific formatter or lint overrides.
- Keep logs limited to useful changes, failures, and recovery; avoid per-frame success messages and repeated warning spam.
- Keep shaders in the existing purpose-based folders (`copy/`, `motion/`, `subsurface/`, `lighting/`, `output/`), not a catch-all `game/shaders/` tree. Preserve existing shader basenames and embed identities when moving files.
- Feature headers must not depend on being included inside `addon.cpp`'s anonymous namespace or on its hidden globals. Preserve contributor copyright and research attribution when combining files.

## Settings and defaults

- Preserve established grading defaults rather than substituting personal tuning. The approved shadow, AO, and indirect-lighting enhancements intentionally default to On.
- Preset Off disables all eight enhancements: directional/spot/point contact hardening, their three screen-space shadow controls, AO, and indirect lighting. Reconstruction selection remains independent and sticky. Reset All restores defaults; Vanilla/0 tonemapping is not full Off.
- Preserve saved keys, UI order, bindings, and reset behavior unless explicitly changing them. Key renames require deliberate handling of existing saved settings.
- Use the single `ShaderInjectData` layout in `shared.h` with effect-local copies for scale, history flags, and filter steps. Never temporarily overwrite shared user settings or add a second settings/control buffer. Copying is not synchronization.
- Bind feature controls to supplied stable storage; addon composition assembles them and core preset/reset code handles cross-feature policy. Do not add another global settings registry.

## Rendering safety

- Verify each pass's actual input and output domains before changing color or lighting math. MGSV encodes scene color early in parts of its pipeline; a pre-tonemap pass or floating-point texture is not proof of linear light. Trace producer writes, sampling/view conversions, and downstream reads, and document the transfer function and any exposure or packing. Do not assume every buffer uses the same encoding.
- Perform light addition, radiance averaging, luminance/moment calculations, and physically based attenuation in linear light. Decode encoded color before those operations and encode back only where the next game pass requires it. Check GI gather, accumulation, denoising, and final addition separately; shadow/AO visibility factors and depth/normal/motion data are not color and must not receive blanket gamma conversions. Preserve existing alpha and packing conventions, and avoid double decoding through sRGB views.
- Treat the current AO/GI color-space behavior as requiring verification, not as proven correct or incorrect. Do not hide encoding changes inside cleanup or apply a guessed `pow(2.2)` fix; establish the actual transfer function and validate the affected lighting separately before changing it.
- Precision upgrades are mod-wide, not owned by HDR output or anti-aliasing. Keep identified velocity upgrades independent of reconstruction/lighting switches, and shared native inputs outside reconstruction's private capture state.
- Preserve original second scatter exactly once -> eligible GI -> AO -> state restoration. Missing GI inputs must not suppress original game work or valid AO. Use the existing chain-wide state preservation, including originally empty and displaced graphics bindings.
- Keep fallback inputs separate from distinct fallback outputs: unused slots still cause read/write conflicts. Check AO depth-mip binding order and GI input/output selection with one, two, and three denoise iterations.
- Keep scatter inputs callback-local, not in a global pre/post frame. Retain cached native views from known-live bindings or protected clone records and follow selected-device/generation retirement. Retention preserves objects, not pixels; CPU recording is not GPU completion.
- Preserve original deferred command lists when canceling reconstruction. Keep new reconstruction-Off motion separate from GI's compatibility selection until producer contents, execution order, and safe history use are proven; do not substitute dummy, previous-frame, or camera-only motion.
- Keep history changes separate from ownership cleanup. Cold-history, odd-dimension, and upper-edge defects need separately approved fixes with evidence; tests that reproduce those defects do not prove correctness.
- Preserve early-registration-wins shader behavior. `on_inject = false` disables constant injection, not replacement bytecode.
- Texture resizing must not rebuild unchanged lighting pipelines. Shared FSR/DLSS preparation code must preserve their distinct resources and lifetimes.

## Local workflow

- Never build while `mgsvtpp.exe` is running; keep the agreed toolchain.
- Tests are local-only: never stage or commit `test/mgsv_rendering`, temporary registration, or test artifacts. Remove task-added test wiring and verify the standalone addon build; leave unrelated local work untouched.
- For structural changes, compare compiled shaders, effective registrations, settings, and shared layouts. Report fixture results separately from actual-game evidence; use the linked verification guides rather than treating a build as visual validation.