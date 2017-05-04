#ifndef IMAGE_H
#define IMAGE_H

#include <vector>

struct Image
{
	unsigned int width;
	unsigned int height;
	std::vector<unsigned char> data;
};

#endif

