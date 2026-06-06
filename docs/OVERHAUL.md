# LUMENMERE — THE IMPRESSIVE OVERHAUL
### *ma pi suno awen — one decisive plan to take the vale from "nice" to "holy shit," on GL3.3 / WebGL2, in a browser, without losing the hush.*

These are the calls. No alternatives. Every item fits the verified pipeline — deferred 4-MRT G-buffer, `InstancedField` (loc4 offset + loc5 tintEmissive + loc6 xform already in `src/InstancedField.h`), analytic `terrainHeight`, floating origin, the half-res sky pass, the `OmniLightGPU` UBO — and degrades through the existing `mLowSpec` switch. Nothing introduces compute, SSBO, geometry/tessellation shaders, or full-res ray-marching.

---

## 1. THE LEAP

Today Lumenmere is a handsome still life: pretty sky, clean fog, a mirror lake, cloned conifers that don't move. The leap is **motion + light that obeys the sun and answers your feet**, delivered by five cheap changes that compound. (a) One vertex-shader wind model turns every blade, branch, willow-strand and reed into a cantilever that *leans together in gusts* and *parts away from your steps* — the entire basin breathes. (b) A chunked `InstancedField` carpets the meadows with 150–250k swaying grass tufts and fills the air with 7–9k shader-time pollen/ember/dust motes at zero per-frame CPU. (c) Quarter-res god-ray shafts rake through the conifers and over the rim at dawn while the flat grey fog becomes a drifting, sun-scattering silver that *pools in the hollows and glows gold toward the sun* — together ~0.5 ms. (d) A film-look composite (time-of-day grade, grain, 1px CA, sharpen, bloom-dirt) makes every frame read as authored. (e) The Mere stops being a painted mirror and truly **doubles the trees, aurora and your lantern** via a half-res planar reflection, with caustics dancing in the lit shallows. The soul is untouched: cool sleeps / warm wakes still holds, emissive still punches through fog as hard bright coals, nothing chases you — but now the world is *alive*, and it visibly notices you walking through it.

---

## 2. ASSET LIBRARY

### 2.1 Generation approach
All assets are generated in C++ in `src/Props.h` from primitives, emitted as `Vertex2` triangle lists, and drawn through the existing geometry pass via **`InstancedField`** with **per-chunk static instance buffers** built on chunk-enter (≤1 chunk/frame), frustum-culled per frame. No per-frame vegetation upload. LOD = dithered `discard` cross-fade, no blending, no sorting.

**New `proc::` primitives (add to `src/Props.h`):** `quad`, `billboard`, `crossQuads` (2–3 quads at 60°), `disc` (fan), `icosphere` (subdiv 0/1/2 = 20/80/320 tris), `branch` (tapered curved tube), `recurseBranch`, `ribbon` (segmented strip, weight ramps to tip), `bladeFan`, `leafCard`. All route through the existing `tri()` so normals/material flow unchanged.

### 2.2 The two enabling foundation changes (decisive)
1. **`Vertex2::Material` → `vec4`**, with **`.w = windWeight`** (0 = rigid trunk/rock/stem base, 1 = leaf-edge/blade-tip). One-line VAO change (`loc3` 3→4 floats). This is the per-vertex stiffness every builder sets.
2. **Wind uses both per-vertex weight AND per-instance bendiness** (see §3.1). This reconciles the asset and living-world models: trunks stay still, tips whip, whole instances tune their responsiveness.

### 2.3 Final per-instance attribute layout (the decisive `InstancedField` layout)
Extends the existing `src/InstancedField.h` (`loc6` already = `scale, yaw, windStiffness, phase`) by **one** new attribute. Default for non-veg VAOs set once at init so the unified VS is a no-op.

| loc | type | meaning | default (attrib const) |
|----|------|---------|------------------------|
| 4 | vec3 | `offset` — world XZ/Y placement (origin-relative via `uOriginOffset`) | — |
| 5 | vec4 | `tintEmissive` — rgb albedo tint, **a = emissive** (0 matte, ≥1 blooms, ramp to 2.5) | (1,1,1,0) |
| 6 | vec4 | **`(scaleXZ, yaw, bendiness, phase)`** — kills cloned orientation | (1,0,0,0) |
| 7 | vec4 | **`(yStretch, variant, lodFade, lean)`** — non-uniform height, FS variant tint, dither LOD, base-skew | (1,0,1,0) |

