#include "particleSystem2D.h"
#include "EucliGen2D_kernel.h"
#include "particles2D_kernel.cuh"

#include <helper_cuda.h>
#include <cuda_runtime.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

#ifndef CUDART_PI_F
#define CUDART_PI_F 3.141592654f
#endif

using namespace std;

static inline uint safeGridDim(uint v) {
	return (v == 0u) ? 1u : v;
}

uint ParticleSystem2D::nextPow2GridBits(uint2 gridSize) {
	// Retained as metadata for parity with the 3D sample. Thrust sort_by_key in
	// the current wrapper does not consume this value directly.
	uint cells = safeGridDim(gridSize.x) * safeGridDim(gridSize.y);
	uint bits = 0;

	while ((1u << bits) < cells && bits < 31u) {
		bits++;
	}
	
	return max(1u, bits);
}

ParticleSystem2D::ParticleSystem2D(uint numParticles, uint2 gridSize) :
	m_bInitialized(false),
	m_particleCapacity(numParticles),
	m_activeParticles(numParticles),
	m_numGridCells(0),
	m_hParticleHash(nullptr),
	m_hCellStart(nullptr),
	m_hCellEnd(nullptr),
	m_gridSize(make_uint2(safeGridDim(gridSize.x), safeGridDim(gridSize.y))),
	m_hPos(nullptr),
	m_hVel(nullptr),
	m_hAcc(nullptr),
	m_hColor(nullptr),
	m_hParticleClass(nullptr),
	m_solverIterations(1),
	m_radVbo(0),
	m_posVbo(0),
	m_colorVBO(0),
	m_gridSortBits(0),
	m_dGridParticleHash(nullptr),
	m_dGridParticleIndex(nullptr),
	m_dCellStart(nullptr),
	m_dCellEnd(nullptr),
	m_dPos(nullptr),
	m_dVel(nullptr),
	m_dAcc(nullptr),
	m_dSortedPos(nullptr),
	m_dSortedVel(nullptr),
	m_cudaPosVBO(nullptr),
	m_cudaColorVBO(nullptr),
	m_dColor(nullptr),
	m_dParticleClass(nullptr),
	m_cuda_pbo_resource(nullptr),
	m_timer(NULL) {

	m_numGridCells = m_gridSize.x * m_gridSize.y;
	m_gridSortBits = nextPow2GridBits(m_gridSize);

	m_params.gridSize = m_gridSize;
	m_params.numCells = m_numGridCells;
	m_params.numBodies = m_activeParticles;
	m_params.maxParticlesPerCell = 0u;

	m_params.particleRadius = 0.025f;
	m_params.boundary = 2.0f;

	m_params.gravity = make_float2(0.0f, 0.0f);
	
	m_params.worldOrigin = make_float2(-m_params.boundary, -m_params.boundary);
	m_params.cellSize = make_float2(
		(2.0f * m_params.boundary) / static_cast<float>(m_gridSize.x), 
		(2.0f * m_params.boundary) / static_cast<float>(m_gridSize.y)
	);

	m_params.shear = 0.1f;
	m_params.spring = 0.5f;
	m_params.damping = 0.02f;
	m_params.attraction = 0.0f;
	m_params.globalDamping = 1.0f;
	m_params.boundaryDamping = -0.5f;

	m_params.REDxRED = -2.0f;
	m_params.REDxBLUE = 4.0f;
	m_params.REDxGREEN = 1.0f;
	m_params.REDxYELLOW = -1.0f;

	m_params.BLUExRED = 1.0f;
	m_params.BLUExBLUE = -2.0f;
	m_params.BLUExGREEN = 4.0f;
	m_params.BLUExYELLOW = 1.0f;

	m_params.GREENxRED = 1.0f;
	m_params.GREENxBLUE = 1.0f;
	m_params.GREENxGREEN = -2.0f;
	m_params.GREENxYELLOW = 4.0f;

	m_params.YELLOWxRED = 4.0f;
	m_params.YELLOWxBLUE = -1.0f;
	m_params.YELLOWxGREEN = 1.0f;
	m_params.YELLOWxYELLOW = -2.0f;

	_initialize(static_cast<int>(numParticles));
}

ParticleSystem2D::~ParticleSystem2D() {

	_finalize();
	m_activeParticles = 0;
}

