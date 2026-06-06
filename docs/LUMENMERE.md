# LUMENMERE — *The Long Dawn*
### *ma pi suno awen — "the land where light stays"*

*A vast, decisive synthesis. Where a call borrows from a vision I name it: **A = Lumenmere** (wilderness), **B = Aurelune** (celestial), **C = Lumène** (garden/ruins), plus the **Sky**, **Activities**, and **Tech** docs. No alternatives. These are the calls.*

---

## 1. THE VISION

**The world is LUMENMERE.** One breathing caldera-vale cupped around a great mirror-lake, asleep in blue-dark until you teach it to glow.

**The soul.** You arrive in the last held hour before dawn at the edge of a still black mere the size of a small country. The air is silver, mist pools in the hollows, the great trees stand dark, the flowers folded shut — and your lantern is the only warm coal burning in the whole basin. Lumenmere is a place that is *almost* awake (A's framing), a cosmos that long ago grew tired and lay down in the grass (B's myth — it's why the aurora pools to the ground on the Steppe), a garden gone soft with moss that *remembers* every light you give it (C's custodianship). Nothing chases you. Nothing can be lost. Everywhere you walk, light *catches*: a flower opens gold beneath your step, a sleeping tree exhales fireflies, a waterfall ignites turquoise crest-to-pool, and the lake doubles a sky you are slowly rearranging. By the end of a session the silent indigo bowl you arrived in is a glittering basin of warm light under a sky bleeding violet into amber — and it will still be glowing when you return.

**The player fantasy.** You are the **Lampbearer** — a small, soft-spoken drifting spark, a gentle custodian, not a conqueror. The one verb is *light*. You carry a single warm coal into a sleeping grey world and wake it, region by region, until the whole land glows on its own and the heavens answer back. The reward is never a score; it is **more visible, more beautiful world**, and the quiet knowledge that you made it.

---

## 2. THE WORLD

