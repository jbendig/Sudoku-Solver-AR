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
#include "Game.h"

static std::set<unsigned char> InvertChoices(const std::set<unsigned char>& choices)
{
	std::set<unsigned char> allChoices;
	for(unsigned char x = 1;x <= Game::MAX_VALUE;x++)
	{
		allChoices.insert(x);
	}

	std::set<unsigned char> invertedChoices;
	std::set_difference(allChoices.begin(),allChoices.end(),
			choices.begin(),choices.end(),
			std::inserter(invertedChoices,invertedChoices.end()));

	return invertedChoices;
}

static std::set<unsigned char> AvailableHorizontalChoices(const Game& game,const unsigned int y)
{
	std::set<unsigned char> unavailableChoices;
	for(unsigned int x = 0;x < Game::WIDTH;x++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}

	return InvertChoices(unavailableChoices);
}

static std::set<unsigned char> AvailableVerticalChoices(const Game& game,const unsigned int x)
{
	std::set<unsigned char> unavailableChoices;
	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}

	return InvertChoices(unavailableChoices);
}

static std::set<unsigned char> AvailableSquareChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	const unsigned int squareStartX = (x / Game::BLOCK_WIDTH) * Game::BLOCK_WIDTH;
	const unsigned int squareStartY = (y / Game::BLOCK_HEIGHT) * Game::BLOCK_HEIGHT;
	std::set<unsigned char> unavailableChoices;
	for(unsigned int y = 0;y < Game::BLOCK_HEIGHT;y++)
	{
		for(unsigned int x = 0;x < Game::BLOCK_WIDTH;x++)
		{
			unavailableChoices.insert(game.Get(squareStartX + x,squareStartY + y));
		}
	}

	return InvertChoices(unavailableChoices);
}

static std::set<unsigned char> AvailableChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	const std::set<unsigned char> hChoices = AvailableHorizontalChoices(game,y);
	const std::set<unsigned char> vChoices = AvailableVerticalChoices(game,x);
	const std::set<unsigned char> sChoices = AvailableSquareChoices(game,x,y);

	std::set<unsigned char> hvChoices;
	std::set_intersection(hChoices.begin(),hChoices.end(),
			vChoices.begin(),vChoices.end(),
			std::inserter(hvChoices,hvChoices.end()));

	std::set<unsigned char> availableChoices;
	std::set_intersection(hvChoices.begin(),hvChoices.end(),
			sChoices.begin(),sChoices.end(),
			std::inserter(availableChoices,availableChoices.end()));

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
			const std::set<unsigned char> choices = AvailableChoices(game,x,y);
			if(choices.find(digit) == choices.end())
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

		const std::set<unsigned char> availableChoices = AvailableChoices(game,x,y);
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