void ParticleSystem2D::_initialize(int numParticles) {
	assert(!m_bInitialized);

	m_particleCapacity = static_cast<uint>(max(0, numParticles));
	m_activeParticles = m_particleCapacity;
	m_params.numBodies = m_activeParticles;

	// ALLOCATE GPU DATA
	const unsigned int cSize = sizeof(uint) * m_numGridCells;
	const unsigned int uSize = sizeof(uint) * m_particleCapacity;
	const unsigned int memSize = sizeof(float) * 4 * m_particleCapacity;
	const unsigned int colorSize = sizeof(uchar4) * m_particleCapacity;

	// ALLOCATE HOST STORAGE
	m_hPos = new float[m_particleCapacity * 4];
	m_hVel = new float[m_particleCapacity * 4];
	m_hAcc = new float[m_particleCapacity * 4];
	memset(m_hPos, 0, memSize);
	memset(m_hVel, 0, memSize);
	memset(m_hAcc, 0, memSize);

	m_hColor = new uchar4[m_particleCapacity];
	memset(m_hColor, 0, colorSize);

	m_hParticleHash = new uint[m_particleCapacity];
	m_hCellStart = new uint[m_numGridCells];
	m_hCellEnd = new uint[m_numGridCells];
	memset(m_hParticleHash, 0, uSize);
	memset(m_hCellStart, 0, cSize);
	memset(m_hCellEnd, 0, cSize);

	m_hParticleClass = new uint[m_particleCapacity];
	memset(m_hParticleClass, 0, m_particleCapacity * sizeof(uint));
	
	if (m_activeParticles > 0) {
		allocateArray((void**)&m_dPos, memSize);
		allocateArray((void**)&m_dVel, memSize);
		allocateArray((void**)&m_dAcc, memSize);
		allocateArray((void**)&m_dSortedPos, memSize);
		allocateArray((void**)&m_dSortedVel, memSize);
		allocateArray((void**)&m_dColor, colorSize);
		allocateArray((void**)&m_dParticleClass, uSize);
		allocateArray((void**)&m_dGridParticleHash, uSize);
		allocateArray((void**)&m_dGridParticleIndex, uSize);
	}

	allocateArray((void**)&m_dCellStart, cSize);
	allocateArray((void**)&m_dCellEnd, cSize);
	
	setParameters(&m_params);
	sdkCreateTimer(&m_timer);

	m_bInitialized = true;
	setParticleClassCounts(0, 0, 0, 0);
	reset(CNFG_DEFAULT_RESTART);
}

void ParticleSystem2D::_finalize() {
	if (!m_bInitialized)
		return;

	delete[] m_hPos;
	delete[] m_hVel;
	delete[] m_hAcc;
	delete[] m_hColor;
	delete[] m_hParticleHash;
	delete[] m_hCellStart;
	delete[] m_hCellEnd;
	delete[] m_hParticleClass;

	freeArray(m_dPos);
	freeArray(m_dVel);
	freeArray(m_dAcc);
	freeArray(m_dColor);
	freeArray(m_dParticleClass);
	freeArray(m_dSortedPos);
	freeArray(m_dSortedVel);
	freeArray(m_dGridParticleHash);
	freeArray(m_dGridParticleIndex);
	freeArray(m_dCellStart);
	freeArray(m_dCellEnd);

	if (m_timer) {
		sdkDeleteTimer(&m_timer);
		m_timer = nullptr;
	}

	m_bInitialized = false;
}

void ParticleSystem2D::setSimBoundary(float x) {
	m_params.boundary = max(m_params.particleRadius * 4.0f, fabs(x));
	m_params.worldOrigin = make_float2(-m_params.boundary, -m_params.boundary);
	m_params.cellSize = make_float2(
		(2.0f * m_params.boundary) / static_cast<float>(m_gridSize.x),
		(2.0f * m_params.boundary) / static_cast<float>(m_gridSize.y)
	);
}

void ParticleSystem2D::setActiveParticleCount(uint count) {
	m_activeParticles = min(count, m_particleCapacity);
	m_params.numBodies = m_activeParticles;
}

