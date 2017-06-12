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
#include <cassert>


static const char* TEXT_FILE_PATH = "training.dat";
static const char* BINARY_FILE_PATH = "training.bin.dat";

static void TrainingDataOutputChoices(const std::vector<std::pair<AlignedVector,unsigned char>>& trainingData,std::vector<unsigned char>& outputChoices)
{
	std::set<unsigned char> outputChoiceSet;
	for(const auto& data : trainingData)
	{
		outputChoiceSet.insert(data.second);
	}

	outputChoices = std::vector<unsigned char>(outputChoiceSet.begin(),outputChoiceSet.end());
	std::sort(outputChoices.begin(),outputChoices.end());
}

static void PrepareVector(AlignedVector& vec)
{
	//Reserve a slot for the 1.0f term.
	vec.push_back(1.0f);

	//Pad vector so it aligns well for SIMD, GPU, etc.
	while((vec.size() % 8) != 0)
	{
		vec.push_back(0.0f);
	}
}

static void InitializeLayerOutputs(const std::vector<std::vector<AlignedVector>>& layers,std::vector<AlignedVector>& layerOutputs)
{
	layerOutputs.resize(layers.size());
	for(unsigned int x = 0;x < layers.size();x++)
	{
		layerOutputs[x].resize(layers[x].size());
		PrepareVector(layerOutputs[x]);
	}
}

template <class T>
static void WriteValue(std::ofstream& stream,const T value)
{
	stream.write(reinterpret_cast<const char*>(&value),sizeof(value));
}

template <class T>
static T ReadValue(std::ifstream& stream)
{
	T value;
	stream.read(reinterpret_cast<char*>(&value),sizeof(value));
	return value;
}

void ExpectedOutput(const std::vector<unsigned char>& outputChoices,const unsigned char value,AlignedVector& expectedOutput)
{
	expectedOutput.resize(outputChoices.size());
	for(unsigned int x = 0;x < outputChoices.size();x++)
	{
		if(outputChoices[x] == value)
			expectedOutput[x] = 1.0f;
		else
			expectedOutput[x] = 0.0f;
	}
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

	//Get original input size first because this is the size of input for the trained network. The
	//input used while training is enlarged for a 1.0f term and to better fit SIMD/GPU processing.
	const unsigned int originalInputSize = trainingData[0].first.size();

	//Convert input data into a more efficient form using floats.
	for(const auto& data : trainingData)
	{
		AlignedVector input;
		for(const unsigned char value : data.first)
		{
			input.push_back(static_cast<float>(value));
		}
		PrepareVector(input);

		this->trainingData.push_back({input,data.second});
	}

	TrainingDataOutputChoices(this->trainingData,outputChoices);
	const unsigned int outputSize = outputChoices.size();

	//Setup NN layers. There needs to be a minimum of one hidden layer and one output layer but
	//there can be as many hidden layers as necessary.
	layers.push_back(std::vector<AlignedVector>(inputSize / 2)); //Hidden layer.
	layers.push_back(std::vector<AlignedVector>(outputSize)); //Output layer.
	InitializeLayerOutputs(layers,layerOutputs);

	//Randomize initial weights.
	std::random_device randomDevice;
	std::mt19937 randomNumberGenerator(randomDevice());
	unsigned int previousLayerSize = inputSize;
	for(std::vector<AlignedVector>& layer : layers)
	{
		for(AlignedVector& neuron : layer)
		{
			while(neuron.size() <= previousLayerSize)
			{
				neuron.push_back(randomNumberGenerator() / static_cast<double>(std::mt19937::max()) - 0.5);
			}
			PrepareVector(neuron);
		}

		previousLayerSize = layer.size();
	}

	inputSize = this->trainingData[0].first.size();
}