Effective scale = `(scaleXZ, scaleXZ·yStretch, scaleXZ)`. `lodFade` drives `if (bayer(gl_FragCoord) > lodFade) discard;`. `variant` flips bark/autumn/moss in the FS. `lean` skews the base for windswept trees.

### 2.4 The final coded-asset set
**Trees (7):** T1 Spire Conifer (Mistwood), T2 Round Broadleaf, T3 White Birch, T4 Weeping Willow (ribbon strands trail in wind), T5 Blossom (petal-drift billboards), T6 Dead Snag, **T★ Heartwood** (one per region, emissive event anchor, ribbon veins light first). 35–300 tris.
**Ground cover (7):** G1 tuft grass (`bladeFan`), G2 crossed-quad grass (far/lowspec), G3 fern, G4 reed/sedge, G5 clover, G6 frost-grass (aurora-driven glow), G7 moss patch. 4–28 tris.
**Flowers (6):** F1 meadow star, F2 bell, F3 lantern-bloom (promotes to real light), F4 dandelion (glide-seed hook), F5 lupine spike, F6 water lily. Bloom-from-bud is universal: `scaleXZ 0→1` + `emissive a 0→2.5` over ~0.6 s. 10–34 tris.
**Fungi / glow flora (7):** M1 glow mushroom (emissive gills), M2 trumpet, M3 bracket, M4 ember-cap (parts fog), C1 glow crystal (faceted icosphere shards, hot spec), L1 lilypad, V1 vine, V2 glow-lichen decal. 2–42 tris.
**Rocks / props (7):** R1 boulder (jittered icosphere subdiv1), R2 pebble, R3 cliff slab (Cascades walls), R4 fallen log (life-cluster host), R5 stump, R6 driftwood, R7 ruined brazier/obelisk (relight → light). 12–120 tris.
**Fauna (6, instanced + animated):** A1 deer (befriend → markings ignite), A2 birds (boids, VS wing-flap), A3 butterfly/moth, A4 fish/glow-mote shoal, A5 sky-creature (Radiance-gated), A6 wisp swarm (follows after Call). 4–220 tris.

### 2.5 Per-region palettes & stocking (linear RGB)
Region selected at scatter time from radius + height + angular wedge; species density cross-fades over the 80–120 m bands.

| Region | Trees | Flora / glow | Fauna | Palette (linear) |
|---|---|---|---|---|
| 0 Mere (r<150) | T4, T5 shore | reeds G4, lilypads L1, lily F6 | fish A4, wisps, butterflies | ink-teal `#08222B`, cyan `#1FD3C4` |
| 1 Meadowlight (r35–220) | sparse T2/T3/T5 | dense G1/G5, F1/F2/F4/F5, F3 lantern-bloom | birds A2, butterflies A3, deer A1 | grass `#20422F→#2E5A3A`, gold `#FFC65A`, rose `#FF8FB0` |
| 2 Mistwood (r150–400) | dense T1, T6 | ferns G3, moss G7, M1/M2 rings, C1 | deer A1, moths | bark `#0E1A12`, cyan `#4FE0FF`, violet `#9A7CFF` |
| 3 Cascades (steep) | T4, T6 | reeds, moss G7, V1, L1 downstream | birds, pool fish | slate `#3A4654`, turquoise `#2BE0C0` |
| 4 Emberfen (hollow) | T6, gnarled T2 | G4, M4 ember-caps, R7 braziers | moths A3 | amber `#FFB24D`, ember `#FF6A2A` |
| 5 Aurora Steppe (shelf) | rare T6 | frost-grass G6, C1 crystals | sky-creatures A5 | silver-blue `#AEC4D6`, violet `#7A4DFF`, jade `#36FFC2` |
| 6 The Reach (r680–880) | dwarfed T1/T3 | F5, frost-grass, C1, R7 obelisks | sky-creatures A5, birds | dawn-rose `#F4A98C`, gold `#FFD27A`, night-stone `#1C2238` |

**Two grade laws every asset obeys:** cool sleeps / warm wakes (dormant = teal/slate/silver), and emissive `a` is the progress bar (sleeping `a=0`, woken `a→2.5`).

**Scale sanity:** ~30 tris avg × ~10k visible instances ≈ 300k veg tris; grass (cheapest tier, most instances) adds 0.5–0.7M with most far fragments dither-killed. Under the 600k/frame budget.

---

## 3. LIVING WORLD

