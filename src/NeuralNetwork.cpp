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

#include "NeuralNetwork.h"
#include <algorithm>
#include <iterator>
#include <iostream>
#include <cassert>
#include <cmath>
#ifdef USE_AVX
#include <immintrin.h> //AVX
#include <pmmintrin.h> //SSE3
#include <xmmintrin.h> //SSE
#endif
#include "DeltaTimer.h"
#include "NeuralNetworkData.h"


static float Sigmoid(const float value)
{
	const float exponent = value;
	return 1.0f / (1.0f + exp(-exponent));
}

static float SigmoidDiff(const float sigmoidValue)
{
	return sigmoidValue * (1 - sigmoidValue);
}

static void UpdateWeights(const AlignedVector& input,const float multiplier,AlignedVector& weights)
{
	assert(input.size() <= weights.size());

#ifdef USE_AVX
	const __m256 mulv = _mm256_set1_ps(multiplier);
	for(unsigned int x = 0;x < input.size();x += 8)
	{
		const __m256 weights8 = _mm256_load_ps(&weights[x]);
		const __m256 input8 = _mm256_load_ps(&input[x]);
		const __m256 sums8 = _mm256_add_ps(weights8,_mm256_mul_ps(input8,mulv));
		_mm256_store_ps(&weights[x],sums8);
	}
#else
	for(unsigned int x = 0;x < input.size();x++)
	{
		weights[x] += input[x] * multiplier;
	}
#endif
}



static void RunNeuron(const AlignedVector& weights,const AlignedVector& input,float& output)
{
	assert(input.size() <= weights.size());

#ifdef USE_AVX
	assert((input.size() % 8) == 0);

	__m256 sumv = _mm256_setzero_ps();
	for(unsigned int x = 0;x < input.size();x += 8)
	{
		const __m256 weights8 = _mm256_load_ps(&weights[x]);
		const __m256 input8 = _mm256_load_ps(&input[x]);
		const __m256 multiplied = _mm256_mul_ps(weights8,input8);
		sumv = _mm256_add_ps(sumv,multiplied);
	}

	const float sum = sumv[0] + sumv[1] + sumv[2] + sumv[3] + sumv[4] + sumv[5] + sumv[6] + sumv[7];
#else
	float sum = 0.0f;
	for(unsigned int x = 0;x < input.size();x++)
	{
		sum += weights[x] * static_cast<float>(input[x]);
	}
#endif

	output = Sigmoid(sum);
}

static void RunNetwork(const std::vector<std::vector<AlignedVector>>& layers,const AlignedVector& data,std::vector<AlignedVector>& layerOutputs)
{
	for(unsigned int x = 0;x < layers.size();x++)
	{
		const std::vector<AlignedVector>& layer = layers[x];

		AlignedVector& outputs = layerOutputs[x];
#pragma omp parallel for
		for(unsigned int y = 0;y < layer.size();y++)
		{
			const AlignedVector& weights = layer[y];

			float output = 0.0f;
			if(x == 0)
				RunNeuron(weights,data,output);
			else
				RunNeuron(weights,layerOutputs[x - 1],output);

			outputs[y] = output;
		}
	}
}

template <class A,class B>
static void RunNeuronTrained(const A& weights,const B& input,float& output)
{
	assert(input.size() < weights.size());

	float sum = 0.0f;
	for(unsigned int x = 0;x < input.size();x++)
	{
		sum += static_cast<float>(weights[x]) * static_cast<float>(input[x]);
	}
	sum += weights[input.size()];

	output = Sigmoid(sum);
}

//RunNetworkTrained is used on a trained network. It differs from the training version in that the
//input data should not be preprocessed.
template <class T>
static void RunNetworkTrained(const std::vector<std::vector<AlignedVector>>& layers,const std::vector<T>& data,std::vector<AlignedVector>& layerOutputs)
{
	//Reserve buffer space only on first call. Not safe but this is measurably faster.
	if(layerOutputs.empty())
	{
		layerOutputs.resize(layers.size());
		for(unsigned int x = 0;x < layers.size();x++)
		{
			layerOutputs[x].resize(layers[x].size());
		}
	}

	for(unsigned int x = 0;x < layers.size();x++)
	{
		const std::vector<AlignedVector>& layer = layers[x];

		AlignedVector& outputs = layerOutputs[x];
#pragma omp parallel for
		for(unsigned int y = 0;y < layer.size();y++)
		{
			const AlignedVector& weights = layer[y];

			float output = 0.0f;
			if(x == 0)
				RunNeuronTrained(weights,data,output);
			else
				RunNeuronTrained(weights,layerOutputs[x - 1],output);

			outputs[y] = output;
		}
	}
}