**Shape & scale.** One seamless **caldera-bowl** (A + C), tilted so that from anywhere you can see *home* — the glowing Mere at the lowest point (B's "always see the Well"). Built on the Tech doc's finite chunked heightfield: **1792 m × 1792 m (28 × 28 chunks of 64 m)**, with a **~1.5 km playable bowl** and the outer ~150 m ramped into a **perimeter mountain ridge** so the horizon has silhouette and the world feels boundless, not boxed. No loading screens, no walls — distance is hidden by aerial-perspective fog that melts terrain into the sky-horizon tint. Regions are radial wedges that **cross-fade over 80–120 m bands** (B) by blending fog colour + emissive density; **following water** (rim → Cascades → Mere → Emberfen) is always a legible thread connecting everything (A).

**The seven places** — chosen, merged, and the duplicates collapsed:

| # | Region | Identity & signature | Light/colour (linear-graded hexes) | From |
|---|--------|----------------------|-----------------------------------|------|
| 0 | **The Mere** (hub, centre, lowest) | Glass-still mirror-lake; bioluminescent shallows; a punt-raft; at its heart the **Wellheart** — the Heart-of-the-Grove reborn as a slowly-turning **orrery of light** you grow, its column visible basin-wide. | ink-teal `#08222B`, cyan glow `#1FD3C4`, mirror-sky | A Mere + B Well of Sky + C Heartglade/Mirrorpools |
| 1 | **Meadowlight** (the Sway) | Rolling grass; wildflowers that **bloom emissive in a trail behind your feet** and stay lit; the most kinetic "the world answers *me*" beat. | teal grass `#20422F`→`#2E5A3A`, gold `#FFC65A`, rose `#FF8FB0` | A Meadowlight + B Lantern Meadows + C Bloomfields |
| 2 | **The Mistwood** (ancient grove) | Dense indigo conifers (12–20 m), low fog, mushroom rings, the first sleeping **Heartwood**, and **singing crystal-trees** tuned to a scale (strike in order → a constellation lights). | bark `#0E1A12`, foliage `#16321F`, mushroom cyan `#4FE0FF` / violet `#9A7CFF` | A Mistwood + B Hollow Wood + C Glasswood folded in |
| 3 | **The Cascades** (spillways) | A staircase of slate cliffs (30–60 m), ribbons of falling water you **ignite turquoise** with carried light-spores; lit water flows downstream into the Mere. The vertical-glide region. | slate `#3A4654`, mist `#CBD9E6`, spore-turquoise `#2BE0C0` | A (unique) |
| 4 | **The Emberfen** (warm springs) | Sunken hollow drowned in **warm** fog; ruined braziers and ember-mushrooms; relighting each **physically parts the haze**. The cosy, intimate warm counterpoint to the cool lake. | amber `#FFB24D`, ember `#FF6A2A`, reed-teal `#2E7D6B` | A Emberfen + C Emberfen (identical → merged) |
| 5 | **The Aurora Steppe** (star-flats) | A near-flat, wet mineral shelf that **mirrors the starfield to the horizon**, under curtains of aurora that **dip to the ground** — and the aurora is *playable*: glide through to bend its colour, linger to pool it across the plain. | silver-blue `#AEC4D6`, violet `#7A4DFF`, jade `#36FFC2` | A Glimmerflats + B Aurora Steppe (merged) |
| 6 | **The Reach** (high rim / Observatory) | The climbable rim +80–95 m above the fog-sea; an open-air **orrery star-dial** where you **weave constellations** and author the sky; the sunrise overlook over the whole bowl. | dawn-rose `#F4A98C`, gold `#FFD27A`, night-stone `#1C2238`, aurora `#5BE0B0`→`#C77CFF` | A Reach + B Drift vistas + C Observatory |

**Cuts, named:** B's *floating Drift islands* (incompatible with the grounded floating-origin heightfield) fold into the Reach's high vistas and glide-launches. B's *Singing Dunes* mark-making folds into Meadowlight's bloom-trails. B's *Tidewater* moonrise folds into the Mere's events. C's *Glasswood* survives as the Mistwood's crystal-tree sub-feature.

**Traversal** (A + B + Activities + Tech): grounded **walk 4 m/s / soft run 6.5 m/s** with terrain-follow camera and gentle head-bob is the tranquil default. **Glide** — leap from any height or ride a dandelion seed; **light gravity (~2.6 m/s², B)** makes a fall a luxurious leaf-drift, never a punishment — this is how you cross the vale calmly. **Wind-rivers** (visible mote-streams) carry you at **18–24 m/s** with zero cost for express crossings. The **Mere raft** (~2 m/s) for night drifting. Every woken **Beacon becomes a fast-travel node** — pick it on the Tab sky-map and streak there as a comet of light.

**Light & colour over the day.** Two truths from A drive the whole grade: **cool sleeps, warm wakes** (dormant Lumenmere is teal/slate/moon-silver; everything *you* wake reads warm against it), and **the sky is a clock you can push** (waking regions nudges the resting dawn-floor forward). Each region carries its identity *through* every phase via its own `uFogColor`/`uAmbient` tint, cross-faded as you walk — the Emberfen stays the warm orange island at midnight, the Mere always reads as "doubled sky," and emissive **punches through fog after the fog term** (`lit += albedo*emissive`, secondPassFrag.frag:119), so every light you wake stays a hard bright point at any hour.

---

## 3. THE SKY

Adopt the **Sky doc wholesale.** The sky is already a function in the deferred lighting pass (`skyColor(ray)`, secondPassFrag.frag:64); we extend that seam.

**The clock (calm pacing).** One continuous `mDayPhase` 0→1 on a **~18-minute cycle that leans nocturnal** (B + Activities: night is the soul, dawn is the reward). Sun rides a tilted arc (leaning −Z so it never hits exact zenith → longer golden hour). Default start in late dusk, sliding to deep night, then dawn. The existing `mDawn` win-mechanic is redefined as *easing `mDayPhase → 0.25` (sunrise)* — so the payoff moment is the **real sky turning gold**, zero special-case art.

**Palettes (LINEAR RGB keyframes, Sky doc §2).** Seven keys from Midnight `top 0.006,0.012,0.030` to Midday `sunlight 1.70,1.55,1.35`, sampled on CPU from **sun elevation + a rising/falling flag** (sunrise cooler/pinker `1.25,0.62,0.34`; sunset redder `1.35,0.50,0.26`). Twilight values exceed 1.0 deliberately to drive the existing bloom. Night keys match current `Window.h` defaults so nothing regresses.

**The single highest-impact change — drive the WORLD from the sky.** There is **no directional light today** (only point-lights + flat `uAmbient`). Add **one directional term** to the lighting loop with a soft-wrap so shadowed sides never go pure black (no shadow maps):
```glsl
float wrap = ndl*0.85 + 0.15;
lit += uSunlight * (wrap*albedo + sd);   // ~6 ALU/pixel
```
This is what makes a sunset *feel* like a sunset on the trees. The lantern/firefly/mushroom point-lights stay untouched, so the cosy local glow that defines Ilo still dominates at night.

**The sky shader — `renderSky(ray, eye)`** (Sky doc §4), a self-contained refactor callable from (a) the background branch, (b) the sky pass, (c) water reflection. Composed of: art-directed **gradient + Henyey-Greenstein sun-glow** (Preetham-lite) → **fbm clouds** on an analytic plane with silver-lining derivative lighting → **galaxy band** → **twinkling stars** (keep existing) → **rare shooting stars** (~1 per 12 s per lane) → **aurora** (3 stacked-plane fbm curtains, green→magenta, night-only) → **sun/moon discs** (HDR ×6 → bloom carries them). Aurora intensity scales by night *and* by how much of the vale you've bloomed — **the more alive the valley, the more the heavens dance** (A).

**The uniform contract** (Sky doc §6) — KEEP `uTime, invViewProj, eyePos, uSkyTop, uSkyHorizon, uMoonDir/Color/Size, uAmbient, uFogColor/Density/HeightFalloff/BaseY`; **repurpose** `uStarFade = mNightW`; **ADD** `uSunDir, uSunlight, uSunDiscColor, uSunDiscSize, uHorizonGlow, uCloudCoverage, uCloudWind, uCloudLit, uCloudShadow, uAuroraStrength, uGalaxyStrength, uSkyTex`. All uploaded in `renderLightingPass`, replacing the old hard-coded dawn-lerp (Window.cpp:~553).

**Calm tuning (non-negotiable):** `mDayLength≈1080s`, `uCloudWind≈0.006`, aurora scroll `0.025–0.35`, meteors rare. Motion felt, not watched. `pow(ray.y,0.42)` fattens the saturated horizon band; RGBA16F + the existing composite dither kill banding.

---

## 4. WHAT YOU DO

**The fate of the warmth/fail-state — DECIDED: delete the loss state.** Adopt the Activities doc's call exactly. Remove `GameState::Lost` entirely. Rename fuel → **Glow (0–100)**. Glow **never kills you**: at 0 the lantern simply shrinks to a 3 m personal halo and Pulse recharges slower (1.5→4 s) — that's the whole penalty. It **drains gently (~1.5/s) only in the genuine dark wilds** (>20 m from anything bloomed), **0 inside bloomed regions**, and **refills near radiance** (+6/s within 12 m of any bloom) and in gulps from acts you enjoy. Why not purely cosmetic: tranquil ≠ no stakes; tranquil = *no harsh fail*. The faint loneliness of the dark and the sanctuary of the lit world *is* the emotional arc — and it self-balances, because the more you bloom, the more refuel exists, so the world itself lets you push farther. Engine map: keep the `packLights` lantern math (`radius 3+9*glow`, `intensity 0.6+2.4*glow`), keep the fuel-vignette as "light = expanded vision," delete the `→ Lost` branch (Window.cpp:533).

**The tranquil loop** (Activities §1): *spot a glimmer → drift/glide to it → Pulse / Channel / Plant → the patch blooms and stays lit → the next glimmer is now visible because you just lit the path to it.* Verbs: **Pulse** (LMB tap — the `Pulse` expanding light, radius 1→14 m), **Channel** (hold LMB — pour a sustained beam), **Plant** (Q), **Call** (E — gather wisps / coax creatures), **Rest** (F).

**The ten activities** (each: interaction → feedback/reward → how the world blooms). "Bloom" everywhere = ramp an instance's `tintEmissive.a` 0→2.5 (free in the G-buffer), warm its albedo, and promote to a real light only if it's among the nearest ~40.

1. **Wake the wildflowers (bloom-trails).** Walk Meadowlight → flowers open in a glowing, persistent trail behind your feet. *World:* a luminous breadcrumb of everywhere you've wandered; the meadow's Bloom and ambient rise. *(A/B/C.)*
2. **Gather drifting motes.** Drift near fireflies-reframed → they refill Glow and **form a swarm that follows you**, redistributable into a dim plant or beacon to pollinate it. *World:* roaming light you carry and re-gift. *(Activities + A; reuses `FireflySystem`.)*
3. **Wake a Heartwood / Beacon** (the region anchor — the biggest act). Channel ~3 s into a sleeping ancient tree or obelisk; its emissive ramps 0.3→4.5, then a **region-wide shockwave Pulse (radius 1→60 m)** sweeps out, blooming every flower/tree it passes in a **staggered travelling wave**, fog recedes, **a constellation ignites overhead**, dawn cracks locally. *World:* a whole biome goes grey→glowing in one sweep; becomes a **fast-travel node + refuel sanctuary**. *(A Heartwood + Activities Beacon + C shrine.)*
4. **Ignite the Cascades.** Harvest a light-spore from a Mistwood mushroom, glide to a waterfall, release → the ribbon ignites turquoise crest-to-pool. *World:* lit water pours downstream into the Mere, lilypads light along the current, fireflies spawn at the pools. *(A, unique.)*
5. **Sow seed-lights / plant lantern-saplings.** Q plants a `cone`/`cylinder` flower that scales 0→full with an emissive ramp, or a sapling that **grows over the session** into a glowing waypoint tree. *World:* you turn dark meadows into rivers of glowing flora — your own marks, permanent. *(Activities + C.)*
6. **Float the Mere & grow the Wellheart.** Board the raft at night (mirror-sky + a gathering **murmuration**), release a paper-lantern, and deposit gathered light into the central **orrery** — each deposit/constellation adds a ring, satellite, colour, and **raises its light-column, visible from anywhere in the basin**. *World:* the sum of your whole journey, turning slowly at the heart. *(A raft/murmuration + B Wellheart + C Heartwell.)*
7. **Weave constellations (sky-authoring).** At a Reach star-lens, pin 3–6 scattered seed-stars; a line traces across the **real sky**, its **fallen twin ignites across the ground** in a chain, an **aurora in its colour unfurls**, and you earn a gentle **boon** (longer glide / brighter lantern / faster bloom — the only "stat"). *World:* the heavens permanently fill in as a record of your play. *(B core loop + C orrery + A star-lens.)*
8. **Play the aurora** on the Steppe. Glide through a curtain to bend its colour and chime; linger to make it **pool and spill across the plain**, igniting frost-grass for tens of metres. *World:* a Steppe you've "played" stays luminous. *(B, unique.)*
9. **Befriend the light-fauna.** Approach gently or **Call** (E); a deer's markings ignite, a heart-light blooms, it trots alongside and **leads you to hidden seed-stars and pools** you missed. Distant **sky-creatures** appear as Radiance rises. *World:* a living, inhabited world that notices it's being cared for. *(All three + `DeerAgent`.)*
10. **Rest, Observe & remember.** F eases the camera into a still seated framing, hides the HUD, and **accelerates time** so you watch the whole cycle roll night→gold over the valley you woke; an auto-capture fills a **Field Journal** (`Screenshot.h`). *World:* nothing to do but witness — and **the vale persists between sessions** ("it remembers you"). *(All three.)*

**Cuts, named:** Activities' *ring-the-chimes* folds into the Mistwood crystal-trees (#3 mechanic); *cast light-bridges* and *ride wind-currents* become traversal sub-systems, not headline acts.

**Meta-progression — dim → radiant.** One global **Radiance 0→100%** (Activities §6) aggregating every act, surfaced as a quiet `%` and a growth-ring on the central world-tree, plus a per-region **Bloom 0→1** (A) as the local heartbeat. Radiance drives the world's whole look through uniforms already passed each frame: **ambient** near-black→warm, **fog density 0.030→0.012** (the world literally *opens up* — distant vistas you couldn't see at the start appear at the end), **resting dawn-floor** rises (B: "colour temperature *is* your progress bar"), aurora/constellations/sky-creatures intensify. **Soft completion — "The Long Dawn":** at 100%, a world-wide bloom event (every instance flares in unison, the sky fills, the Wellheart blazes), then the world **settles permanently radiant — no ending screen, no menu kick**. A **new seed appears at the Mere**; planting it reveals a fresh `proc::`-seeded region. There is always more world, and what you lit stays lit.

