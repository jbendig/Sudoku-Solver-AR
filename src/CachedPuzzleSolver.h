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

#include <deque>
#include <future>
#include <map>
#include <vector>
#include "Game.h"

class CachedPuzzleSolver
{
	public:
		CachedPuzzleSolver();

		bool Solve(const std::vector<unsigned char>& digits,std::vector<unsigned char>& solution);
		bool GetMostLikelySolution(std::vector<unsigned char>& solution) const;
	private:
		struct Solution
		{
			std::vector<unsigned char> digits;
			unsigned int recentlyUsedCount;
		};
		using SolutionMap = std::map<std::vector<unsigned char>,Solution>;
		class UpdateRecentlyUsedSolutions
		{
			public:
				UpdateRecentlyUsedSolutions(std::deque<CachedPuzzleSolver::SolutionMap::iterator>& recentlyUsedSolutions);
				~UpdateRecentlyUsedSolutions();

				void AddSolution(CachedPuzzleSolver::SolutionMap::iterator& iter);
			private:
				std::deque<CachedPuzzleSolver::SolutionMap::iterator>* recentlyUsedSolutions;

				void PopSolution();
		};

		SolutionMap solvedPuzzles;
		std::deque<SolutionMap::iterator> recentlyUsedSolutions;
		std::vector<unsigned char> solvingDigits;
		Game solvingGame;
		std::future<bool> solvingFuture;

		bool GetMostLikelySolution(SolutionMap::const_iterator& solutionIter) const;
};

#endif

