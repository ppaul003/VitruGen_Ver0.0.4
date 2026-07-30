#ifndef _KERNEL_2D_IMPL_H_
#define _KERNEL_2D_IMPL_H_

#include <stdio.h>
#include <math.h>
#include <helper_math.h>

#include "particles2D_kernel.cuh"

#if USE_TEX
texture<float4, 1, cudaReadModeElementType> oldPosTex;
texture<float4, 1, cudaReadModeElementType> oldVelTex;

texture<uint, 1, cudaReadModeElementType> gridParticleHashTex;
texture<uint, 1, cudaReadModeElementType> cellStartTex;
texture<uint, 1, cudaReadModeElementType> cellEndTex;
#endif

__constant__ SimParams params;

struct integrate_functor_2D {

	float deltaTime;

	__host__ __device__ integrate_functor_2D(float delta_time) : deltaTime(delta_time) {}

	template <typename Tuple>
	__device__ void operator()(Tuple t) {

		volatile float4 posData = thrust::get<0>(t);
		volatile float4 velData = thrust::get<1>(t);
		volatile float4 accData = thrust::get<2>(t);

		float2 pos = make_float2(posData.x, posData.y);
		float2 vel = make_float2(velData.x, velData.y);
		float2 acc = make_float2(accData.x, accData.y);

		vel += acc * deltaTime;
		vel += params.gravity * deltaTime;
		vel *= params.globalDamping;

		// new position = old position + velocity * deltaTime
		pos += vel * deltaTime;

		// set this to zero to disable collisions with cube sides
#if 1
		if (pos.x > params.boundary - velData.w) {
			pos.x = params.boundary - velData.w;
			vel.x *= params.boundaryDamping;
		}
		if (pos.x < -params.boundary + velData.w) {
			pos.x = -params.boundary + velData.w;
			vel.x *= params.boundaryDamping;
		}

		if (pos.y > params.boundary - velData.w) {
			pos.y = params.boundary - velData.w;
			vel.y *= params.boundaryDamping;
		}

		if (pos.y < -params.boundary + velData.w) {
			pos.y = -params.boundary + velData.w;
			vel.y *= params.boundaryDamping;
		}

#endif

		// store new position and velocity
		thrust::get<0>(t) = make_float4(pos.x, pos.y, 0, posData.w);
		thrust::get<1>(t) = make_float4(vel.x, vel.y, 0, velData.w);
	}
};

__device__
unsigned char clip(int n) { return n > 255 ? 255 : (n < 0 ? 0 : n); }

__device__ int clampGridCoord2D(int v, int lo, int hi) {
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}

__device__ int2 calcGridPos2D(float3 p) {
	int2 gridPos;

	gridPos.x = floor(
		(p.x - params.worldOrigin.x) / params.cellSize.x
	);

	gridPos.y = floor(
		(p.y - params.worldOrigin.y) / params.cellSize.y
	);

	return gridPos;
}

__device__ uint calcGridHash2D(int2 gridPos) {
	gridPos.x = clampGridCoord2D(gridPos.x, 0, (int)params.gridSize.x - 1);
	gridPos.y = clampGridCoord2D(gridPos.y, 0, (int)params.gridSize.y - 1);

	return (uint)(gridPos.y * params.gridSize.x + gridPos.x);
}

__device__ bool isValidGridPos2D(int2 gridPos) {
	return (
		gridPos.x >= 0 &&
		gridPos.y >= 0 &&
		gridPos.x < (int)params.gridSize.x &&
		gridPos.y < (int)params.gridSize.y);
}

enum ParticleLifeClass : uint {
	PL_NONE = 0u,
	PL_RED = 1u,
	PL_BLUE = 2u,
	PL_GREEN = 3u,
	PL_YELLOW = 4u
};

__device__ float getParticleLifeForce(uint sourceClass, uint targetClass) {
	switch (sourceClass) {
	case PL_RED:
		switch (targetClass) {
		case PL_RED:
			return params.REDxRED;
		case PL_BLUE:
			return params.REDxBLUE;
		case PL_GREEN:
			return params.REDxGREEN;
		case PL_YELLOW:
			return params.REDxYELLOW;
		default:
			return 0.0f;
		}

	case PL_BLUE:
		switch (targetClass) {
		case PL_RED:    
			return params.BLUExRED;
		case PL_BLUE:   
			return params.BLUExBLUE;
		case PL_GREEN:  
			return params.BLUExGREEN;
		case PL_YELLOW: 
			return params.BLUExYELLOW;
		default:        
			return 0.0f;
		}

	case PL_GREEN:
		switch (targetClass) {
		case PL_RED:
			return params.GREENxRED;
		case PL_BLUE:
			return params.GREENxBLUE;
		case PL_GREEN:
			return params.GREENxGREEN;
		case PL_YELLOW:
			return params.GREENxYELLOW;
		default:
			return 0.0f;
		}

	case PL_YELLOW:
		switch (targetClass) {
		case PL_RED:    
			return params.YELLOWxRED;
		case PL_BLUE:   
			return params.YELLOWxBLUE;
		case PL_GREEN:  
			return params.YELLOWxGREEN;
		case PL_YELLOW: 
			return params.YELLOWxYELLOW;
		default:        
			return 0.0f;
		}

	default:
		return 0.0f;
	}
}

