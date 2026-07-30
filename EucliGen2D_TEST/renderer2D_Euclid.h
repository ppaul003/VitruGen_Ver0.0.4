#ifndef __RENDER_2D_EUCLID__
#define __RENDER_2D_EUCLID__

#include <GL/glew.h>
#include <vector>
#include <cstdint>
#include <vector_types.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

class EuclidRenderer2D {
public:

	enum DisplayMode {
		PARTICLE_POINTS,
		PARTICLE_SPHERES,
		PARTICLE_NUM_MODES
	};

	EuclidRenderer2D();
	~EuclidRenderer2D();

	// Lifetime / GL resource management
	bool initialize(int windowW, int windowH);
	bool isInitialized() const { return m_bInitialized; }
	void finalize();

	// Window / framebuffer
	void setWindowSize(int w, int h);
	int getWindowWidth() const { return m_window_w; }
	int getWindowHeight() const { return m_window_h; }
	
	// CUDA particle buffers.
	// Both buffers are expected to be float4 arrays:
	//   pos = (x, y, z=0, mass)
	//   vel = (vx, vy, vz=0, radius)
	void setParticleDeviceBuffers(float* dPosFloat4, float* dVelFloat4, int numParticles);
	void setPositions(float* dPosFloat4, int numParticles);
	void setVelocities(float* dVelFloat4, int numParticles);
	void setRadius(float* r, int numParticles);

	// 2D camera / workplane controls
	void setCameraCenter(const glm::vec2& centerWorld) { m_cameraCenterWorld = centerWorld; }
	glm::vec2 getCameraCenter() const { return m_cameraCenterWorld; }

	void setPixelsPerWorldUnit(float pixelsPerWorldUnit);
	float getPixelsPerWorldUnit() const { return m_pixelsPerWorldUnit; }

	// Positive dx moves the mouse right; the world view pans accordingly.
	// Dragging the mouse right usually means the camera should reveal the world
	// to the left. This mimics grabbing and dragging a workplane.
	void panPixels(float dxPixels, float dyPixels) { m_cameraCenterWorld -= screenDeltaToWorldDelta(dxPixels, dyPixels); };
	void panWorld(const glm::vec2& deltaWorld) { m_cameraCenterWorld += deltaWorld; };

	// zoomFactor > 1 zooms in. zoomFactor < 1 zooms out.
	void zoom(float zoomFactor);
	void zoomAtScreenPoint(float zoomFactor, int mouseX, int mouseY);

	glm::vec2 screenToWorld(int sx, int sy) const;
	glm::vec2 screenDeltaToWorldDelta(float dxPixels, float dyPixels) const;

	// Grid / simulation boundary used by CUDA pixel renderer
	void setGrid(const glm::ivec2& gridDim, const glm::vec2& worldOrigin, const glm::vec2& cellSize);
	void setGridStyle(int majorEvery, bool drawMinor);
	void setGridEnabled(bool enabled) { m_gridEnabled = enabled; }
	void setDrawAxes(bool enabled) { m_drawAxes = enabled; }

	glm::ivec2 getGridDim() const { return m_gridDim; }
	glm::vec2 getGridOrigin() const { return m_gridOrigin; }
	glm::vec2 getCellSize() const { return m_cellSize; }

	// convenience square-boundary controls for the current kernel
	void setBoundary(float boundaryMin, float boundaryMax);
	float getBoundaryMin() const { return m_boundaryMin; }
	float getBoundaryMax() const { return m_boundaryMax; }

	void setPointSize(float s) { m_pointSize = s; }
	void setParticleRadius(float r) { m_particleRadius = r; }
	void setFOV(float fovDegrees) { m_fov = fovDegrees; }

	// main render entry point
	void display(DisplayMode mode = PARTICLE_SPHERES);

	void setShowParticles(bool show) { m_showParticles = show; }

	void setParticleColorBuffer(uchar4* dColor) { m_particleColors = dColor; }
	void setFallbackParticleColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
		m_fallbackParticleColor = uchar4{ r, g, b, a };
	}

	void setFallBackParticleColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
		setFallbackParticleColor(r, g, b, a);
	}

	void setVisibleGridCellsPerAxis(int cellsPerAxis);
	void setGridSizeSelection(int selection);

	int getVisibleGridCellsPerAxis() const { return m_visibleGridCellsPerAxis; }
	int getVisibleGridCellCount() const {
		return m_visibleGridCellsPerAxis * m_visibleGridCellsPerAxis;
	}
protected:
	void _initGL();
	void _initialize();

	void _createPBOAndTexture(int w, int h);
	void _destroyPBOAndTexture();
	void _renderCUDA(DisplayMode mode);
	void _drawFullscreenTexture();
	void _drawCPUOverlay2D();

	float _gridStepWorld() const;
	float _visibleGridStepWorld() const;

	void _syncBoundaryFromGrid();

protected:
	bool m_bInitialized;
	bool m_gridEnabled = true;
	bool m_drawMinorGrid = false;
	bool m_drawAxes = true;
	bool m_showParticles = false;
	bool m_cpuOverlayEnabled = false;

	int m_window_w, m_window_h;
	int m_radCapacity;
	int m_numParticles;
	int m_gridMajorEvery = 8;
	int m_visibleGridCellsPerAxis = 8;

	float* m_rad;
	float* m_pos;
	float* m_vel;

	float m_fov;
	float m_pointSize;
	float m_particleRadius;

	glm::ivec2 m_gridDim{ 0, 0 };
	glm::vec2 m_gridOrigin{ 0.0f, 0.0f };
	glm::vec2 m_cellSize{ 1.0f, 1.0f };

	glm::mat4 m_root = glm::mat4(1.0f);

	// 2D camera state
	glm::vec2 m_cameraCenterWorld{ 0.0f, 0.0f };
	float m_pixelsPerWorldUnit = 120.0f;
	float m_minPixelsPerWorldUnit = 10.0f;
	float m_maxPixelsPerWorldUnit = 5000.0f;

	// square sim boundary
	float m_boundaryMin = -2.0f;
	float m_boundaryMax = 2.0f;

	uchar4* m_particleColors;
	uchar4 m_fallbackParticleColor{ 255, 210, 80, 255 };

	GLuint m_pbo = 0;
	GLuint m_tex = 0;
	struct cudaGraphicsResource* m_cuda_pbo_resource = nullptr;
	
};
#endif
