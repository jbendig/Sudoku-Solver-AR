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

#include <fstream>
#include <iostream>
#include "Game.h"
#include "Solve.h"

static unsigned char AsciiToUChar(const char input)
{
	if(input >= '1' && input <= '9')
		return input - '0';
	return 0;
}

static bool LoadFromFile(const char* filePath,Game& game)
{
	game.Clear();

	std::ifstream file(filePath);
	if(!file.is_open())
	{
		std::cerr << "Could not open file." << std::endl;
		return false;
	}

	char buffer[Game::WIDTH + 1];
	for(unsigned int y = 0;y < Game::HEIGHT;y++)
	{
		file.getline(buffer,sizeof(buffer));
		for(unsigned int x = 0;x < Game::WIDTH;x++)
		{
			if(buffer[x] == '\0')
				break;
			game.Set(x,y,AsciiToUChar(buffer[x]));
		}
	}

	return true;
}

int main(int argc,char* argv[])
{
	if(argc < 2)
	{
		std::cerr << "Usage: sudoku_solver <filename>" << std::endl;
		return 0;
	}

	Game game;
	if(!LoadFromFile(argv[1],game))
		return -1;

	if(!Solve(game))
		return -1;

	game.Print();

	return 0;
}