void ParticleSystem2D::setParticleClassCounts(
	uint redCount, 
	uint blueCount, 
	uint greenCount, 
	uint yellowCount) {

	if (!m_hParticleClass || !m_hColor || !m_dColor || !m_dParticleClass) 
		return;

	uint index = 0;

	const uint redLimit = min(index + redCount, m_particleCapacity);
	for (; index < redLimit; index++) {
		m_hParticleClass[index] = RED;
		m_hColor[index] = make_uchar4(255, 80, 80, 255);
	}

	const uint blueLimit = min(index + blueCount, m_particleCapacity);
	for (; index < blueLimit; index++) {
		m_hParticleClass[index] = BLUE;
		m_hColor[index] = make_uchar4(80, 150, 255, 255);
	}

	const uint greenLimit = min(index + greenCount, m_particleCapacity);
	for (; index < greenLimit; index++) {
		m_hParticleClass[index] = GREEN;
		m_hColor[index] = make_uchar4(80, 255, 120, 255);
	}

	const uint yellowLimit = min(index + yellowCount, m_particleCapacity);
	for (; index < yellowLimit; index++) {
		m_hParticleClass[index] = YELLOW;
		m_hColor[index] = make_uchar4(255, 230, 80, 255);
	}

	const uint activeTotal = index;

	for (; index < m_particleCapacity; index++) {
		m_hParticleClass[index] = PARTICLE_NONE;
		m_hColor[index] = make_uchar4(0, 0, 0, 0);
	}

	setActiveParticleCount(activeTotal);

	copyArrayToDevice(
		m_dColor,
		m_hColor,
		0,
		static_cast<int>(sizeof(uchar4) * m_particleCapacity)
	);

	copyArrayToDevice(
		m_dParticleClass,
		m_hParticleClass,
		0,
		static_cast<int>(sizeof(uint) * m_particleCapacity)
	);
}

static inline uint ceilSqrt(uint n) {
	return static_cast<uint>(ceil(sqrt(static_cast<float>(max(1u, n)))));
}

static inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void ParticleSystem2D::initGrid(uint* size, float spacing, float jitter, uint numParticles) {
	if (!m_hPos || !m_hVel || numParticles == 0)
		return;

	const uint nx = max(1u, size ? size[0] : ceilSqrt(numParticles));
	const uint ny = max(1u, size ? size[1] : ceilSqrt(numParticles));
	const float radius = m_params.particleRadius;
	const float safeMin = -m_params.boundary + radius;
	const float safeMax = m_params.boundary - radius;

	mt19937 rng(1337u);
	uniform_real_distribution<float> jdist(-jitter, jitter);

	uint i = 0;
	for (uint y = 0; y < ny && i < numParticles; y++) {
		for (uint x = 0; x < nx && i < numParticles; x++, i++) {
			const float px = (static_cast<float>(x) - 0.5f * static_cast<float>(nx - 1u)) * spacing + jdist(rng);
			const float py = (static_cast<float>(y) - 0.5f * static_cast<float>(ny - 1u)) * spacing + jdist(rng);

			m_hPos[i * 4 + 0] = clampf(px, safeMin, safeMax);
			m_hPos[i * 4 + 1] = clampf(py, safeMin, safeMax);
			m_hPos[i * 4 + 2] = 0.0f;
			m_hPos[i * 4 + 3] = 1.0f;

			m_hVel[i * 4 + 0] = 0.0f;
			m_hVel[i * 4 + 1] = 0.0f;
			m_hVel[i * 4 + 2] = 0.0f;
			m_hVel[i * 4 + 3] = radius;

			m_hAcc[i * 4 + 0] = 0.0f;
			m_hAcc[i * 4 + 1] = 0.0f;
			m_hAcc[i * 4 + 2] = 0.0f;
			m_hAcc[i * 4 + 3] = 0.0f;
		}
	}
	// If the requested rectangular grid was too small, fill the rest in a
	// deterministic random cloud inside the same boundary.
	uniform_real_distribution<float> pdist(safeMin, safeMax);
	for (; i < numParticles; i++) {
		m_hPos[i * 4 + 0] = pdist(rng);
		m_hPos[i * 4 + 1] = pdist(rng);
		m_hPos[i * 4 + 2] = 0.0f;
		m_hPos[i * 4 + 3] = 1.0f;

		m_hVel[i * 4 + 0] = 0.0f;
		m_hVel[i * 4 + 1] = 0.0f;
		m_hVel[i * 4 + 2] = 0.0f;
		m_hVel[i * 4 + 3] = radius;

		m_hAcc[i * 4 + 0] = 0.0f;
		m_hAcc[i * 4 + 1] = 0.0f;
		m_hAcc[i * 4 + 2] = 0.0f;
		m_hAcc[i * 4 + 3] = 0.0f;
	}
}

