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

#ifndef CACHEDPUZZLESOLVER_H
#define CACHEDPUZZLESOLVER_H

#include <map>
#include <vector>

class CachedPuzzleSolver
{
	public:
		CachedPuzzleSolver();

		bool Solve(const std::vector<unsigned char>& digits,std::vector<unsigned char>& solution);
		bool GetLastUsedSolution(std::vector<unsigned char>& solution) const;
	private:
		using SolutionMap = std::map<std::vector<unsigned char>,std::vector<unsigned char>>;
		SolutionMap solvedPuzzles;
		SolutionMap::iterator lastUsedSolution;
};

#endif

