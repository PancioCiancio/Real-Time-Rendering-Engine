#include <SDL2/SDL.h>
#include "graphics.h"

int main()
{
	// Init window
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Orda", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	// Init graphics
	Graphics::Initialize(window);

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

		Graphics::Update(delta_time);

		// Slow down the process if running faster than the target frame.
		if (delta_time < kTargetFrameTime)
		{
			SDL_Delay(static_cast<Uint32>(kTargetFrameTime - delta_time));
		}
	}

	Graphics::Shutdown();

	return 0;
}