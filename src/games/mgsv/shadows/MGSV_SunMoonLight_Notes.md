# MGSV sun and moon directional light notes (retail 1.0.15.4 EN)

Written 21/09/2026. Source: Ghidra read-only work on /mgsvtpp.exe, image base 0x140000000, plus the dev-exe decomp for names.
All addresses are retail unless marked dev-exe. Labels: VERIFIED (read in retail decompile or bytes), SUSPECTED (matched by role or structure), GUESS.

## 1. What this covers

How the game picks the sun or the moon as the directional light near the horizon, where the light direction is built, and where the stepped-move jumps come from. Made for the SmoothTimeCycle hook and any other mod that touches the day/night light.

## 2. Call chain (VERIFIED)

```
FUN_1406b0f20   caller of the per-frame atmosphere update
FUN_14079e3a0   atmosphere per-frame update (param_1 = atmosphere object)
  FUN_14079ea50   if atm+0x3e0 bit0 (time changed): recompute sun and moon direction
  FUN_14079ce30   controller Update (dev-exe DirectionalLightController::Update 0x14075dd80)
    FUN_14079ce70   colour and SUN/MOON DECISION (dev-exe UpdateGrDirectionalLightColor 0x14075ddc0)
    FUN_14079d330   builds the direction (dev-exe UpdateLightDirection)
      FUN_140262480   SetShadowEdgeAdjustment
      FUN_140262080   DirectionalLightSource::SetDirection
```

Dev to retail offset is 0x3F0B0 for the ce30 and ce70 pair (arithmetic check only, no hash match).

## 3. Atmosphere object layout (VERIFIED unless noted)

| Offset | Meaning |
|---|---|
| +0x0c0 | atmosphere context (embedded) |
| +0x140 | sun direction, vec4, w = +1/-1 sign |
| +0x160 | moon direction, vec4, w = +1/-1 sign |
| +0x180 | date and time (hour/min/sec u16 at +0x186/+0x188/+0x18a) |
| +0x198 | sky object pointer |
| +0x1b0 | name hash used by the manager setters |
| +0x1d0 | sun fade value, +0x1d4 moon fade value |
| +0x1dc | sun sign (context+0x11c), +0x1e0 moon sign (context+0x120) |
| +0x1f0 | directional light controller (embedded) |
| +0x3e0 | dirty flags, bit0 = time changed, recompute directions |

## 4. Direction calc: FUN_14079ea50 (VERIFIED)

- Calls FUN_14079c700 (ephemeris, SUSPECTED) and FUN_14079dc30 (builds vectors and w signs, SUSPECTED).
- Writes sun to +0x140, moon to +0x160, signs to +0x1dc and +0x1e0.
- Sun elevation is `-dir.y`.
- Fade constants: sin(-7.9 deg) = -0.1374445 with scale 192.88, sin(-8.2 deg) = -0.14262904 with scale 96.55.
- Outputs: +0x1d0 = sun strength * fade, +0x1d4 = moon strength * fade.

## 5. The flip: FUN_14079ce70 (VERIFIED)

Axis at 0x142beefc0 is (0,-1,0), so `elev = -sun.y` (sine of elevation).

```
sinA = sin(rad * ctrl+0x90)
sinB = sin(rad * ctrl+0x94)
sinC = sin((ctrl+0x94 - 0.3) * rad)

w_sun  = 1.0 if elev >= sinA
         else 1 - (sinA - elev) / (sinA - sinB), clamped 0..1   (only if sinA - sinB > 1e-06)
m      = 1.0 if elev >= sinB
         else 1 - (sinB - elev) / (sinB - sinC), clamped 0..1
w_moon = context+0xb0 * (1 - m)

mode (ctrl+0xc8):
  1 (sun)  if w_sun > 0.0001 or w_moon < w_sun
  0 (none) if w_moon <= 0.0001
  2 (moon) otherwise
```

Notes:
- The flip happens when the sun weight reaches 0.0001, at about ctrl+0x94 degrees. The moon weight ramps over the 0.3 degrees below that.
- Colour and intensity blend across the flip. The direction does not: it jumps from the sun vector to the moon vector in one frame.
- If ctrl+0x90 equals ctrl+0x94 (both 0 by default, see section 8) the mode never leaves sun.

## 6. Direction build: FUN_14079d330 (VERIFIED)

- Copies context+0x80 and +0xa0 (sun and moon dirs) to ctrl+0x60 and +0x70.
- Mode 1: threshold ctrl+0x88, vector ctrl+0x10 or +0x20, chosen by `context+0x11c <= 0` (sign).
- Mode 2: threshold ctrl+0x8c, vector ctrl+0x30 or +0x40, chosen by `context+0x120 <= 0` (sign).
- Mode 0 or no match: threshold -999.0, no override.
- If `asinf(-y) * 57.29578 <= threshold` and the chosen vector is non-zero, the direction is replaced by the normalised fixed vector (fallback vector at 0x142beefc0 region).
- Stepped move: if ctrl+0xcc bit3 is set, latches a new target when the change exceeds `2 * sin(((90 - threshold) / divisions) * 0.5 deg)`, then smoothsteps between the previous and the new target over ctrl+0xbc seconds (defaults 3.0 s, 175 divisions).
- Ends with FUN_140262480(light, flag) and FUN_140262080(light, &dir). SetDirection is where SmoothTimeCycle filters.
- Constants: 0x142a390cc = 0.017453292 (deg to rad), 0x142a390d0 = 57.29578 (rad to deg).

## 7. Stepped move is weather data (VERIFIED)

FUN_1406b96e0 (weather to atmosphere, per frame) pushes weather-record values with FUN_140550410(record, id) into manager setters:

