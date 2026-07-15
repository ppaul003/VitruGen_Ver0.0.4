#ifndef __EUCLID_ENGINE_H__
#define __EUCLID_ENGINE_H__

#ifdef _WIN32
#include <Windows.h>
#endif

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/freeglut.h>

#include <helper_functions.h>
#include <helper_cuda.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>
#include <future>
#include <string>

#include <paramgl.h>

#include "Interactions.h"
#include "Camera.h"
#include "ViewPort.h"
#include "TheArbiter.h"
#include "TheTesseract.h"

#include "marchingCubes.h"
#include "particleSystem.h"
#include "renderer_Euclid.h"

class EuclidEngine {
public:
	EuclidEngine();
	~EuclidEngine();

	bool init(int argc, char** argv);
	void run();
	void shutdown();

private:
	static EuclidEngine* s_instance;

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
	void initParticleSystem(uint numParticles, uint3 gridSize);
	void initVolumeField();
	void initMarchingCubes();
	void initPixelBuffer();
	void initMenus();
	void rebuildMenus();

	// --- ENGINE SERVICES ---
	void computeFPS();
	void requestExit();

	void freeVolumeField();
	void freeMarchingCubes();
	void destroyPixelBuffer();
	void regenerateVolumeField();

	void setRadii(uint numParticles);
	void placeSingleParticleAtOrigin();
	void startSingleParticleConfigPreview();

	// --- WORKSPACE RENDERS ---
	void drawTesseractGridAndPlane();

	// --- WORKSPACE / RESOURCE SYNC ---
	void syncRenderingWithParticleSystem();
	void syncTesseractWorkspaceFromArbiter();
	void syncCameraBehaviorFromArbiter();
	void syncVolumeBoundaryStatusFromTesseract();

	void applyParticleSelectionsToSystem();
	void applySingleParticleConfigToSystem();
	void applySelectedParticleColorToSystem();

	void classifyMarchingCubesOnly();
	void extractMarchingCubesMesh();
	void exportCurrentMeshOBJ();

	void applyVoxelBaseCommit();

	void beginObjExportJob();
	void advanceObjExportJob();

	void appendObjExportLog(const std::string& line);
	void failObjExportJob(const std::string& reason);

	void closeObjExportPanel();

	bool handleObjExportModalKeyboard(const KeyboardInput::KeyEvent& event);

	bool isObjExportModalActive() const;
	bool isObjExportWorking() const;

	enum class ObjExportStage {
		NONE = 0,

		PREPARE_MESH,
		REPORT_CLASSIFICATION,
		REPORT_MC_VBO,
		REPORT_BOUNDS,
		REPORT_EXTRACTION,
		REPORT_ENGINE_EXTRACTION,

		BEGIN_FILE_WRITE,
		WAIT_FOR_FILE_WRITE,
		REPORT_FILE_WRITTEN,

		BEGIN_MESH_RELOAD,
		RELOAD_MESH,
		REPORT_MESH_LOADED,

		ACTIVATE_MESH_MODE,
		FINISH
	};

private:
	// --- CONSTANTS ---
	static constexpr uint kWidth = 1920;
	static constexpr uint kHeight = 1080;
	static constexpr uint kGridSize = 64;
	static constexpr uint kNumParticles = 256;

	static constexpr int MENU_NOP = -1;
	static constexpr int MENU_EDIT_SCALE_WHOLE = '1';
	static constexpr int MENU_EDIT_SCALE_Z = '2';
	static constexpr int MENU_EDIT_SCALE_Y = '3';
	static constexpr int MENU_EDIT_SCALE_X = '4';
	static constexpr int MENU_RESET_OBJECT_SCALE = '0';
	static constexpr int MENU_EDIT_ROT_PITCH = '5';
	static constexpr int MENU_EDIT_ROT_YAW = '6';
	static constexpr int MENU_EDIT_ROT_ROLL = '7';
	static constexpr int MENU_RESET_OBJECT_ROTATION = '8';
	static constexpr int MENU_SELECT_WORKPLANE_PARTICLE = 'e';
	static constexpr int MENU_EDIT_PARTICLE_MESH = 'e';
	static constexpr int MENU_GO_BACK_SUBLAYER = 'q';
	static constexpr int MENU_QUIT = 27;
	static constexpr int MENU_EXPORT_OBJ = 1001;
	// SUB_LAYER_3 Marching Cubes navigation.
	// These deliberately reuse the existing keyboard transition paths.
	static constexpr int MENU_MC_TO_SUB_LAYER_2 = 1002;
	static constexpr int MENU_MC_TO_SUB_LAYER_0 = 1003;
	// SUB_LAYER_2 assembly-node navigation.
	static constexpr int MENU_TO_NODE_PREVIEW = 1101;
	static constexpr int MENU_TO_NODE_EDIT_OBJECT = 1102;
	static constexpr int MENU_TO_NODE_OFFSET_OBJECT = 1103;
	static constexpr int MENU_TO_NODE_APPLY_BASE = 1104;
	static constexpr int MENU_RUN_MC_MODE = 1105;

