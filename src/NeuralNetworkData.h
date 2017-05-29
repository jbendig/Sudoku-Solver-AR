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

#ifndef NODENETWORKDATA_H
#define NODENETWORKDATA_H

#include <vector>

struct NeuralNetworkData
{
	unsigned int inputSize;
	std::vector<unsigned char> outputChoices;
	std::vector<std::pair<std::vector<float>,unsigned char>> trainingData;
	std::vector<std::vector<std::vector<float>>> layers;

	void Clear();
	void InitializeWithTrainingData(const std::vector<std::pair<std::vector<unsigned char>,unsigned char>>& trainingData);

	void Save();
	bool Load();
};

#endif