void ParticleSystem2D::reset(ParticleConfig config) {

	memset(m_hPos, 0, m_activeParticles * 4 * sizeof(float));
	memset(m_hVel, 0, m_activeParticles * 4 * sizeof(float));
	memset(m_hAcc, 0, m_activeParticles * 4 * sizeof(float));

	switch (config) {
	default:
	case CNFG_DEFAULT_RESTART: {
		const uint side = ceilSqrt(m_activeParticles);
		uint size[2] = { side, side };
		const float spacing = max(m_params.particleRadius * 3.0f, 0.075f);
		initGrid(size, spacing, 0.0f, m_activeParticles);
		break;
	}

	case CNFG_RANDOM_RESTART: {
		mt19937 rng(2026u);
		const float radius = m_params.particleRadius;
		const float safeMin = -m_params.boundary + radius;
		const float safeMax = m_params.boundary - radius;
		uniform_real_distribution<float> pdist(safeMin, safeMax);
		uniform_real_distribution<float> vdist(-0.10f, 0.10f);

		for (uint i = 0; i < m_activeParticles; i++) {
			m_hPos[i * 4 + 0] = pdist(rng);
			m_hPos[i * 4 + 1] = pdist(rng);
			m_hPos[i * 4 + 2] = 0.0f;
			m_hPos[i * 4 + 3] = 1.0f;

			m_hVel[i * 4 + 0] = vdist(rng);
			m_hVel[i * 4 + 1] = vdist(rng);
			m_hVel[i * 4 + 2] = 0.0f;
			m_hVel[i * 4 + 3] = radius;

			m_hAcc[i * 4 + 0] = 0.0f;
			m_hAcc[i * 4 + 1] = 0.0f;
			m_hAcc[i * 4 + 2] = 0.0f;
			m_hAcc[i * 4 + 3] = 0.0f;
		}
		break;
	}
	}
	
	setArray(POSITION, m_hPos, 0, m_activeParticles);
	setArray(VELOCITY, m_hVel, 0, m_activeParticles);
	setArray(ACCELERATION, m_hAcc, 0, m_activeParticles);
}

void ParticleSystem2D::update(float deltaTime) {
	assert(m_bInitialized);

	if (m_activeParticles == 0) return;

	m_params.numBodies = m_activeParticles;
	setParameters(&m_params);

	const int iterations = max(1, m_solverIterations);
	const float subDt = deltaTime / static_cast<float>(iterations);
	
	for (int iter = 0; iter < m_solverIterations; iter++) {

		forcesKernel2D(
			m_dPos, 
			m_dAcc, 
			m_dParticleClass,
			m_activeParticles
		);

		integrateSystem(
			m_dPos, 
			m_dVel, 
			m_dAcc, 
			subDt, 
			m_activeParticles
		);

		/*
		calcHash2D(
			m_dGridParticleHash, 
			m_dGridParticleIndex, 
			m_dPos, 
			m_activeParticles
		);

		sortParticles2D(
			m_dGridParticleHash, 
			m_dGridParticleIndex, 
			m_activeParticles
		);

		reorderDataAndFindCellStart(
			m_dCellStart,
			m_dCellEnd,
			m_dSortedPos,
			m_dSortedVel,
			m_dGridParticleHash,
			m_dGridParticleIndex,
			m_dPos,
			m_dVel,
			m_activeParticles,
			m_numGridCells
		);

		collide2D(
			m_dVel,
			m_dSortedPos,
			m_dSortedVel,
			m_dGridParticleIndex,
			m_dCellStart,
			m_dCellEnd,
			m_activeParticles,
			m_numGridCells
		);
		*/
	}


	
}

float* ParticleSystem2D::getArray(ParticleArray array) {
	assert(m_bInitialized);

	float* hdata = nullptr;
	float* ddata = nullptr;

	switch (array) {
	default:
	case POSITION:
		hdata = m_hPos;
		ddata = m_dPos;
		break;

	case VELOCITY:
		hdata = m_hVel;
		ddata = m_dVel;
		break;

	case ACCELERATION:
		hdata = m_hAcc;
		ddata = m_dAcc;
		break;
	}

	const unsigned int memSize = sizeof(float) * 4 * m_activeParticles;
	copyArrayFromDevice(hdata, ddata, nullptr, memSize);
	return hdata;
}

