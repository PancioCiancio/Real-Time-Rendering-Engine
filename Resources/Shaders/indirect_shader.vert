// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#version 460

layout(set = 0, binding = 0) uniform PerFrameData {
    mat4 view;
    mat4 proj;
} ubo;

layout(std140, set = 0, binding = 1) readonly buffer InstanceData {
    mat4 modelMatrices[];
} ssbo;

layout(location = 0) in vec3 positions;
layout(location = 1) in vec3 normals;
layout(location = 2) in vec4 colors;
layout(location = 0) out vec4 fragColor;

void main() {
    mat4 model = ssbo.modelMatrices[gl_InstanceIndex];

    gl_Position = ubo.proj * ubo.view * model * vec4(positions, 1.0);
    fragColor = colors * clamp(dot(normals, positions + vec3(-0.0, 100000.0, 100.0)), 0.1, .6);
    fragColor.r = fract(gl_InstanceIndex / 10.0);
}