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

#ifndef GAME_H
#define GAME_H

#include <vector>

class Game
{
	public:
		static constexpr unsigned int WIDTH = 9;
		static constexpr unsigned int HEIGHT = 9;
		static constexpr unsigned int BLOCK_WIDTH = WIDTH / 3;
		static constexpr unsigned int BLOCK_HEIGHT = HEIGHT / 3;
		static constexpr unsigned char MAX_VALUE = 9;
		static constexpr unsigned char EMPTY_VALUE = 0;

		Game();

		void Clear();
		bool Set(const unsigned int x,const unsigned int y,const unsigned char value);
		unsigned char Get(const unsigned int x,const unsigned int y) const;
		void Print();
	private:
		std::vector<unsigned char> state;
};

#endif

