// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#version 460
#extension GL_EXT_nonuniform_qualifier : require    // required for bindless textures

layout(binding = 2) uniform sampler2D bindless_textures[];

layout(location = 0) in flat uint i_texture_id; // flat is required for integer interpolants
layout(location = 1) in vec2 i_uv;

layout(location = 0) out vec4 o_color;

void main() {
    o_color = texture(bindless_textures[nonuniformEXT(i_texture_id)], i_uv);
}