---

## 5. TECH ARCHITECTURE

Adopt the **Tech doc** as ground truth. Verified against code: `mMaxY=6` (Camera.h:18), `HARD_BOUND=24` (Window.cpp:20), far plane `200.0f` in two places (Window.cpp:660,698), `lights[128]`+`uLightCount` UBO (Render.h:127), emissive-after-fog (secondPassFrag.frag:119), pass order `geometry→lighting→bloom→composite→hud` (Window.cpp:848). The calls:

**Precision (do first, non-negotiable). Floating origin.** RT0 stores world position in fp16 → at 1000 m the grid snaps to ~0.5 m and lighting shimmers. Maintain `glm::dvec3 mWorldOrigin` snapped to a 128 m grid (updated only on cell crossing), add `uOriginOffset` to the geometry/terrain/water vertex shaders, build `mView` and upload all light positions relative to origin. RT0 then stays within a few hundred metres of origin everywhere. (Depth-reconstruction to drop RT0 entirely = documented later bandwidth win; water shoreline currently *wants* RT0, so keep it for now.)

**Terrain.** Finite chunked heightfield, **1792 m (28×28 × 64 m chunks)**, perimeter ridge in the outer ring. Heightmap baked once on CPU via `proc::Rng` fbm into a **1024² R16F texture** (~2 m/texel, ~2 MB GPU; keep the CPU array resident ~4 MB for height/normal/collision queries). Mesh = **4 shared flat LOD lattices + vertex-texture-fetch** (`textureLod` — no derivatives in VS); a chunk draw = bind lattice + set `uChunkOriginXZ`. Normals via 4-tap central differences in the FS. **LOD rings** 96/192/384/700 m (64²/32²/16²/8² quads); **skirts, not stitching**, for cracks. Albedo/material by **height-band × slope** (grass on flat, rock on steep, snow on ridge) + cheap noise. New `terrain.vert/.frag`. **~120–300 k terrain tris/frame** — trivial.

