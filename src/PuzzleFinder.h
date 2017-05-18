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

#ifndef PUZZLEFINDER_H
#define PUZZLEFINDER_H

#include <utility>
#include <vector>
#include "Geometry.h"

struct Image;

class PuzzleFinder
{
	public:
		bool Find(const unsigned int targetWidth,const unsigned int targetHeight,const Image& houghTransformFrame,std::vector<Point>& puzzlePoints);

		//Internal use only variables made public to ease debugging.
		std::vector<Line> lines; //All lines found.
		std::vector<std::vector<Line>> lineClusters; //Lines grouped by theta.
		std::vector<std::vector<Line>> possiblePuzzleLineClusters; //Cluster lines that are evenly spaced.
		std::vector<std::pair<std::vector<Line>,std::vector<Line>>> puzzleLines; //Pairs of cluster lines that are PI/2 radians apart from each other.
};

#endif

