#ifndef __EUCLID_ENGINE_2D_H__
#define __EUCLID_ENGINE_2D_H__


#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/freeglut.h>

#include <helper_functions.h>
#include <helper_cuda.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

#include <param.h>
#include <paramgl.h>

#include <vector_types.h>
#include <vector_functions.h>

#include "Mouse2D.h"
#include "Camera2D.h"
#include "ViewPort2D.h"
#include "TheArbiter2D.h"
#include "TheTesseract2D.h"

#include "particleSystem2D.h"
#include "renderer2D_Euclid.h"

class EuclidEngine2D {
public:
	EuclidEngine2D();
	~EuclidEngine2D();

	bool init(int argc, char** argv);
	void run();
	void shutdown();

private:
	static EuclidEngine2D* s_instance;

	// --- GLUT STATIC CALLBACK THUNKS ---
	static void sDisplay();
	static void sReshape(int w, int h);
	static void sMouse(int button, int state, int x, int y);
	static void sMotion(int x, int y);
	static void sPassiveMotion(int x, int y);
	static void sMainMenu(int value);
	static void sKeyboard(unsigned char key, int x, int y);
	static void sSpecial(int key, int x, int y);
	static void sIdle();
	static void sClose();

	// --- REAL INSTANCE HANDLERS ---
	void onDisplay();
	void onReshape(int w, int h);
	void onMouse(int button, int state, int x, int y);
	void onMotion(int x, int y);
	void onPassiveMotion(int x, int y);
	void onKeyboard(unsigned char key, int x, int y);
	void onSpecial(int key, int x, int y);
	void onIdle();
	void onClose();

	// --- ENGINE SETUP ---
	void initGL(int* argc, char** argv);
	void initParticleSystem(unsigned int numParticles, uint2 gridSize);
	void initMenus();
	void rebuildMenus();

	// --- ENGINE SERVICES ---
	void updateSimulation();
	void draw2DGridScene();
	void computeFPS();
	void syncRenderingWithParticleSystem();
	void setRadii(unsigned int numParticles);
	void requestExit();

	void resetParticlesFromArbiterConfig();
	void applyGridSizeSelectionToSystem(bool frameCameraOnChange);
	float computeGridFitPixelsPerWorldUnit(float simBox) const;
	
	// --- PARTICLE LIFE PARAMETER SLIDERS ---
	void initParticleLifeSliders();
	void destroyParticleLifeSliders();

	void toggleParticleLifeSliders();
	void drawParticleLifeSliders();

	void syncParticleLifeParamsToSystem();
	void resetParticleLifeParams();

	// --- VIEWPORT / INPUT HELPERS ---
	void applyCommonSimulationParams();
	void configureRendererForCurrentViewport();
	bool getMouseGridLocal(int sx, int sy, bool allowOutside, int& gx, int& gy) const;
	void syncCameraFromRenderer();
	void syncRendererFromCamera(bool useLaggedCamera);


private:
	// --- CONSTANTS ---
	static constexpr uint kWidth = 1920;
	static constexpr uint kHeight = 1080;
	static constexpr uint kGridSize = 64;
	static constexpr uint kNumParticles = 4096;

	static constexpr int MENU_NOP = -1;

	static constexpr float kDefaultSimBox = 4.0f;
	static constexpr float kDefaultPixelsPerWorldUnit = 160.0f;

	static constexpr float kParticleLifeForceMin = -12.0f;
	static constexpr float kParticleLifeForceMax = 12.0f;
	static constexpr float kParticleLifeForceStep = 0.01f;

	// --- CORE COMPONENTS ---
	TheArbiter2D m_arbiter;
	Tesseract2D m_tesseract;

	// --- VIEW / DISPLAY COMPONENTS ---
	ViewPort2D m_viewport;
	CameraProcessor2D m_camera;

	// --- INPUT PORTS ---
	MouseInput2D m_mouse;
	KeyboardInput2D m_keyboard;

	// --- CUDA KERNEL / SIM / RENDER COMPONENTS ---
	ParticleSystem2D* m_psystem = nullptr;
	EuclidRenderer2D* m_renderer = nullptr;
	EuclidRenderer2D::DisplayMode m_displayMode =
		EuclidRenderer2D::PARTICLE_SPHERES;

	// --- PARTICLE RADIUS BUFFER DATA ---
	std::vector<float> m_rad;

	// --- ENGINE STATE ---
	bool m_displayEnabled = true;
	bool m_sysMode = false;
	bool m_exiting = false;
	bool m_cleaned = false;
	bool m_bPause = false;
	bool m_displaySliders = false;

	uint m_numParticles = 0;
	uint m_lastAppliedCellsPerAxis = 0;

	uint2 m_gridSizeDim{};

	// --- SIM PARAMETERS ---
	float m_timestep = 0.02f;
	float m_simTime = 0.0f;

	float m_damping = 1.0f;
	float m_gravity = 0.0f;
	int m_iterations = 1;

	float m_collideSpring = 0.0f;
	float m_collideDamping = 0.0f;
	float m_collideShear = 0.0f;
	float m_collideAttraction = 0.0f;

	float m_lastAppliedSimBox = -1.0f;

	// --- PARTICLE LIFE FORCE MATRIX ---
	// Row = source particle color, column = target/neighbor color.
	// Positive values can mean attraction; negative values can mean repulsion.
	float m_REDxRED = -2.0f;
	float m_REDxBLUE = 4.0f;
	float m_REDxGREEN = 1.0f;
	float m_REDxYELLOW = -1.0f;

	float m_BLUExRED = 1.0f;
	float m_BLUExBLUE = -2.0f;
	float m_BLUExGREEN = 4.0f;
	float m_BLUExYELLOW = 1.0f;

	float m_GREENxRED = 1.0f;
	float m_GREENxBLUE = 1.0f;
	float m_GREENxGREEN = -2.0f;
	float m_GREENxYELLOW = 4.0f;

	float m_YELLOWxRED = 4.0f;
	float m_YELLOWxBLUE = -1.0f;
	float m_YELLOWxGREEN = 1.0f;
	float m_YELLOWxYELLOW = -2.0f;

	// --- GLUT / CUDA SDK SUPPORT ---
	int m_menuId = 0;
	int m_fpsCount = 0;
	int m_fpsLimit = 1;

	ParamListGL* m_particleLifeSliders = nullptr;
	StopWatchInterface* m_timer = nullptr;
	
};
#endif