### 3.1 WIND — the single biggest "alive" win, near-free
Replace the `firstPassVertex.vert:27` base-jitter hack with a cantilever model. One global gust field (layered sines sweeping **along** `uWindDir`) so neighbours move *together in waves*; bend grows as `height²`; per-vertex `windWeight` × per-instance `bendiness` scales it; the tip dips for length-conservation; blades within 1.2 m **lean away from the player**.

```glsl
float w = inMtlProps.w;                          // per-vertex stiffness (0 base → 1 tip)
float h = max(p.y, 0.0);
float along = dot(baseW.xz, uWindDir);
float gust = sin(along*0.05 - time*0.9 + ph)*0.60
           + sin(along*0.17 - time*1.7 + ph*1.7)*0.30
           + sin(time*3.1 + ph*2.3)*0.10;        // slow sway + ripple + flutter
float bend = uWindStrength * inSway.z * w*w * h * (0.55 + 0.45*gust);
vec2 toP = baseW.xz - uPlayerXZ; float d = length(toP);
float part = (1.0 - smoothstep(0.0,1.2,d)) * 0.8;
vec2 off = uWindDir*bend + (d>1e-3?toP/d:vec2(0))*(part*h);
p.x += off.x; p.z += off.y; p.y -= 0.5*dot(off,off)/max(h,0.05);  // length conservation
```
Global wind state on CPU in `Window::update`: `uWindDir` wanders ±9°, `uWindStrength` breathes via low-freq gust (`mWindBase≈0.5 m`). Per-instance: `bendiness ∈ [0.8,1.0]` grass / `[0.15,0.35]` branches / `0` trunks; `phase = hash·2π`. **Cost: ~15 ALU/vert, zero CPU, 4 small uniforms.**

### 3.2 GROUND-COVER at scale — chunked static `InstancedField`
- **Chunk = 64 m**, resident window **5×5 (≤25 buffers)**. Scatter on chunk-enter with `proc::Rng` seeded by `(cx,cz)` → deterministic, no revisit pop. Snap to `terrainHeight`; skip `y<0.6` (water/shore), steep slopes (central-difference normal), out-of-band. **Build budget ≤1 chunk/frame.**
- **Mesh:** 3-blade tuft = 9 verts. Densities: near ≤40 m **4 tufts/m² (~80k)**, mid 40–80 m **1/m² (~60k)**, far >80 m dither-killed. In view: **~150–250k tufts, ~0.5–0.7M tris/frame.** Halve on `mLowSpec`.
- **Dithered LOD:** ordered-Bayer `discard` keyed to distance; survivors fatten (`scaleXZ *= keep`) so individual-blade popping is impossible. Frustum-cull whole chunks (AABB vs 6 planes). VBO mem ≈ 44 B × 24k × 25 ≈ **26 MB**.
- **Cost:** CPU ~0 after build (no per-frame upload); GPU tri/fill-bound, controlled by the table.

### 3.3 PARTICLES / ATMOSPHERE — shader-time, zero per-frame CPU
Static seed buffer; positions computed entirely in the VS from `time` + hash, **wrapped into a box centred on the eye** (mod), rendered as `GL_POINTS` (perspective `gl_PointSize`), **additive into HDR after lighting, before bloom** so they bloom. Soft-depth fade against `gPosition` so motes nestle into grass/fog.

| layer | count | box R | notes |
|---|---:|---:|---|
| pollen/spores (day) | 2.5–4k | 35 m | warm white, slow rise; fades with sun elevation |
| ambient embers (night) | 1.5k | 30 m | ember-orange, bloom-lit |
| dust motes | 1.5k | 25 m | faint, near-still, always |
| mist wisps | 40–80 | 60 m | big soft additive **instanced quads** low in hollows |
| falling leaves | ~300 | 40 m | **instanced quads**, VS tumble + gravity + sway |

**Lights from particles:** reuse `FireflySystem::appendLights` — push the ≤16 nearest night embers into the `OmniLightGPU` UBO (headroom under the 48 cap). **Cost: ~7–9k verts, one draw call each, zero CPU/upload.** Second-biggest payoff after wind.

