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
#include <stack>
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
						if(index >= choices.size() || choices[index] != Game::EMPTY_VALUE)
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
			std::fill(choices.begin(),choices.end(),Game::EMPTY_VALUE);
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

static void UnavailableHorizontalChoices(const Game& game,const unsigned int y,DigitSet& unavailableChoices)
{
	for(unsigned int x = 0;x < Game::WIDTH;x++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}
}

static void UnavailableVerticalChoices(const Game& game,const unsigned int x,DigitSet& unavailableChoices)
{
	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}
}

static void UnavailableBlockChoices(const Game& game,const unsigned int x,const unsigned int y,DigitSet& unavailableChoices)
{
	const unsigned int squareStartX = (x / Game::BLOCK_WIDTH) * Game::BLOCK_WIDTH;
	const unsigned int squareStartY = (y / Game::BLOCK_HEIGHT) * Game::BLOCK_HEIGHT;
	for(unsigned int y = 0;y < Game::BLOCK_HEIGHT;y++)
	{
		for(unsigned int x = 0;x < Game::BLOCK_WIDTH;x++)
		{
			unavailableChoices.insert(game.Get(squareStartX + x,squareStartY + y));
		}
	}
}

static DigitSet AvailableChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	DigitSet unavailableChoices;
	UnavailableHorizontalChoices(game,y,unavailableChoices);
	UnavailableVerticalChoices(game,x,unavailableChoices);
	UnavailableBlockChoices(game,x,y,unavailableChoices);

	return unavailableChoices.complemented();
}

static bool NextOpenPosition(const Game& game,const unsigned int x,const unsigned int y,unsigned int& nextX,unsigned int& nextY)
{
	for(unsigned int index = y * Game::WIDTH + x + 1;index < (Game::WIDTH * Game::HEIGHT);index++)
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

bool Solve(Game& game)
{
	//State for DFS used to determine whether we are searching forward (Set) or backtracking
	//(Clear).
	enum class GuessType
	{
		Set,
		Clear
	};
	struct Guess
	{
		GuessType type;
		unsigned int x;
		unsigned int y;
		unsigned char value;
	};
	std::stack<Guess> availableGuesses;

	auto PushAvailableGuesses = [&](const unsigned int x,const unsigned int y) {
		const DigitSet availableChoices = AvailableChoices(game,x,y);
		for(const unsigned char choice : availableChoices)
		{
			availableGuesses.push({GuessType::Set,x,y,choice});
		}
	};

	//Find first open position.
	unsigned int positionX = 0;
	unsigned int positionY = 0;
	if(game.Get(0,0) != Game::EMPTY_VALUE)
	{
		if(!NextOpenPosition(game,0,0,positionX,positionY))
			return true; //No open positions, already solved?
	}
	PushAvailableGuesses(positionX,positionY);

	//Perform DFS until game is solved.
	while(!availableGuesses.empty())
	{
		Guess guess = availableGuesses.top();
		availableGuesses.pop();

		if(guess.type == GuessType::Set)
		{
			game.Set(guess.x,guess.y,guess.value);

			//Replace Set with a Clear so it can be undone if a solution isn't found.
			guess.type = GuessType::Clear;
			availableGuesses.push(guess);

			unsigned int nextX = 0;
			unsigned int nextY = 0;
			if(!NextOpenPosition(game,guess.x,guess.y,nextX,nextY))
				return true; //No more open positions, game solved!

			PushAvailableGuesses(nextX,nextY);
		}
		else //if(guess.type == GuessType::Clear)
			game.Set(guess.x,guess.y,Game::EMPTY_VALUE);
	}

	//Out of possible guesses, cannot be solved.
	return false;
}