__device__ 
float2 bodyBodyInteractions(
	float2 bi, float4 bj,
	uint classI, uint classJ,
	float2 ai) {

	if (classI == PL_NONE || classJ == PL_NONE) {
		return ai;
	}

	float2 posi = bi;
	float2 posj = make_float2(bj.x, bj.y);

	float2 r = posj - posi;

	float distSqr = dot(r, r);

	if (distSqr < EPS2) {
		return ai;
	}

	const float dist = sqrtf(distSqr);
	const float2 dir = r / dist;

	// Interaction radius. tune this later.
	const float interactionRadius = fmaxf(
		params.particleRadius * 16.0f,
		fmaxf(params.cellSize.x, params.cellSize.y) * 2.0f
	);

	if (dist > interactionRadius) {
		return ai;
	}

	// Smooth falloff: strongest nearby, zero at interactionRadius.
	const float q = dist / interactionRadius;
	const float falloff = 1.0f - q;

	const float strength = getParticleLifeForce(classI, classJ);
	
	ai += dir * strength * falloff;

	return ai;
}

__device__
float2 tile_calculation(
	float2 myPosition,
	float4* shPosition,
	uint myClass,
	uint* shClass,
	uint myBodyId,
	float2 acc,
	uint tile,
	uint numParticles) {

	for (uint j = 0; j < blockDim.x; j++) {
		const uint other = tile * blockDim.x + j;

		if (other < numParticles && other != myBodyId) {
			acc = bodyBodyInteractions(
				myPosition, 
				shPosition[j], 
				myClass,
				shClass[j],
				acc
			);
		}
	}

	return acc;
}

__global__
void calcHashD_2D(
	uint* gridParticleHash, 
	uint* gridParticleIndex, 
	float4* pos, 
	uint numParticles) {

	uint index = blockIdx.x * blockDim.x + threadIdx.x;

	if (index >= numParticles) return;
	volatile float4 p = pos[index];

	// get addres in grid
	int2 gridPos = calcGridPos2D(make_float3(p.x, p.y, 0.0f));
	uint hash = calcGridHash2D(gridPos);

	// store grid hash and particle index
	gridParticleHash[index] = hash;
	gridParticleIndex[index] = index;
}

__global__
void reorderDataAndFindCellStartD(
	uint* cellStart, 
	uint* cellEnd, 
	float4* sortedPos, 
	float4* sortedVel,
	uint* gridParticleHash, 
	uint* gridParticleIndex, 
	float4* oldPos, 
	float4* oldVel, 
	uint numParticles) {

	extern __shared__ uint sharedHash[]; // blockSize + 1 elements
	const uint index = blockIdx.x * blockDim.x + threadIdx.x;

	uint hash;
	// handle case when no. of particles not multiple of block size
	if (index < numParticles) {
		hash = gridParticleHash[index];
		// Load hash data into shared memory so that we can look
		// at neighboring particle's hash value without loading
		// two hash values per thread
		sharedHash[threadIdx.x + 1] = hash;

		if (index > 0 && threadIdx.x == 0) {
			// first thread in block must load neighbor particle hash
			sharedHash[0] = gridParticleHash[index - 1];
		}
	}

	__syncthreads();

	if (index < numParticles) {
		// If this particle has a different cell index to the previous
		// particle then it must be the first particle in the cell,
		// so store the index of this particle in the cell.
		// As it isn't the first particle, it must also be the cell end of
		// the previous particle's cell
		if (index == 0 || hash != sharedHash[threadIdx.x]) {
			cellStart[hash] = index;
			if (index > 0)
				cellEnd[sharedHash[threadIdx.x]] = index;
		}
		if (index == numParticles - 1) {
			cellEnd[hash] = index + 1;
		}

		// Now use the sorted index to reorder the pos and vel data
		uint sortedIndex = gridParticleIndex[index];
		float4 pos = FETCH(oldPos, sortedIndex); // macro does either global read or texture fetch
		float4 vel = FETCH(oldVel, sortedIndex);

		sortedPos[index] = pos;
		sortedVel[index] = vel;
	}
}