### 3.4 WILDLIFE — cheap CPU sims feeding instances; periodic animation in the VS
- **Birds:** 30–80 in 1–3 flocks, real boids (O(n²) at n≤80 = µs) + "wheel over the vale" attractor + altitude hold; 2-tri delta-wing, wingtip flaps in VS (`flapHz≈6`); orient mesh to velocity via `loc6.yaw`. Silhouette by day, faint emissive by night.
- **Butterflies/moths:** 20–40, Lissajous bob around nearest bloomed flower, retarget every few s, VS flap `flapHz≈12`, faint tip glow.
- **Fish/dimples:** CPU spawns a ripple ring every 0.5–2 s at a lake point; pass ≤8 active dimples `(cx,cz,age)` as a uniform array to `water.frag` (expanding decaying ring on the normal + spec brighten). Fish shoal A4 emissive `a≈2` lights the shallows.
- **Deer (extend `DeerAgent`):** add `fleeing` (player within ~8 m & closing → flee target, ×3 speed for 2 s, then resume wander); when calm and lingered-near, ramp `loc5.a` emissive (markings ignite) — the befriend beat, free.
- **Sky-creatures A5:** slow great-circle drift overhead, count keyed to Radiance, emissive-only (no dynamic light).
**Cost:** all sims ≤100 agents → µs; per-frame stream is the same `STREAM_DRAW` fireflies already do.

### 3.5 World-reacts-to-presence polish
- **Water vertical motion:** tessellate the water quad to **64×64**, add a 2–3 wave Gerstner-lite VS displacement (tiny amplitude, shares `uWindDir`) so the surface undulates and breaks the shore line.
- **Bloom-trail (Meadowlight signature):** ring buffer of recent player XZ+time; flowers (an `InstancedField` layer) within ~1.5 m of any trail point ramp `loc5.a` 0→full and **stay lit**. Proximity-test only flowers in the current chunk (a few hundred) → cheap. Pairs visually with the grass-parting already in the wind VS.
- **Lantern spring + walk-bob:** critically-damped spring lags the lantern light down-and-behind the camera so the warm pool swings and settles; subtle ~1.8 Hz view bob while moving. Tiny amplitudes — felt, not watched.

**Living-world budget summary:** wind 0 CPU / ~15 ALU/vert; grass ~0 CPU / 0.5–0.7M tris; particles 0 CPU / ~9k verts; boids µs; water 64×64 + ≤8 dimples; bloom-trail/lantern µs. Lights ≤48 desktop / ≤24 lowspec, 128 UBO max.

---

## 4. RENDERING / LIGHTING UPGRADES

The chosen set, each with pass, honest perf verdict, and fallback. Every effect is half/quarter-res and reuses RT0 (origin-relative world pos, `gPosition`) + RT1 (`gNormal`, sky mask via `dot(N,N)<0.25`) + the live `Sky` uniforms.

| # | Upgrade | Pass | Cost | Web@60 | Verdict / Fallback |
|---|---------|------|------|--------|--------------------|
| 1 | **God-ray shafts** | ¼-res radial blur (24 taps) toward CPU-projected `uSunScreen`, sampling sky-mask × `uSkyTex` luminance, additive into HDR **before bloom** so shafts bloom | ~0.3 ms | Yes | **SHIP FIRST.** Trees/rim block shafts for free. Fallback: 12 taps ⅛-res, or off. |
| 2 | **Volumetric mist + sun in-scatter** | inside lighting pass — `fbm` density breakup + basin pooling + `pow(dot(toEye,sunDir),4)` warm in-scatter on `uFogColor` | ~free | Yes | **SHIP.** Keep *before* the emissive add so woken coals still punch through. Fallback: 3-octave fbm on lowspec. |
| 3 | **Film-look composite** | folds into `thirdPassFrag.frag` — ToD grade (lift/gamma/gain from `Sky`), grain (luminance-scaled), 1px radial CA, 5-tap sharpen, bloom-dirt mask | ~free (2 extra taps) | Yes | **SHIP.** Dialed low: CA ~1px, grain ~1.5/255, sharpen ~0.2. |
| 4 | **Planar Mere reflection** | mirrored camera across y=0 → ½-res `reflectionFBO` with cheap lighting (`albedo*(ambient+sun)+emissive`), clip y≥0; sampled by screen-UV perturbed by wave normal, mixed by Fresnel | 1–2.5 ms | Yes, **gated** | **SHIP (gated).** Only when water AABB on-screen; cull to terrain+trees+brightest 16 lights, skip grass. **Lowspec fallback:** analytic `reflSky` + re-projected lantern/firefly glints. |
| 5 | **Sun shadow map** | pass (0) depth-only into 2048² (1024² lowspec) `DEPTH_COMPONENT24`, camera-box ortho, texel-snapped origin-relative; 3×3 hardware PCF (`sampler2DShadow`) on the sun term only, faded to lit at box rim | 0.5–1.5 ms | Yes | **SHIP NEXT.** Ship the **contact-shadow march** (8-step along `uSunDir` in `gPosition`, ~0.3 ms) *first*; add the real map when budget allows. Fallback: 1024² 1-tap. |
| 6 | **HBAO from G-buffer** | ½-res, 12 poisson taps on `gPosition`/`gNormal`, range-checked, bilateral blur; multiply **ambient term only** | 0.4–0.8 ms | Yes | **SHIP NEXT.** Stash in free `gNormal.a`. **Lowspec fallback:** baked vertex AO computed at mesh-build in `Props.h` (free at runtime). |
| 7 | **Water refraction + caustics** | folds into `water.frag` (refraction = HDR scene at normal-distorted UV, depth-tinted) + lighting (caustics on `oMtlProps.a` lakebed flag, shallow-masked dual-sin) | ~free | Yes | **SHIP with #4.** Turquoise caustics tie into ignite-the-Cascades fiction. |
| 8 | **Soft additive particles** | fireflies/spores/embers as camera-facing additive sprites into HDR before bloom, soft-depth vs `gPosition` | low | Yes | **SHIP NEXT.** Sells "a sleeping tree exhales fireflies." |
| 9 | **DOF (gentle far-field)** | `gPosition`-distance blur, **Rest-mode only**, subtle | low | Yes | **DEFER to Phase 10 only.** Exploration stays crisp. |
| 10 | **SSR (non-planar)** | — | high | Risky | **CUT.** Planar #4 owns the one mirror that matters; fresnel-tint fakes the rest. |