**Vegetation at scale.** Refactor `Player`/`Firefly`/`Mushroom`/`Props` into one **`InstancedField`** with an extended instance layout — add `loc6 vec4 (scaleXYZ, lodFade)` alongside the existing `loc4 pos`+`loc5 tintEmissive` and a per-instance **`yaw`** (kills the current cloned-orientation look from the shared `model` uniform). **Per-chunk static instance buffers, built on chunk-enter and freed on chunk-leave — not per frame** (budget ≤1–2 builds/frame to avoid hitches); per-frame work is just chunk-AABB frustum cull + draw. Density-map scatter snapped to terrain height. **LOD/billboard cross-fade via hashed dithered discard** (`if(hash(gl_FragCoord.xy) > lodFade) discard;`) — no blending in a deferred G-buffer, no sorting, no popping. Realistic visible budget **~300 k vegetation tris**; total well under 600 k/frame.

**Water.** Flat plane per `WaterBody`, **dedicated pass between lighting and bloom** (so bloom+ACES still apply), AABB-scissored. Procedural animated normals (2–3 scrolling sines) — no Gerstner v1. **Fresnel mix:** reflection = **`renderSky(reflectedRay)`** (the same analytic sky → clouds drift in it, aurora ripples, moon glitters, dawn turns it molten gold — Sky doc §7); refraction = HDR scene tinted by depth; shoreline foam from RT0 position. Flag water in `oMtlProps.a` (currently reserved). The single most mesmerising-per-byte feature.

