#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;
in vec3 Tangent;
in vec3 Bitangent;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_normal;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float ambientStrength;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // PCF filter (stejny kod jako predtim)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(Normal, normalize(lightPos - FragPos))), 0.0005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0).xy;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    // Vypocet normaly z normal mapy
    vec3 normal = texture(texture_normal, TexCoords).rgb;
    normal = normal * 2.0 - 1.0;

    // Vytvoreni TBN matice pro transformaci do tangentoveho prostoru
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    vec3 B = normalize(Bitangent);
    mat3 TBN = transpose(mat3(T, B, N));

    // Transformace normaly a svetelne pozice do tangentoveho prostoru
    vec3 lightDir = TBN * normalize(lightPos - FragPos);
    vec3 viewDir = TBN * normalize(viewPos - FragPos);
    vec3 fragNormal = TBN * normalize(normal);

    // Osvětlení v tangentovém prostoru
    vec3 ambient = ambientStrength * lightColor * texture(texture_diffuse, TexCoords).rgb;
    float shadow = ShadowCalculation(FragPosLightSpace);

    vec3 lighting = vec3(0.0);
    if (shadow < 1.0)
    {
        // Difuzni
        float diff = max(dot(fragNormal, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Spekularni
        vec3 reflectDir = reflect(-lightDir, fragNormal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = spec * lightColor;

        lighting = (diffuse + specular) * texture(texture_diffuse, TexCoords).rgb;
    }

    vec3 result = ambient + lighting;

    FragColor = vec4(result, 1.0);
}
