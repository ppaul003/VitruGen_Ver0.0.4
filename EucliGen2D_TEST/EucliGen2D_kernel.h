#ifndef KERNEL_2D_H
#define KERNEL_2D_H
#include <cstddef>
#include <vector_types.h>

struct SimParams;

extern "C" {

	void cudaInit(int argc, char** argv);
	void cudaGLInit(int argc, char** argv);

	void allocateArray(void** devPtr, size_t size);
	void freeArray(void* devPtr);
	void threadSync();

	void copyArrayToDevice(void* device, const void* host, int offset, int size);

	void registerGLBufferObject(unsigned int vbo, struct cudaGraphicsResource** cuda_vbo_resource);
	void unregisterGLBufferObject(struct cudaGraphicsResource* cuda_vbo_resource);
	void* mapGLBufferObject(struct cudaGraphicsResource** cuda_vbo_resource);
	void unmapGLBufferObject(struct cudaGraphicsResource* cuda_vbo_resource);
	void copyArrayFromDevice(void* host, const void* device,
		struct cudaGraphicsResource** cuda_vbo_resource, int size);

	void setParameters(SimParams* hostPrams);

	void integrateSystem(float* pos, float* vel, float* acc, float deltaTime, unsigned int numParticles);

	void calcHash2D(unsigned int* gridParticleHash, unsigned int* gridParticleIndex, float* pos, int numParticles);
	
	void forcesKernel2D(
		float* pos, 
		float* acc, 
		unsigned int* particleClass,
		int numParticles
	);

	void kernelLauncher(uchar4* d_out, int w, int h, int2 pos);

	void particles2D_kernelLauncher(
		uchar4* d_out,
		int w, int h,
		float2 cameraCenterWorld,
		float pixelPerWorldUnit,
		float boundaryMin, float boundaryMax,
		float gridStep,
		float* d_pos2D,
		float* d_vel2D,
		uchar4* d_particleColors,
		unsigned int numParticles,
		int showParticles,
		uchar4 fallbackParticleColor
	);

	void reorderDataAndFindCellStart(
		unsigned int* cellStart,
		unsigned int* cellEnd,
		float* sortedPos,
		float* sortedVel,
		unsigned int* gridParticleHash,
		unsigned int* gridParticleIndex,
		float* oldPos,
		float* oldVel,
		unsigned int numParticles,
		unsigned int numCells
	);

	void collide2D(
		float* newVel,
		float* sortedPos,
		float* sortedVel,
		unsigned int* gridParticleIndex,
		unsigned int* cellStart,
		unsigned int* cellEnd,
		unsigned int numParticles,
		unsigned int numCells
	);

	void sortParticles2D(
		unsigned int* dGridParticleHash, 
		unsigned int* dGridParticleIndex, 
		unsigned int numParticles
	);
}
#endif