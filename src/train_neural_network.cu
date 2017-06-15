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

#include <iostream>
#include <numeric>
#include <vector>
#include <cassert>
#include <ctime>
#include <cuda_runtime.h>
#include <cuda.h>
#include "NeuralNetworkData.h"


static const char* TRAINING_DATA_FILE_PATH = "training.dat";

//CUDA block and thread counts here were manually tuned for a GTX 660.
static const unsigned int PROCESS_NEURON_WEIGHTS_BLOCK_COUNT = 80;
static const unsigned int PROCESS_NEURON_WEIGHTS_THREAD_COUNT = 128;
static const unsigned int UPDATE_OUTPUT_LAYER_BLOCK_COUNT = 80;
static const unsigned int UPDATE_OUTPUT_LAYER_THREAD_COUNT = 512;
static const unsigned int UPDATE_HIDDEN_LAYER_BLOCK_COUNT = 80;
static const unsigned int UPDATE_HIDDEN_LAYER_THREAD_COUNT = 128;

__global__ void ProcessNeuronWeights(float* inputValues,float* inputWeights,float* output,unsigned int neuronCount,unsigned int weightCount)
{
	const unsigned int localIndex = threadIdx.x;

	__shared__ float localSums[PROCESS_NEURON_WEIGHTS_THREAD_COUNT];
	for(unsigned int neuronIndex = blockIdx.x;neuronIndex < neuronCount;neuronIndex += gridDim.x)
	{
		//Compute sum for the current neuron in parallel.
		float sum = 0.0f;
		unsigned int inputWeightsBase = neuronIndex * weightCount;
		for(unsigned int x = localIndex;x < weightCount;x += blockDim.x)
		{
			sum += inputValues[x] * inputWeights[inputWeightsBase + x];
		}
		localSums[localIndex] = sum;
		__syncthreads();

		//Compute total sum for current neuron in parallel.
		for(unsigned int x = PROCESS_NEURON_WEIGHTS_THREAD_COUNT / 2;x > 0;x >>= 1)
		{
			if(localIndex < x)
				localSums[localIndex] += localSums[localIndex + x];
			__syncthreads();
		}

		//Store final result for current neuron.
		if(localIndex == 0)
			output[neuronIndex] = 1.0f / (1.0f + exp(-localSums[0]));
	}
}

__global__ void UpdateOutputLayer(float* previousLayerOutput,float* layerOutput,float* expectedOutput,float* layerWeights,float* layerErrors,unsigned int neuronCount,unsigned int weightCount)
{
	for(unsigned int neuronIndex = blockIdx.x;neuronIndex < neuronCount;neuronIndex += gridDim.x)
	{
		const float error = (expectedOutput[neuronIndex] - layerOutput[neuronIndex]) * layerOutput[neuronIndex] * (1.0f - layerOutput[neuronIndex]);
		if(threadIdx.x == 0)
			layerErrors[neuronIndex] = error;

		//Compute weight changes for the current neuron in parallel.
		const float multiplier = 0.005f * error;
		const unsigned int weightsBase = neuronIndex * weightCount;
		for(unsigned int x = threadIdx.x;x < weightCount;x += blockDim.x)
		{
			layerWeights[weightsBase + x] += previousLayerOutput[x] * multiplier;
		}
	}
}

__global__ void UpdateHiddenLayer(float* previousLayerOutput,float* layerOutput,float* layerWeights,float* nextLayerWeights,float* layerErrors,float* nextLayerErrors,unsigned int neuronCount,unsigned int nextLayerNeuronCount,unsigned int weightCount)
{
	__shared__ float sharedErrors[UPDATE_HIDDEN_LAYER_THREAD_COUNT];
	for(unsigned int neuronIndex = blockIdx.x;neuronIndex < neuronCount;neuronIndex += gridDim.x)
	{
		float error = 0.0f;
		for(unsigned int x = threadIdx.x;x < nextLayerNeuronCount;x += blockDim.x)
		{
			error += nextLayerErrors[x] * nextLayerWeights[x * nextLayerNeuronCount + neuronIndex];
		}
		sharedErrors[threadIdx.x] = error;
		__syncthreads();

		for(unsigned int x = UPDATE_HIDDEN_LAYER_THREAD_COUNT / 2;x > 0;x >>= 1)
		{
			if(threadIdx.x < x)
				sharedErrors[threadIdx.x] += sharedErrors[threadIdx.x + x];
			__syncthreads();
		}

		if(threadIdx.x == 0)
		{
			float finalError = sharedErrors[0];
			finalError *= layerOutput[neuronIndex] * (1.0f - layerOutput[neuronIndex]);
			layerErrors[neuronIndex] = finalError;
			sharedErrors[0] = finalError;
		}
		__syncthreads();

		const float multiplier = 0.005f * sharedErrors[0];
		const unsigned int weightsBase = neuronIndex * weightCount;
		for(unsigned int x = threadIdx.x;x < weightCount;x += blockDim.x)
		{
			layerWeights[weightsBase + x] += previousLayerOutput[x] * multiplier;
		}
	}
}

