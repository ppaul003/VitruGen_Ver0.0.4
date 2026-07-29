#include "particleSystem.h"
#include "kernel.h"

#include <cuda_runtime.h>

#include <helper_functions.h>
#include <helper_cuda.h>

#include <assert.h>
#include <math.h>
#include <vector>
#include <memory.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <GL/glew.h>

#ifndef CUDART_PI_F
#define CUDART_PI_F 3.141592654f
#endif

using namespace std;

ParticleSystem::ParticleSystem(
	uint numParticles,
	uint3 gridSize,
	bool bUseOpenGL) :
	m_bInitialized(false),
	m_numParticles(numParticles),
	m_numGridCells(0),
	m_hParticleHash(0),
	m_hCellStart(0),
	m_hCellEnd(0),
	m_gridSize(gridSize),
	m_hPos(0),
	m_hVel(0),
	m_hAcc(0),
	m_solverIterations(1),
	m_dPos(0),
	m_dVel(0),
	m_dAcc(0),
	m_radVbo(0),
	m_posVbo(0),
	m_colorVBO(0),
	m_cuda_posvbo_resource(nullptr),
	m_cuda_radvbo_resource(nullptr),
	m_cuda_colorvbo_resource(nullptr),
	m_gridSortBits(0),
	m_dGridParticleHash(nullptr),
	m_dGridParticleIndex(nullptr),
	m_dCellStart(nullptr),
	m_dCellEnd(nullptr),
	m_dSortedPos(nullptr),
	m_dSortedVel(nullptr),
	m_cudaPosVBO(nullptr),
	m_cudaColorVBO(nullptr) {

	m_numGridCells = m_gridSize.x * m_gridSize.y * m_gridSize.z;
	m_gridSortBits = 18;
	m_params.gridSize = m_gridSize;
	m_params.numCells = m_numGridCells;
	m_params.numBodies = m_numParticles;

	m_params.maxParticlesPerCell = 0;
	m_params.particleRadius = 0.012f;

	float cellSize = m_params.particleRadius * 2.0f;
	m_params.gravity = make_float3(0.0f, 0.0f, 0.0f);
	m_params.worldOrigin = make_float3(-1.0f, -1.0f, -1.0f);
	m_params.cellSize = make_float3(cellSize, cellSize, cellSize);

	m_params.shear = 0.1f;
	m_params.spring = 0.5f;
	m_params.damping = 0.02f;
	m_params.boundary = 1.0f;
	m_params.attraction = 0.0f;
	m_params.globalDamping = 1.0f;
	m_params.boundaryDamping = -0.5f;

	_initialize(numParticles);
}
ParticleSystem::~ParticleSystem() {

	_finalize();
	m_numParticles = 0;
}

