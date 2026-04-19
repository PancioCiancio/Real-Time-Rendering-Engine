// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.

#include <SDL2/SDL.h>

#include "vk_renderer.h"

int main()
{
	// Init window
	// Vulkan rendering support and resizeable
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Orda", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	// Create the vulkan renderer.
	Renderer renderer = {};
	renderer.Load(window);

	Uint64 now = SDL_GetPerformanceCounter();
	Uint64 last = 0;

	constexpr int kMaxFps = 60;
	constexpr double kTargetFrameTime = 1000.0 / kMaxFps;	// milliseconds

	bool running = true;

	while (running)
	{
		last = now;
		now = SDL_GetPerformanceCounter();
		double delta_time = static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceCounter());

		SDL_Event event = {};
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT: running = false; break;
				default: break;
			}
		}

		renderer.Update(delta_time);

		// Slow down the process if running faster than the target frame.
		if (delta_time < kTargetFrameTime)
		{
			SDL_Delay(static_cast<Uint32>(kTargetFrameTime - delta_time));
		}
	}

	renderer.Unload();

	return 0;
}