### The new pass order (decisive)
```
(0) sun shadow depth  → (1) geometry  → (2) HBAO(½)  → (3) sky(½)
→ (4) reflection: mirrored geo+cheap light(½)   [only when water on-screen]
→ (5) lighting (full: +shadow PCF +AO·ambient +rich volumetric fog +caustics)
→ (6) water (+planar reflection +refraction +caustics)
→ (7) god rays (¼, additive into HDR)  → (8) bloom  → (9) composite (film look)  → (10) hud
```
New shaders: `godray.frag`, `ssao.frag`, `shadowDepth.vert`(+trivial frag), `reflectLighting.frag`. Edits: `secondPassFrag.frag` (fog/shadow/AO/caustics), `water.frag` (planar+refraction), `thirdPassFrag.frag` (film look), `firstPassFragment.frag` (water flag + AO channel), `Window.cpp` (pass insertions + FBOs + sun-UV/light-matrix uniforms), `Sky.h` (emit grade/grain/caustic strengths).

---

## 5. BUILD ORDER

Shippable, screenshotable sub-phases folded into LUMENMERE Phases 5–10 (1–4 assumed in place). Biggest "alive"/"wow" wins front-loaded: the world breathes and the dawn bleeds light *before* a single new gameplay verb ships.

### Phase 5 — Life at Scale (+ the near-free atmosphere wow)
- **5a Foundations.** `Vertex2::Material→vec4` windWeight; the cantilever wind VS (§3.1); `InstancedField` `loc7` extension; new `proc::` primitives (icosphere/disc/quad/billboard/branch/recurseBranch/ribbon/bladeFan); global wind state in `Window`. Nothing new on screen, but everything after moves. **verify: the existing conifers and mushrooms now sway in coherent travelling gusts, trunks still, tips whipping.**
- **5b God rays + volumetric mist + film-look composite** (rendering #1/#2/#3, ~0.5 ms total). Applies to the *existing* world immediately. **verify: dawn shafts rake through the conifers and over the rim, mist pools in the hollows and glows gold toward the sun, the frame reads as graded film.**
- **5c Ground-cover at scale** (§3.2): chunked grass G1/G2 + fern G3, dithered LOD, player-parting. **verify: the Meadowlight is a moving carpet of grass that ripples in gusts and bends open around your feet as you walk.**
- **5d Atmosphere particles** (§3.3): pollen/ember/dust points + mist wisps + falling leaves, soft-depth, additive pre-bloom, embers feed the light UBO. **verify: the air is full of drifting motes — warm pollen by day, blooming embers at night that light the fog around them.**
- **5e Tree species T1–T6 + rocks R1/R4 + reeds G4 + lilypads L1 + fungi M1/M2/M3 + crystals C1**, all via `loc6/loc7` (non-cloned yaw/scale/lean/variant). **verify: the Mistwood reads as a real grove of varied trees, willows trail over the Mere, glow-mushroom rings and crystal-trees light the forest floor.**
- **5f Grounding** (rendering #5 contact-shadow march → sun shadow map, #6 HBAO/baked vertex AO). **verify: trees, rocks and the Lampbearer cast grounded shadows; creases under the canopy and rock crevices darken — assets read as deliberately built.**

### Phase 6 — The Bloom Loop
- **6a Flowers F1–F6 bloom-from-bud + bloom-trail** (§3.5) wired to Pulse/Channel/Plant + per-region Bloom and global Radiance. **verify: wildflowers ignite gold in a persistent trail behind your feet; one Pulse blooms a whole patch in a 1→14 m ring; Radiance % rises and the fog visibly opens.**
- **6b Per-region palettes + lantern spring + walk-bob.** Cross-faded `uFogColor`/`uAmbient` per wedge as you walk; critically-damped lantern pool. **verify: biomes read instantly distinct (warm Emberfen island at midnight, teal Mere); the warm lantern pool swings and settles as you stop and turn.**
- **6c The Mere lives** (§3.5 water waves + §3.4 fish dimples + fish A4 shoal). **verify: the lake surface undulates and breaks the shoreline; rings dimple where fish rise; a glowing shoal lights the shallows.**

### Phase 7 — Region Anchors
- **7a Heartwood T★ + bloom shockwave.** Channel ~3 s → emissive 0.3→4.5, then a region-wide 1→60 m Pulse staggers a travelling chain-bloom across the `InstancedField`, fog recedes, palette flips grey→radiant, becomes a fast-travel node. **verify: the "Heartwood Bloom" shot — a shockwave ring races across the forest floor igniting every flower in sequence, fog flushing cold-blue to dawn-rose.**
- **7b Planar Mere reflection + refraction + caustics + ignite-the-Cascades** (rendering #4/#7, gated). **verify: "Mirror at Murmuration Hour" — the still lake doubles the trees, aurora and your lantern's gold glitter-trail; turquoise caustics dance in the lit shallows as ignited water pours downstream.**

### Phase 8 — The Sky You Author
- **8a Soft additive particle polish** (rendering #8) on fireflies/spores. **verify: fireflies are soft warm coals that bloom and visibly light the mist around them.**
- **8b Sky-creatures A5 (Radiance-gated) + playable aurora + frost-grass G6.** **verify: complete a constellation — a line draws across the live sky, an aurora unfurls in its colour and pools across the Steppe igniting frost-grass jade; manta sky-creatures drift overhead as Radiance rises.**

### Phase 9 — Inhabitants & Traversal
- **9a Birds A2 boids + butterflies A3 + fish A4 schooling** (§3.4). **verify: a flock wheels over the vale, butterflies orbit the flowers you bloomed, a fish shoal schools beneath the raft.**
- **9b Deer A1 startle/befriend + wisp swarm A6.** **verify: a deer trusts you, its markings ignite and a heart-light blooms, and it trots alongside leading you to a hidden pool; gathered wisps follow you after Call.**

### Phase 10 — Rest, Memory & The Long Dawn
- **10a Rest/Observe + subtle far-field DOF (rest-mode only, rendering #9) + time-scrub.** **verify: sit into the still dawn framing, HUD hidden, far vista softly defocused, time scrubbing night→gold over the valley you woke.**
- **10b The Long Dawn.** World-wide unison bloom — every `InstancedField` instance flares emissive in lockstep, the Wellheart blazes, then the world settles permanently radiant; a new seed waits at the Mere. **verify: the whole bloomed valley flares in unison under a golden sky, then reload and find it still glowing with a new seed at the water.**

---

*Files to touch (all absolute):* `/config/workspace/Project-ilo/src/Props.h`, `/config/workspace/Project-ilo/src/Vertex.h`, `/config/workspace/Project-ilo/src/InstancedField.{h,cpp}`, `/config/workspace/Project-ilo/src/Window.cpp`, `/config/workspace/Project-ilo/src/Sky.h`, `/config/workspace/Project-ilo/src/Firefly.cpp` (+ new fauna systems modeled on it); shaders `/config/workspace/Project-ilo/shaders/firstPassVertex.vert`, `firstPassFragment.frag`, `secondPassFrag.frag`, `water.{vert,frag}`, `thirdPassFrag.frag`, and new `godray.frag`, `ssao.frag`, `shadowDepth.vert`, `reflectLighting.frag`.