**Light budget (≤48 desktop / ≤24 lowspec).** Generalize `packLights` into a tiered **`LightSystem`**: **near ring** = individual lights as today; **mid ring** = merge a chunk's small emitters into ~1 centroid cluster light; **far ring** = **emissive-only** (glows + blooms in the G-buffer, casts no dynamic light — free glow at any distance). Score `intensity/(d²+1)`, `nth_element` to budget, lantern always kept. The fullscreen loop length × pixels is the real cost — capping count is what protects web framerate.

**Sky cost.** Full-res procedural sky over an open vista is fill-heavy (20–40 noise taps × ~0.5M px). **v1 call: half-res dedicated sky pass** (`skyFBO` half-res RGBA16F LINEAR, `sky.frag` reusing `renderSky`) → lighting background branch is one bilinear fetch of `uSkyTex`; quarters the heavy math, and the slight softening reads as pleasant bloom-glow. **Documented upgrade if framerate slips: a 256×128 lat-long sky LUT** (Tech §6) refreshed only when the clock moves meaningfully — cheaper still, since the sky is low-frequency (keep stars as a cheap procedural layer on top).

**New components:** `Terrain`, `World` (chunk/stream manager), `InstancedField`, `Sky`/`TimeOfDay` (+`skyBake.frag` if LUT), `Water`, `LightSystem`. **Refactors:** add `uOriginOffset`+`loc6` to `firstPassVertex.vert`; insert `renderWater()`; add `Camera` ground-follow + remove `HARD_BOUND` (→ soft fog-edge clamp) + raise `mMaxY` 6→~120; **far plane 200→~700, near 0.1→~0.4** (both perspective builds); retune fog for **aerial perspective** (lower density, tint toward `skyHorizon` so distant terrain melts into sky — *this is the vastness cue*); retire `makeGroveProps` soup → density scatter. Bump `CMakeLists` `-std=c++14→c++17`.