	// Node 2 / Node 3 offset controls.
	// These remain placeholders during this checkpoint.
	static constexpr int MENU_OFFSET_Z_VECTOR = 1201;
	static constexpr int MENU_OFFSET_Y_VECTOR = 1202;
	static constexpr int MENU_OFFSET_X_VECTOR = 1203;
	static constexpr int MENU_RESET_OBJECT_OFFSET = 1204;
	// VOLUME_1 injection brush Boolean mode.
	static constexpr int MENU_TOGGLE_INJECTION_MODE = 1205;
	// Node 3 basis-rebase action.
	static constexpr int MENU_COMMIT_NEW_BASE_VECTOR = 1301;
	// Node_1 injection-brush local basis commit.
	static constexpr int MENU_COMMIT_BRUSH_BASE = 1302;
	

	static constexpr float kSimBox = 4.0f;
	static constexpr float kMarchingCubesIsoValue = 0.0f;

	// --- CORE COMPONENTS ---
	TheArbiter m_arbiter;
	Tesseract m_tesseract;

	// --- VIEW / DISPLAY COMPONENTS ---
	ViewPort m_viewport;
	CameraProcessor m_camera;

	// --- INPUT PORTS ---
	MouseInput m_mouse;
	KeyboardInput m_keyboard;

	// --- CUDA KERNEL / SIM / RENDER COMPONENTS ---
	ParticleSystem* m_psystem = nullptr;
	MarchingCubes* m_marchingCubes = nullptr;
	EuclidRenderer* m_renderer = nullptr;

	EuclidRenderer::DisplayMode m_displayMode =
		EuclidRenderer::PARTICLE_SPHERES;

	ViewPort::ObjExportPanelData m_objExportPanel;
	ObjExportStage m_objExportStage = ObjExportStage::NONE;

	// --- PARTICLE RADIUS BUFFER DATA ---
	std::vector<float> m_rad;

	// --- EXPORT .OBJ BUFFER DATA ---
	std::future<bool> m_objExportFuture;
	std::string m_objExportPath =
		"SINGLE_PARTICLE_DATA/p0.obj";

	// --- ENGINE STATE ---
	bool m_displayEnabled = true;
	bool m_sysMode = false;
	bool m_exiting = false;
	bool m_cleaned = false;
	bool m_bPause = false;
	bool m_displaySliders = false;
	bool m_singleParticlePlaced = false;
	bool m_mcMeshGenerated = false;
	bool m_mcRevealAnimating = false;
	bool m_objExportFutureActive = false;

	int m_iterations = 1;

	// --- GLUT / CUDA SDK SUPPORT ---
	int m_menuId = 0;
	int m_fpsCount = 0;
	int m_fpsLimit = 1;

	int m_objExportNextStepMs = 0;
	int m_objExportLastSpinnerMs = 0;
	int m_objExportCompleteUntilMs = 0;

	uint m_numParticles = 0;
	uint3 m_gridSizeDim{};

	// --- SIM PARAMETERS ---
	float m_timestep = 0.002f;
	float m_simTime = 0.0f;

	float m_damping = 1.0f;
	float m_gravity = 0.0f;

	float m_collideSpring = 0.0f;
	float m_collideDamping = 0.0f;
	float m_collideShear = 0.0f;
	float m_collideAttraction = 0.0f;

	float m_volumeFrameTheta = 0.0f;
	float m_volumeFramePhi = 0.0f;
	// Scroll target. Mouse wheel modifies this,
	// then m_volumeFrameZs eases toward it.

	float m_volumeThreshold = 0.0f;
	float m_volumeSliceDistance = 0.0f;

	float m_mcRevealT = 0.0f;

	GLuint m_pbo = 0;
	GLuint m_tex = 0;
	struct cudaGraphicsResource* m_cudaPboResource = nullptr;

	ParamListGL* m_params = nullptr;
	StopWatchInterface* m_timer = nullptr;
};
#endif