__device__ float2 collideSpheres_2D(
	float4 posA, 
	float4 posB, 
	float4 velA, 
	float4 velB, 
	float attraction) {

	const float2 pos_A = make_float2(posA.x, posA.y);
	const float2 pos_B = make_float2(posB.x, posB.y);
	const float2 vel_A = make_float2(velA.x, velA.y);
	const float2 vel_B = make_float2(velB.x, velB.y);

	float2 relPos = pos_B - pos_A;

	float dist = length(relPos);

	float collideDist = velA.w + velB.w;

	float2 force = make_float2(0.0f);

	if (dist < collideDist) {
		if (dist < 1.0e-6f)
			return make_float2(0.0f, 0.0f);

		float2 norm = relPos / dist;
		float2 relVel = vel_B - vel_A;
		float2 tanVel = relVel - (dot(relVel, norm) * norm);

		// spring force
		force = -params.spring * (collideDist - dist) * norm;
		// dashpot (damping) force
		force += params.damping * relVel;
		// tangential shear force
		force += params.shear * tanVel;
		// attraction
		force += attraction * relPos;
	}

	return force;
}

__device__ float2 collideCell_2D(
	int2 gridPos, 
	uint index,
	float4 pos, 
	float4 vel, 
	float4* oldPos, 
	float4* oldVel, 
	uint* cellStart, 
	uint* cellEnd) {

	float2 force = make_float2(0.0f);
	if (!isValidGridPos2D(gridPos))
		return force;

	uint gridHash = calcGridHash2D(gridPos);
	uint startIndex = FETCH(cellStart, gridHash);

	if (startIndex == 0xffffffff)
		return force;
	
	uint endIndex = FETCH(cellEnd, gridHash);

	for (uint j = startIndex; j < endIndex; j++) {
		if (j == index)
			continue;

		float4 pos2 = FETCH(oldPos, j);
		float4 vel2 = FETCH(oldVel, j);

		force += collideSpheres_2D(
			pos, pos2,
			vel, vel2,
			params.attraction);
	}

	return force;
}

__global__
void collideD_2D(
	float4* newVel, 
	float4* oldPos, 
	float4* oldVel, 
	uint* gridParticleIndex, 
	uint* cellStart, 
	uint* cellEnd, 
	uint numParticles) {

	const uint index = blockIdx.x * blockDim.x + threadIdx.x;
	if (index >= numParticles) 
		return;

	// read particle data from sorted arrays
	float4 pos = FETCH(oldPos, index);
	float4 vel = FETCH(oldVel, index);

	float2 v_new = make_float2(vel.x, vel.y);

	// get address in grid
	int2 gridPos = calcGridPos2D(make_float3(pos.x, pos.y, pos.z));
	// examine neighbouring cells
	float2 force = make_float2(0.0f);

	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			int2 neighbourPos = gridPos + make_int2(x, y);
			force += collideCell_2D(
				neighbourPos,
				index,
				pos, vel,
				oldPos, oldVel,
				cellStart, cellEnd);
		}
	}
	const uint originalIndex = gridParticleIndex[index];
	newVel[originalIndex] = make_float4(v_new.x + force.x, v_new.y + force.y, 0.0f, vel.w);
}

__global__
void calculate_forces_2D(
	float4* d_b, 
	float4* d_a, 
	uint* d_particleClass,
	uint numParticles) {

	extern __shared__ unsigned char shmem[];
	float4* shPosition = reinterpret_cast<float4*>(shmem);
	uint* shClass = reinterpret_cast<uint*>(&shPosition[blockDim.x]);

	const uint body_id = blockIdx.x * blockDim.x + threadIdx.x;
	const bool active = body_id < numParticles;

	float4 myPosition = make_float4(0.0f);
	uint myClass = PL_NONE;

	if (active) {
		myPosition = d_b[body_id];
		myClass = d_particleClass ? d_particleClass[body_id] : PL_NONE;
	}

	const float2 myPosition2D = make_float2(myPosition.x, myPosition.y);
	float2 acc = make_float2(0.0f);

	const uint numTiles = (numParticles + blockDim.x - 1u) / blockDim.x;
	for (uint tile = 0; tile < numTiles; tile++) {
		const uint idx = tile * blockDim.x + threadIdx.x;

		if (idx < numParticles) {
			shPosition[threadIdx.x] = d_b[idx];
			shClass[threadIdx.x] = d_particleClass ? d_particleClass[idx] : PL_NONE;
		}
		else {
			shPosition[threadIdx.x] = make_float4(0.0f);
			shClass[threadIdx.x] = PL_NONE;
		}

		__syncthreads();

		acc = tile_calculation(
			myPosition2D, 
			shPosition,
			myClass, 
			shClass,
			body_id, 
			acc, 
			tile, 
			numParticles
		);

		__syncthreads();

	}
	
	if (active) {
		d_a[body_id] = make_float4(acc.x, acc.y, 0.0f, 0.0f);
	}
}

