#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inMtlProps;          // shininess, specStrength, ambient
layout (location = 4) in vec3 instanceOffset;
layout (location = 5) in vec4 instanceTintEmissive; // rgb = colour tint, a = emissive strength
layout (location = 6) in vec4 instanceXform;        // x=scale, y=yaw, z=windStiffness, w=phase

uniform mat4 persp;
uniform mat4 model;
uniform mat4 view;
uniform mat3 normalMatrix; // transpose(inverse(mat3(model))), computed once on the CPU
uniform vec3 uOriginOffset; // floating-origin: G-buffer stores positions relative to this
uniform vec2 uWind;         // wind direction * strength
uniform vec3 uPlayerPos;    // for proximity bloom: glowing things brighten as you near them
uniform vec4 uWake[8];      // woken Heartwoods: xy = centre XZ, z = ignite time
uniform int uWakeCount;
uniform float time;
uniform float grassWave;   // 1.0 for the forest (animate low verts), 0.0 otherwise
uniform float uBloom;      // global unison flare (0 in play; rises during The Long Dawn)

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vAlbedo;
out vec3 vMtlProps;
out float vEmissive;

void main() {
    vec3 vertPos = inPosition;
    // Gentle grass/undergrowth sway: only the lowest geometry, only the forest.
    if (grassWave > 0.5 && inPosition.y < 0.1) {
        vertPos.y += 0.2 * sin(time + inPosition.x + inPosition.z);
    }

    // Per-instance scale + yaw (loc6 defaults to scale 1 / no rotation for non-fields).
    float yaw = instanceXform.y;
    float cy = cos(yaw), sy = sin(yaw);
    vec3 p = vertPos * instanceXform.x;
    p = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);

    // Wind: a cantilever bend. A gust field that sweeps ALONG the wind direction makes
    // neighbours lean together in travelling waves; the bend grows with height; the tip
    // dips slightly to conserve length so blades arc rather than stretch.
    float stiff = instanceXform.z;
    float ph = instanceXform.w;
    vec2 wdir = length(uWind) > 1e-4 ? normalize(uWind) : vec2(1.0, 0.0);
    float str = length(uWind);
    float h = max(vertPos.y, 0.0) * instanceXform.x;
    float along = dot(instanceOffset.xz, wdir);
    float gust = sin(along * 0.05 - time * 0.9 + ph) * 0.60 +
                 sin(along * 0.17 - time * 1.7 + ph * 1.7) * 0.30 +
                 sin(time * 3.1 + ph * 2.3) * 0.10;
    float bend = stiff * str * h * (0.55 + 0.45 * gust);
    p.x += wdir.x * bend;
    p.z += wdir.y * bend;
    p.y -= 0.5 * bend * bend / max(h, 0.05);

    vec4 worldPos = model * vec4(p + instanceOffset, 1.0);
    gl_Position = persp * view * worldPos;

    // Store position relative to a nearby snapped origin so the fp16 G-buffer stays
    // precise no matter how far the player roams from the world centre.
    vWorldPos = worldPos.xyz - uOriginOffset;
    vec3 nrm = vec3(cy * inNormal.x + sy * inNormal.z, inNormal.y, -sy * inNormal.x + cy * inNormal.z);
    vNormal = normalMatrix * nrm;
    vAlbedo = inColor * instanceTintEmissive.rgb;
    vMtlProps = inMtlProps;
    // Proximity bloom: emissive flora flares as the Lampbearer draws near — the world
    // lights up around you as you wander through it.
    float prox = 1.0 - smoothstep(0.0, 11.0, distance(worldPos.xz, uPlayerPos.xz));

    // Wake shockwaves: when a Heartwood is woken, an expanding ring of bloom sweeps
    // out from it, leaving the region permanently brighter behind the front.
    float wake = 0.0;
    for (int i = 0; i < uWakeCount; i++) {
        // A saved/loaded beacon pins a FROZEN region radius in .w (no replayed sweep);
        // a live wake leaves .w = 0 and expands from its ignite time.
        float radius = uWake[i].w > 0.5 ? uWake[i].w : (time - uWake[i].z) * 32.0;
        float d = distance(worldPos.xz, uWake[i].xy);
        if (radius > d)
            wake = max(wake, 0.5 + 1.6 * smoothstep(10.0, 0.0, abs(d - radius)));
    }

    // + uBloom: one global term flares every geometry-pass instance in unison when the
    // Long Dawn breaks, then settles to a permanent low glow floor.
    vEmissive = instanceTintEmissive.a * (1.0 + 1.6 * prox + wake) + uBloom;
}
