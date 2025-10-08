#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform samplerCube environmentMap;

// Uber Parametry:
uniform int renderMode;            // 0: Reflexe/Sklo (default), 1: Cista Barva (difuzni)
uniform vec3 materialColor;        // Barva pro difuzni nebo tonovani reflexe/skla
uniform float alpha;               // Globalni pruhlednost (pro Blending)

// Reflexe/Sklo Parametry (Mode 0):
uniform float refractionIndex;     // Index lomu (1.0/IOR)
uniform float fresnelPower;        // Mocnitel pro Schlickovu aproximaci (typicky 5.0)
uniform float reflectionStrength;  // Sila odrazu (mix)

void main()
{
    vec3 N = normalize(Normal);
    vec3 I = normalize(Position - cameraPos);

    vec3 finalColor = vec3(0.0);
    float finalAlpha = alpha;

    if (renderMode == 1) // CISTA BARVA (Difuzni/Matna)
    {
        finalColor = materialColor;
    }
    else // renderMode == 0 (Reflexe/Sklo/Chrom)
    {
        // --- 1. REFLEXE (Odraz) ---
        vec3 reflectedRay = reflect(I, N);
        vec3 reflectedColor = texture(environmentMap, reflectedRay).rgb;

        // --- 2. FRESNELŮV EFEKT ---
        float R0 = 0.04;
        float fresnel = R0 + (1.0 - R0) * pow(1.0 - max(0.0, dot(-I, N)), fresnelPower);

        // Finalni mix faktor, ktery bere v potaz uzivatelskou Reflection Strength
        float finalMix = mix(fresnel, reflectionStrength, 0.5);

        // ----------------------------------------------------
        // KRITICKÁ LOGIKA PRO CHROM/KOVOVÉ MATERIÁLY
        // ----------------------------------------------------

        if (alpha >= 0.99 && reflectionStrength >= 0.99) // Čistý CHROME / KOV
        {
            // Plná, čistá reflexe.
            finalColor = reflectedColor;
        }
        else // SKLO / Tónovaný reflexní dielektrikum
        {
            // --- 3. REFRAKCE (Lámání světla) ---
            vec3 refractedRay = refract(I, N, refractionIndex);

            // Zpracování totalniho odrazu
            if (dot(refractedRay, refractedRay) == 0.0) {
                refractedRay = reflect(I, N);
            } else {
                refractedRay = normalize(refractedRay);
            }
            vec3 refractedColor = texture(environmentMap, refractedRay).rgb;

            // Smichani lomu (refraction) a odrazu (reflection) pomoci Fresnelova jevu
            finalColor = mix(refractedColor, reflectedColor, finalMix);
        }

        // Aplikace barvy pro tonovani (pro chrom i sklo)
        finalColor *= materialColor;
    }

    // Tonemapping a Gamma korekce (aplikuje se vzdy)
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0/2.2));

    FragColor = vec4(finalColor, finalAlpha);
}
