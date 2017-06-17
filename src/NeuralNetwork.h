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

#ifndef NEURALNETWORK_H
#define NEURALNETWORK_H

#include <memory>
#include <utility>
#include <vector>

struct NeuralNetworkData;

class NeuralNetwork
{
	public:
		static NeuralNetwork Train(std::function<void(std::vector<std::pair<std::vector<unsigned char>,unsigned char>>&)> buildTrainingDataFunc);
		unsigned char Run(const std::vector<unsigned char>& inputData) const;
	private:
		std::shared_ptr<NeuralNetworkData> data;

		NeuralNetwork();
};

#endif

