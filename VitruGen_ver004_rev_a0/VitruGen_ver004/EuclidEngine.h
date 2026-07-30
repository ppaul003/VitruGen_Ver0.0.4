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
	// --- ENGINE-LOCAL TYPES ---
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

	static EuclidEngine* s_instance;

	// --- GLUT CALLBACK BRIDGE ---
	static void sDisplay();
	static void sReshape(int w, int h);
	static void sMouse(int button, int state, int x, int y);
	static void sMotion(int x, int y);
	static void sPassiveMotion(int x, int y);
	static void sMainMenu(int value);
	static void sKeyboard(unsigned char key, int x, int y);
	static void sIdle();
	static void sClose();

	// --- APPLICATION LIFECYCLE / PLATFORM SETUP ---
	void initGL(int* argc, char** argv);
	void initMenus();
	void rebuildMenus();
	void computeFPS();
	void requestExit();

	// --- RUNTIME EVENT HANDLERS ---
	void onDisplay();
	void onReshape(int w, int h);
	void onMouse(int button, int state, int x, int y);
	void onMotion(int x, int y);
	void onPassiveMotion(int x, int y);
	void onKeyboard(unsigned char key, int x, int y);
	void onIdle();
	void onClose();

	// --- WORKSPACE ROUTING / GLOBAL PRESENTATION ---
	void drawTesseractGridAndPlane();
	void syncTesseractWorkspaceFromArbiter();
	void syncCameraBehaviorFromArbiter();

	// --- SHARED PARTICLE RESOURCES ---
	void initRenderer();
	void initParticleSystems();
	void syncRenderingWithParticleSystem();


	// --- PARTICLE_SIM WORKSPACE (SIMCAD_4D) ---
	void applyParticleSelectionsToSystem();
	void applyPSSelectedParticleColorToSystem();

	// --- SINGLE_PARTICLE_MCAD WORKSPACE (GRID_3D) ---
	void initVolumeField();
	void initPixelBuffer();
	void destroyPixelBuffer();
	void freeVolumeField();
	void regenerateVolumeField();

	void placeSingleParticleAtOrigin();
	void startSingleParticleConfigPreview();
	void applySingleParticleConfigToSystem();
	void applySPSelectedParticleColorToSystem();
	void applyVoxelBaseCommit();
	void syncVolumeBoundaryStatusFromTesseract();

	// --- MARCHING CUBES / OBJ EXPORT PIPELINE ---
	void initMarchingCubes();
	void freeMarchingCubes();
	void extractMarchingCubesMesh();
	void exportCurrentMeshOBJ();
	void beginObjExportJob();
	void advanceObjExportJob();
	void appendObjExportLog(const std::string& line);
	void failObjExportJob(const std::string& reason);
	void closeObjExportPanel();
	bool handleObjExportModalKeyboard(const KeyboardInput::KeyEvent& event);
	bool isObjExportModalActive() const;
	bool isObjExportWorking() const;

	// --- CONSTANTS ---
	static constexpr uint kWidth = 1920;
	static constexpr uint kHeight = 1080;
	static constexpr uint kGridSize = 64;
	//static constexpr uint kNumParticles = 256;

	static constexpr uint kParticleSimCapacity = 16384;
	static constexpr uint kSingleParticleCapacity = 1;

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

	static constexpr float kMarchingCubesIsoValue = 0.0f;

	// --- APPLICATION CORE ---
	TheArbiter m_arbiter;
	Tesseract m_tesseract;
	ViewPort m_viewport;
	CameraProcessor m_camera;

	// --- INPUT ---
	MouseInput m_mouse;
	KeyboardInput m_keyboard;

	// --- SHARED PARTICLE SIMULATION / RENDER RESOURCES ---
	uint3 m_gridSizeDim{};
	EuclidRenderer* m_renderer = nullptr;

	// WORKSPACE RESOURCE OWNERSHIP BRANCH:
	//
	// SIMCAD_4D / PARTICLE_SIMULATION:
	ParticleSystem* m_particleSimSystem = nullptr;
	std::vector<float> m_particleSimRadii;

	uint m_particleSimCapacity = kParticleSimCapacity;
	// Reserved until ParticleSystem supports allocated capacity
	// separately from active simulation count.
	uint m_particleSimActiveCount = kParticleSimCapacity;

	//
	// GRID_3D / SINGLE_PARTICLE_MCAD:
	ParticleSystem* m_singleParticleSystem = nullptr;
	std::vector<float> m_singleParticleRadii;
	//
	// --- SINGLE_PARTICLE_MCAD / VOLUME / MESH RESOURCES ---
	GLuint m_pbo = 0;
	GLuint m_tex = 0;
	MarchingCubes* m_marchingCubes = nullptr;
	struct cudaGraphicsResource* m_cudaPboResource = nullptr;
	//
	float m_volumeFrameTheta = 0.0f;
	float m_volumeFramePhi = 0.0f;
	float m_volumeThreshold = 0.0f;
	float m_volumeSliceDistance = 0.0f;
	bool m_singleParticlePlaced = false;
	// --- SINGLE_PARTICLE_MCAD SUB COMPONENT:
	// --- OBJ EXPORT JOB ---
	ViewPort::ObjExportPanelData m_objExportPanel;
	ObjExportStage m_objExportStage = ObjExportStage::NONE;
	//
	bool m_objExportFutureActive = false;
	int m_objExportNextStepMs = 0;
	int m_objExportLastSpinnerMs = 0;
	int m_objExportCompleteUntilMs = 0;
	//
	std::future<bool> m_objExportFuture;
	std::string m_objExportPath = "SINGLE_PARTICLE_DATA/p0.obj";
	//
	// --- RUNTIME STATE / GLUT SUPPORT ---
	bool m_displayEnabled = true;
	bool m_exiting = false;
	bool m_cleaned = false;
	int m_menuId = 0;
	int m_fpsCount = 0;
	int m_fpsLimit = 1;
	StopWatchInterface* m_timer = nullptr;
};
#endif
