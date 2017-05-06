// Copyright 2017 James Bendig. See the COPYRIGHT file at the top-level
// directory of this distribution.
//
// Licensed under:
//   the MIT license
//     <LICENSE-MIT or https://opensource.org/licenses/MIT>
//   or the Apache License, Version 2.0
//     <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>,
// at your option. This file may not be copied, modified, or distributed
// except according to those terms.

#ifndef IMAGE_H
#define IMAGE_H

#include <vector>

struct Image
{
	unsigned int width;
	unsigned int height;
	std::vector<unsigned char> data;

	Image()
		: width(0),
		  height(0),
		  data()
	{
	}
};

#endif

