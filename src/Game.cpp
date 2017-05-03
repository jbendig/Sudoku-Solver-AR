#include "Game.h"
#include <iostream>

static unsigned int Index(const unsigned int x,const unsigned int y)
{
	return y * Game::WIDTH + x;
}

Game::Game()
 : state(Game::WIDTH * Game::HEIGHT)
{
	Clear();
}

void Game::Clear()
{
	std::fill(state.begin(),state.end(),Game::EMPTY_VALUE);
}

bool Game::Set(const unsigned int x,const unsigned int y,const unsigned char value)
{
	if(x >= Game::WIDTH || y >= Game::HEIGHT || (value - 1) >= Game::MAX_VALUE)
		return false;

	state[Index(x,y)] = value;

	return true;
}

unsigned char Game::Get(const unsigned int x,const unsigned int y) const
{
	if(x >= Game::WIDTH || y >= Game::HEIGHT)
		return Game::EMPTY_VALUE;

	return state[Index(x,y)];
}

void Game::Print()
{
	auto PrintHorizontalDivider = []() {
		for(unsigned int x = 0;x < (Game::WIDTH + (Game::WIDTH / Game::BLOCK_WIDTH) + 1);x++)
		{
			std::cout << "-";
		}
		std::cout << std::endl;
	};

	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		if(y % Game::BLOCK_HEIGHT == 0)
			PrintHorizontalDivider();

		for(unsigned int x = 0;x < Game::WIDTH;x++)
		{
			if(x % Game::BLOCK_WIDTH == 0)
				std::cout << "|";

			const unsigned int index = Index(x,y);
			if(state[index] == Game::EMPTY_VALUE)
				std::cout << " ";
			else
				std::cout << static_cast<unsigned int>(state[index]);
		}
		std::cout << "|" << std::endl;
	}
	PrintHorizontalDivider();
}

