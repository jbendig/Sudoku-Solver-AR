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
#include "AlignedVector.h"

using Neuron = AlignedVector;
using Layer = std::vector<Neuron>;
using Layers = std::vector<Layer>;

struct NeuralNetworkData
{
	unsigned int inputSize;
	std::vector<unsigned char> outputChoices;
	std::vector<std::pair<AlignedVector,unsigned char>> trainingData;
	Layers layers;
	std::vector<AlignedVector> layerOutputs;

	void Clear();
	void InitializeWithTrainingData(const std::vector<std::pair<std::vector<unsigned char>,unsigned char>>& trainingData);

	//Save/load using an inefficient text format for debugging.
	void SaveAsText();
	bool LoadFromText();

	//Save/load using an inefficient binary format.
	void SaveAsBinary();
	bool LoadFromBinary();
};

#endif