**New pass order:** `(0) sky bake on-demand → (1) geometry (terrain LOD chunks → OBJ meshes → instanced veg/props → mushrooms → fireflies) → (2) lighting (≤48 tiered lights + sky LUT sample + aerial fog) → (3) water (fresnel + sky reflection + shoreline) → (4) bloom → (5) composite (ACES+vignette+dither+gamma) → (6) HUD`.

**Risks + mitigations (and what won't hit web framerate → the cheaper fallback):**
- **fp16 RT0 precision** → floating origin (front-loaded as Phase 2).
- **Web fragment cost (the real ceiling)** → cap lights ≤48/≤24, tier+merge, half-res sky. **Fallback if it still slips: half-res lighting pass + upscale — the single biggest web win**, gated by the existing `ILO_LOWSPEC`/`mLowSpec`.
- **Full-res procedural sky over open vistas will not hold web framerate** → ship half-res sky from day one; LUT as the next cut.
- **Grass density / tree billboard distance** → halve on `mLowSpec`; quarter-res sky + 3-octave fbm + `#define` out clouds/galaxy on lowspec.
- **Draw calls** (chunk×field×LOD) → instancing (thousands/call), merge per-chunk fields, target <200 draws/frame.
- **Streaming hitches** → heightmap baked once at startup; ≤1–2 chunk builds/frame; deterministic scatter.
- **Cloned-looking instances** → `loc6` per-instance yaw/scale.
- **Memory** → free distant chunk buffers; cap resident chunks; `ALLOW_MEMORY_GROWTH` already on.

---

## 6. THE BUILD PLAN

Ten shippable, screenshotable phases. Beauty shows up in Phase 1; the scary precision and scale risk is front-loaded (Phases 2–5); the game stays playable throughout.

**Phase 1 — The Sky Awakens** *(safe foundation; the gorgeous, low-risk first step on the existing grove).*
Build: `Sky`/`TimeOfDay` driver (`mDayPhase`, palette sampler), refactor `skyColor`→`renderSky(ray,eye)` (gradient+Mie, clouds, galaxy, shooting stars, aurora, sun/moon discs), the **half-res `skyFBO`+`sky.frag`**, and the **one directional light term** (`uSunDir`/`uSunlight`) in `secondPassFrag.frag`. New: `src/Sky.{h,cpp}`, `shaders/sky.frag`; refactor `secondPassFrag.frag`, `renderLightingPass` uniform block.
**verify:** the existing grove runs a slow night→golden-dawn cycle, the sun rakes warm light across the trees, aurora + shooting stars overhead at night — and the old loop still plays unchanged.

**Phase 2 — Floating Origin & Unbounded Space** *(front-load the precision risk).*
Build: `glm::dvec3 mWorldOrigin` on a 128 m grid, `uOriginOffset` in vertex shaders, origin-relative view + light upload, **far plane 200→700 / near→0.4**, remove `HARD_BOUND` (→ soft fog-edge), raise `mMaxY`, add `Camera` ground-follow.
**verify:** fly 600 m out from spawn — lighting/specular/normals stay rock-stable (no fp16 shimmer), no invisible wall, and you can rise high above the old 6 m ceiling.

**Phase 3 — The Terrain** *(the bowl appears).*
Build: `src/Terrain.{h,cpp}`, `terrain.vert/.frag`, R16F heightmap (fbm caldera with a central lake basin + perimeter ridge), shared LOD lattices + VTF, skirts, height/slope colouring, chunk LOD rings; retune fog for aerial perspective + sky-horizon tint.
**verify:** a 1.5 km rolling caldera-bowl you can walk, distant terrain melting into sky-coloured fog, the rim ridge cut against the dawn — recognisably Lumenmere.

**Phase 4 — The Mere & Water** *(the first true wow).*
Build: `src/Water.{h,cpp}`, `water.{vert,frag}`, water pass between lighting and bloom, `oMtlProps.a` water flag, fresnel + `renderSky` reflection + RT0 shoreline; carve the central Mere; drop a placeholder Wellheart beacon.
**verify:** the "Mirror at Murmuration Hour" shot — the dead-still lake doubling the moon, aurora, and your lantern's gold glitter-trail.

**Phase 5 — Life at Scale** *(the world fills with light, at framerate).*
Build: `InstancedField` refactor (`loc6` yaw/scale, per-chunk static buffers, dithered LOD fade), `World` chunk manager, density-scatter conifers/rocks/grass/flowers/mushrooms; `LightSystem` tiered ≤48/≤24 budget.
**verify:** forested slopes and meadows with thousands of *non-cloned* instances holding 60 fps in-browser, lights capped, zero popping — the Mistwood reads as a vast grove.

**Phase 6 — The Bloom Loop** *(the gameplay pivot).*
Build: delete `GameState::Lost`, soften fuel→**Glow**, the Pulse/Channel/Plant/Call verbs, bloom = emissive ramp on instances, the Pulse travelling-wave, per-region Bloom + global **Radiance** driving fog/ambient/sky. Wildflower trails, mote-gather, seed-sowing. HUD: Glow ring + `RADIANCE %`, drop the Lost screen.
**verify:** walk the meadow — flowers ignite in your wake and *stay lit*; a Pulse blooms one spectacularly; Radiance % rises and the fog visibly opens.

**Phase 7 — Region Anchors** *(the dramatic transformations).*
Build: Channel-to-wake **Heartwood/Beacon** with the region-wide shockwave + staggered chain-bloom, fast-travel nodes, the **Wellheart orrery** growing its basin-wide light-column, and ignite-the-Cascades.
**verify:** the "Heartwood Bloom" shot — a shockwave ring races across the forest floor igniting every seeded flower in sequence, the fog flushing cold-blue to dawn-rose.

**Phase 8 — The Sky You Author** *(close the world↔sky loop).*
Build: **constellation weaving** at the Reach (pin seed-stars → real-sky line + fallen-twin ground ignite + aurora unfurl + boon), **playable aurora** on the Steppe, sky-creatures keyed to Radiance.
**verify:** complete a constellation — a line draws across the live sky, an aurora blooms in its colour, its ground-twin lights across the wedge; the sunrise-from-the-Reach panorama over the fog-sea.

**Phase 9 — Inhabitants & Traversal Polish** *(the world feels alive and a joy to cross).*
Build: befriend deer/light-fauna (`DeerAgent` — trust, follow, lead-to-hidden), wind-rivers + glide tuning + light gravity, the Mere raft, the fast-travel comet move.
**verify:** a deer trusts you and leads you to a hidden pool; ride a wind-river across the vale; glide down off the Reach like a falling leaf.

**Phase 10 — Rest, Memory & The Long Dawn** *(the soft, endless ending).*
Build: Rest/Observe mode + photo **Field Journal** (`Screenshot.h`), time-scrub at rest spots, session **persistence/save** ("the vale remembers"), the **Long Dawn** world-wide bloom + permanent radiant settle + **new-seed-at-the-Mere** endless hook, the dawn-chorus event.
**verify:** sit at the Reach, scrub to dawn, watch the whole bloomed valley flare in unison under a golden sky — then reload and find it still glowing, with a new seed waiting at the water.
