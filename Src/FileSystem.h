//
// Created by apant on 07/07/2025.
//

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <vector>

/// Not instantiable class.
class FileSystem
{
public:
	// Delete constructor. This class is not instantiable.
	FileSystem() = delete;

	/// Read the file at the given path and copy the content into the given ptr.
	/// @param path			path to the file to read.
	/// @return the copy of the content.
	///
	/// @warning 	Is it better to provide a ptr as parameter to fill or return the string?
	///				Since we are low level, I'd prefer to provide the ptr. Will see...
	static std::vector<char> ReadFile(const char* path);
};

#endif //FILESYSTEM_H