static void ExpectedOutput(const std::vector<unsigned char> outputChoices,const unsigned char value,AlignedVector& expectedOutput)
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

NeuralNetwork NeuralNetwork::Train(const std::vector<std::pair<std::vector<unsigned char>,unsigned char>>& trainingData)
{
	if(trainingData.empty())
		return NeuralNetwork();

	//Try to load previous training session and resume from it if possible. Otherwise, start a new
	//session.
	NeuralNetwork nn;
	if(!nn.data->Load())
		nn.data->InitializeWithTrainingData(trainingData);

	//Skip training if it was already completed.
	//TODO: This won't work correctly until we store the output choices when saving too.
	if(nn.data->trainingData.empty())
		return nn;

	//Train by back propagation.
	const float correctionIncrement = 0.005f;
	AlignedVector expectedOutput;
	std::vector<AlignedVector> layerLittleDeltas(nn.data->layers.size());
	DeltaTimer deltaTimer;
	for(unsigned int x = 0;x < 1500;x++)
	{
		std::cout << "Training " << x << " ... ";
		std::cout.flush();

		float totalError = 0;
		for(const std::pair<AlignedVector,unsigned char>& data : nn.data->trainingData)
		{
			RunNetwork(nn.data->layers,data.first,nn.data->layerOutputs);
			ExpectedOutput(nn.data->outputChoices,data.second,expectedOutput);

			//Adjust the output weights first.
			std::vector<AlignedVector>& outputLayer = nn.data->layers.back();
			const AlignedVector& outputs = nn.data->layerOutputs.back();
			layerLittleDeltas.back().resize(outputLayer.size());
			for(unsigned int y = 0;y < outputLayer.size();y++)
			{
				const float littleDelta = (expectedOutput[y] - outputs[y]) * SigmoidDiff(outputs[y]);
				layerLittleDeltas.back()[y] = littleDelta;
				totalError += fabsf(littleDelta);

				const float multiplier = correctionIncrement * littleDelta;
				AlignedVector& weights = outputLayer[y];
				UpdateWeights(nn.data->layerOutputs[nn.data->layerOutputs.size() - 2],multiplier,weights);
			}

			//Adjust the hidden layer weights in reverse order starting with those just before the
			//output layer.
			for(int l = nn.data->layerOutputs.size() - 2;l >= 0;l--)
			{
				std::vector<AlignedVector>& layer = nn.data->layers[l];
				const std::vector<AlignedVector>& nextLayer = nn.data->layers[l + 1];
				const AlignedVector& outputs = nn.data->layerOutputs[l];

				layerLittleDeltas[l].resize(layer.size());
#pragma omp parallel for
				for(unsigned int y = 0;y < layer.size();y++)
				{
					float littleDelta = 0.0f;
					for(unsigned int z = 0;z < layerLittleDeltas[l + 1].size();z++)
					{
						littleDelta += layerLittleDeltas[l + 1][z] * nextLayer[z][y];
					}
					littleDelta *= SigmoidDiff(outputs[y]);

					AlignedVector& weights = layer[y];
					const float multiplier = correctionIncrement * littleDelta;
					if(l == 0)
						UpdateWeights(data.first,multiplier,weights);
					else
						UpdateWeights(nn.data->layerOutputs[l - 1],multiplier,weights);

					layerLittleDeltas[l][y] = littleDelta;
				}
			}
		}

		deltaTimer.Update();
		std::cout  << deltaTimer.Delta() << " sec(s) with error " << totalError << std::endl;

		//Save every once in a while since processing can take hours.
		if(totalError < 1.0f || (x != 0 && (x % 25) == 0))
		{
			nn.data->Save();
			deltaTimer.Update();
			std::cout << "Took " << deltaTimer.Delta() << " sec(s) to save" << std::endl;
		}
	}

	//Done training, no reason to keep the (potentially large) training data around.
	nn.data->trainingData.clear();
	nn.data->trainingData.shrink_to_fit();

	return nn;
}

unsigned char NeuralNetwork::Run(const std::vector<unsigned char>& inputData) const
{
	if(inputData.size() != data->inputSize)
		return 0;

	std::vector<AlignedVector> layerOutputs;
	RunNetworkTrained(data->layers,inputData,layerOutputs);

	const AlignedVector::const_iterator maxElementIter = std::max_element(layerOutputs.back().cbegin(),
																		  layerOutputs.back().cend());
	if(maxElementIter == layerOutputs.back().cend())
		return 0;
	return data->outputChoices[std::distance(layerOutputs.back().cbegin(),maxElementIter)];
}

NeuralNetwork::NeuralNetwork()
	: data(new NeuralNetworkData)
{
}

