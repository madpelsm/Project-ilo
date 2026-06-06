#pragma once
// Time-of-day + atmosphere driver. Given a continuous day phase (0..1) it produces
// all the sky/lighting parameters — sun & moon directions, the graded palette, a
// directional sunlight term, fog, aurora/star/cloud strengths — that the sky pass
// and the deferred lighting pass consume. Calm, art-directed keyframes.
//
//   dayPhase: 0.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset.
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

struct Sky {
    // ---- outputs (filled by update) ----
    glm::vec3 sunDir = glm::vec3(0, -1, 0);
    glm::vec3 moonDir = glm::vec3(0, 1, 0);
    glm::vec3 sunlight = glm::vec3(0);    // directional radiance (colour * intensity)
    glm::vec3 sunDiscColor = glm::vec3(1.0f, 0.9f, 0.7f);
    glm::vec3 skyTop = glm::vec3(0.008f, 0.015f, 0.05f);
    glm::vec3 skyHorizon = glm::vec3(0.03f, 0.05f, 0.09f);
    glm::vec3 horizonGlow = glm::vec3(0);  // warm twilight band near the sun
    glm::vec3 ambient = glm::vec3(0.012f, 0.016f, 0.028f);
    glm::vec3 fogColor = glm::vec3(0.02f, 0.035f, 0.06f);
    glm::vec3 moonColor = glm::vec3(0.55f, 0.62f, 0.85f);
    glm::vec3 sunlightTint = glm::vec3(1.0f); // for cloud lighting
    float fogDensity = 0.035f;
    float starFade = 1.0f;
    float auroraStrength = 0.0f;
    float galaxyStrength = 1.0f;
    float cloudCoverage = 0.42f;
    float sunDiscSize = 0.015f;
    float moonSize = 0.07f;
    float nightAmount = 1.0f; // 1 deep night .. 0 full day
    glm::vec3 mistColor = glm::vec3(0.62f, 0.70f, 0.82f); // pooling ground mist
    float mistDensity = 0.0f;                              // distance falloff rate (~0 at noon)

    static glm::vec3 mix3(const glm::vec3 &a, const glm::vec3 &b, float t) {
        t = std::max(0.0f, std::min(1.0f, t));
        return a + (b - a) * t;
    }
    static float smooth(float a, float b, float x) {
        float t = std::max(0.0f, std::min(1.0f, (x - a) / (b - a)));
        return t * t * (3.0f - 2.0f * t);
    }

    void update(float dayPhase, float radiance01 = 0.0f) {
        const float TAU = 6.2831853f;
        float ang = TAU * (dayPhase - 0.25f);
        sunDir = glm::normalize(glm::vec3(std::cos(ang), std::sin(ang), -0.35f));
        moonDir = -sunDir;
        float e = sunDir.y; // sun elevation, ~ -0.94 .. 0.94
        bool rising = std::cos(ang) > 0.0f;

        float dayAmt = smooth(-0.10f, 0.32f, e);          // 0 night .. 1 day
        float twilight = std::exp(-(e / 0.13f) * (e / 0.13f)); // peaks at the horizon
        nightAmount = 1.0f - dayAmt;

        // Sky gradient.
        glm::vec3 nightTop(0.008f, 0.015f, 0.05f), dayTop(0.14f, 0.34f, 0.78f);
        glm::vec3 nightHoriz(0.03f, 0.05f, 0.09f), dayHoriz(0.58f, 0.74f, 0.96f);
        skyTop = mix3(nightTop, dayTop, dayAmt);
        skyHorizon = mix3(nightHoriz, dayHoriz, dayAmt);

        // Warm twilight glow along the horizon toward the sun (pinker rising, redder setting).
        glm::vec3 sunrise(1.30f, 0.62f, 0.42f), sunset(1.45f, 0.46f, 0.24f);
        horizonGlow = (rising ? sunrise : sunset) * twilight * 1.1f;

        // Directional sunlight: warm at the horizon, bright/white high up, ~0 below.
        float sunUp = smooth(-0.05f, 0.30f, e);
        glm::vec3 sunWarm(1.35f, 0.58f, 0.28f), sunWhite(1.65f, 1.52f, 1.34f);
        sunDiscColor = mix3(sunWarm, sunWhite, smooth(0.04f, 0.40f, e));
        sunlight = sunDiscColor * (sunUp * 1.15f);
        sunlightTint = mix3(glm::vec3(1.0f, 0.85f, 0.7f), glm::vec3(1.0f), smooth(0.04f, 0.4f, e));

        // Fog + ambient follow the day.
        glm::vec3 nightFog(0.02f, 0.035f, 0.06f), dayFog(0.55f, 0.66f, 0.80f);
        fogColor = mix3(nightFog, dayFog, dayAmt) + horizonGlow * 0.10f;
        glm::vec3 nightAmb(0.012f, 0.016f, 0.028f), dayAmb(0.24f, 0.27f, 0.32f);
        ambient = mix3(nightAmb, dayAmb, dayAmt);
        // Gentle aerial-perspective fog: distant terrain melts into the sky over
        // hundreds of metres (this is the cue that sells the vastness).
        fogDensity = mix3(glm::vec3(0.0017f), glm::vec3(0.0011f), dayAmt).x;

        // Night-only flourishes.
        starFade = 1.0f - smooth(-0.16f, 0.04f, e);
        moonColor = glm::vec3(0.55f, 0.62f, 0.85f) * (0.25f + 0.75f * nightAmount);
        auroraStrength = nightAmount * (0.35f + 0.65f * std::max(0.0f, std::min(1.0f, radiance01)));
        galaxyStrength = starFade;
        cloudCoverage = 0.40f + 0.10f * dayAmt;

        // Radiance (the player's progress) opens the world dim -> radiant: a warmer
        // ambient lift, thinner fog so distant vistas appear, and a brighter resting sky.
        float rad = std::max(0.0f, std::min(1.0f, radiance01));
        ambient += glm::vec3(0.045f, 0.05f, 0.055f) * rad;
        fogDensity *= (1.0f - 0.45f * rad);
        skyHorizon += glm::vec3(0.05f, 0.045f, 0.06f) * rad * (0.4f + 0.6f * nightAmount);

        // Volumetric ground-mist: silver air pooling at dawn/dusk/night, nearly gone at
        // noon (twilight->0), thinned as the vale brightens. Catches the horizon glow.
        mistDensity = 0.0065f * (twilight + 0.30f * nightAmount + 0.05f) * (1.0f - 0.4f * rad);
        glm::vec3 mistNight(0.60f, 0.68f, 0.82f), mistDay(0.80f, 0.83f, 0.88f);
        mistColor = mix3(mistNight, mistDay, dayAmt) + horizonGlow * 0.15f;
    }
};
