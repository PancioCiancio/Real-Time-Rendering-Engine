// Orda - Proprietary
//
// Copyright (c) 2026 apant. All rights reserved.
//
// This file is part of Orda. Unauthorized copying. distribution,
// or modification of this file, via any medium, is strictly prohibited.


#include "Renderer.h"

int main()
{
	Renderer::Renderer renderer = {};
	renderer.Init();
	renderer.Update(0.0);
	renderer.Teardown();

	return 0;
}