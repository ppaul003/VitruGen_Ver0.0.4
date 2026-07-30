#include "renderer2D_Euclid.h"
#include "EucliGen2D_kernel.h"

#include <GL/freeglut.h>
#include <vector_functions.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace std;
using namespace glm;

EuclidRenderer2D::EuclidRenderer2D() :
	m_bInitialized(false),
	m_rad(nullptr),
	m_pos(nullptr),
	m_vel(nullptr),
	m_particleColors(nullptr),
	m_radCapacity(0),
	m_numParticles(0),
	m_particleRadius(0.125f),
	m_fov(60.0f),
	m_window_w(0),
	m_window_h(0) {
}

EuclidRenderer2D::~EuclidRenderer2D() {
	finalize();
}

bool EuclidRenderer2D::initialize(int windowW, int windowH) {
	if (m_bInitialized) {
		setWindowSize(windowW, windowH);
		return true;
	}

	m_window_w = max(1, windowW);
	m_window_h = max(1, windowH);

	_initGL();
	_initialize();

	return m_bInitialized;
}

void EuclidRenderer2D::finalize() {
	if (!m_bInitialized && !m_pbo && !m_tex && !m_rad)
		return;

	_destroyPBOAndTexture();

	delete[] m_rad;
	m_rad = nullptr;
	m_radCapacity = 0;

	m_pos = nullptr;
	m_vel = nullptr;
	m_particleColors = nullptr;
	m_numParticles = 0;
	m_bInitialized = false;
}