static void CheckCudaError(const cudaError_t err)
{
	if(err == cudaSuccess)
		return;

	std::cerr << "Got CUDA error: " << cudaGetErrorString(err) << std::endl;
	std::abort();
}

static unsigned long long Milliseconds()
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC,&tp);
	return static_cast<unsigned long long>(tp.tv_sec) * 1000 +
		   static_cast<unsigned long long>(tp.tv_nsec) / 1000000;
}

static void FetchWeightsFromGPU(const std::vector<float*>& deviceLayerWeights,NeuralNetworkData& nnData)
{
	assert(nnData.layers.size() == deviceLayerWeights.size());

	for(unsigned int l = 0;l < nnData.layers.size();l++)
	{
		Layer& layer = nnData.layers[l];
		assert(!layer.empty());
		const unsigned int neuronSize = layer[0].size();

		for(unsigned int x = 0;x < layer.size();x++)
		{
			const cudaError_t err = cudaMemcpy(&layer[x][0],&deviceLayerWeights[l][x * neuronSize],neuronSize * sizeof(float),cudaMemcpyDeviceToHost);
			CheckCudaError(err);
		}
	}
}

int main(int argc,char* argv[])
{
	//Load existing neural network from file to resume with.
	NeuralNetworkData nnData;
	if(!nnData.LoadFromBinary(TRAINING_DATA_FILE_PATH))
	{
		std::cerr << "Could not load training data." << std::endl;
		return -1;
	}

	cudaError_t err = cudaSuccess;

	//Query devices for debug reasons. Set selected device manually for now.
	//to a display.
	std::cout << "CUDA Devices" << std::endl;
	int deviceCount = 0;
	cudaGetDeviceCount(&deviceCount);
	for(int x = 0;x < deviceCount;x++)
	{
		cudaDeviceProp deviceProp;
		cudaGetDeviceProperties(&deviceProp,x);
		std::cout << "[" << x << "]: " << deviceProp.name << std::endl;
	}
	const unsigned int device = 0;
	std::cout << "Using device " << device << std::endl;
	cudaSetDevice(device);

	//Make room on the GPU.
	std::vector<float*> deviceTrainingInputs;
	std::vector<float*> deviceExpectedOutputs;
	for(unsigned int x = 0;x < nnData.trainingData.size();x++)
	{
		float* deviceInput = nullptr;
		err = cudaMalloc(reinterpret_cast<void**>(&deviceInput),nnData.layers[0][0].size() * sizeof(float));
		CheckCudaError(err);
		deviceTrainingInputs.push_back(deviceInput);

		float* deviceExpected = nullptr;
		err = cudaMalloc(reinterpret_cast<void**>(&deviceExpected),nnData.outputChoices.size() * sizeof(float));
		CheckCudaError(err);
		deviceExpectedOutputs.push_back(deviceExpected);
	}

	std::vector<float*> deviceLayerWeights;
	std::vector<float*> deviceLayerOutputs;
	std::vector<float*> deviceLayerErrors;
	for(unsigned int x = 0;x < nnData.layers.size();x++)
	{
		const Layer& layer = nnData.layers[x];

		float* deviceWeights = nullptr;
		err = cudaMalloc(reinterpret_cast<void**>(&deviceWeights),layer.size() * layer[0].size() * sizeof(float));
		CheckCudaError(err);
		deviceLayerWeights.push_back(deviceWeights);

		float* deviceOutput = nullptr;
		err = cudaMalloc(reinterpret_cast<void**>(&deviceOutput),nnData.layerOutputs[x].size() * sizeof(float));
		CheckCudaError(err);
		deviceLayerOutputs.push_back(deviceOutput);

		float* deviceErrors = nullptr;
		err = cudaMalloc(reinterpret_cast<void**>(&deviceErrors),nnData.layerOutputs[x].size() * sizeof(float));
		CheckCudaError(err);
		deviceLayerErrors.push_back(deviceErrors);
	}

	//Transfer data to GPU.
	for(unsigned int x = 0;x < nnData.trainingData.size();x++)
	{
		err = cudaMemcpy(deviceTrainingInputs[x],&nnData.trainingData[x].first[0],nnData.layers[0][0].size() * sizeof(float),cudaMemcpyHostToDevice);
		CheckCudaError(err);

		AlignedVector expectedOutput;
		ExpectedOutput(nnData.outputChoices,nnData.trainingData[x].second,expectedOutput);
		err = cudaMemcpy(deviceExpectedOutputs[x],&expectedOutput[0],nnData.outputChoices.size() * sizeof(float),cudaMemcpyHostToDevice);
		CheckCudaError(err);
	}

	for(unsigned int x = 0;x < nnData.layers.size();x++)
	{
		const std::vector<AlignedVector>& layer = nnData.layers[x];
		const unsigned int spanSize = layer[0].size() * sizeof(float);
		for(unsigned int y = 0;y < layer.size();y++)
		{
			err = cudaMemcpy(&deviceLayerWeights[x][y * layer[0].size()],&layer[y][0],spanSize,cudaMemcpyHostToDevice);
			CheckCudaError(err);
		}

		err = cudaMemcpy(deviceLayerOutputs[x],&nnData.layerOutputs[x][0],nnData.layerOutputs[x].size() * sizeof(float),cudaMemcpyHostToDevice);
		CheckCudaError(err);
	}

	//Run kernels.
	std::vector<float> outputErrors(nnData.layers.back().size());
	for(unsigned int x = 0;x < 1001;x++)
	{
		const auto startMS = Milliseconds();
		float totalError = 0.0f;
		for(unsigned int y = 0;y < nnData.trainingData.size();y++)
		{
			//Perform forward pass.
			for(unsigned int l = 0;l < nnData.layers.size();l++)
			{
				const unsigned int neuronCount = nnData.layers[l].size();
				const unsigned int weightCount = nnData.layers[l][0].size();
				float* inputValues = nullptr;
				if(l == 0)
					inputValues = deviceTrainingInputs[y];
				else
					inputValues = deviceLayerOutputs[l - 1];
				ProcessNeuronWeights<<<PROCESS_NEURON_WEIGHTS_BLOCK_COUNT,PROCESS_NEURON_WEIGHTS_THREAD_COUNT>>>(inputValues,
																												 deviceLayerWeights[l],
																												 deviceLayerOutputs[l],
																												 neuronCount,
																												 weightCount);
				err = cudaGetLastError();
				CheckCudaError(err);
			}

			//Correct output weights.
			UpdateOutputLayer<<<UPDATE_OUTPUT_LAYER_BLOCK_COUNT,UPDATE_OUTPUT_LAYER_THREAD_COUNT>>>(deviceLayerOutputs[deviceLayerOutputs.size() - 2],
																									deviceLayerOutputs.back(),
																									deviceExpectedOutputs[y],
																									deviceLayerWeights.back(),
																									deviceLayerErrors.back(),
																									nnData.layers.back().size(),
																									nnData.layers.back()[0].size());
			err = cudaGetLastError();
			CheckCudaError(err);

			//Correct hidden layers' weights.
			for(int l = nnData.layers.size() - 2;l >= 0;l--)
			{
				float* previousLayerOutput = nullptr;
				if(l == 0)
					previousLayerOutput = deviceTrainingInputs[y];
				else
					previousLayerOutput = deviceLayerOutputs[l - 1];
				UpdateHiddenLayer<<<UPDATE_HIDDEN_LAYER_BLOCK_COUNT,UPDATE_HIDDEN_LAYER_THREAD_COUNT>>>(previousLayerOutput,
																										deviceLayerOutputs[l],
																										deviceLayerWeights[l],
																										deviceLayerWeights[l + 1],
																										deviceLayerErrors[l],
																										deviceLayerErrors[l + 1],
																										nnData.layers[l].size(),
																										nnData.layers[l + 1].size(),
																										nnData.layers[l][0].size());
				err = cudaGetLastError();
				CheckCudaError(err);
			}

			//Calculate output error.
			err = cudaMemcpy(&outputErrors[0],deviceLayerErrors.back(),outputErrors.size() * sizeof(float),cudaMemcpyDeviceToHost);
			CheckCudaError(err);
			for(const float error : outputErrors)
			{
				totalError += fabsf(error);
			}
		}

		std::cout << "Training " << x << " took " << (Milliseconds() - startMS) << " ms with error " << totalError << std::endl;

		if(totalError < 1.0f || ((x % 100) == 0 && x != 0))
		{
			FetchWeightsFromGPU(deviceLayerWeights,nnData);
			nnData.SaveAsBinary(TRAINING_DATA_FILE_PATH);
			std::cout << "Saved." << std::endl;
		}
	}

	return 0;
}

