#include <GL/freeglut.h>

#include <cstdlib>
#include <cstdio>
#include <string.h>
#include <algorithm>

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include <helper_cuda.h>
#include <helper_cuda_gl.h>

#include <helper_functions.h>
#include "thrust/device_ptr.h"
#include "thrust/for_each.h"
#include "thrust/iterator/zip_iterator.h"
#include "thrust/sort.h"

#define TX 32
#define TY 32

#include "EucliGen2D_kernel.h"
#include "kernel2D_impl.cuh"

extern "C" {

	void cudaInit(int argc, char** argv) {
		int devID;

		devID = findCudaDevice(argc, (const char**)argv);

		if (devID < 0) {

			printf("No CUDA Capable devices found, exiting...\n");
			exit(EXIT_SUCCESS);
		}
	}

	void cudaGLInit(int argc, char** argv) {
		findCudaGLDevice(argc, (const char**)argv);
	}

	void allocateArray(void** devPtr, size_t size) {
		cudaMalloc(devPtr, size);
	}

	void freeArray(void* devPtr) {
		cudaFree(devPtr);
	}

	void threadSync() {
		cudaDeviceSynchronize();
	}

	void copyArrayToDevice(void* device, const void* host, int offset, int size) {
		cudaMemcpy((char*)device + offset, host, size, cudaMemcpyHostToDevice);
	}

	void registerGLBufferObject(uint vbo, struct cudaGraphicsResource** cuda_vbo_resource) {
		cudaGraphicsGLRegisterBuffer(cuda_vbo_resource, vbo, cudaGraphicsMapFlagsNone);
	}

	void unregisterGLBufferObject(struct cudaGraphicsResource* cuda_vbo_resource) {
		cudaGraphicsUnregisterResource(cuda_vbo_resource);
	}

	void* mapGLBufferObject(struct cudaGraphicsResource** cuda_vbo_resource) {

		void* ptr;
		cudaGraphicsMapResources(1, cuda_vbo_resource, 0);
		size_t num_bytes;
		cudaGraphicsResourceGetMappedPointer((void**)&ptr, &num_bytes, *cuda_vbo_resource);
		return ptr;
	}

	void unmapGLBufferObject(struct cudaGraphicsResource* cuda_vbo_resource) {
		cudaGraphicsUnmapResources(1, &cuda_vbo_resource, 0);
	}

	void copyArrayFromDevice(void* host, const void* device, struct cudaGraphicsResource** cuda_vbo_resource, int size) {

		if (cuda_vbo_resource && *cuda_vbo_resource) {
			device = mapGLBufferObject(cuda_vbo_resource);
		}

		cudaMemcpy(host, device, size, cudaMemcpyDeviceToHost);

		if (cuda_vbo_resource && *cuda_vbo_resource) {
			unmapGLBufferObject(*cuda_vbo_resource);
		}
	}

	void setParameters(SimParams* hostParams) {
		cudaMemcpyToSymbol(params, hostParams, sizeof(SimParams));
	}

	uint iDivUp(uint a, uint b) {
		return (a % b != 0) ? (a / b + 1) : (a / b);
	}

	void computeGridSize(uint n, uint blockSize, uint& numBlocks, uint& numThreads) {
		if (n == 0) {
			numThreads = 0;
			numBlocks = 0;
			return;
		}
		numThreads = std::min(blockSize, n);
		numBlocks = iDivUp(n, numThreads);
	}

	void integrateSystem(float* pos, float* vel, float* acc, float deltaTime, uint numParticles) {

		thrust::device_ptr<float4> d_pos4((float4*)pos);
		thrust::device_ptr<float4> d_vel4((float4*)vel);
		thrust::device_ptr<float4> d_acc4((float4*)acc);

		thrust::for_each(
			thrust::make_zip_iterator(thrust::make_tuple(d_pos4, d_vel4, d_acc4)),
			thrust::make_zip_iterator(thrust::make_tuple(d_pos4 + numParticles, d_vel4 + numParticles, d_acc4 + numParticles)),
			integrate_functor_2D(deltaTime));
	}

	void calcHash2D(uint* gridParticleHash, uint* gridParticleIndex, float* pos, int numParticles) {
		if (numParticles == 0) return;
		uint numThreads, numBlocks;
		computeGridSize(numParticles, 256, numBlocks, numThreads);

		calcHashD_2D << <numBlocks, numThreads >> > (
			gridParticleHash, 
			gridParticleIndex, 
			(float4*)pos, 
			numParticles);

		checkCudaErrors(cudaGetLastError());

	}

	void reorderDataAndFindCellStart(uint* cellStart, uint* cellEnd, float* sortedPos, float* sortedVel, uint* gridParticleHash, uint* gridParticleIndex,
		float* oldPos, float* oldVel, uint numParticles, uint numCells) {
		if (numParticles == 0) return;

		uint numThreads, numBlocks;
		computeGridSize(numParticles, 256, numBlocks, numThreads);

		// set all cells to empty
		cudaMemset(cellStart, 0xffffffff, numCells * sizeof(uint));

#if USE_TEX
		cudaBindTexture(0, oldPosTex, oldPos, numParticles * sizeof(float4));
		cudaBindTexture(0, oldVelTex, oldVel, numParticles * sizeof(float4));
#endif

		uint smemSize = sizeof(uint) * (numThreads + 1);
		reorderDataAndFindCellStartD << <numBlocks, numThreads, smemSize >> > (
			cellStart, 
			cellEnd, 
			(float4*)sortedPos, 
			(float4*)sortedVel,
			gridParticleHash, 
			gridParticleIndex, 
			(float4*)oldPos, 
			(float4*)oldVel, 
			numParticles);

		checkCudaErrors(cudaGetLastError());

#if USE_TEX
		cudaUnbindTexture(oldPosTex);
		cudaUnbindTexture(oldVelTex);
#endif
	}

	void collide2D(
		float* newVel, 
		float* sortedPos, 
		float* sortedVel, 
		uint* gridParticleIndex,
		uint* cellStart, 
		uint* cellEnd, 
		uint numParticles, 
		uint numCells) {

		if (numParticles == 0) return;

#if USE_TEX
		cudaBindTexture(0, oldPosTex, sortedPos, numParticles * sizeof(float4));
		cudaBindTexture(0, oldVelTex, sortedVel, numParticles * sizeof(float4));
		cudaBindTexture(0, cellStartTex, cellStart, numCells * sizeof(uint));
		cudaBindTexture(0, cellEndTex, cellEnd, numCells * sizeof(uint));
#endif

		uint numThreads, numBlocks;
		computeGridSize(numParticles, 64, numBlocks, numThreads);

		collideD_2D << <numBlocks, numThreads >> > (
			(float4*)newVel, 
			(float4*)sortedPos, 
			(float4*)sortedVel,
			gridParticleIndex, 
			cellStart, 
			cellEnd, 
			numParticles);

		checkCudaErrors(cudaGetLastError());

#if USE_TEX
		cudaUnbindTexture(oldPosTex);
		cudaUnbindTexture(oldVelTex);
		cudaUnbindTexture(cellStartTex);
		cudaUnbindTexture(cellEndTex);
#endif
	}

	void forcesKernel2D(float* pos, float* acc, uint* dParticleClass, int numParticles) {
		if (numParticles == 0) return;

		const uint numThreads = 256;
		const uint numBlocks = (numParticles + numThreads - 1u) / numThreads;

		const uint smSz =
			numThreads * sizeof(float4) +
			numThreads * sizeof(uint);

		calculate_forces_2D << <numBlocks, numThreads, smSz >> > (
			(float4*)pos, 
			(float4*)acc,
			dParticleClass,
			numParticles);

		checkCudaErrors(cudaGetLastError());
	}

	void kernelLauncher(uchar4* d_out, int w, int h, int2 pos) {
		const dim3 blockSize(TX, TY);
		const dim3 gridSize = dim3(
			(w + blockSize.x - 1) / blockSize.x,
			(h + blockSize.y - 1) / blockSize.y);

		distanceKernel << <gridSize, blockSize >> > (d_out, w, h, pos);

		checkCudaErrors(cudaGetLastError());
	}

	void particles2D_kernelLauncher(
		uchar4* d_out, 
		int w, int h, 
		float2 cameraCenterWorld, 
		float pixelsPerWorldUnit,
		float boundaryMin, float boundaryMax, 
		float gridStep, 
		float* d_pos2D, 
		float* d_vel2D, 
		uchar4* d_particleColors,
		uint numParticles,
		int showParticles,
		uchar4 fallbackParticleColor) {

		const dim3 blockSize(TX, TY);
		const dim3 gridSize(
			(w + blockSize.x - 1) / blockSize.x,
			(h + blockSize.y - 1) / blockSize.y
		);

		particles2D_kernel << <gridSize, blockSize >> > (
			d_out,
			w, h,
			cameraCenterWorld,
			pixelsPerWorldUnit,
			boundaryMin, boundaryMax,
			gridStep,
			(float4*)d_pos2D,
			(float4*)d_vel2D,
			d_particleColors,
			numParticles,
			showParticles,
			fallbackParticleColor);

		checkCudaErrors(cudaGetLastError());
	}
	

	void sortParticles2D(
		uint* dGridParticleHash, 
		uint* dGridParticleIndex, 
		uint numParticles) {
		if (numParticles == 0) return;

		thrust::sort_by_key(
			thrust::device_ptr<uint>(dGridParticleHash),
			thrust::device_ptr<uint>(dGridParticleHash + numParticles),
			thrust::device_ptr<uint>(dGridParticleIndex));
	}
}
