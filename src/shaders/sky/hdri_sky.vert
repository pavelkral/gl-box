#version 330 core
layout(location=0) in vec3 aPos;
layout(location=5) in ivec4 aBoneIDs;
layout(location=6) in vec4 aWeights;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;
uniform mat4 uBones[100];

void main() {
    mat4 skinMat = mat4(0.0);
    skinMat += aWeights.x * uBones[aBoneIDs.x];
    skinMat += aWeights.y * uBones[aBoneIDs.y];
    skinMat += aWeights.z * uBones[aBoneIDs.z];
    skinMat += aWeights.w * uBones[aBoneIDs.w];

    vec4 skinnedPos = skinMat * vec4(aPos,1.0);
    gl_Position = lightSpaceMatrix * model * skinnedPos;
}