#ifndef PARTICLES_KERNEL_H
#define PARTICLES_KERNEL_H

#define G 1.0f

#define EPS2 1e-4f

#define USE_TEX 0

#if USE_TEX
#define FETCH(t, i) tex1Dfetch(t##Tex, i)
#else
#define FETCH(t, i) t[i]
#endif

#include "vector_types.h"

typedef unsigned int uint;
struct int2;
struct uchar4;
struct float4;

struct SimParams {

	uint numCells;
	uint numBodies;
	uint maxParticlesPerCell;

	uint2 gridSize;

	float2 gravity;
	float2 cellSize;
	float2 worldOrigin;

	float globalDamping;
	float particleRadius;

	float shear;
	float spring;
	float damping;
	float boundary;
	float attraction;
	float boundaryDamping;

	float REDxRED;
	float REDxBLUE;
	float REDxGREEN;
	float REDxYELLOW;

	float BLUExRED;
	float BLUExBLUE;
	float BLUExGREEN;
	float BLUExYELLOW;

	float GREENxRED;
	float GREENxBLUE;
	float GREENxGREEN;
	float GREENxYELLOW;

	float YELLOWxRED;
	float YELLOWxBLUE;
	float YELLOWxGREEN;
	float YELLOWxYELLOW;
};

#endif