#pragma once

#include <SDL2/SDL.h>

/// @brief Renderer/graphcis interface
namespace Graphics
{
    void Initialize(SDL_Window* window);
    void Update(double deltaTime);
    void Shutdown();
}