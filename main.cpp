#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>
#include <stack>
#include <vector>

//TODO: Remove magic numbers.

class Game
{
	public:
		Game()
		: placedDigits(0),
		  state(9*9)
		{
			Clear();
		}

		void Clear()
		{
			placedDigits = 0;
			std::fill(state.begin(),state.end(),0);
		}

		bool Set(const unsigned int x,const unsigned int y,const unsigned char value)
		{
			if(x >= 9 || y >= 9 || (value - 1) >= 9)
				return false;

			const unsigned int index = y * 9 + x;
			const unsigned char oldValue = state[index];
			state[index] = value;

			if(oldValue != value)
			{
				if(value == 0)
					placedDigits -= 1;
				else
					placedDigits += 1;
			}
			return true;
		}

		unsigned char Get(const unsigned int x,const unsigned int y) const
		{
			if(x >= 9 || y >= 9)
				return 0;

			return state[y * 9 + x];
		}

		bool Solved() const
		{
			return placedDigits == state.size();
		}

		void Print()
		{
			auto PrintHorizontalDivider = []() {
				for(unsigned int x = 0;x < 13;x++)
				{
					std::cout << "-";
				}
				std::cout << std::endl;
			};

			for(unsigned int y = 0;y < 9;y++)
			{
				if(y % 3 == 0)
					PrintHorizontalDivider();

				for(unsigned int x = 0;x < 9;x++)
				{
					if(x % 3 == 0)
						std::cout << "|";

					const unsigned int index = 9 * y + x;
					if(state[index] == 0)
						std::cout << " ";
					else
						std::cout << static_cast<unsigned int>(state[index]);
				}
				std::cout << "|" << std::endl;
			}
			PrintHorizontalDivider();
		}
	private:
		unsigned int placedDigits;
		std::vector<unsigned char> state;
};

std::set<unsigned char> InvertChoices(const std::set<unsigned char>& choices)
{
	std::set<unsigned char> allChoices;
	for(unsigned char x = 1;x <= 9;x++)
	{
		allChoices.insert(x);
	}

	std::set<unsigned char> invertedChoices;
	std::set_difference(allChoices.begin(),allChoices.end(),
			choices.begin(),choices.end(),
			std::inserter(invertedChoices,invertedChoices.end()));

	return invertedChoices;
}

std::set<unsigned char> AvailableHorizontalChoices(const Game& game,const unsigned int y)
{
	std::set<unsigned char> unavailableChoices;
	for(unsigned int x = 0;x < 9;x++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}

	return InvertChoices(unavailableChoices);
}

std::set<unsigned char> AvailableVerticalChoices(const Game& game,const unsigned int x)
{
	std::set<unsigned char> unavailableChoices;
	for(unsigned int y = 0;y < 9;y++)
	{
		unavailableChoices.insert(game.Get(x,y));
	}

	return InvertChoices(unavailableChoices);
}

std::set<unsigned char> AvailableSquareChoices(const Game& game,const unsigned int x,const unsigned int y)
{
	const unsigned int squareStartX = (x / 3) * 3;
	const unsigned int squareStartY = (y / 3) * 3;
	std::set<unsigned char> unavailableChoices;
	for(unsigned int y = 0;y < 3;y++)
	{
		for(unsigned int x = 0;x < 3;x++)
		{
			unavailableChoices.insert(game.Get(squareStartX + x,squareStartY + y));
		}
	}

	return InvertChoices(unavailableChoices);
}

std::set<unsigned char> AvailableChoices(const Game& game,const unsigned int x,const unsigned int y)
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

bool NextOpenPosition(const Game& game,const unsigned int x,const unsigned int y,unsigned int& nextX,unsigned int& nextY)
{
	for(unsigned int index = y * 9 + x + 1;index < 9 * 9;index++)
	{
		nextX = index % 9;
		nextY = index / 9;
		if(game.Get(nextX,nextY) == 0)
			return true;
	}

	return false;
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
		availableMoves.push({MoveType::Undo,0,0,0});

		const std::set<unsigned char> availableChoices = AvailableChoices(game,x,y);
		for(const unsigned char choice : availableChoices)
		{
			availableMoves.push({MoveType::Move,x,y,choice});
		}
	};

	//Find first open position.
	unsigned int positionX = 0;
	unsigned int positionY = 0;
	if(game.Get(0,0) != 0)
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
			game.Set(appliedMove.x,appliedMove.y,0);
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

static unsigned char AsciiToUChar(const char input)
{
	if(input >= 49 && input <= 57)
		return input - 48;
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

	char buffer[10];
	for(unsigned int y = 0;y < 9;y++)
	{
		file.getline(buffer,10);
		for(unsigned int x = 0;x < 9;x++)
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

	Solve(game);

	game.Print();
	return 0;
}

