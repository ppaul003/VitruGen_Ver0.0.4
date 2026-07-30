#ifndef __PARTICLESYSTEM_2D_H__
#define __PARTICLESYSTEM_2D_H__

#define DEBUG_GRID 0
#define DO_TIMING 0

#include <helper_functions.h>
#include <vector_types.h>
#include <vector_functions.h>

#include "particles2D_kernel.cuh"

class ParticleSystem2D {
public:
	ParticleSystem2D(uint numParticles, uint2 gridSize);
	~ParticleSystem2D();

	enum ParticleConfig {
		CNFG_DEFAULT_RESTART,
		CNFG_RANDOM_RESTART,
		_NUM_CONFIGS
	};

	enum ParticleArray {
		POSITION,
		VELOCITY,
		ACCELERATION
	};

	enum ParticleClass {
		PARTICLE_NONE = 0,
		RED,
		BLUE,
		GREEN,
		YELLOW
	};

	int getNumParticles() const { return static_cast<int>(m_activeParticles); }
	int getActiveParticleCount() const { return static_cast<int>(m_activeParticles); }
	int getMaxParticles() const { return static_cast<int>(m_particleCapacity); }

	uint2 getGridSize() const { return m_params.gridSize; }
	float2 getCellSize() const { return m_params.cellSize; }
	float2 getWorldOrigin() const { return m_params.worldOrigin; }
	uint getNumGridCells() const { return m_numGridCells; }

	float getParticleRadius() const { return m_params.particleRadius; }
	float getBoundary() const { return m_params.boundary; }

	// PBO renderer path: EuclidRenderer2D consumes these raw CUDA pointers.
	float* getDevicePositionBuffer() const { return m_dPos; }
	float* getDeviceVelocityBuffer() const { return m_dVel; }
	float* getDeviceAccelerationBuffer() const { return m_dAcc; }
	uchar4* getDeviceColorBuffer() const { return m_dColor; }
	uint* getDeviceParticleClassBuffer() const { return m_dParticleClass; }

	// Kept for API compatibility with the old 3D particle system. In this 2D
	// PBO renderer path these remain zero unless you later add VBO rendering.
	unsigned int getCurrentReadBuffer() const { return m_posVbo; }
	unsigned int getRadiiBuffer() const { return m_radVbo; }
	unsigned int getColorBuffer() const { return m_colorVBO; }

	float* getArray(ParticleArray array);
	float4 getParticle(ParticleArray array, uint index);

	void dumpGrid();
	void dumpRadii(float* rad);
	void dumpParticles(uint start, uint count);

	void update(float deltaTime);
	void reset(ParticleConfig config);
	void setActiveParticleCount(uint count);

	void setParticleClassCounts(
		uint redCount, 
		uint blueCount, 
		uint greenCount, 
		uint yellowCount = 0
	);

	void setParticle(ParticleArray array, int index, float* data);
	void setArray(ParticleArray array, const float* data, int start, int count);

	void setIterations(int i) { m_solverIterations = i; }
	void setDamping(float x) { m_params.globalDamping = x; }
	void setGravity(float y) { m_params.gravity = make_float2(0.0f, y); }
	void setParticleRadius(float x) { m_params.particleRadius = x; }
	void setCollideSpring(float x) { m_params.spring = x; }
	void setCollideDamping(float x) { m_params.damping = x; }
	void setCollideShear(float x) { m_params.shear = x; }
	void setCollideAttraction(float x) { m_params.attraction = x; }
	void setSimBoundary(float x);

	void setRedParticleLifeForces(float REDxRED, float REDxBLUE, float REDxGREEN, float REDxYELLOW);
	void setBlueParticleLifeForces(float BLUExRED, float BLUExBLUE, float BLUExGREEN, float BLUExYELLOW);
	void setGreenParticleLifeForces(float GREENxRED, float GREENxBLUE, float GREENxGREEN, float GREENxYELLOW);
	void setYellowParticleLifeForces(float YELLOWxRED, float YELLOWxBLUE, float YELLOWxGREEN, float YELLOWxYELLOW);

protected:
	ParticleSystem2D() = delete;

	void _initialize(int numParticles);
	void _finalize();
	void initGrid(uint* size, float spacing, float jitter, uint numParticles);

	static uint nextPow2GridBits(uint2 gridSize);

protected:
	bool m_bInitialized;

	/// <CPU DATA> //////////////////////////////////////////////
	uint m_particleCapacity;
	uint m_activeParticles;
	uint m_numGridCells;

	uint* m_hParticleHash;
	uint* m_hCellStart;
	uint* m_hCellEnd;
	uint2 m_gridSize;

	float* m_hPos;
	float* m_hVel;
	float* m_hAcc;

	uchar4* m_hColor;
	uint* m_hParticleClass;
	/// </CPU DATA> //////////////////////////////////////////////

	int m_solverIterations;

	/// <GPU DATA> //////////////////////////////////////////////
	uint m_radVbo;
	uint m_posVbo;
	uint m_colorVBO;
	uint m_gridSortBits;

	uint* m_dGridParticleHash;
	uint* m_dGridParticleIndex;
	uint* m_dCellStart;
	uint* m_dCellEnd;

	float* m_dPos;
	float* m_dVel;
	float* m_dAcc;
	
	float* m_dSortedPos;
	float* m_dSortedVel;
	float* m_cudaPosVBO;
	float* m_cudaColorVBO;

	uchar4* m_dColor;
	uint* m_dParticleClass;

	struct cudaGraphicsResource* m_cuda_pbo_resource;
	/// </GPU DATA> //////////////////////////////////////////////

	SimParams m_params;
	
	StopWatchInterface* m_timer;

};
#endif
