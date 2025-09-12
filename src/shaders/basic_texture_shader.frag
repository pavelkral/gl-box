#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float ambientStrength;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
       projCoords = projCoords * 0.5 + 0.5;

       float currentDepth = projCoords.z;

       if(currentDepth > 1.0)
           return 0.0;

       float bias = max(0.005 * (1.0 - dot(Normal, normalize(lightPos - FragPos))), 0.0005);

       float shadow = 0.0;
       vec2 texelSize = 1.0 / textureSize(shadowMap, 0).xy;
       for(int x = -1; x <= 1; ++x)
       {
           for(int y = -1; y <= 1; ++y)
           {
               float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
               shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
           }
       }
       shadow /= 9.0;

       return shadow;
}

void main()
{
    // Ambientní složka se počítá vždy, aby osvětlila celou scénu
    vec3 ambient = ambientStrength * lightColor * texture(diffuseTexture, TexCoords).rgb;

    // Vypočet stínového koeficientu pomocí PCF
    float shadow = ShadowCalculation(FragPosLightSpace);

    // Osvětlení (difúzní a spekulární) se aplikuje jen tehdy, když není fragment ve stínu
    vec3 lighting = vec3(0.0);
    if (shadow < 1.0)
    {
        // Difúzní osvětlení
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;

        // Spekulární osvětlení
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = spec * lightColor;

        lighting = (diffuse + specular) * texture(diffuseTexture, TexCoords).rgb;
    }

    vec3 result = ambient + lighting;

    FragColor = vec4(result, 1.0);
}
