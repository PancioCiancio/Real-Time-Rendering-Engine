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

struct ObjectData {
    uint texture_id;
    mat4 model_matrix;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceData {
    ObjectData objects[];
} ssbo;

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_uv;

layout(location = 0) out flat uint o_texture_id;
layout(location = 1) out vec2 o_uv;

void main() {
    mat4 model = ssbo.objects[gl_InstanceIndex].model_matrix;
    o_texture_id = ssbo.objects[gl_InstanceIndex].texture_id;
    o_uv = i_uv;
    gl_Position = ubo.proj * ubo.view * model * vec4(i_position, 1.0);
}