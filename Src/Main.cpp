#include "Renderer.h"

int main()
{
	Renderer::Renderer renderer = {};
	renderer.Init();
	renderer.Update(0.0);
	renderer.Teardown();

	return 0;
}