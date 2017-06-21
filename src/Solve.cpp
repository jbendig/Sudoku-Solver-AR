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
#include <set>
#include <stack>
#include <cassert>
#include "Game.h"


namespace
{

class Choices
{
	public:
		class const_iterator
		{
			public:
				const_iterator(const unsigned char* choices,const unsigned int index)
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
						if(index >= 10 || choices[index] != 0)
							break;
					}

					return *this;
				}
				bool operator==(const const_iterator& other) const
				{
					return index == other.index || (index >= 10 && other.index >= 10);
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
				const unsigned char* choices;
				unsigned int index;
		};

		Choices()
		{
			std::fill(choices,choices + 10,0);
		}

		const_iterator begin() const
		{
			return const_iterator(choices,0);
		}

		const_iterator end() const
		{
			return const_iterator(choices,10);
		}

		void insert(const unsigned char choice)
		{
			assert(choice < 10);
			choices[choice] = 1;
		}

		void remove(const unsigned char choice)
		{
			assert(choice < 10);
			choices[choice] = 0;
		}

		bool contains(const unsigned char choice) const
		{
			assert(choice < 10);
			return choices[choice] == 1;
		}

		Choices inverted() const
		{
			Choices invertedChoices;
			for(unsigned int x = 1;x < 10;x++)
			{
				invertedChoices.choices[x] = choices[x] ^ 1;
			}

			return invertedChoices;
		}

		Choices intersection(const Choices& other) const
		{
			Choices intersectionChoices;
			for(unsigned int x = 1;x < 10;x++)
			{
				if(choices[x] == 1 && other.choices[x] == 1)
					intersectionChoices.choices[x] = 1;
			}

			return intersectionChoices;
		}
	private:
		unsigned char choices[10];
};

}

static Choices AvailableHorizontalChoices(const Game& game,const unsigned int y)
{
	Choices unavailableChoices;
	for(unsigned int x = 0;x < Game::WIDTH;x++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}

	return unavailableChoices.inverted();
}

static Choices AvailableVerticalChoices(const Game& game,const unsigned int x)
{
	Choices unavailableChoices;
	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}

	return unavailableChoices.inverted();
}

static Choices AvailableSquareChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	const unsigned int squareStartX = (x / Game::BLOCK_WIDTH) * Game::BLOCK_WIDTH;
	const unsigned int squareStartY = (y / Game::BLOCK_HEIGHT) * Game::BLOCK_HEIGHT;
	Choices unavailableChoices;
	for(unsigned int y = 0;y < Game::BLOCK_HEIGHT;y++)
	{
		for(unsigned int x = 0;x < Game::BLOCK_WIDTH;x++)
		{
			unavailableChoices.insert(game.Get(squareStartX + x,squareStartY + y));
		}
	}

	return unavailableChoices.inverted();
}

static Choices AvailableChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	const Choices hChoices = AvailableHorizontalChoices(game,y);
	const Choices vChoices = AvailableVerticalChoices(game,x);
	const Choices sChoices = AvailableSquareChoices(game,x,y);

	const Choices hvChoices = hChoices.intersection(vChoices);
	const Choices availableChoices = hvChoices.intersection(sChoices);

	return availableChoices;
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
			const Choices choices = AvailableChoices(game,x,y);
			if(!choices.contains(digit))
				return false;
			game.Set(x,y,digit);
		}
	}

	return true;
}

bool Solve(Game& game)
{
	enum class MoveType
	{
		Move,
		Undo
	};
	struct Move
	{
		MoveType type;
		unsigned int x;
		unsigned int y;
		unsigned char value;
	};
	std::stack<Move> availableMoves;
	auto PushMoves = [&](const unsigned int x,const unsigned int y) {
		availableMoves.push({MoveType::Undo,0,0,Game::EMPTY_VALUE});

		const Choices availableChoices = AvailableChoices(game,x,y);
		for(const unsigned char choice : availableChoices)
		{
			availableMoves.push({MoveType::Move,x,y,choice});
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
	PushMoves(positionX,positionY);

	//Perform DFS until game is solved.
	std::stack<Move> appliedMoves;
	while(!availableMoves.empty())
	{
		const Move move = availableMoves.top();
		availableMoves.pop();
		if(move.type == MoveType::Undo)
		{
			if(appliedMoves.empty())
				break; //No moves applied, out of possible moves.

			const Move appliedMove = appliedMoves.top();
			appliedMoves.pop();
			game.Set(appliedMove.x,appliedMove.y,Game::EMPTY_VALUE);
			continue;

		}
		game.Set(move.x,move.y,move.value);
		appliedMoves.push(move);

		unsigned int nextX = 0;
		unsigned int nextY = 0;
		if(!NextOpenPosition(game,move.x,move.y,nextX,nextY))
			return true; //No more open positions, game solved!

		PushMoves(nextX,nextY);
	}

	//Out of possible moves, cannot be solved.
	return false;
}
