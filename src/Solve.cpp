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

#include "Solve.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include <cassert>
#include "Game.h"


namespace
{

class DigitSet
{
	public:
		static constexpr unsigned int MAXIMUM_SIZE = 10;

		class const_iterator
		{
			public:
				const_iterator(const std::array<unsigned char,MAXIMUM_SIZE>& choices,const unsigned int index)
					: choices(choices),
					  index(index)
				{
					operator++();
				}

				const_iterator& operator++()
				{
					while(1)
					{
						index += 1;
						if(index >= choices.size() || choices[index] != 0)
							break;
					}

					return *this;
				}

				bool operator==(const const_iterator& other) const
				{
					return index == other.index || (index >= choices.size() && other.index >= choices.size());
				}

				bool operator!=(const const_iterator& other) const
				{
					return !operator==(other);
				}

				unsigned char operator*() const
				{
					return index;
				}
			private:
				const std::array<unsigned char,MAXIMUM_SIZE>& choices;
				unsigned int index;
		};

		DigitSet()
		{
			std::fill(choices.begin(),choices.end(),0);
		}

		const_iterator begin() const
		{
			return const_iterator(choices,0);
		}

		const_iterator end() const
		{
			return const_iterator(choices,choices.size());
		}

		void insert(const unsigned char choice)
		{
			assert(choice < choices.size());
			choices[choice] = 1;
		}

		bool contains(const unsigned char choice) const
		{
			assert(choice < choices.size());
			return choices[choice] == 1;
		}

		DigitSet complemented() const
		{
			DigitSet complementedDigitSet;
			for(unsigned int x = 1;x < choices.size();x++)
			{
				complementedDigitSet.choices[x] = choices[x] ^ 1;
			}

			return complementedDigitSet;
		}
	private:
		std::array<unsigned char,MAXIMUM_SIZE> choices;
};

}

static void UnavailableRowChoices(const Game& game,const unsigned int y,DigitSet& unavailableChoices)
{
	for(unsigned int x = 0;x < Game::WIDTH;x++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}
}

static void UnavailableColumnChoices(const Game& game,const unsigned int x,DigitSet& unavailableChoices)
{
	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}
}

static void UnavailableBlockChoices(const Game& game,const unsigned int x,const unsigned int y,DigitSet& unavailableChoices)
{
	const unsigned int blockStartX = (x / Game::BLOCK_WIDTH) * Game::BLOCK_WIDTH;
	const unsigned int blockStartY = (y / Game::BLOCK_HEIGHT) * Game::BLOCK_HEIGHT;
	for(unsigned int y = 0;y < Game::BLOCK_HEIGHT;y++)
	{
		for(unsigned int x = 0;x < Game::BLOCK_WIDTH;x++)
		{
			unavailableChoices.insert(game.Get(blockStartX + x,blockStartY + y));
		}
	}
}

static DigitSet AvailableChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	DigitSet unavailableChoices;
	UnavailableRowChoices(game,y,unavailableChoices);
	UnavailableColumnChoices(game,x,unavailableChoices);
	UnavailableBlockChoices(game,x,y,unavailableChoices);

	return unavailableChoices.complemented();
}

static bool NextOpenPosition(const Game& game,const unsigned int lastX,const unsigned int lastY,unsigned int& nextX,unsigned int& nextY)
{
	for(unsigned int index = lastY * Game::WIDTH + lastX + 1;index < (Game::WIDTH * Game::HEIGHT);index++)
	{
		nextX = index % Game::WIDTH;
		nextY = index / Game::HEIGHT;
		if(game.Get(nextX,nextY) == Game::EMPTY_VALUE)
			return true;
	}

	return false;
}

bool Solvable(Game game)
{
	for(unsigned int y = 0; y < Game::HEIGHT;y++)
	{
		for(unsigned int x = 0;x < Game::WIDTH;x++)
		{
			const unsigned char digit = game.Get(x,y);
			if(digit == 0)
				continue;

			game.Set(x,y,0);
			const DigitSet choices = AvailableChoices(game,x,y);
			if(!choices.contains(digit))
				return false;
			game.Set(x,y,digit);
		}
	}

	return true;
}

static bool SolveNext(Game& game,const unsigned int lastX,const unsigned int lastY)
{
	//Find the next position in the puzzle without a digit.
	unsigned int positionX = 0;
	unsigned int positionY = 0;
	if(!NextOpenPosition(game,lastX,lastY,positionX,positionY))
		return true; //No open positions, already solved?

	//Get the set of possible digits for this position.
	const DigitSet availableChoices = AvailableChoices(game,positionX,positionY);
	for(const unsigned char choice : availableChoices)
	{
		//Try this digit.
		game.Set(positionX,positionY,choice);

		//Recursively keep searching at the next open position.
		if(SolveNext(game,positionX,positionY))
			return true;
	}

	//None of the attempted digits for this position were correct, clear it before backtracking so
	//it doesn't incorrectly influence other paths.
	game.Set(positionX,positionY,Game::EMPTY_VALUE);

	return false;
}

bool Solve(Game& game)
{
	//Use an overflow to start solving at (0,0).
	const unsigned int lastX = std::numeric_limits<unsigned int>::max();
	const unsigned int lastY = 0;

	//Recursively search for a solution in depth-first order.
	return SolveNext(game,lastX,lastY);
}

