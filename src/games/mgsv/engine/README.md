# Engine table: updating it for a new game executable

This guide is for whoever, person or AI, moves the mod to a new build of `mgsvtpp.exe`. Everything the mod relies on inside the executable is in [build.hpp](build.hpp); [image.hpp](image.hpp) checks it once at startup. Game updates do not change the renderer: its code, structures and shaders stay the same, and a rebuilt executable only moves code and data to new addresses. Updating the mod therefore means finding the same code at its new addresses and editing the table, then building the mod again.

## How to tell an update happened

The game log (`ReShade.log` beside the executable) shows:

```
MgsvEngine: this game executable (time stamp 0x..., size 0x...) is not the supported build; features that hook the engine stay off.
```

Every feature that hooks the engine is then off, and a red note at the top of the menu ("This version of the game is not supported by: ...") lists them. HDR output, tone mapping, grading and shader replacements keep working, but render targets stay 8-bit.

## What the table holds

- `IMAGE_TIMESTAMP` and `IMAGE_SIZE`: the supported build's identity, from its PE header (`FileHeader.TimeDateStamp` and `OptionalHeader.SizeOfImage`).
- `ENTRIES`: one row per address, in the order of the `Entry` enum. `.rva` is the address relative to the image base. `.bytes` are the bytes expected there, with `??` where a byte moves when code is relinked (call targets, references to global data). `.bytes_before` is nonzero when the bytes before the address identify it, as for a return address, whose bytes are the call that precedes it. The name gives the engine function, or the containing function and what the entry is inside it.
- Kinds of entries, 81 in all: 46 functions listed in the executable's function table; 6 small leaf functions that have no unwind information and so are missing from that table (`CommandListNode::BeginRecord`, `GetMainViewport`, `GetBufferFormatFromParameter`, `ResidentResourceManager::GetResourceHandle`, `gn::swapchain::Present`, `GetPauseSystem`); 6 identified by their own bytes inside a function or in data (two places inside functions, the three depth-of-field constants and the spherical-harmonic vertex counts); 16 identified by the bytes before them, mostly return addresses after a call; and 7 with no bytes, confirmed only through other entries (below). Five of the functions are never hooked and exist only so structure fields can be checked in their code; `GetPauseSystem` is never hooked either and only leads to the pause system.
- `REFERENCES`: an instruction inside one entry that leads to another entry, a call or a load relative to the instruction pointer. At startup the displacement must lead exactly to the target. Entries without bytes (globals such as the render target manager, the shader manager and the pause system, and functions reached only through a call) are confirmed this way.
- `SLOTS`: a virtual table slot that must hold another entry's address.
- `FIELDS`: every structure offset, stride, shift, mask and flag bit the mod uses. `.value` is the constant feature code reads through `engine::Value`. `.source`, `.instruction`, `.position` and `.size` name one instruction in a verified entry that encodes the same value; the check reads `.size` bytes at entry address + `.instruction` + `.position`.
- Each feature lists the entries and fields it needs in a `Requirement`. A feature whose requirement fails stays off; the others keep running.

[shader_names.hpp](shader_names.hpp) names the game shaders feature code recognizes by CRC32. Shaders do not change in updates, so it needs no edit.

## Moving the table to a new build

Keep a copy of the supported executable outside the game folder before Steam replaces it; comparing the two builds is much easier than working from the new one alone. A disassembler is needed (Ghidra, or Python with `capstone`). The community decompile database's names and addresses belong to the supported build, so use them on the old executable only.

1. Read the new identity from the new executable's PE header and set `IMAGE_TIMESTAMP` and `IMAGE_SIZE`. Update the build date in the comment above them.
2. Find each function entry in the new executable. Start with its stored bytes. The executable's exception directory lists the start and end of every function that has unwind information, so a function's bytes need only be searched at those starts; the five leaf functions above are not listed and have to be searched in the whole code section. Prologues often repeat, so a search usually returns several candidates; narrow them down by:
   - comparing more of the function with the old build, masking call targets and other relocated fields;
   - the references in `REFERENCES`: a candidate is right only if the instructions recorded there lead to the right targets;
   - the neighbors: code keeps its order when it is relinked, so the entries around it usually moved by the same distance;
   - strings, callers and callees, compared in a decompiler.
3. A place inside a function (a return address, a call site, an instruction) keeps its offset from the start of its containing function, which its name gives. Its new address is the containing function's new start plus the old offset. Confirm its bytes.
4. Data constants: find the instruction that reads each one in its containing function and follow the new address it encodes.
5. Entries without bytes: decode each from the instruction its reference names, in the already-moved source entry. The virtual table slot keeps its index; find the table through the code that loads it, and check that the slot holds the moved function.
6. Change only `.rva` values. Change `.bytes` only where the bytes really differ, keeping `??` on every byte that moves with the code.
7. Fields need no edit while their source entries' code is unchanged, because each instruction keeps its offset inside its function. If startup reports a field failing, the instruction moved inside its function or the structure changed. Find the instruction that uses the field's value in that function and update `.instruction`; if the value itself changed, the renderer changed, which no update has done so far, and the feature that uses it needs review.
8. Build the mod (and any local add-on built against the same headers), install it with the game closed, and start the game.

## Checking the result

The log must show every entry and field verified, for example:

```
MgsvEngine: verified 81 of 81 engine entries and 61 of 61 structure fields in the supported executable.
```

and no `failed verification` lines. The hooks must install as before (`MgsvTargets: installed render target creation hooks`, `MgsvShadows: installed the native local-light hook`, `MgsvTaa: installed native projection hook`, the render-size and DoF hooks, the post-processing start hook, `MgsvVelocity: installed the native velocity hooks`), and the menu must show no red note. Then check in game that upscaling, shadows and Global Illumination behave as before.

A failed entry is named in the log with its address and reason: its bytes differ, a reference between entries does not hold, its virtual table slot does not hold, or no verified entry refers to it. A failed field is named with its value and whether its instruction or its source entry failed.