void NeuralNetworkData::SaveAsText()
{
	std::ofstream outFile(TEXT_FILE_PATH);
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
		for(const AlignedVector& neuron : layer)
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

bool NeuralNetworkData::LoadFromText()
{
	//WARNING: This function makes no attempt to validate that the loaded data is safe.

	Clear();

	std::ifstream inFile(TEXT_FILE_PATH);
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
		AlignedVector inputValues;
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
		std::vector<AlignedVector> layer;
		std::cout << "Layer " << x << " has " << layerSize << " neurons" << std::endl;
		for(unsigned int y = 0;y < layerSize;y++)
		{
			unsigned int neuronSize = 0;
			inFile >> neuronSize;
			AlignedVector neuron;
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
	InitializeLayerOutputs(layers,layerOutputs);

	return true;
}

void NeuralNetworkData::SaveAsBinary()
{
	std::ofstream outFile(BINARY_FILE_PATH,std::ios::binary);
	if(!outFile)
	{
		std::cerr << "Could not save neural network training." << std::endl;
		return;
	}

	//Save training data.
	WriteValue<unsigned int>(outFile,trainingData.size());
	for(const auto& data : trainingData)
	{
		WriteValue<unsigned int>(outFile,data.second); //Expected output.
		WriteValue<unsigned int>(outFile,data.first.size()); //Input entry count.
		outFile.write(reinterpret_cast<const char*>(&data.first[0]),data.first.size() * sizeof(float));
	}

	//Save testing data.
	WriteValue<unsigned int>(outFile,0); //Not yet supported. Always 0.

	//Save layer weights.
	WriteValue<unsigned int>(outFile,layers.size());
	for(const auto& layer : layers)
	{
		WriteValue<unsigned int>(outFile,layer.size());
		for(const auto& neuron : layer)
		{
			WriteValue<unsigned int>(outFile,neuron.size());
			outFile.write(reinterpret_cast<const char*>(&neuron[0]),neuron.size() * sizeof(float));
		}
	}

	//Save output choices.
	WriteValue<unsigned int>(outFile,outputChoices.size());
	outFile.write(reinterpret_cast<const char*>(&outputChoices[0]),outputChoices.size());
}

bool NeuralNetworkData::LoadFromBinary()
{
	//WARNING: This function makes no attempt to validate that the loaded data is safe.

	Clear();

	std::ifstream inFile(BINARY_FILE_PATH,std::ios::binary);
	if(!inFile)
		return false;

	//Load training data.
	const unsigned int trainingDataSize = ReadValue<unsigned int>(inFile);
	for(unsigned int x = 0;x < trainingDataSize;x++)
	{
		const unsigned int expectedValue = ReadValue<unsigned int>(inFile);
		const unsigned int inputValueSize = ReadValue<unsigned int>(inFile);
		AlignedVector inputValues(inputValueSize,0.0f);
		inFile.read(reinterpret_cast<char*>(&inputValues[0]),inputValueSize * sizeof(float));

		trainingData.push_back(std::make_pair(inputValues,static_cast<unsigned char>(expectedValue)));
	}

	//Load testing data.
	const unsigned int testingDataSize = ReadValue<unsigned int>(inFile);
	assert(testingDataSize == 0);

	//Load layer weights.
	const unsigned int layersSize = ReadValue<unsigned int>(inFile);
	for(unsigned int x = 0;x < layersSize;x++)
	{
		const unsigned int layerSize = ReadValue<unsigned int>(inFile);
		Layer layer;
		for(unsigned int y = 0;y < layerSize;y++)
		{
			const unsigned int neuronSize = ReadValue<unsigned int>(inFile);
			Neuron neuron(neuronSize,0.0f);
			inFile.read(reinterpret_cast<char*>(&neuron[0]),neuronSize * sizeof(float));

			layer.push_back(neuron);
		}

		layers.push_back(layer);
	}

	//Load output choices.
	const unsigned int outputChoicesSize = ReadValue<unsigned int>(inFile);
	outputChoices.resize(outputChoicesSize);
	inFile.read(reinterpret_cast<char*>(&outputChoices[0]),outputChoicesSize);

	//Figure out the remaining parameters from the loaded data.
	inputSize = trainingData[0].first.size();
	InitializeLayerOutputs(layers,layerOutputs);

	return true;
}