uint ParticleSystem::createVBO(uint size) {

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return vbo;
}
void ParticleSystem::_initialize(uint numParticles) {
	assert(!m_bInitialized);

	m_numParticles = static_cast<uint>(numParticles);

	// ALLOCATE GPU DATA
	unsigned int cSize = sizeof(uint) * m_numGridCells;
	unsigned int uSize = sizeof(uint) * m_numParticles;
	unsigned int mSize = sizeof(float) * m_numParticles;
	unsigned int memSize = sizeof(float) * 4 * m_numParticles;

	// ALLOCATE HOST STORAGE
	m_hPos = new float[m_numParticles * 4];
	m_hVel = new float[m_numParticles * 4];
	m_hAcc = new float[m_numParticles * 4];
	memset(m_hPos, 0, memSize);
	memset(m_hVel, 0, memSize);
	memset(m_hAcc, 0, memSize);

	m_hParticleHash = new uint[m_numParticles];
	m_hCellStart = new uint[m_numGridCells];
	m_hCellEnd = new uint[m_numGridCells];
	memset(m_hParticleHash, 0, uSize);
	memset(m_hCellStart, 0, cSize);
	memset(m_hCellEnd, 0, cSize);

	m_particleClass = new ParticleClass[m_numParticles];
	memset(m_particleClass, 0, m_numParticles * sizeof(ParticleClass));
	if (m_numParticles > 0) m_particleClass[0] = RED;
	if (m_numParticles > 1) m_particleClass[1] = GREEN;
	if (m_numParticles > 2) m_particleClass[2] = BLUE;

	m_radVbo = createVBO(mSize);
	registerGLBufferObject(m_radVbo, &m_cuda_radvbo_resource);

	m_posVbo = createVBO(memSize);
	registerGLBufferObject(m_posVbo, &m_cuda_posvbo_resource);

	allocateArray((void**)&m_dVel, memSize);
	allocateArray((void**)&m_dAcc, memSize);
	allocateArray((void**)&m_dSortedPos, memSize);
	allocateArray((void**)&m_dSortedVel, memSize);
	allocateArray((void**)&m_dGridParticleHash, uSize);
	allocateArray((void**)&m_dGridParticleIndex, uSize);
	allocateArray((void**)&m_dCellStart, cSize);
	allocateArray((void**)&m_dCellEnd, cSize);

	m_colorVBO = createVBO(memSize);
	registerGLBufferObject(m_colorVBO, &m_cuda_colorvbo_resource);

	glBindBufferARB(GL_ARRAY_BUFFER, m_colorVBO);
	float* data = (float*)glMapBufferARB(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

	float* ptr = data;
	for (uint i = 0; i < numParticles; i++) {
		switch (m_particleClass[i]) {
		case GREEN: ptr[0] = 0.0f; ptr[1] = 1.0f; ptr[2] = 0.0f; break; // green
		case BLUE: ptr[0] = 0.0f; ptr[1] = 0.0f; ptr[2] = 1.0f; break; // blue
		case RED: ptr[0] = 1.0f; ptr[1] = 0.0f; ptr[2] = 0.0f; break; // red
		default: ptr[0] = 0.5f; ptr[1] = 0.5f; ptr[2] = 0.5f; break; // gray
		}

		ptr += 3;
		*ptr++ = 1.0f;
	}

	glUnmapBufferARB(GL_ARRAY_BUFFER);
	glBindBufferARB(GL_ARRAY_BUFFER, 0);

	setParameters(&m_params);

	m_bInitialized = true;
}
void ParticleSystem::_finalize() {
	assert(m_bInitialized);

	delete[] m_hPos;
	delete[] m_hVel;
	delete[] m_hAcc;
	delete[] m_hParticleHash;
	delete[] m_hCellStart;
	delete[] m_hCellEnd;
	delete[] m_particleClass;

	freeArray(m_dVel);
	freeArray(m_dAcc);
	freeArray(m_dSortedPos);
	freeArray(m_dSortedVel);
	freeArray(m_dGridParticleHash);
	freeArray(m_dGridParticleIndex);
	freeArray(m_dCellStart);
	freeArray(m_dCellEnd);

	unregisterGLBufferObject(m_cuda_colorvbo_resource);
	unregisterGLBufferObject(m_cuda_posvbo_resource);
	unregisterGLBufferObject(m_cuda_radvbo_resource);

	glDeleteBuffers(1, (const GLuint*)&m_colorVBO);
	glDeleteBuffers(1, (const GLuint*)&m_posVbo);
	glDeleteBuffers(1, (const GLuint*)&m_radVbo);

}

void ParticleSystem::update(float deltaTime) {
	assert(m_bInitialized);

	float* dPos = (float*)mapGLBufferObject(&m_cuda_posvbo_resource);

	setParameters(&m_params);
	integrateSystem(dPos, m_dVel, m_dAcc, deltaTime, m_numParticles);

	if (m_nbody)
		forcesKernel(dPos, m_dAcc, m_numParticles);

	if (m_collisions) {
		calcHash(m_dGridParticleHash, m_dGridParticleIndex, dPos, m_numParticles);
		sortParticles(m_dGridParticleHash, m_dGridParticleIndex, m_numParticles);
		reorderDataAndFindCellStart(m_dCellStart, m_dCellEnd, m_dSortedPos, m_dSortedVel, m_dGridParticleHash, m_dGridParticleIndex, dPos, m_dVel, m_numParticles, m_numGridCells);
		collide(m_dVel, m_dSortedPos, m_dSortedVel, m_dGridParticleIndex, m_dCellStart, m_dCellEnd, m_numParticles, m_numGridCells);
	}

	unmapGLBufferObject(m_cuda_posvbo_resource);
}
void ParticleSystem::dumpGrid() {
	// dump grid information

	copyArrayFromDevice(m_hCellStart, m_dCellStart, 0, sizeof(uint) * m_numGridCells);
	copyArrayFromDevice(m_hCellEnd, m_dCellEnd, 0, sizeof(uint) * m_numGridCells);

	uint maxCellSize = 0;
	for (uint i = 0; i < m_numGridCells; i++) {
		if (m_hCellStart[i] != 0xffffffff) {
			uint cellSize = m_hCellEnd[i] - m_hCellStart[i];

			// printf("cell: %d, %d particles\n", i, cellSize);
			if (cellSize > maxCellSize) maxCellSize = cellSize;
		}
	}

	printf("maximum particles per cell = %d\n", maxCellSize);
}
void ParticleSystem::dumpParticles(uint start, uint count) {
	// debug

	copyArrayFromDevice(m_hPos, 0, &m_cuda_posvbo_resource, sizeof(float) * 4 * count);
	copyArrayFromDevice(m_hVel, m_dVel, 0, sizeof(float) * 4 * count);
	for (uint i = start; i < start + count; i++) {
		//        printf("%d: ", i);
		printf("pos: (%.4f, %.4f, %.4f, %.4f)\n", m_hPos[i * 4 + 0], m_hPos[i * 4 + 1], m_hPos[i * 4 + 2], m_hPos[i * 4 + 3]);
		printf("vel: (%.4f, %.4f, %.4f, %.4f)\n", m_hVel[i * 4 + 0], m_hVel[i * 4 + 1], m_hVel[i * 4 + 2], m_hVel[i * 4 + 3]);
	}
}
void ParticleSystem::dumpRadii(float* rad) {

	copyArrayFromDevice(m_hVel, m_dVel, 0, sizeof(float) * 4 * m_numParticles);
	for (uint i = 0; i < m_numParticles; i++) {
		rad[i] = m_hVel[i * 4 + 3];
	}
}

float* ParticleSystem::getArray(ParticleArray array) {
	assert(m_bInitialized);

	float* hdata = 0;
	float* ddata = 0;
	struct cudaGraphicsResource* cuda_vbo_resource = 0;

	switch (array) {
	default:
	case POSITION:
		hdata = m_hPos;
		ddata = m_dPos;
		cuda_vbo_resource = m_cuda_posvbo_resource;
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

	unsigned int memSize = sizeof(float) * 4 * m_numParticles;
	copyArrayFromDevice(hdata, ddata, &cuda_vbo_resource, memSize);

	return hdata;
}

float4 ParticleSystem::
getSingleParticle(ParticleArray array, uint index) {

	if (!m_bInitialized || index >= m_numParticles) {
		return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	const unsigned int memSize =
		sizeof(float) * 4 * m_numParticles;

	switch (array) {
	default:
	case POSITION:
		copyArrayFromDevice(
			m_hPos,
			nullptr,
			&m_cuda_posvbo_resource,
			memSize
		);

		return make_float4(
			m_hPos[index * 4 + 0],
			m_hPos[index * 4 + 1],
			m_hPos[index * 4 + 2],
			m_hPos[index * 4 + 3]
		);

		break;

	case VELOCITY:

		copyArrayFromDevice(
			m_hVel,
			m_dVel,
			nullptr,
			memSize
		);

		return make_float4(
			m_hVel[index * 4 + 0],
			m_hVel[index * 4 + 1],
			m_hVel[index * 4 + 2],
			m_hVel[index * 4 + 3]
		);


		break;

	case ACCELERATION:

		copyArrayFromDevice(
			m_hAcc,
			m_dAcc,
			nullptr,
			memSize
		);

		return make_float4(
			m_hAcc[index * 4 + 0],
			m_hAcc[index * 4 + 1],
			m_hAcc[index * 4 + 2],
			m_hAcc[index * 4 + 3]
		);
		break;
	}
}

void ParticleSystem::setParticle(ParticleArray array, int index, float* data) {
	assert(m_bInitialized);

	if (!data) return;
	if (index < 0 || static_cast<uint>(index) >= m_numParticles) return;

	float* host = nullptr;
	switch (array) {
	default:
	case POSITION:
		host = m_hPos;
		break;

	case VELOCITY:
		host = m_hVel;
		break;

	case ACCELERATION:
		host = m_hAcc;
		break;
	}

	const int base = index * 4;
	for (int c = 0; c < 4; ++c) {
		host[base + c] = data[c];
	}

	setArray(array, &host[base], index, 1);
}
void ParticleSystem::setArray(ParticleArray array, const float* data, int start, int count) {
	assert(m_bInitialized);

	switch (array) {
	default:
	case POSITION: {
		unregisterGLBufferObject(m_cuda_posvbo_resource);
		glBindBuffer(GL_ARRAY_BUFFER, m_posVbo);
		glBufferSubData(GL_ARRAY_BUFFER, start * 4 * sizeof(float), count * 4 * sizeof(float), data);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		registerGLBufferObject(m_posVbo, &m_cuda_posvbo_resource);
	}
				 break;

	case VELOCITY:
		copyArrayToDevice(m_dVel, data, start * 4 * sizeof(float), count * 4 * sizeof(float));
		break;

	case ACCELERATION:
		copyArrayToDevice(m_dAcc, data, start * 4 * sizeof(float), count * 4 * sizeof(float));
		break;
	}
}
void ParticleSystem::setUniformParticleColor(float r, float g, float b, float a) {
	m_uniformParticleColor = make_float4(r, g, b, a);

	if (!m_bInitialized || m_colorVBO == 0) return;

	vector<float> colors(m_numParticles * 4);
	for (uint i = 0; i < m_numParticles; i++) {
		colors[i * 4 + 0] = r;
		colors[i * 4 + 1] = g;
		colors[i * 4 + 2] = b;
		colors[i * 4 + 3] = a;
	}

	glBindBufferARB(GL_ARRAY_BUFFER, m_colorVBO);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		static_cast<GLsizeiptr>(colors.size() * sizeof(float)),
		colors.data()
	);
	glBindBufferARB(GL_ARRAY_BUFFER, 0);
}

inline float frand() {
	return rand() / (float)RAND_MAX;
}

void ParticleSystem::initGrid(uint* size, float spacing, float jitter, uint numParticles) {
	srand(1973);
	for (uint z = 0; z < size[2]; z++) {
		for (uint y = 0; y < size[1]; y++) {
			for (uint x = 0; x < size[0]; x++) {

				uint i = (z * size[1] * size[0]) + (y * size[0]) + x;

				if (i < numParticles) {
					m_hPos[i * 4] = (spacing * x) + m_params.particleRadius - 1.0f + (frand() * 2.0f - 1.0f) * jitter;
					m_hPos[i * 4 + 1] = (spacing * y) + m_params.particleRadius - 1.0f + (frand() * 2.0f - 1.0f) * jitter;
					m_hPos[i * 4 + 2] = (spacing * z) + m_params.particleRadius - 1.0f + (frand() * 2.0f - 1.0f) * jitter;
					m_hPos[i * 4 + 3] = 1.0f;

					m_hVel[i * 4] = 0.0f;
					m_hVel[i * 4 + 1] = 0.0f;
					m_hVel[i * 4 + 2] = 0.0f;
					m_hVel[i * 4 + 3] = m_params.particleRadius;

					m_hAcc[i * 4] = 0.0f;
					m_hAcc[i * 4 + 1] = 0.0f;
					m_hAcc[i * 4 + 2] = 0.0f;
					m_hAcc[i * 4 + 3] = 0.0f;
				}
			}
		}
	}
}
void ParticleSystem::reset(ParticleConfig config) {

	switch (config) {
	default:
	case CNFG_DEFAULT_RESTART: {
		float jitter = m_params.particleRadius * 0.01f;
		uint s = (int)ceilf(powf((float)m_numParticles, 1.0f / 3.0f));
		uint gridSize[3];
		gridSize[0] = gridSize[1] = gridSize[2] = s;
		initGrid(gridSize, m_params.particleRadius * 2.0f, jitter, m_numParticles);
	}
							 break;

	case CNFG_RANDOM_RESTART: {
		int p = 0, v = 0, a = 0;

		for (uint i = 0; i < m_numParticles; i++) {

			float point[3];
			point[0] = frand();
			point[1] = frand();
			point[2] = frand();

			m_hPos[p++] = 2 * (point[0] - 0.5f);
			m_hPos[p++] = 2 * (point[1] - 0.5f);
			m_hPos[p++] = 2 * (point[2] - 0.5f);
			m_hPos[p++] = 1.0f;

			m_hVel[v++] = 0.0f;
			m_hVel[v++] = 0.0f;
			m_hVel[v++] = 0.0f;
			m_hVel[v++] = m_params.particleRadius;

			m_hAcc[a++] = 0.0f;
			m_hAcc[a++] = 0.0f;
			m_hAcc[a++] = 0.0f;
			m_hAcc[a++] = 0.0f;
		}
	}
							break;
	}

	setArray(POSITION, m_hPos, 0, m_numParticles);
	setArray(VELOCITY, m_hVel, 0, m_numParticles);
	setArray(ACCELERATION, m_hAcc, 0, m_numParticles);
}
ParticleProxy3D ParticleSystem::getSingleParticleProxy(uint index) {
	ParticleProxy3D p;

	if (!m_bInitialized ||
		index >= m_numParticles) return p;

	p.position =
		getSingleParticle(POSITION, index);

	p.velocity =
		getSingleParticle(VELOCITY, index);

	p.acceleration =
		getSingleParticle(ACCELERATION, index);

	p.radius =
		(p.velocity.w > 0.0f)
		? p.velocity.w
		: m_params.particleRadius;

	p.objectId = 0;

	return p;
}

uint ParticleSystem::addSphere(
	uint start,
	const float* position,
	const float* velocity,
	int latticeRadius,
	float spacing) {

	if (!m_bInitialized ||
		!position ||
		!velocity ||
		start >= m_numParticles ||
		latticeRadius < 0 ||
		spacing <= 0.0f) return 0;

	uint index = start;

	const float jitter =
		m_params.particleRadius * 0.01f;

	// A lattice radius of r extends approximately r spacing units
	// outward from the supplied center.

	const float sphereRadius =
		spacing * static_cast<float>(latticeRadius);

	for (int z = -latticeRadius;
		z <= latticeRadius && index < m_numParticles; z++) {

		for (int y = -latticeRadius;
			y <= latticeRadius && index < m_numParticles; y++) {

			for (int x = -latticeRadius;
				x <= latticeRadius && index < m_numParticles; x++) {

				const float dx = static_cast<float>(x) * spacing;
				const float dy = static_cast<float>(y) * spacing;
				const float dz = static_cast<float>(z) * spacing;

				const float distance =
					sqrtf(dx * dx + dy * dy + dz * dz);

				if (distance > sphereRadius) continue;

				const uint base = index * 4;

				m_hPos[base + 0] =
					position[0] + dx + (frand() * 2.0f - 1.0f) * jitter;

				m_hPos[base + 1] =
					position[1] + dy + (frand() * 2.0f - 1.0f) * jitter;

				m_hPos[base + 2] =
					position[2] + dz + (frand() * 2.0f - 1.0f) * jitter;
				// Current VitruGen position convention.
				m_hPos[base + 3] = 1.0f; // mass

				m_hVel[base + 0] = velocity[0];
				m_hVel[base + 1] = velocity[1];
				m_hVel[base + 2] = velocity[2];
				// Current VitruGen radius convention.
				m_hVel[base + 3] =
					velocity[3] > 0.0f
					? velocity[3]
					: m_params.particleRadius;

				m_hAcc[base + 0] = 0.0f;
				m_hAcc[base + 1] = 0.0f;
				m_hAcc[base + 2] = 0.0f;
				m_hAcc[base + 3] = 0.0f;

				index++;
			}
		}
	}

	const uint writtenCount = index - start;
	if (writtenCount == 0) return 0;

	setArray(
		POSITION,
		m_hPos + start * 4,
		static_cast<int>(start),
		static_cast<int>(writtenCount)
	);

	setArray(
		VELOCITY,
		m_hVel + start * 4,
		static_cast<int>(start),
		static_cast<int>(writtenCount)
	);

	setArray(
		ACCELERATION,
		m_hAcc + start * 4,
		static_cast<int>(start),
		static_cast<int>(writtenCount)
	);

	return writtenCount;
}