| Setter | Writes | Weather id |
|---|---|---|
| FUN_1406b3120 | ctrl+0xcc bit3 (stepped move on/off) | 0x23a (true if above about 0) |
| FUN_1406b3900 | ctrl+0xbc (interpolate seconds) | 0x238 |
| FUN_1406b2fe0 | ctrl+0xd0 (division count) | 0x239 (cast to int) |

Other setters in the same pass write ctrl+0x98, +0x9c, +0xa0, +0xa4, +0xa8, +0xac, +0xb0 (times 0.001), +0xb4, +0xb8, +0xc0 and flag bit2 of +0xcc. None of them write +0x88, +0x8c, +0x90 or +0x94.

Setter template: first argument is the atmosphere manager (list at +0xe0, count at +0xd8). Each setter finds atmospheres whose name hash at atm+0x1b0 matches, writes one value and sets a dirty bit in atm+0x3e0.

## 8. Who owns +0x88, +0x8c, +0x90, +0x94 (mixed)

- VERIFIED: no code writer found. The dev-exe constructor (DirectionalLightParameter) zeroes them. No manager setter touches them.
- VERIFIED: FUN_140770b90 (atmosphere property registration) registers these named properties through FUN_140077940(registry 0x142bed650, type 8, name, id):

| Property | Name string | Id |
|---|---|---|
| dirLightFadeStart | 0x142207750 | 0x23c |
| dirLightFadeLength | 0x142207768 | 0x240 |
| dirLightSunLimitAngle | 0x142207878 | 0x264 |
| dirLightMoonLimitAngle | 0x142207890 | 0x268 |

- Other property name strings (ids not read): fixedLightDirSunRise 0x1422078d8, fixedLightDirSunSet 0x1422078f0, fixedLightDirMoonRise 0x142207908, fixedLightDirMoonSet 0x142207920, fixedRisingSunDir 0x142207938, fixedFallingSunDir 0x142207950, fixedRisingMoonDir 0x142207968, fixedFallingMoonDir 0x142207980, isSteppedMoveOfDirectionalLight 0x142207b00, interpolateTimeInSecondOfDirLightSteppedMove 0x1422079f0, divisonNumOfDirLightSteppedMove 0x142207a20.
- SUSPECTED (matched by role): ctrl+0x88 = SunLimitAngle, +0x8c = MoonLimitAngle, +0x10/+0x20/+0x30/+0x40 = fixed sun rise/set and moon rise/set directions, +0x90 = FadeStart, +0x94 = FadeLength (the weakest link: the code uses +0x94 as an end angle, the name says length).

## 9. Notes for mods

1. SmoothTimeCycle snaps when the direction changes by more than snapDegrees (10). The sun to moon flip is a large angle, so it snaps and the smoothing is skipped. That is why a visible jump can remain at the flip.
2. Ideas, none implemented or tested (GUESS):
   - Hook FUN_14079ce70, read the mode (+0xc8) and both weights, and crossfade the direction between sun and moon over the ramp instead of snapping.
   - Hold the light at the horizon until the moon weight is up.
   - Read ctrl+0x94 at runtime so the mod matches the data.
3. The stepped-move bit is per weather record. Clearing bit 3 of ctrl+0xcc around the FUN_14079d330 call (what SmoothTimeCycle does) works for every weather.
4. Not a fixed constant: the flip angle is data driven and can differ per weather or region.

## 10. NOT DONE / open items

1. Reader of property ids 0x23c, 0x240, 0x264, 0x268. I found the registration (FUN_140770b90) but not the code that reads these ids from the weather or atmosphere data and fills the controller. Full-program instruction searches for `mov edx, 0x264` and similar timed out when the Ghidra link dropped. This would confirm the name to field mapping in section 8.
2. Actual values of ctrl+0x88, +0x8c, +0x90, +0x94. They are runtime data, not in the exe. Read them live in a running game at atmosphere+0x1f0 plus the offset, in several weathers and both rise and set.
3. Ids of the fixedLightDir* and stepped-move property strings (only the four in section 8 were read).
4. FUN_14079c700 (ephemeris) and FUN_14079dc30 (vector build, w signs): matched to the dev decomp by structure only. No dev name, no hash match, so the dev-exe addresses are not quoted.
5. Retail writer of context+0x11c and +0x120 is FUN_14079ea50 by offset (atm+0x1dc/+0x1e0), which matches dev UpdateAtmosphereContext by structure only.
6. FUN_1406b8920 (writes a block with +0x94 = data id 0x113 and +0x90 = a computed value from FUN_1406b8390): I think this is a sibling parameter set, not the controller, because it puts a vector at +0x80 where the controller keeps a light pointer. Not confirmed.
7. The stepped-move ids 0x238, 0x239, 0x23a are matched to the property name strings by the setter role only.


## 11. Address quick list (retail)

```
0x14079e3a0  atmosphere per-frame update
0x14079ea50  sun/moon direction calc
0x14079ce30  controller Update wrapper
0x14079ce70  sun or moon decision, mode at ctrl+0xc8
0x14079d330  direction build (hooked by SmoothTimeCycle)
0x140262080  SetDirection (hooked by SmoothTimeCycle)
0x140262480  SetShadowEdgeAdjustment
0x1406b96e0  weather to atmosphere per-frame pass
0x1406b3120  stepped move enable setter
0x1406b3900  stepped move seconds setter
0x1406b2fe0  stepped move divisions setter
0x140770b90  atmosphere property registration
0x140077940  property register helper
0x142beefc0  axis (0,-1,0)
0x142a390cc  deg to rad
0x142a390d0  rad to deg
```

Disclaimer: This information was found in Ghidra locally with an MCP server via AI automation in a strict bespoke environment that has constant sanity checking, it is unlikely to be wrong but AI can make mistakes
