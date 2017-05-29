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

#include "NeuralNetworkData.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <set>


static const char* FILE_PATH = "training.dat";

static void TrainingDataOutputChoices(const std::vector<std::pair<std::vector<float>,unsigned char>>& trainingData,std::vector<unsigned char>& outputChoices)
{
	std::set<unsigned char> outputChoiceSet;
	for(const auto& data : trainingData)
	{
		outputChoiceSet.insert(data.second);
	}

	outputChoices = std::vector<unsigned char>(outputChoiceSet.begin(),outputChoiceSet.end());
	std::sort(outputChoices.begin(),outputChoices.end());
}

void NeuralNetworkData::Clear()
{
	inputSize = 0;
	outputChoices.clear();
	trainingData.clear();
	layers.clear();
}

void NeuralNetworkData::InitializeWithTrainingData(const std::vector<std::pair<std::vector<unsigned char>,unsigned char>>& trainingData)
{
	Clear();

	//Convert input data into a more efficient form using floats.
	for(const auto& data : trainingData)
	{
		std::vector<float> input;
		for(const unsigned char value : data.first)
		{
			input.push_back(static_cast<float>(value));
		}
		this->trainingData.push_back({input,data.second});
	}

	TrainingDataOutputChoices(this->trainingData,outputChoices);
	const unsigned int outputSize = outputChoices.size();

	//Setup NN layers. There needs to be a minimum of one hidden layer and one output layer but
	//there can be as many hidden layers as necessary.
	inputSize = trainingData[0].first.size();
	layers.push_back(std::vector<std::vector<float>>(inputSize / 2)); //Hidden layer.
	layers.push_back(std::vector<std::vector<float>>(outputSize)); //Output layer.

	//Randomize initial weights.
	std::random_device randomDevice;
	std::mt19937 randomNumberGenerator(randomDevice());
	unsigned int previousLayerSize = inputSize;
	for(std::vector<std::vector<float>>& layer : layers)
	{
		for(std::vector<float>& neuron : layer)
		{
			while(neuron.size() <= previousLayerSize)
			{
				neuron.push_back(randomNumberGenerator() / static_cast<double>(std::mt19937::max()) - 0.5);
			}
		}

		previousLayerSize = layer.size();
	}
}

void NeuralNetworkData::Save()
{
	std::ofstream outFile(FILE_PATH);
	if(!outFile)
	{
		std::cerr << "Could not save neural network training." << std::endl;
		return;
	}

	//Save input data.
	outFile << trainingData.size() << std::endl;
	for(const auto& data : trainingData)
	{
		outFile << static_cast<unsigned int>(data.second) << " " << data.first.size() << " ";
		for(const float value : data.first)
		{
			outFile << value << " ";
		}
		outFile << std::endl;
	}

	//Save layer weights.
	outFile << layers.size() << std::endl;
	for(const auto& layer : layers)
	{
		outFile << layer.size() << " ";
		for(const std::vector<float>& neuron : layer)
		{
			outFile << neuron.size() << " ";
			for(const float weight : neuron)
			{
				outFile << weight << " ";
			}
		}
		outFile << std::endl;
	}
}

bool NeuralNetworkData::Load()
{
	//WARNING: This function makes no attempt to validate that the loaded data is safe.

	Clear();

	std::ifstream inFile(FILE_PATH);
	if(!inFile)
		return false;

	//Load input data.
	unsigned int trainingDataSize = 0;
	inFile >> trainingDataSize;
	std::cout << "Loading " << trainingDataSize << " training data" << std::endl;
	for(unsigned int x = 0;x < trainingDataSize;x++)
	{
		unsigned int expectedValue = 0;
		inFile >> expectedValue;
		unsigned int inputValueSize = 0;
		inFile >> inputValueSize;
		std::vector<float> inputValues;
		for(unsigned int y = 0;y < inputValueSize;y++)
		{
			float value = 0;
			inFile >> value;

			inputValues.push_back(value);
		}

		trainingData.push_back(std::make_pair(inputValues,static_cast<unsigned char>(expectedValue)));
	}

	//Load layer weights.
	unsigned int layersSize = 0;
	inFile >> layersSize;
	std::cout << "Loading " << layersSize << " layers" << std::endl;
	for(unsigned int x = 0;x < layersSize;x++)
	{
		unsigned int layerSize = 0;
		inFile >> layerSize;
		std::vector<std::vector<float>> layer;
		std::cout << "Layer " << x << " has " << layerSize << " neurons" << std::endl;
		for(unsigned int y = 0;y < layerSize;y++)
		{
			unsigned int neuronSize = 0;
			inFile >> neuronSize;
			std::vector<float> neuron;
			for(unsigned int z = 0;z < neuronSize;z++)
			{
				float weight = 0.0f;
				inFile >> weight;

				neuron.push_back(weight);
			}

			layer.push_back(neuron);
		}

		layers.push_back(layer);
	}

	//Figure out the remaining parameters from the loaded data.
	inputSize = trainingData[0].first.size();
	TrainingDataOutputChoices(trainingData,outputChoices);

	return true;
}

