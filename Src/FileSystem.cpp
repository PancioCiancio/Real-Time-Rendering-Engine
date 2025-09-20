//
// Created by apant on 07/07/2025.
//

#include "FileSystem.h"

#include <fstream>

std::vector<char> FileSystem::ReadFile(const char* path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file");
	}

	const std::streamsize file_size = static_cast<std::streamsize>(file.tellg());
	std::vector<char>     output_file(file_size);

	file.seekg(0);
	file.read(&output_file[0], file_size);

	file.close();

	return output_file;
}