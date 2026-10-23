# MGSV shadow resolution, range, and the cost of a virtual shadow map

Companion to `MGSV_GrassShadows_Feasibility.md`, which raised this.

## The budget

The directional shadow map is 8192×8192, laid out as a 2×2 atlas of four cascades —
so **4096² per cascade**. Cascade extents come from `m_shadowProjection2`'s ladder
(`_m03` / `_m13` / `_m23`):

| cascade | extent | texel size |
|---|---|---|
| 0 | 13.2 m | **3.2 mm** |
| 1 | 31.8 m | 7.8 mm |
| 2 | 64.9 m | 15.9 mm |
| 3 | 122.9 m | 30.0 mm |

The near resolution is genuinely good. 3.2 mm is roughly five times finer than a
typical 2048²-over-30 m setup. The near end is not the problem.

## Resolution and range are one budget

A fixed 8192² has to be spread across whatever range is asked for. Holding 3.2 mm
out to 123 m would need **32768²** — 16× the memory, about 1.6 GB for one 24-bit
map. There is no configuration that gives both.

That is why the shadow-map mod's `CascadeShadowRangeScale=4` is not free. It
stretches the same map over 4× the distance, and every texel scales with it:

| cascade | extent at 4× | texel at 4× |
|---|---|---|
| 0 | 52.6 m | 12.8 mm |
| 1 | 127 m | 31.1 mm |
| 2 | 260 m | 63.5 mm |
| 3 | 492 m | 120 mm |

12 cm texels in the far cascade. This is also why the sun's PCSS needed a texel
floor: at 4× the same metric penumbra is a quarter of the texel radius it was.

## What the game already does right

The cascade distribution is roughly ×2 per cascade — 13 / 32 / 65 / 123 — which is
the standard logarithmic split. Cascaded shadow maps are the accepted fix for
exactly this problem, and MGSV already ships them. The remaining range limit is
inherent to the fixed-budget approach, not an oversight.

## Virtual shadow maps: what they actually require

A VSM replaces the fixed map with a **page table plus a physical page pool** —
typically 128×128 texel pages, allocated on demand so texel density follows what is
visible rather than a fixed cascade footprint. The work breaks down as:

| Part | Difficulty |
|---|---|
| Page-table indirection in the sampling shader | easy — a day |
| Deciding which pages are needed each frame | hard — a compute pass over visible casters, GPU-driven in UE5 |
| **Rendering casters into the allocated pages** | **very hard — see below** |
| Cache invalidation as geometry moves | hard — about a week |
| LOD / clipmap management | hard |

## Why the decompiled code does not help

This is the counterintuitive part. The shader side — the lookup, the page-table
indirection — is the *easiest* part of a VSM, and it is the only part the
decompiled shaders give us. They also give us the cascade math and the shadow
comparison pattern, both of which we already have working.

**The hard part is not in the shaders at all.** It is draw orchestration: the
shadow-casting geometry has to be rasterised into specific pages, and a single
object may need to land in several pages. That means one draw becomes N, with the
light's view matrix adjusted per page. In a mod we can *observe* draws through
ReShade, but re-issuing them means capturing each object's constant buffer,
rewriting the transform, and pushing it back — a stall per draw, hundreds of times
per frame.

So having the decompiled code removes perhaps 5% of the work and none of the risk.
VSM is an engine-level change. It is not reachable from a shader replacement.

## The realistic alternative

If the goal is finer shadow detail without an engine rewrite, the lever that does
exist is a **screen-space shadow complement**: ray-march the depth buffer toward
the light and use the result to refine the cascade lookup. It is:

- **shader-only** — the sun shader already binds depth at `t2`, and the grass
  shader has it at `t13`
- **high-frequency by construction** — screen resolution, not shadow-map resolution
- **complementary to CSM** — it supplies the detail the cascades cannot afford, and
  the cascades cover what screen space cannot see

Its limits are known and structural: it only works where the caster is *on screen*,
and it struggles at grazing angles and with off-screen occluders. But that failure
mode lines up with where the cascade map is weakest anyway — nearby, high-frequency
shadow detail — so the two cover for each other rather than compete.

Note this is *not* the screen-space contact shadows rejected early in this project.
That decision was about contact hardening, where screen-space adds nothing the
penumbra mask does not already do. Here the target is missing resolution, which is
a different problem.

Cost unmeasured, as with everything else here.