__global__
void distanceKernel(uchar4* d_out, int w, int h, int2 pos) {
	const int c = blockIdx.x * blockDim.x + threadIdx.x;
	const int r = blockIdx.y * blockDim.y + threadIdx.y;
	if ((c >= w || r >= h)) return; // checks if within image bounds
	const int i = c + r * w;
	const int dist = sqrtf(
		(c - pos.x) * (c - pos.x) +
		(r - pos.y) * (r - pos.y)
	);
	const unsigned char intensity = clip(255 - dist);
	d_out[i].x = intensity;
	d_out[i].y = intensity;
	d_out[i].z = 0;
	d_out[i].w = 255;
}

__global__
void particles2D_kernel(
	uchar4* d_out,
	int w, int h,
	float2 cameraCenterWorld,
	float pixelsPerWorldUnit,
	float boundaryMin, float boundaryMax,
	float gridStep,
	float4* pos,
	float4* vel,
	uchar4* particleColors,
	uint numParticles,
	int showParticles,
	uchar4 fallbackParticleColor) {

	const int c = blockIdx.x * blockDim.x + threadIdx.x;
	const int r = blockIdx.y * blockDim.y + threadIdx.y;
	if (c >= w || r >= h) return; // checks if within image bounds

	const int idx = c + r * w;
	const float2 screen = make_float2((float)c, (float)r);
	const float2 center = make_float2(0.5f * (float)w, 0.5f * (float)h);

	const float2 world = make_float2(
		cameraCenterWorld.x + (screen.x - center.x) / pixelsPerWorldUnit,
		cameraCenterWorld.y - (screen.y - center.y) / pixelsPerWorldUnit
	);

	float3 pixelColor = make_float3(0.0f, 10.0f, 24.0f);

	const bool insideBoundary =
		world.x >= boundaryMin && world.x <= boundaryMax &&
		world.y >= boundaryMin && world.y <= boundaryMax;

	if (insideBoundary) {
		pixelColor = make_float3(12.0f, 16.0f, 36.0f);
	}

	// GRID OVERLAY
	if (insideBoundary && gridStep > 0.0f) {
		const float gx = fabsf(world.x / gridStep - nearbyintf(world.x / gridStep));
		const float gy = fabsf(world.y / gridStep - nearbyintf(world.y / gridStep));
		const float lineWidthWorld = 1.25f / pixelsPerWorldUnit;

		if (gx * gridStep < lineWidthWorld || gy * gridStep < lineWidthWorld) {
			pixelColor += make_float3(22.0f, 28.0f, 42.0f);

		}
	}
	
	// BOUNDARY LINE
	const float boundaryLine = 2.0f / pixelsPerWorldUnit;
	if (insideBoundary) {
		const float dxMin = fabsf(world.x - boundaryMin);
		const float dxMax = fabsf(world.x - boundaryMax);
		const float dyMin = fabsf(world.y - boundaryMin);
		const float dyMax = fabsf(world.y - boundaryMax);
		if (dxMin < boundaryLine || dxMax < boundaryLine ||
			dyMin < boundaryLine || dyMax < boundaryLine) {
			pixelColor = make_float3(210.0f, 220.0f, 255.0f);
		}
	}

	// PARTICLE SPLATS.
	if (showParticles) {
		for (uint i = 0; i < numParticles; i++) {
			const float4 p4 = pos[i];
			const float4 v4 = vel[i];

			const float2 p = make_float2(p4.x, p4.y);
			const float radius = (v4.w > 0.0f) ? v4.w : params.particleRadius;

			const float2 d = world - p;
			const float dist2 = dot(d, d);
			const float radius2 = radius * radius;

			if (radius2 > 0.0f && dist2 <= radius2) {
				const uchar4 pc = particleColors ? particleColors[i] : fallbackParticleColor;

				// optional: skip inactive / invisible particles
				if (pc.w == 0) continue;

				const float t = 1.0f - dist2 / radius2;
				const float glow = t * t;

				pixelColor += make_float3(
					(float)pc.x * glow,
					(float)pc.y * glow,
					(float)pc.z * glow
				);
			}
		}
	}
	
	d_out[idx] = make_uchar4(
		clip((int)pixelColor.x),
		clip((int)pixelColor.y),
		clip((int)pixelColor.z),
		255
	);

}

#endif