float4 ParticleSystem2D::getParticle(ParticleArray array, uint index) {
	if (m_activeParticles == 0) {
		return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	float* hdata = getArray(array);
	index = min(index, m_activeParticles - 1u);
	
	return make_float4(
		hdata[index * 4u + 0u],
		hdata[index * 4u + 1u],
		hdata[index * 4u + 2u],
		hdata[index * 4u + 3u]
	);
}

static inline int clampi(int v, int lo, int hi) {
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void ParticleSystem2D::setArray(ParticleArray array, const float* data, int start, int count) {
	assert(m_bInitialized);

	if (!data || count <= 0 || start < 0 || static_cast<uint>(start) >= m_activeParticles)
		return;

	count = clampi(count, 0, static_cast<int>(m_activeParticles) - start);
	const int offset = start * 4 * sizeof(float);
	const int size = count * 4 * sizeof(float);

	switch (array) {
	default:
	case POSITION:
		copyArrayToDevice(m_dPos, data, offset, size);
		break;

	case VELOCITY:
		copyArrayToDevice(m_dVel, data, offset, size);
		break;

	case ACCELERATION:
		copyArrayToDevice(m_dAcc, data, offset, size);
		break;
	}
}

void ParticleSystem2D::setParticle(ParticleArray array, int index, float* data) {
	if (!data || index < 0 || static_cast<uint>(index) >= m_activeParticles)
		return;

	setArray(array, data, index, 1);
}

void ParticleSystem2D::dumpGrid() {
	if (!m_dCellStart || !m_dCellEnd) 
		return;
	
	copyArrayFromDevice(m_hCellStart, m_dCellStart, nullptr, sizeof(uint) * m_numGridCells);
	copyArrayFromDevice(m_hCellEnd, m_dCellEnd, nullptr, sizeof(uint) * m_numGridCells);

	uint maxCellSize = 0;
	uint nonEmpty = 0;
	for (uint i = 0; i < m_numGridCells; i++) {
		if (m_hCellStart[i] != 0xffffffffu) {
			const uint cellSize = m_hCellEnd[i] - m_hCellStart[i];
			maxCellSize = max(maxCellSize, cellSize);
			nonEmpty++;
		}
	}

	printf(
		"2D grid: %u / %u non-empty cells, max particles per cell = %u\n", 
		nonEmpty, 
		m_numGridCells, 
		maxCellSize
	);
}

void ParticleSystem2D::dumpRadii(float* rad) {

	copyArrayFromDevice(m_hVel, m_dVel, nullptr, sizeof(float) * 4 * m_activeParticles);
	for (uint i = 0; i < m_activeParticles; i++) {
		rad[i] = m_hVel[i * 4 + 3];
	}
}

void ParticleSystem2D::dumpParticles(uint start, uint count) {
	if (start >= m_activeParticles)
		return;

	count = min(count, m_activeParticles - start);
	getArray(POSITION);
	getArray(VELOCITY);

	for (uint i = start; i < start + count; i++) {
		printf("particle %u\n", i);

		printf(
			" pos: (%.4f, %.4f, %.4f, %.4f)\n",
			m_hPos[i * 4u + 0u], m_hPos[i * 4u + 1u],
			m_hPos[i * 4u + 2u], m_hPos[i * 4u + 3u]
		);

		printf(
			" vel: (%.4f, %.4f, %.4f, %.4f)\n",
			m_hVel[i * 4u + 0u], m_hVel[i * 4u + 1u],
			m_hVel[i * 4u + 2u], m_hVel[i * 4u + 3u]
		);
	}
}

void ParticleSystem2D::setRedParticleLifeForces(float REDxRED, float REDxBLUE, float REDxGREEN, float REDxYELLOW) {
	m_params.REDxRED = REDxRED;
	m_params.REDxBLUE = REDxBLUE;
	m_params.REDxGREEN = REDxGREEN;
	m_params.REDxYELLOW = REDxYELLOW;
}

void ParticleSystem2D::setBlueParticleLifeForces(float BLUExRED, float BLUExBLUE, float BLUExGREEN, float BLUExYELLOW) {
	m_params.BLUExRED = BLUExRED;
	m_params.BLUExBLUE = BLUExBLUE;
	m_params.BLUExGREEN = BLUExGREEN;
	m_params.BLUExYELLOW = BLUExYELLOW;
}

void ParticleSystem2D::setGreenParticleLifeForces(float GREENxRED, float GREENxBLUE, float GREENxGREEN, float GREENxYELLOW) {
	m_params.GREENxRED = GREENxRED;
	m_params.GREENxBLUE = GREENxBLUE;
	m_params.GREENxGREEN = GREENxGREEN;
	m_params.GREENxYELLOW = GREENxYELLOW;
}

void ParticleSystem2D::setYellowParticleLifeForces(float YELLOWxRED, float YELLOWxBLUE, float YELLOWxGREEN, float YELLOWxYELLOW) {
	m_params.YELLOWxRED = YELLOWxRED;
	m_params.YELLOWxBLUE = YELLOWxBLUE;
	m_params.YELLOWxGREEN = YELLOWxGREEN;
	m_params.YELLOWxYELLOW = YELLOWxYELLOW;
}
