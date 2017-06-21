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

#include "CachedPuzzleSolver.h"
#include <algorithm>
#include <numeric>
#include "Solve.h"


static Game DigitsToGame(const std::vector<unsigned char>& digits)
{
	Game game;
	for(unsigned int y = 0;y < 9;y++)
	{
		for(unsigned int x = 0;x < 9;x++)
		{
			const unsigned int index = y * 9 + x;
			game.Set(x,y,digits[index]);
		}
	}

	return game;
}

static std::vector<unsigned char> GameToDigits(const Game& game)
{
	std::vector<unsigned char> digits;
	for(unsigned int y = 0;y < 9;y++)
	{
		for(unsigned int x = 0;x < 9;x++)
		{
			const unsigned char digit = game.Get(x,y);
			digits.push_back(digit);
		}
	}

	return digits;
}

CachedPuzzleSolver::CachedPuzzleSolver()
	: solvedPuzzles()
{
}

bool CachedPuzzleSolver::Solve(const std::vector<unsigned char>& digits,std::vector<unsigned char>& solution)
{
	//Manage the recently used solutions. The oldest solution is always discarded with each call to
	//this function UNLESS an exactly matched puzzle is found (and added to recently used
	//solutions) AND the maximum number of recently used solutions has not been reached.
	UpdateRecentlyUsedSolutions updateRecentlyUsedSolutions(recentlyUsedSolutions);

	//Grab and cache results if a previous puzzle was solved since last call.
	if(solvingFuture.valid() && solvingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		if(solvingFuture.get())
		{
			//Cache solution to save time and so it can be used when requested later.
			solvedPuzzles[solvingDigits] = {GameToDigits(solvingGame),0};
		}
	}

	//Is this a valid puzzle?
	if(digits.size() != 81)
		return false;
	else if(std::any_of(digits.begin(),digits.end(),[](const unsigned char digit) {
		return digit > 9;
	}))
		return false;

	Game game = DigitsToGame(digits);
	if(!Solvable(game))
		return false;

	//Does the puzzle have a solution that can be found in a reasonable amount of time?
	const unsigned int nonZeroDigits = std::accumulate(digits.begin(),digits.end(),0,[](const unsigned char previous,const unsigned char digit) {
		return previous + (digit > 0);
	});
	if(nonZeroDigits < 21)
		return false;

	//Has this puzzle already been solved once? Use the previous solution.
	auto iter = solvedPuzzles.find(digits);
	if(iter != solvedPuzzles.end())
	{
		solution = iter->second.digits;

		updateRecentlyUsedSolutions.AddSolution(iter);
		return true;
	}

	//If the most common recently used solution is a near match, assume that's the solution we
	//want. This just means one or more digits were OCR'd incorrectly.
	SolutionMap::const_iterator mostRecentlyUsedSolution;
	if(GetMostLikelySolution(mostRecentlyUsedSolution))
	{
		unsigned int differentDigitCount = 0;
		for(unsigned int x = 0;x < digits.size();x++)
		{
			if(digits[x] != mostRecentlyUsedSolution->first[x])
				differentDigitCount += 1;
		}

		if(differentDigitCount < 4)
		{
			solution = mostRecentlyUsedSolution->second.digits;
			return true;
		}
	}

	//If a puzzle is currently being solved in the background, discard the requested solve attempt.
	//New puzzles should be infrequent enough that there is no reason to queue them up. Finding the
	//solution asynchronously prevents the video from locking the GUI.
	if(solvingFuture.valid())
		return false;

	//Solve the puzzle in a background thread.
	solvingDigits = digits;
	solvingGame = game;
	solvingFuture = std::async(std::launch::async,[this]() {
		return ::Solve(solvingGame);
	});

	return false;
}

bool CachedPuzzleSolver::GetMostLikelySolution(std::vector<unsigned char>& solution) const
{
	SolutionMap::const_iterator solutionIter;
	if(!GetMostLikelySolution(solutionIter))
		return false;

	solution = solutionIter->second.digits;
	return true;
}

bool CachedPuzzleSolver::GetMostLikelySolution(SolutionMap::const_iterator& solutionIter) const
{
	std::deque<SolutionMap::iterator>::const_iterator mostRecentlyUsedSolution = std::max_element(recentlyUsedSolutions.cbegin(),recentlyUsedSolutions.cend(),[](const auto& lhs,const auto& rhs) {
		return lhs->second.recentlyUsedCount < rhs->second.recentlyUsedCount;
	});
	if(mostRecentlyUsedSolution == recentlyUsedSolutions.cend())
		return false;

	solutionIter = *mostRecentlyUsedSolution;
	return true;
}

CachedPuzzleSolver::UpdateRecentlyUsedSolutions::UpdateRecentlyUsedSolutions(std::deque<CachedPuzzleSolver::SolutionMap::iterator>& recentlyUsedSolutions)
	: recentlyUsedSolutions(&recentlyUsedSolutions)
{
}

CachedPuzzleSolver::UpdateRecentlyUsedSolutions::~UpdateRecentlyUsedSolutions()
{
	PopSolution();
}

void CachedPuzzleSolver::UpdateRecentlyUsedSolutions::AddSolution(CachedPuzzleSolver::SolutionMap::iterator& iter)
{
	constexpr unsigned int MAXIMUM_RECENTLY_USED_SOLUTIONS = 10;

	if(recentlyUsedSolutions == nullptr)
		return;

	iter->second.recentlyUsedCount += 1;
	recentlyUsedSolutions->push_back(iter);
	if(recentlyUsedSolutions->size() > MAXIMUM_RECENTLY_USED_SOLUTIONS)
		PopSolution();
	recentlyUsedSolutions = nullptr;
}

void CachedPuzzleSolver::UpdateRecentlyUsedSolutions::PopSolution()
{
	if(recentlyUsedSolutions == nullptr || recentlyUsedSolutions->empty())
		return;

	recentlyUsedSolutions->front()->second.recentlyUsedCount -= 1;
	recentlyUsedSolutions->pop_front();
	recentlyUsedSolutions = nullptr;
}