void EuclidRenderer2D::_initGL() {
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void EuclidRenderer2D::_initialize() {
	assert(!m_bInitialized);

	_createPBOAndTexture(m_window_w, m_window_h);
	m_bInitialized = true;
}

void EuclidRenderer2D::setWindowSize(int w, int h) {
	w = max(1, w);
	h = max(1, h);

	if (w == m_window_w && 
		h == m_window_h && 
		m_pbo && 
		m_tex) return;

	m_window_w = w;
	m_window_h = h;

	if (m_bInitialized) 
		_createPBOAndTexture(m_window_w, m_window_h);
}

void EuclidRenderer2D::_destroyPBOAndTexture() {
	if (m_cuda_pbo_resource) {
		unregisterGLBufferObject(m_cuda_pbo_resource);
		m_cuda_pbo_resource = nullptr;
	}

	if (m_pbo) {
		glDeleteBuffers(1, &m_pbo);
		m_pbo = 0;
	}

	if (m_tex) {
		glDeleteTextures(1, &m_tex);
		m_tex = 0;
	}
}

void EuclidRenderer2D::_createPBOAndTexture(int w, int h) {
	_destroyPBOAndTexture();

	const size_t imageBytes =
		static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(GLubyte) * 4u;

	glGenBuffers(1, &m_pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, imageBytes, nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	registerGLBufferObject(m_pbo, &m_cuda_pbo_resource);

	glGenTextures(1, &m_tex);
	glBindTexture(GL_TEXTURE_2D, m_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, NULL
	);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void EuclidRenderer2D::setParticleDeviceBuffers(float* dPosFloat4, float* dVelFloat4, int numParticles) {
	m_pos = dPosFloat4;
	m_vel = dVelFloat4;
	m_numParticles = max(0, numParticles);
}

void EuclidRenderer2D::setPositions(float* dPosFloat4, int numParticles) {
	m_pos = dPosFloat4;
	m_numParticles = max(0, numParticles);
}

void EuclidRenderer2D::setVelocities(float* dVelFloat4, int numParticles) {
	m_vel = dVelFloat4;
	m_numParticles = max(0, numParticles);
}

void EuclidRenderer2D::setRadius(float* r, int numParticles) {
	numParticles = max(0, numParticles);

	if (m_radCapacity != numParticles) {
		delete[] m_rad;
		m_rad = nullptr;
		m_radCapacity = 0;

		if (numParticles > 0) {
			m_rad = new float[numParticles];
			m_radCapacity = numParticles;
		}
	}

	if (m_rad && r && numParticles > 0) 
		memcpy(m_rad, r, sizeof(float) * static_cast<size_t>(numParticles));
	
	m_numParticles = numParticles;
}

static inline float clampf(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi ? hi : v);
}

void EuclidRenderer2D::setPixelsPerWorldUnit(float pixelsPerWorldUnit) {
	m_pixelsPerWorldUnit = clampf(
		pixelsPerWorldUnit,
		m_minPixelsPerWorldUnit,
		m_maxPixelsPerWorldUnit
	);
}

void EuclidRenderer2D::zoom(float zoomFactor) {
	if (zoomFactor <= 0.0f)
		return;
		
	setPixelsPerWorldUnit(m_pixelsPerWorldUnit * zoomFactor);
}

void EuclidRenderer2D::zoomAtScreenPoint(float zoomFactor, int mouseX, int mouseY) {
	if (zoomFactor <= 0.0f)
		return;

	const vec2 before = screenToWorld(mouseX, mouseY);
	zoom(zoomFactor);
	const vec2 after = screenToWorld(mouseX, mouseY);

	// Keep the world point under the cursor fixed while zooming.
	m_cameraCenterWorld += (before - after);
}

vec2 EuclidRenderer2D::screenToWorld(int sx, int sy) const {
	const vec2 center(
		0.5f * static_cast<float>(m_window_w),
		0.5f * static_cast<float>(m_window_h)
	);

	const vec2 screen(static_cast<float>(sx), static_cast<float>(sy));

	return vec2(
		m_cameraCenterWorld.x + (screen.x - center.x) / m_pixelsPerWorldUnit,
		m_cameraCenterWorld.y - (screen.y - center.y) / m_pixelsPerWorldUnit
	);
}

vec2 EuclidRenderer2D::screenDeltaToWorldDelta(float dxPixels, float dyPixels) const {
	return vec2(
		dxPixels / m_pixelsPerWorldUnit,
		-dyPixels / m_pixelsPerWorldUnit
	);
}

void EuclidRenderer2D::setGrid(
	const ivec2& gridDim,
	const vec2& worldOrigin,
	const vec2& cellSize) {

	m_gridDim = ivec2(max(0, gridDim.x), max(0, gridDim.y));
	m_gridOrigin = worldOrigin;
	m_cellSize = vec2(
		max(1.0e-6f, cellSize.x),
		max(1.0e-6f, cellSize.y)
	);

	_syncBoundaryFromGrid();
}

void EuclidRenderer2D::setGridStyle(int majorEvery, bool drawMinor) {
	m_gridMajorEvery = max(1, majorEvery);
	m_drawMinorGrid = drawMinor;
}

static inline int clampi(int v, int lo, int hi) {
	return (v < lo) ? lo : (v > hi ? hi : v);
}

void EuclidRenderer2D::setVisibleGridCellsPerAxis(int cellsPerAxis) {
	// For current EucliGen2D design:
	// 64 cells   -> 8 x 8
	// 256 cells  -> 16 x 16
	// 1024 cells -> 32 x 32
	// 4096 cells -> 64 x 64
	m_visibleGridCellsPerAxis = clampi(cellsPerAxis, 1, 64);
}

void EuclidRenderer2D::setGridSizeSelection(int selection) {
	// Accepts either the old selection values {4, 8, 16, 32}
	// or the new total-cell values {64, 256, 1024, 4096}.

	switch (selection) {
	case 4:
		setVisibleGridCellsPerAxis(8);
		break;

	case 8:
		setVisibleGridCellsPerAxis(16);
		break;

	case 16:
		setVisibleGridCellsPerAxis(32);
		break;

	case 32:
		setVisibleGridCellsPerAxis(64);
		break;

	case 64:
		setVisibleGridCellsPerAxis(8);
		break;

	case 256:
		setVisibleGridCellsPerAxis(16);
		break;

	case 1024:
		setVisibleGridCellsPerAxis(32);
		break;

	case 4096:
		setVisibleGridCellsPerAxis(64);
		break;

	default:
		// Conservative fallback.
		setVisibleGridCellsPerAxis(8);
		break;
	}
}

void EuclidRenderer2D::setBoundary(float boundaryMin, float boundaryMax) {
	if (boundaryMin > boundaryMax)
		swap(boundaryMin, boundaryMax);

	m_boundaryMin = boundaryMin;
	m_boundaryMax = boundaryMax;
}

void EuclidRenderer2D::_syncBoundaryFromGrid() {
	if (m_gridDim.x <= 0 || m_gridDim.y <= 0)
		return;

	const vec2 maxCorner = m_gridOrigin + vec2(m_gridDim) * m_cellSize;
	const float mn = min(m_gridOrigin.x, m_gridOrigin.y);
	const float mx = max(maxCorner.x, maxCorner.y);
	setBoundary(mn, mx);
}

float EuclidRenderer2D::_gridStepWorld() const {
	return _visibleGridStepWorld();
}

float EuclidRenderer2D::_visibleGridStepWorld() const {
	if (!m_gridEnabled || m_visibleGridCellsPerAxis <= 0)
		return 0.0f;

	const float span = m_boundaryMax - m_boundaryMin;
	return span / static_cast<float>(m_visibleGridCellsPerAxis);
}

void EuclidRenderer2D::display(DisplayMode mode) {
	if (!m_bInitialized)
		initialize(max(1, m_window_w), max(1, m_window_h));

	if (!m_pbo || !m_tex || !m_cuda_pbo_resource)
		return;

	_renderCUDA(mode);
	_drawFullscreenTexture();

	if (m_cpuOverlayEnabled)
		_drawCPUOverlay2D();
}

void EuclidRenderer2D::_renderCUDA(DisplayMode mode) {
	(void)mode;

	uchar4* dOut =
		static_cast<uchar4*>(mapGLBufferObject(&m_cuda_pbo_resource));

	if (!dOut)
		return;

	// If velocity has not been connected yet, the render kernel can still draw
	// the grid/background, but it cannot safely read radius from vel[i].w.
	const unsigned int safeParticleCount =
		(m_pos && m_vel && m_numParticles > 0)
		? static_cast<unsigned int>(m_numParticles)
		: 0u;

	particles2D_kernelLauncher(
		dOut,
		m_window_w, m_window_h,
		make_float2(m_cameraCenterWorld.x, m_cameraCenterWorld.y),
		m_pixelsPerWorldUnit,
		m_boundaryMin, m_boundaryMax,
		_visibleGridStepWorld(),
		m_pos,
		m_vel,
		m_particleColors,
		safeParticleCount,
		m_showParticles ? 1 : 0,
		m_fallbackParticleColor
	);

	unmapGLBufferObject(m_cuda_pbo_resource);

	// Copy CUDA-written PBO into the OpenGL texture.
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
	glBindTexture(GL_TEXTURE_2D, m_tex);
	glTexSubImage2D(
		GL_TEXTURE_2D, 0, 0, 0, m_window_w, m_window_h,
		GL_RGBA, GL_UNSIGNED_BYTE, NULL
	);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void EuclidRenderer2D::_drawFullscreenTexture() {

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(
		0.0,
		static_cast<double>(m_window_w),
		static_cast<double>(m_window_h),
		0.0
	);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glUseProgram(0);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, m_tex);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
	glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, static_cast<float>(m_window_h));
	glTexCoord2f(1.0f, 1.0f); glVertex2f(static_cast<float>(m_window_w), static_cast<float>(m_window_h));
	glTexCoord2f(1.0f, 0.0f); glVertex2f(static_cast<float>(m_window_w), 0.0f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}

void EuclidRenderer2D::_drawCPUOverlay2D() {
	// Optional immediate-mode debug overlay. The CUDA kernel already draws the
	// main grid. This routine is deliberately minimal so the PBO remains the
	// source of truth for the 2D display.
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(
		0.0,
		static_cast<double>(m_window_w),
		static_cast<double>(m_window_h),
		0.0
	);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glUseProgram(0);

	if (m_drawAxes) {
		const vec2 originScreen(
			0.5f * static_cast<float>(m_window_w) - m_cameraCenterWorld.x * m_pixelsPerWorldUnit,
			0.5f * static_cast<float>(m_window_h) + m_cameraCenterWorld.y * m_pixelsPerWorldUnit
		);

		glLineWidth(2.0f);
		glBegin(GL_LINES);
			glColor3f(1.0f, 0.25f, 0.25f);
			glVertex2f(originScreen.x, originScreen.y);
			glVertex2f(originScreen.x + 80.0f, originScreen.y);

			glColor3f(0.25f, 1.0f, 0.25f);
			glVertex2f(originScreen.x, originScreen.y);
			glVertex2f(originScreen.x, originScreen.y - 80.0f);
		glEnd();
		glLineWidth(1.0f);
	}

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}