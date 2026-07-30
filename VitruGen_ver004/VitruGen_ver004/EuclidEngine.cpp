#ifdef _WIN32
#include <direct.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <cstdio>
#include <fstream>
#include <string>
#include <sstream>

#include "kernel.h"
#include "EuclidEngine.h"

using namespace std;
using namespace glm;

EuclidEngine* EuclidEngine::s_instance = nullptr;

#ifdef _WIN32

namespace {

	HICON g_vitruGenLargeIcon = nullptr;
	HICON g_vitruGenSmallIcon = nullptr;

	bool applyVitruGenIconFromFile(
		const char* iconFilename) {

		if (!iconFilename || iconFilename[0] == '\0') {

			return false;
		}

		// Immediately after glutCreateWindow(), this should
		// resolve to the FreeGLUT window.
		HWND windowHandle = GetActiveWindow();

		// Fallback lookup using the original window title.
		if (!windowHandle) {

			windowHandle =
				FindWindowA(nullptr, "VitruGen Ver0.0.4");
		}

		if (!windowHandle) {

			printf(
				"[EuclidEngine] WARNING: "
				"Could not locate the VitruGen window.\n"
			);

			return false;
		}

		const int largeWidth = GetSystemMetrics(SM_CXICON);
		const int largeHeight = GetSystemMetrics(SM_CYICON);
		const int smallWidth = GetSystemMetrics(SM_CXSMICON);
		const int smallHeight = GetSystemMetrics(SM_CYSMICON);

		g_vitruGenLargeIcon =
			static_cast<HICON>(
				LoadImageA(
					nullptr,
					iconFilename,
					IMAGE_ICON,
					largeWidth,
					largeHeight,
					LR_LOADFROMFILE
				)
			);

		g_vitruGenSmallIcon =
			static_cast<HICON>(
				LoadImageA(
					nullptr,
					iconFilename,
					IMAGE_ICON,
					smallWidth,
					smallHeight,
					LR_LOADFROMFILE
				)
			);

		if (!g_vitruGenLargeIcon &&
			!g_vitruGenSmallIcon) {

			printf(
				"[EuclidEngine] WARNING: "
				"Could not load app icon: %s\n",
				iconFilename
			);

			return false;
		}

		if (g_vitruGenLargeIcon) {

			SendMessageA(
				windowHandle,
				WM_SETICON,
				ICON_BIG,
				reinterpret_cast<LPARAM>(g_vitruGenLargeIcon)
			);
		}

		if (g_vitruGenSmallIcon) {

			SendMessageA(
				windowHandle,
				WM_SETICON,
				ICON_SMALL,
				reinterpret_cast<LPARAM>(g_vitruGenSmallIcon)
			);
		}

		printf(
			"[EuclidEngine] Application icon loaded: %s\n",
			iconFilename
		);

		return true;
	}

	void releaseVitruGenWindowIcons() {

		if (g_vitruGenLargeIcon) {

			DestroyIcon(g_vitruGenLargeIcon);
			g_vitruGenLargeIcon = nullptr;
		}

		if (g_vitruGenSmallIcon) {

			DestroyIcon(g_vitruGenSmallIcon);
			g_vitruGenSmallIcon = nullptr;
		}
	}

} // namespace

#endif

// =============================================================================
// APPLICATION LIFECYCLE / INITIALIZATION
// =============================================================================
EuclidEngine::EuclidEngine() {}
EuclidEngine::~EuclidEngine() { shutdown(); }

bool EuclidEngine::init(int argc, char** argv) {
	printf("VitruGen Starting... \n\n");
	printf("Welcome to the Tesseract Generator Matrix! \n");
	printf("Anaheim Systems Dynamics Corp.\n\n");

	s_instance = this;

	// -----------------------------------------------------------------
	// Resource capacities
	// These are allocation limits, not current Arbiter selections.
	// -----------------------------------------------------------------
	m_particleSimCapacity = kParticleSimCapacity;
	m_particleSimActiveCount = kParticleSimCapacity;

	const uint gridDim = kGridSize;
	m_gridSizeDim = make_uint3(gridDim, gridDim, gridDim);


	printf(
		"particle simulation grid: %u x %u x %u = %u cells\n",
		m_gridSizeDim.x,
		m_gridSizeDim.y,
		m_gridSizeDim.z,
		m_gridSizeDim.x * m_gridSizeDim.y * m_gridSizeDim.z
	);

	printf(
		"particle simulation capacity: %u\n",
		m_particleSimCapacity
	);

	printf(
		"single-particle MCAD capacity: %u\n",
		kSingleParticleCapacity
	);

	initGL(&argc, argv);
	cudaGLInit(argc, argv);
	//
	initRenderer();
	initParticleSystems();
	//
	initVolumeField();
	initPixelBuffer();
	initMarchingCubes();
	//
	glutDisplayFunc(&EuclidEngine::sDisplay);
	glutReshapeFunc(&EuclidEngine::sReshape);
	glutMouseFunc(&EuclidEngine::sMouse);
	glutMotionFunc(&EuclidEngine::sMotion);
	glutPassiveMotionFunc(&EuclidEngine::sPassiveMotion);
	glutKeyboardFunc(&EuclidEngine::sKeyboard);
	glutIdleFunc(&EuclidEngine::sIdle);
	//
	glutCloseFunc(&EuclidEngine::sClose);
	return true;
}

void EuclidEngine::initGL(int* argc, char** argv) {
	glutInit(argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutInitWindowSize(kWidth, kHeight);
	glutCreateWindow("VitruGen Ver0.0.4");

#ifdef _WIN32

	applyVitruGenIconFromFile("VitruGen_Ver004.ico");

#endif

	initMenus();
	glewInit();
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.05f, 0.05f, 0.15f, 1.0f);

	m_viewport.resize(kWidth, kHeight);
	m_viewport.applyPerspective(60.0f);
}

// Shared ParticleSystem and EuclidRenderer resources used by both active
// particle workspaces.
void EuclidEngine::initRenderer() {

	m_renderer = new EuclidRenderer;

	m_renderer->setWindowSize(
		m_viewport.getWidth(),
		m_viewport.getHeight()
	);

	m_renderer->setFOV(60.0f);

	// shared default visual style
	m_renderer->setGridStyle(4, false);
	m_renderer->setGridMode3D();
	m_renderer->setParticleHighlighted(false);
}

void EuclidEngine::initParticleSystems() {
	// -------------------------------------------------------------
	// SIMCAD_4D particle simulation
	// -------------------------------------------------------------
	m_particleSimRadii.assign(
		kParticleSimCapacity,
		0.0f
	);

	m_particleSimSystem = new ParticleSystem(
		kParticleSimCapacity,
		m_gridSizeDim,
		true
	);

	// Establish the authoritative 4 × 4 × 4 domain before
	// the menu or any workspace attempts to render the grid.
	m_particleSimSystem->setSimulationDomain(
		m_tesseract.getPSConfig().simulationBoxSize
	);

	m_particleSimSystem->setDefaultColorRamp();

	m_particleSimSystem->reset(
		ParticleSystem::CNFG_DEFAULT_RESTART
	);

	// -------------------------------------------------------------
	// GRID_3D / SINGLE_PARTICLE_MCAD anchor
	// -------------------------------------------------------------
	m_singleParticleRadii.assign(
		kSingleParticleCapacity,
		0.0f
	);

	const uint3 singleParticleGrid = make_uint3(1u, 1u, 1u);

	m_singleParticleSystem = new ParticleSystem(
		kSingleParticleCapacity,
		singleParticleGrid,
		true
	);

	m_singleParticleSystem->setUniformParticleColor(1.0f, 0.0f, 0.0f, 1.0f);

	m_singleParticleSystem->reset(
		ParticleSystem::CNFG_DEFAULT_RESTART
	);

	/// Bind each resource to its proper Tesseract workspace.
	m_tesseract.bindParticleSimulationResources(
		m_particleSimSystem,
		m_renderer,
		&m_particleSimRadii
	);
	//
	m_tesseract.bindSingleParticleResources(
		m_singleParticleSystem,
		m_renderer,
		&m_singleParticleRadii
	);
	/// </summary>

	sdkCreateTimer(&m_timer);
}

// SINGLE_PARTICLE_MCAD scalar-field resources.
void EuclidEngine::initVolumeField() {
	const int3& v = m_tesseract.getVolumeSize();
	const size_t volumeBytes = m_tesseract.getVolumeBytes();

	// ---------------------------------------------------------
	// Validate the volume before allocating any CUDA resources.
	// ---------------------------------------------------------
	if (v.x <= 0 || v.y <= 0 || v.z <= 0 || volumeBytes == 0) {
		printf(
			"[EuclidEngine] Volume allocation skipped: "
			"invalid volume size.\n"
		);

		return;
	}

	// -----------------------------------------------------------------
	// Display/preview field.
	//
	// This is the field rendered by the CUDA raycaster and passed to
	// Marching Cubes.
	// -----------------------------------------------------------------
	if (!m_tesseract.hasVolume()) {
		float* dPreviewVolume = nullptr;

		allocateArray(
			reinterpret_cast<void**>(&dPreviewVolume),
			volumeBytes
		);

		m_tesseract.bindVolume(dPreviewVolume);
	}

	// -----------------------------------------------------------------
	// Persistent committed/base field.
	//
	// This holds geometry that has been permanently fused or cut.
	// The editable primitive is composed over this into dPreviewVolume.
	// -----------------------------------------------------------------
	if (!m_tesseract.hasCommittedVolume()) {
		float* dCommittedVolume = nullptr;

		allocateArray(
			reinterpret_cast<void**>(&dCommittedVolume),
			volumeBytes
		);

		m_tesseract.bindCommittedVolume(dCommittedVolume);
		// Positive SDF infinity means an initially empty field.
		m_tesseract.clearSPCommittedVolume();

	}
	// -----------------------------------------------------------------
	// Injection brush field.
	//
	// This is a separate scalar field used for Checkpoint 4C:
	//     pass 1 -> VOLUME_0 anchor
	//     pass 2 -> VOLUME_1 brush overlay
	// -----------------------------------------------------------------
	if (!m_tesseract.hasBrushVolume()) {
		float* dBrushVolume = nullptr;

		allocateArray(
			reinterpret_cast<void**>(&dBrushVolume),
			volumeBytes
		);

		m_tesseract.bindBrushVolume(dBrushVolume);

		clearVolumeKernelLauncher(dBrushVolume, v, 1.0e6f);
	}
	// ---------------------------------------------------------
	// Boundary-contact sensor resources.
	//
	// This allocates:
	//     device boundary mask
	//     device unsafe-patch counter
	//     CPU boundary-mask mirror
	// ---------------------------------------------------------
	if (!m_tesseract.initializeSPVolumeBoundarySensor()) {
		printf(
			"[EuclidEngine] WARNING: "
			"volume boundary sensor allocation failed.\n"
		);
	}
	printf(
		"[EuclidEngine] Volume fields allocated: "
		"%d x %d x %d, %.2f MB each, %.2f MB total\n",
		v.x, v.y, v.z,
		static_cast<double>(volumeBytes) / (1024.0 * 1024.0),
		static_cast<double>(3 * volumeBytes) / (1024.0 * 1024.0)
	);
}

void EuclidEngine::initMarchingCubes() {
	if (m_marchingCubes) return;


	m_marchingCubes = new MarchingCubes();

	if (!m_marchingCubes->init(m_tesseract.getVolumeSize())) {
		delete m_marchingCubes;
		m_marchingCubes = nullptr;

		printf("[EuclidEngine] Marching Cubes initialization failed.\n");
		return;
	}

	printf("[EuclidEngine] Marching Cubes initialized.\n");
}

// SINGLE_PARTICLE_MCAD CUDA/OpenGL volume-render interop resources.
void EuclidEngine::initPixelBuffer() {
	if (m_pbo || m_tex) return;

	glGenBuffers(1, &m_pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);

	glBufferData(
		GL_PIXEL_UNPACK_BUFFER,
		static_cast<size_t>(m_viewport.getWidth()) *
		static_cast<size_t>(m_viewport.getHeight()) *
		sizeof(GLubyte) *
		4,
		nullptr,
		GL_STREAM_DRAW
	);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glGenTextures(1, &m_tex);
	glBindTexture(GL_TEXTURE_2D, m_tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);

	registerGLBufferObject(m_pbo, &m_cudaPboResource);
	m_tesseract.bindSPCadVolumeResource(&m_cudaPboResource);

	if (m_renderer) {
		m_renderer->attachPixelBuffer(m_pbo);
		m_renderer->attachTexture(m_tex);
	}
}

void EuclidEngine::initMenus() {
	rebuildMenus();
}

void EuclidEngine::run() { glutMainLoop(); }

void EuclidEngine::shutdown() {
	if (m_cleaned) return;
	m_cleaned = true;

	if (m_timer) {
		sdkDeleteTimer(&m_timer);
		m_timer = nullptr;
	}

	if (m_objExportFutureActive && m_objExportFuture.valid()) {
		m_objExportFuture.wait();
		m_objExportFutureActive = false;
	}

	destroyPixelBuffer();
	freeMarchingCubes();
	freeVolumeField();

	if (m_renderer) {
		delete m_renderer;
		m_renderer = nullptr;
	}

	if (m_particleSimSystem) {
		delete m_particleSimSystem;
		m_particleSimSystem = nullptr;
	}

	if (m_singleParticleSystem) {
		delete m_singleParticleSystem;
		m_singleParticleSystem = nullptr;
	}

	m_particleSimRadii.clear();
	m_particleSimRadii.shrink_to_fit();

	m_singleParticleRadii.clear();
	m_singleParticleRadii.shrink_to_fit();

#ifdef _WIN32

	releaseVitruGenWindowIcons();

#endif
}

void EuclidEngine::computeFPS() {
	m_fpsCount++;

	if (m_fpsCount == m_fpsLimit) {
		char fps[256];
		float ifps = 1.f / (sdkGetAverageTimerValue(&m_timer) / 1000.f);
		sprintf(fps, "VitruGen 0.0.4:  %3.1f fps", ifps);

		glutSetWindowTitle(fps);
		m_fpsCount = 0;

		m_fpsLimit = (int)std::max(ifps, 1.f);
		sdkResetTimer(&m_timer);
	}
}

void EuclidEngine::requestExit() {
	if (m_exiting) return;

	m_exiting = true;
	glutIdleFunc(nullptr);
	s_instance = nullptr;
}

// =============================================================================
// CONTEXT MENU PRESENTATION / COMMAND ROUTING
// =============================================================================
void EuclidEngine::rebuildMenus() {
	glutDetachMenu(GLUT_RIGHT_BUTTON);

	if (m_menuId != 0) {
		glutDestroyMenu(m_menuId);
		m_menuId = 0;
	}

	m_menuId = glutCreateMenu(&EuclidEngine::sMainMenu);

	glutAddMenuEntry("=========================================", MENU_NOP);
	glutAddMenuEntry("- VitruGen Tesseract Behavioral Object -", MENU_NOP);
	glutAddMenuEntry("=========================================", MENU_NOP);

	if (m_arbiter.isSingleParticleReferenceSubLayer()) {
		glutAddMenuEntry(
			"* Select Workplane/particle",
			MENU_SELECT_WORKPLANE_PARTICLE
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
	}
	else if (m_arbiter.isWorkplaneParticleSelectSubLayer()) {
		if (m_arbiter.hasSelectedParticle()) {
			glutAddMenuEntry(
				"* Edit Particle Mesh",
				MENU_EDIT_PARTICLE_MESH
			);
		}

		glutAddMenuEntry("* Go back", MENU_GO_BACK_SUBLAYER);
		glutAddMenuEntry("=========================================", MENU_NOP);
	}
	else if (m_arbiter.isVolumeRenderSubLayer()) {

		switch (m_arbiter.getVolumeAssemblyNode()) {

		// -----------------------------------------------------------------
		// Node 0: Preview Object
		// -----------------------------------------------------------------
		default:
		case TheArbiter::VOLUME_NODE_PREVIEW:
			glutAddMenuEntry(
				"- Sub-Layer 2: Preview Object",
				MENU_NOP
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			glutAddMenuEntry(
				"Next Assembly Node:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Edit Object",
				MENU_TO_NODE_EDIT_OBJECT
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			glutAddMenuEntry(
				"To Sub-Layer 3:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Run MC mode",
				MENU_RUN_MC_MODE
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);
			break;

		// -----------------------------------------------------------------
		// Node 1: Edit Object
		// -----------------------------------------------------------------
		case TheArbiter::VOLUME_NODE_EDIT_OBJECT: {

			const bool injectionSelected =
				m_arbiter.hasInjectionVoxelSelected();

			const bool editingBrush =
				injectionSelected &&
				m_arbiter.isEditingInjectionVoxel1();

			// -------------------------------------------------------------
			// Context-sensitive Node_1 title.
			// -------------------------------------------------------------
			const char* nodeTitle =
				!injectionSelected
				? "- Sub-Layer 2: Edit Object"
				: editingBrush
				? "- Sub-Layer 2: Edit Brush Object"
				: "- Sub-Layer 2: Edit Anchor Object";

			glutAddMenuEntry(
				nodeTitle,
				MENU_NOP
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			// -------------------------------------------------------------
			// Shape editing.
			// -------------------------------------------------------------
			glutAddMenuEntry(
				"Edit Object Shape:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Scale whole object",
				MENU_EDIT_SCALE_WHOLE
			);

			glutAddMenuEntry(
				"* Scale z-axis",
				MENU_EDIT_SCALE_Z
			);

			glutAddMenuEntry(
				"* Scale y-axis",
				MENU_EDIT_SCALE_Y
			);

			glutAddMenuEntry(
				"* Scale x-axis",
				MENU_EDIT_SCALE_X
			);

			// Preserve the original reset command for the normal
			// Injection Voxels { NONE } Node_1 menu.
			if (!injectionSelected) {

				glutAddMenuEntry(
					"* Reset Scale",
					MENU_RESET_OBJECT_SCALE
				);
			}

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			// -------------------------------------------------------------
			// Rotation editing.
			// -------------------------------------------------------------
			glutAddMenuEntry(
				"Edit Object Rotation:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Pitch",
				MENU_EDIT_ROT_PITCH
			);

			glutAddMenuEntry(
				"* Yaw",
				MENU_EDIT_ROT_YAW
			);

			glutAddMenuEntry(
				"* Roll",
				MENU_EDIT_ROT_ROLL
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			// -------------------------------------------------------------
			// VOLUME_1 only: bake the brush-local basis.
			// -------------------------------------------------------------
			if (editingBrush) {

				glutAddMenuEntry(
					"Commit Brush Base:",
					MENU_NOP
				);

				glutAddMenuEntry(
					"* Commit Brush",
					MENU_COMMIT_BRUSH_BASE
				);

				glutAddMenuEntry(
					"=========================================",
					MENU_NOP
				);
			}

			// -------------------------------------------------------------
			// Node traversal.
			// -------------------------------------------------------------
			glutAddMenuEntry(
				"Next Assembly Node:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Offset Object",
				MENU_TO_NODE_OFFSET_OBJECT
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			glutAddMenuEntry(
				"Previous Assembly Node:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Preview Object",
				MENU_TO_NODE_PREVIEW
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			break;
		}

		// -----------------------------------------------------------------
		// Node 2: Offset Object
		// -----------------------------------------------------------------
		case TheArbiter::VOLUME_NODE_OFFSET_OBJECT: {

			const bool injectionSelected =
				m_arbiter.hasInjectionVoxelSelected();

			const bool editingBrush =
				injectionSelected &&
				m_arbiter.isEditingInjectionVoxel1();

			const bool editingAnchor =
				injectionSelected &&
				m_arbiter.isEditingInjectionVoxel0();

			// -------------------------------------------------------------
			// Context-sensitive Node_2 title.
			// -------------------------------------------------------------
			const char* nodeTitle =
				editingBrush
				? "- Sub-Layer 2: Offset Brush Object"
				: editingAnchor
				? "- Sub-Layer 2: Offset Anchor Object"
				: "- Sub-Layer 2: Offset Object";

			glutAddMenuEntry(
				nodeTitle,
				MENU_NOP
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			// -------------------------------------------------------------
			// Offset controls shared by VOLUME_0 and VOLUME_1.
			// -------------------------------------------------------------
			glutAddMenuEntry(
				"Edit Object Offset:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Offset z-vector",
				MENU_OFFSET_Z_VECTOR
			);

			glutAddMenuEntry(
				"* Offset y-vector",
				MENU_OFFSET_Y_VECTOR
			);

			glutAddMenuEntry(
				"* Offset x-vector",
				MENU_OFFSET_X_VECTOR
			);

			glutAddMenuEntry(
				"* Reset Offset",
				MENU_RESET_OBJECT_OFFSET
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			// -------------------------------------------------------------
			// VOLUME_1: brush-local injection mode.
			// -------------------------------------------------------------
			if (editingBrush) {

				glutAddMenuEntry(
					"Toggle Injection Brush:",
					MENU_NOP
				);

				char injectionModeLine[128];

				snprintf(
					injectionModeLine,
					sizeof(injectionModeLine),
					"* Injection Brush {%s}",
					m_arbiter.getVolumeInjectionModeName()
				);

				glutAddMenuEntry(
					injectionModeLine,
					MENU_TOGGLE_INJECTION_MODE
				);

				glutAddMenuEntry(
					"=========================================",
					MENU_NOP
				);
			}
			else {
				// ---------------------------------------------------------
				// VOLUME_0 / normal Node_2: advance toward Node_3.
				// ---------------------------------------------------------
				glutAddMenuEntry(
					"Next Assembly Node:",
					MENU_NOP
				);

				if (m_arbiter.canApplyVolumeToBase()) {

					glutAddMenuEntry(
						"* Apply to Base {READY}",
						MENU_TO_NODE_APPLY_BASE
					);
				}
				else if (m_arbiter.isVolumeBoundarySensorReady()) {

					glutAddMenuEntry(
						"- Apply to Base {BOUNDARY CONTACT}",
						MENU_NOP
					);
				}
				else {

					glutAddMenuEntry(
						"- Apply to Base {CHECKING}",
						MENU_NOP
					);
				}

				glutAddMenuEntry(
					"=========================================",
					MENU_NOP
				);

				glutAddMenuEntry(
					"Previous Assembly Node:",
					MENU_NOP
				);

				glutAddMenuEntry(
					"* Edit Object",
					MENU_TO_NODE_EDIT_OBJECT
				);

				glutAddMenuEntry(
					"=========================================",
					MENU_NOP
				);
			}
			break;
		}

		// -----------------------------------------------------------------
		// Node 3: Apply to Base
		// -----------------------------------------------------------------
		case TheArbiter::VOLUME_NODE_APPLY_TO_BASE:
			glutAddMenuEntry(
				"- Sub-Layer_2 Node: Apply to Base",
				MENU_NOP
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			glutAddMenuEntry(
				"Edit Base Vector:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Commit New Base Vector",
				MENU_COMMIT_NEW_BASE_VECTOR
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			glutAddMenuEntry(
				"Next Assembly Node:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Preview Object",
				MENU_TO_NODE_PREVIEW
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);

			glutAddMenuEntry(
				"Previous Assembly Node:",
				MENU_NOP
			);

			glutAddMenuEntry(
				"* Offset Object",
				MENU_TO_NODE_OFFSET_OBJECT
			);

			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);
			break;
		}
	}
	else if (m_arbiter.isMarchingCubesSubLayer()) {
		// -------------------------------------------------------------
		// SUB_LAYER_3: Marching Cubes
		// -------------------------------------------------------------
		glutAddMenuEntry(
			"- Sub-Layer 3: Marching Cubes",
			MENU_NOP
		);

		glutAddMenuEntry(
			"=========================================",
			MENU_NOP
		);

		// -------------------------------------------------------------
		// Mesh output.
		// -------------------------------------------------------------
		glutAddMenuEntry(
			"Output:",
			MENU_NOP
		);

		glutAddMenuEntry(
			"* Export .Obj",
			MENU_EXPORT_OBJ
		);

		glutAddMenuEntry(
			"=========================================",
			MENU_NOP
		);

		// -------------------------------------------------------------
		// Return to SUB_LAYER_2 Preview / Assembly Nodes.
		// -------------------------------------------------------------
		glutAddMenuEntry(
			"Previous Sub-Layer:",
			MENU_NOP
		);

		glutAddMenuEntry(
			"* Sub-Layer 2: Assembly Nodes",
			MENU_MC_TO_SUB_LAYER_2
		);

		glutAddMenuEntry(
			"=========================================",
			MENU_NOP
		);

		// -------------------------------------------------------------
		// Complete the CAD workflow and return to SUB_LAYER_0.
		// -------------------------------------------------------------
		glutAddMenuEntry(
			"Next Sub-Layer:",
			MENU_NOP
		);

		glutAddMenuEntry(
			"* Sub-Layer 0: Simulation Run",
			MENU_MC_TO_SUB_LAYER_0
		);

		glutAddMenuEntry(
			"=========================================",
			MENU_NOP
		);
	}

	glutAddMenuEntry("* Quit (esc)", MENU_QUIT);
	glutAddMenuEntry("=========================================", MENU_NOP);

	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

// =============================================================================
// GLUT CALLBACK BRIDGE
// =============================================================================
void EuclidEngine::sDisplay() {
	if (s_instance) s_instance->onDisplay();
}

void EuclidEngine::sReshape(int w, int h) {
	if (s_instance) s_instance->onReshape(w, h);
}

void EuclidEngine::sMouse(int b, int s, int x, int y) {
	if (s_instance) s_instance->onMouse(b, s, x, y);
}

void EuclidEngine::sMotion(int x, int y) {
	if (s_instance) s_instance->onMotion(x, y);
}

void EuclidEngine::sPassiveMotion(int x, int y) {
	if (s_instance) s_instance->onPassiveMotion(x, y);
}

void EuclidEngine::sMainMenu(int value) {
	if (!s_instance) return;
	if (value == MENU_NOP) return;

	auto applyArbiterMenuResult =
		[&](const TheArbiter::ArbiterResult& result) {

		if (result.commitVolumeFuse) {
			s_instance->applyVoxelBaseCommit();
		}

		// Menu-driven volume operations use the same regeneration
		// path as keyboard-controlled editing.
		if (result.regenerateVolume &&
			!result.commitVolumeFuse) {

			s_instance->regenerateVolumeField();
		}

		if (result.enterMarchingCubes) {
			s_instance->extractMarchingCubesMesh();
		}

		if (result.exportObjRequested) {
			s_instance->exportCurrentMeshOBJ();
		}

		s_instance->syncCameraBehaviorFromArbiter();

		if (result.rebuildMenu) {
			s_instance->rebuildMenus();
		}

		if (result.requestRedraw) {
			glutPostRedisplay();
		}
	};

	// ---------------------------------------------------------
	// Assembly-node transition helper.
	// ---------------------------------------------------------
	auto goToAssemblyNode =
		[&](TheArbiter::VolumeAssemblyNode node) {

		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter.setVolumeAssemblyNode(node);

		applyArbiterMenuResult(result);
	};

	// ---------------------------------------------------------
	// Node_2 offset-vector selection helper.
	// ---------------------------------------------------------
	auto selectOffsetVector =
		[&](TheArbiter::OffsetVector vector) {

		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter.setOffsetVectorSelection(vector);

		applyArbiterMenuResult(result);
	};

	switch (value) {

		// =========================================================
		// Assembly-node navigation
		// =========================================================
	case MENU_TO_NODE_PREVIEW:
		goToAssemblyNode(
			TheArbiter::VOLUME_NODE_PREVIEW
		);
		return;

	case MENU_TO_NODE_EDIT_OBJECT:
		goToAssemblyNode(
			TheArbiter::VOLUME_NODE_EDIT_OBJECT
		);
		return;

	case MENU_TO_NODE_OFFSET_OBJECT:
		goToAssemblyNode(
			TheArbiter::VOLUME_NODE_OFFSET_OBJECT
		);
		return;

	case MENU_TO_NODE_APPLY_BASE:
		goToAssemblyNode(
			TheArbiter::VOLUME_NODE_APPLY_TO_BASE
		);
		return;

	case MENU_RUN_MC_MODE: {
		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.enterMarchingCubesFromPreview();

		applyArbiterMenuResult(result);
		return;
	}

	// =========================================================
	// Node_1 injection-brush local basis commit
	// =========================================================
	case MENU_COMMIT_BRUSH_BASE: {

		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.commitBrushBaseFromMenu();

		applyArbiterMenuResult(result);

		return;
	}

	// =========================================================
	// Node_3 commit
	// =========================================================
	case MENU_COMMIT_NEW_BASE_VECTOR: {
		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.commitObjectBasisAndReturnToPreview();

		applyArbiterMenuResult(result);
		return;
	}

	// =========================================================
	// Node_2 offset-vector commands
	// =========================================================
	case MENU_OFFSET_X_VECTOR:
		selectOffsetVector(
			TheArbiter::OFFSET_VECTOR_X
		);
		return;

	case MENU_OFFSET_Y_VECTOR:
		selectOffsetVector(
			TheArbiter::OFFSET_VECTOR_Y
		);
		return;

	case MENU_OFFSET_Z_VECTOR:
		selectOffsetVector(
			TheArbiter::OFFSET_VECTOR_Z
		);
		return;

	// =========================================================
	// Node_2 VOLUME_1 injection mode
	// =========================================================
	case MENU_TOGGLE_INJECTION_MODE: {

		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.toggleVolumeInjectionModeFromMenu();

		applyArbiterMenuResult(result);

		return;
	}

	case MENU_RESET_OBJECT_OFFSET: {
		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.clearObjectOffsetFromMenu();

		applyArbiterMenuResult(result);
		return;
	}


	// =========================================================
	// Marching Cubes sub-layer navigation
	// =========================================================
	case MENU_MC_TO_SUB_LAYER_2: {

		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.activateMarchingCubesPanelItemFromMenu(
				TheArbiter::MC_LIST_TO_SUB_LAYER_2
			);

		applyArbiterMenuResult(result);
		return;
	}

	case MENU_MC_TO_SUB_LAYER_0: {

		TheArbiter::ArbiterResult result =
			s_instance->m_arbiter
			.activateMarchingCubesPanelItemFromMenu(
				TheArbiter::MC_LIST_TO_SUB_LAYER_0
			);

		applyArbiterMenuResult(result);
		return;
	}
	// =========================================================
	// OBJ export
	// =========================================================
	case MENU_EXPORT_OBJ:
		s_instance->exportCurrentMeshOBJ();
		glutPostRedisplay();
		return;

	default:
		break;
	}

	// Existing keyboard-backed menu commands:
	// scale, rotation, workplane selection, back, and quit.
	s_instance->onKeyboard(
		static_cast<unsigned char>(value),
		0,
		0
	);
}

void EuclidEngine::sKeyboard(unsigned char k, int x, int y) {
	if (s_instance) s_instance->onKeyboard(k, x, y);
}

void EuclidEngine::sIdle() {
	if (s_instance) s_instance->onIdle();
}

void EuclidEngine::sClose() {
	if (s_instance) s_instance->onClose();
}

// =============================================================================
// RESOURCE TEARDOWN
// =============================================================================
void EuclidEngine::destroyPixelBuffer() {
	if (m_cudaPboResource) {
		unregisterGLBufferObject(m_cudaPboResource);
		m_cudaPboResource = nullptr;
	}

	if (m_pbo) {
		glDeleteBuffers(1, &m_pbo);
		m_pbo = 0;
	}

	if (m_tex) {
		glDeleteTextures(1, &m_tex);
		m_tex = 0;
	}

	if (m_renderer) {
		m_renderer->attachPixelBuffer(0);
		m_renderer->attachTexture(0);
	}
}

void EuclidEngine::freeVolumeField() {
	m_tesseract.releaseSPVolumeBoundarySensor();
	m_arbiter.setVolumeBoundaryStatus(false, 0);

	float* dPreviewVolume = m_tesseract.getVolume();
	if (dPreviewVolume) {
		freeArray(dPreviewVolume);
		m_tesseract.clearVolumeBinding();
	}

	float* dCommittedVolume = m_tesseract.getCommittedVolume();
	if (dCommittedVolume) {
		freeArray(dCommittedVolume);
		m_tesseract.clearCommittedVolumeBinding();
	}

	float* dBrushVolume = m_tesseract.getBrushVolume();
	if (dBrushVolume) {
		freeArray(dBrushVolume);
		m_tesseract.clearBrushVolumeBinding();
	}
}

void EuclidEngine::freeMarchingCubes() {
	if (!m_marchingCubes) return;

	m_marchingCubes->shutdown();

	delete m_marchingCubes;
	m_marchingCubes = nullptr;
}

// =============================================================================
// SHARED PARTICLE CONFIGURATION
// =============================================================================

void EuclidEngine::drawTesseractGridAndPlane() {
	if (!m_tesseract.applyWorkspaceBoundaryGridVisual() || 
		!m_renderer) return;

	const Tesseract::WorkspaceGridVisualConfig& gridConfig =
		m_tesseract.getWorkspaceGridVisualConfig();

	// Draw the full 3D Tesseract boundary/grid.
	m_renderer->setGridMode3D();
	m_renderer->displayGrid();

	// ---------------------------------------------------------
	// Menu-layer moving diagnostic slice.
	// ---------------------------------------------------------

	if (m_arbiter.isMenuLayer() ||
		m_tesseract.isTransitioningToWorkspace()) {

		const int halfSlice =
			m_tesseract.getWorkspaceGridHalfSliceRange();

		const float cycle =
			m_tesseract.getSliceAnimation();

		const int segment =
			static_cast<int>(cycle);

		const float local =
			cycle - static_cast<float>(segment);

		const int sliceOffset =
			static_cast<int>(round(-halfSlice +
					local * static_cast<float>(halfSlice * 2)));

		EuclidRenderer::WorkPlane plane =
			EuclidRenderer::PLANE_XY;

		if (segment == 0) {
			// XY plane moves through Z.
			plane = EuclidRenderer::PLANE_XY;
		}
		else if (segment == 1) {
			// XZ plane moves through Y.
			plane = EuclidRenderer::PLANE_XZ;
		}
		else {
			// YZ plane moves through X.
			plane = EuclidRenderer::PLANE_YZ;
		}

		m_renderer->setGridMode2D(
			plane,
			sliceOffset
		);

		m_renderer->displayGrid();

		// Restore the shared renderer's default state.
		m_renderer->setGridMode3D();
	}
}

// =============================================================================
// PARTICLE_SIM WORKSPACE SUPPORT (SIMCAD_4D)
// =============================================================================
void EuclidEngine::applyParticleSelectionsToSystem() {
	if (!m_particleSimSystem) return;

	ParticleSystem::ParticleConfig config = ParticleSystem::CNFG_DEFAULT_RESTART;
	if (m_arbiter.getParticleResetMode() == TheArbiter::PARTICLE_RESET_RANDOM)
		config = ParticleSystem::CNFG_RANDOM_RESTART;

	if (!m_tesseract.resetPSWorkspace(config))
		return;

	applyPSSelectedParticleColorToSystem();
	syncRenderingWithParticleSystem();
}

void EuclidEngine::applyPSSelectedParticleColorToSystem() {
	if (!m_particleSimSystem) return;

	const TheArbiter::ParticleSimDraftConfig& draft =
		m_arbiter.getParticleSimDraftConfig();

	if (draft.colorMode == TheArbiter::ParticleColorMode::Default) {

		m_particleSimSystem->setDefaultColorRamp();

		return;
	}

	// RGB runtime assignment is implemented in the later
	// active-count / RGB allocation sprint.
	//
	// For now, do not pretend the RGB draft values have been
	// applied.

	m_particleSimSystem->setDefaultColorRamp();
}
// =============================================================================
// GLOBAL TESSERACT PRESENTATION
// =============================================================================


// =============================================================================
// SINGLE_PARTICLE_MCAD WORKSPACE SUPPORT (GRID_3D)
// =============================================================================
void EuclidEngine::regenerateVolumeField() {
	if (!m_tesseract.hasVolume()) {
		initVolumeField();
	}

	m_tesseract.regenerateSPVolumeField(m_arbiter);
	syncVolumeBoundaryStatusFromTesseract();
}

void EuclidEngine::applySingleParticleConfigToSystem() {
	applySPSelectedParticleColorToSystem();

	m_tesseract.applySPConfig(m_arbiter.getParticleRadius());
	m_singleParticlePlaced = m_tesseract.isPlacedSP();
}

void EuclidEngine::applySPSelectedParticleColorToSystem() {
	if (!m_singleParticleSystem) return;

	switch (m_arbiter.getParticleColorSelection()) {
	case TheArbiter::PARTICLE_COLOR_BLUE:
		m_singleParticleSystem->setUniformParticleColor(0.0f, 0.25f, 1.0f, 1.0f);
		break;

	case TheArbiter::PARTICLE_COLOR_GREEN:
		m_singleParticleSystem->setUniformParticleColor(0.0f, 1.0f, 0.25f, 1.0f);
		break;

	default:
	case TheArbiter::PARTICLE_COLOR_RED:
		m_singleParticleSystem->setUniformParticleColor(1.0f, 0.05f, 0.0f, 1.0f);
		break;
	}
}

void EuclidEngine::placeSingleParticleAtOrigin() {
	m_tesseract.enterWorkspace(
		TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD
	);

	applySPSelectedParticleColorToSystem();

	m_singleParticlePlaced =
		m_tesseract.placeSPAnchor(
			m_arbiter.getParticleRadius()
		);
}

void EuclidEngine::startSingleParticleConfigPreview() {
	if (!m_arbiter.isParticleConfigLayer() ||
		!m_arbiter.isSingleParticleSelected()) return;

	// ---------------------------------------------------------
	// Layer_2 SINGLE_PARTICLE live preview.
	//
	// This is not the Layer_3 run commit. It only creates the
	// visible preview anchor so the user can monitor:
	//
	//     color
	//     radius
	//     render mode
	//
	// before pressing RUN SIMULATION LAYER.
	// ---------------------------------------------------------
	m_tesseract.enterWorkspace(TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD);
	applySPSelectedParticleColorToSystem();

	if (!m_tesseract.isPlacedSP()) {

		m_singleParticlePlaced =
			m_tesseract.placeSPAnchor(m_arbiter.getParticleRadius());
	}
	else {

		m_tesseract.applySPConfig(m_arbiter.getParticleRadius());
		m_singleParticlePlaced = true;
	}

	if (m_renderer) {
		m_renderer->setGridMode3D();
		m_renderer->setParticleHighlighted(false);
	}

	m_camera.setBehaviorMode(
		CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
	);
}

void EuclidEngine::applyVoxelBaseCommit() {
	if (!m_tesseract.hasCommittedVolume())
		return;

	if (!m_tesseract.commitSPWorkingVolume(m_arbiter)) {

		syncVolumeBoundaryStatusFromTesseract();
		glutPostRedisplay();
		return;
	}

	m_arbiter.finalizeVoxelBaseCommit();
	m_tesseract.copyCommittedVolumeToPreview();
	// The copied BASE is still the exact safe field that passed
	// the authoritative commit-time sensor check.

	m_tesseract.updateSPVolumeBoundarySensor(0.0f, 0.0f);
	syncVolumeBoundaryStatusFromTesseract();
}

// =============================================================================
// WORKSPACE ROUTING / SHARED RESOURCE SYNCHRONIZATION
// =============================================================================
void EuclidEngine::syncRenderingWithParticleSystem() {
	m_tesseract.syncPSRendering();
}

void EuclidEngine::syncTesseractWorkspaceFromArbiter() {
	TheArbiter::WorkspaceId targetWorkspace =
		TheArbiter::WorkspaceId::NONE;

	const bool singleParticleConfigPreviewActive =
		m_arbiter.isParticleConfigLayer() &&
		m_arbiter.isSingleParticleSelected() &&
		m_tesseract.isPlacedSP();

	const bool singleParticleRunActive =
		m_arbiter.isSimulationRunLayer() &&
		m_arbiter.isSingleParticleSelected();

	const bool particleSimulationRunActive =
		m_arbiter.isSimulationRunLayer() &&
		m_arbiter.isParticleSimulationSelected();

	if (particleSimulationRunActive) {
		targetWorkspace =
			TheArbiter::WorkspaceId::PARTICLE_SIMULATION;
	}
	else if (singleParticleConfigPreviewActive || singleParticleRunActive) {
		targetWorkspace =
			TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD;
	}

	if (m_tesseract.getActiveWorkspace() == targetWorkspace) {
		return;
	}

	if (targetWorkspace == TheArbiter::WorkspaceId::NONE) {
		m_tesseract.exitWorkspace();
	}
	else {
		m_tesseract.enterWorkspace(targetWorkspace);
	}
}
void EuclidEngine::syncCameraBehaviorFromArbiter() {
	if (m_arbiter.isMenuLayer()) {
		m_camera.setBehaviorMode(
			CameraProcessor::CAM_MENU_PREVIEW
		);
		return;
	}

	if (m_arbiter.isSingleParticleSelected()) {
		if (m_arbiter.isParticleConfigLayer()) {
			m_camera.setBehaviorMode(
				CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
			);
			return;
		}

		if (m_arbiter.isSimulationRunLayer()) {
			if (m_arbiter.isWorkplaneParticleSelectSubLayer()) {
				m_camera.setBehaviorMode(
					CameraProcessor::CAM_SINGLE_PARTICLE_WORKPLANE_LOCKED
				);
				return;
			}

			if (m_arbiter.isVolumeRenderSubLayer()) {
				m_camera.setBehaviorMode(
					CameraProcessor::CAM_SINGLE_PARTICLE_VOLUME
				);
				return;
			}

			if (m_arbiter.isMarchingCubesSubLayer()) {
				m_camera.setBehaviorMode(
					CameraProcessor::CAM_SINGLE_PARTICLE_MARCHING_CUBES
				);
				return;
			}

			if (m_arbiter.isSingleParticleReferenceSubLayer()) {
				m_camera.setBehaviorMode(
					CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
				);
				return;
			}

			// Safety fallback for future SINGLE_PARTICLE sub-layers.
			m_camera.setBehaviorMode(
				CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
			);
			return;
		}
	}

	m_camera.setBehaviorMode(
		CameraProcessor::CAM_STANDARD_3D_ORBIT
	);
}

void EuclidEngine::syncVolumeBoundaryStatusFromTesseract() {
	const bool changed =
		m_arbiter.setVolumeBoundaryStatus(m_tesseract.isSPVolumeBoundarySensorReady(),
			m_tesseract.getSPVolumeBoundaryUnsafeCount());

	if (changed && m_arbiter.isVolumeRenderSubLayer()) {
		rebuildMenus();
	}
}

// =============================================================================
// OBJ EXPORT MODAL INPUT
// =============================================================================
bool EuclidEngine::handleObjExportModalKeyboard(const KeyboardInput::KeyEvent& event) {
	if (!isObjExportModalActive()) return false;

	using PanelMode = ViewPort::ObjExportPanelMode;
	if (m_objExportPanel.mode == PanelMode::CONFIRM) {

		switch (event.signal) {

		case KeyboardInput::KEY_W:
		case KeyboardInput::KEY_S:
		case KeyboardInput::KEY_A:
		case KeyboardInput::KEY_D:
			m_objExportPanel.yesSelected =
				!m_objExportPanel.yesSelected;

			glutPostRedisplay();
			return true;

		case KeyboardInput::KEY_1:
			m_objExportPanel.yesSelected = true;
			glutPostRedisplay();
			return true;

		case KeyboardInput::KEY_2:
			m_objExportPanel.yesSelected = false;
			glutPostRedisplay();
			return true;

		case KeyboardInput::KEY_E:
		case KeyboardInput::KEY_ENTER:

			if (m_objExportPanel.yesSelected) {
				beginObjExportJob();
			}
			else {
				closeObjExportPanel();
			}

			return true;

		case KeyboardInput::KEY_Q:
		case KeyboardInput::KEY_ESCAPE:
			closeObjExportPanel();
			return true;

		default:
			return true;
		}
	}

	// Ignore all normal controls while the export is running.
	if (m_objExportPanel.mode ==
		PanelMode::WORKING) {

		return true;
	}

	// Failure and completion may also be dismissed manually.
	if (event.signal == KeyboardInput::KEY_E ||
		event.signal == KeyboardInput::KEY_ENTER ||
		event.signal == KeyboardInput::KEY_Q ||
		event.signal == KeyboardInput::KEY_ESCAPE) {

		closeObjExportPanel();
		return true;
	}

	return true;
}

// =============================================================================
// RUNTIME EVENT HANDLERS
// =============================================================================
void EuclidEngine::onReshape(int w, int h) {
	m_viewport.resize(w, h);
	m_viewport.applyPerspective(60.0F);

	if (m_renderer) {
		m_renderer->setWindowSize(w, h);
		m_renderer->setFOV(60.0f);
	}

	destroyPixelBuffer();
	initPixelBuffer();
}

void EuclidEngine::onDisplay() {
	if (m_exiting || m_cleaned) return;
	sdkStartTimer(&m_timer);

	// 1. Sync active workspace and update runtime behavior.
	syncTesseractWorkspaceFromArbiter();
	Tesseract::WorkspaceUpdateContext updateCtx{};
	m_tesseract.updateActiveWorkspace(updateCtx);

	// 2. Prepare display monitor / viewport.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	m_viewport.applyPerspective(60.0f);

	// 3. Update Tesseract internal animation state.
	float timeS = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
	m_tesseract.updateAnimBehavior(timeS);

	// 4. Apply camera view transform based on Arbiter layer.
	syncCameraBehaviorFromArbiter();
	m_camera.updateLag();
	m_camera.updatePocketZoomLag();

	if (m_tesseract.consumeCameraFocusRequest()) {
		const bool singleParticleCameraContext =
			m_arbiter.isSingleParticleSelected() &&
			(m_arbiter.isParticleConfigLayer() ||
				m_arbiter.isSimulationRunLayer());

		if (singleParticleCameraContext) {
			syncCameraBehaviorFromArbiter();
		}
		else {
			m_camera.focus3DFromMenu(m_tesseract.getPreviewRotation());
		}
	}

	if (m_arbiter.isMenuLayer() || m_tesseract.isOrientingToWorkspace()) {
		m_camera.applyMenuCameraTransform(
			m_tesseract.getPreviewRotation()
		);
	}
	else {
		m_camera.applyViewCameraTransform();
	}

	// 5. Capture current model-view matrix into the Tesseract.
	// 6. Draw the production Tesseract grid only when the active
	// workspace is not the local SINGLE_PARTICLE_MCAD workspace.
	const float degreesToRadians = 0.01745329251994329577f;
	const float* rot = m_camera.getLaggedRotation();

	m_volumeFramePhi = rot[0] * degreesToRadians;
	m_volumeFrameTheta = rot[1] * degreesToRadians;

	const bool singleParticleWorkspaceActive =
		m_tesseract.getActiveWorkspace() ==
		TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD;

	const bool particleSimulationWorkspaceActive =
		m_tesseract.getActiveWorkspace() ==
		TheArbiter::WorkspaceId::PARTICLE_SIMULATION;

	// The global diagnostic grid is used by the menu and configuration
	// layers. Active particle workspaces own their local render context.
	if (!singleParticleWorkspaceActive &&
		!particleSimulationWorkspaceActive) {

		drawTesseractGridAndPlane();
	}

	if (particleSimulationWorkspaceActive ||
		singleParticleWorkspaceActive) {
		Tesseract::WorkspaceRenderContext renderCtx;

		renderCtx.arbiter = &m_arbiter;
		renderCtx.marchingCubes = m_marchingCubes;

		renderCtx.displayMode = EuclidRenderer::PARTICLE_SPHERES;
		renderCtx.displayEnabled = m_displayEnabled;

		renderCtx.viewportW = m_viewport.getWidth();
		renderCtx.viewportH = m_viewport.getHeight();

		renderCtx.thetaRad = m_volumeFrameTheta;
		renderCtx.phiRad = m_volumeFramePhi;

		renderCtx.particleWorkspaceZs =
			m_camera.getParticleWorkspaceZs();
		renderCtx.volumeRenderZs =
			m_camera.getVolumeRenderZs();

		renderCtx.threshold = m_volumeThreshold;
		renderCtx.sliceDistance = m_volumeSliceDistance;

		m_tesseract.renderActiveWorkspace(renderCtx);
		syncVolumeBoundaryStatusFromTesseract();
	}

	// 7. Draw particles only when a mode explicitly requests particle rendering.
	const bool meshAvailable =
		m_renderer &&
		m_renderer->hasParticleMeshOBJ();

	ViewPort::MarchingCubesPanelData mcPanelData;
	const ViewPort::MarchingCubesPanelData* mcPanelDataPtr = nullptr;

	if (m_arbiter.isMarchingCubesSubLayer()) {

		mcPanelDataPtr = &mcPanelData;

		if (m_marchingCubes &&
			m_marchingCubes->isInitialized()) {

			const uint3 gridSize =
				m_marchingCubes->getGridSize();

			mcPanelData.available = true;
			mcPanelData.meshReady =
				m_marchingCubes->hasTriangleData();

			mcPanelData.gridX = gridSize.x;
			mcPanelData.gridY = gridSize.y;
			mcPanelData.gridZ = gridSize.z;

			mcPanelData.totalVoxels =
				m_marchingCubes->getNumVoxels();

			mcPanelData.activeVoxels =
				m_marchingCubes->getActiveVoxelCount();

			mcPanelData.totalVertices =
				m_marchingCubes->getTotalVertexCount();

			mcPanelData.totalTriangles =
				m_marchingCubes->getGeneratedTriangleCount();

			mcPanelData.isoValue = kMarchingCubesIsoValue;
		}
	}

	const ViewPort::ObjExportPanelData* exportPanelDataPtr = nullptr;
	if (m_objExportPanel.mode != ViewPort::ObjExportPanelMode::HIDDEN) {

		exportPanelDataPtr = &m_objExportPanel;
	}

	// 8. Draw screen-space overlay.
	m_viewport.drawOverlay(
		m_arbiter,
		mcPanelDataPtr,
		exportPanelDataPtr,
		m_tesseract.isActiveWorkspacePaused(),
		meshAvailable);

	// 9. End frame.
	sdkStopTimer(&m_timer);
	computeFPS();
	glutSwapBuffers();
}
void EuclidEngine::onMouse(int button, int state, int x, int y) {
	if (isObjExportModalActive()) return;

	const bool isWheel = (button == 3 || button == 4);

	const bool singleParticleConfigPreviewActive =
		m_arbiter.isParticleConfigLayer() &&
		m_arbiter.isSingleParticleSelected() &&
		m_tesseract.isPlacedSP();

	const bool singleParticleOpenGLCameraActive =
		singleParticleConfigPreviewActive ||
		(m_arbiter.isSimulationRunLayer() &&
			m_arbiter.isSingleParticleSelected() &&
			(m_arbiter.isSingleParticleReferenceSubLayer() ||
				m_arbiter.isWorkplaneParticleSelectSubLayer()));

	const bool singleParticleVolumeCameraActive =
		m_arbiter.isSimulationRunLayer() &&
		m_arbiter.isSingleParticleSelected() &&
		(m_arbiter.isVolumeRenderSubLayer() ||
			m_arbiter.isMarchingCubesSubLayer());

	// IMPORTANT:
	// Only intercept mouse wheel for SINGLE_PARTICLE camera-distance routing.
	// Do not intercept left click, because MouseInput::onButton()
	// must receive left-button down/up to enable orbit/view mode.
	if (isWheel) {
		// SINGLE_PARTICLE sub-layer 2 / 3:
		// CUDA volume ray camera still uses volumeRenderZs.
		if (singleParticleVolumeCameraActive) {
			m_camera.zoomVolumeRenderByWheel(button);

			glutPostRedisplay();
			return;
		}

		// SINGLE_PARTICLE Layer 2 preview + Layer 3 sub-layer 0 / 1:
		// The OpenGL particle CAD pocket does NOT use CameraProcessor Z directly.
		// It uses m_volumeFrameZs -> renderCtx.particleWorkspaceZs.
		if (singleParticleOpenGLCameraActive) {
			m_camera.zoomParticleWorkspaceByWheel(button);

			glutPostRedisplay();
			return;
		}

		// Normal global camera modes:
		// Menu/layer transitions, PARTICLE_SIMULATION, non-SP view.
		m_camera.zoomByWheel(button);

		glutPostRedisplay();
		return;
	}

	if (m_tesseract.handleWorkspaceMouse(
		m_arbiter,
		button,
		state,
		x,
		y,
		m_viewport.getWidth(),
		m_viewport.getHeight())) {

		rebuildMenus();
		glutPostRedisplay();
		return;
	}

	if (m_mouse.onButton(
		button,
		state,
		x,
		y,
		m_camera.getTranslation())) {

		glutPostRedisplay();
	}
}
void EuclidEngine::onMotion(int x, int y) {
	if (isObjExportModalActive()) return;

	if (m_tesseract.handleWorkspaceMotion(
		m_arbiter,
		x,
		y,
		m_viewport.getWidth(),
		m_viewport.getHeight())) {

		glutPostRedisplay();
		return;
	}

	if (!m_camera.orbitEnabled()) return;
	if (m_mouse.onMotion(
		x,
		y,
		m_camera.getRotation(),
		m_camera.getOrbitSensitivityScale())) {

		glutPostRedisplay();
	}
}
void EuclidEngine::onPassiveMotion(int x, int y) {
	if (isObjExportModalActive()) return;

	if (m_tesseract.handleWorkspacePassiveMotion(
		m_arbiter,
		x,
		y,
		m_viewport.getWidth(),
		m_viewport.getHeight())) {

		glutPostRedisplay();
		return;
	}

	if (m_mouse.onPassiveMotion(x, y)) {
		glutPostRedisplay();
	}
}
void EuclidEngine::onKeyboard(unsigned char key, int x, int y) {

	KeyboardInput::KeyEvent event = m_keyboard.onKey(key, x, y);
	if (handleObjExportModalKeyboard(event)) return;

	TheArbiter::ApplicationLayer previousLayer =
		m_arbiter.getApplicationLayer();

	TheArbiter::WorkspaceId previousWorkspace =
		m_arbiter.getSelectedWorkspace();

	TheArbiter::SingleParticleSubLayer previousSubLayer =
		m_arbiter.getSingleParticleSubLayer();

	TheArbiter::VolumePrimitive previousPrimitive =
		m_arbiter.getVolumePrimitiveSelection();

	float previousScaleWhole = m_arbiter.getVolumeScaleWhole();
	float previousScaleX = m_arbiter.getVolumeScaleX();
	float previousScaleY = m_arbiter.getVolumeScaleY();
	float previousScaleZ = m_arbiter.getVolumeScaleZ();

	float previousPitchDeg = m_arbiter.getRotationPitchDeg();
	float previousYawDeg = m_arbiter.getRotationYawDeg();
	float previousRollDeg = m_arbiter.getRotationRollDeg();

	TheArbiter::ArbiterResult result = m_arbiter.processKeyboard(event);

	const bool committedToVoxelBase = result.commitVolumeFuse;
	if (committedToVoxelBase) {
		applyVoxelBaseCommit();
	}

	bool enteredWorkspaceDomainFromMenu =
		(previousLayer == TheArbiter::ApplicationLayer::GLOBAL_SHELL) &&
		(!m_arbiter.isMenuLayer());

	bool returnedToMenu =
		(previousLayer != TheArbiter::ApplicationLayer::GLOBAL_SHELL) &&
		m_arbiter.isMenuLayer();

	bool enteredSingleParticleConfig =
		(previousLayer == TheArbiter::ApplicationLayer::DOMAIN_SELECTION) &&
		m_arbiter.isParticleConfigLayer() &&
		m_arbiter.isSingleParticleSelected();

	bool returnedFromSingleParticleConfig =
		(previousLayer == TheArbiter::ApplicationLayer::WORKSPACE_CONFIGURATION) &&
		m_arbiter.isEnvironmentConfigLayer() &&
		(previousWorkspace ==
			TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD);

	bool enteredVolumeRenderSubLayer =
		(previousSubLayer != TheArbiter::SP_SUB_LAYER_VOLUME_RENDER) &&
		m_arbiter.isVolumeRenderSubLayer();


	bool enteredMarchingCubesSubLayer =
		(previousSubLayer != TheArbiter::SP_SUB_LAYER_MARCHING_CUBES) &&
		m_arbiter.isMarchingCubesSubLayer();

	bool volumePrimitiveChanged =
		(previousPrimitive != m_arbiter.getVolumePrimitiveSelection());

	bool volumeScaleChanged =
		fabs(previousScaleWhole - m_arbiter.getVolumeScaleWhole()) > 0.0001f ||
		fabs(previousScaleX - m_arbiter.getVolumeScaleX()) > 0.0001f ||
		fabs(previousScaleY - m_arbiter.getVolumeScaleY()) > 0.0001f ||
		fabs(previousScaleZ - m_arbiter.getVolumeScaleZ()) > 0.0001f;

	bool volumeRotationChanged =
		fabs(previousPitchDeg - m_arbiter.getRotationPitchDeg()) > 0.0001f ||
		fabs(previousYawDeg - m_arbiter.getRotationYawDeg()) > 0.0001f ||
		fabs(previousRollDeg - m_arbiter.getRotationRollDeg()) > 0.0001f;

	if (enteredMarchingCubesSubLayer) {
		extractMarchingCubesMesh();
	}

	if (enteredWorkspaceDomainFromMenu) {
		float timeS = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

		m_tesseract.beginAnimTransition(
			Tesseract::ANIM_TRANS_IDLE_TO_WORKSPACE,
			timeS
		);
	}

	if (returnedToMenu) {
		float timeS = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

		m_tesseract.beginAnimTransition(
			Tesseract::ANIM_TRANS_WORKSPACE_TO_IDLE,
			timeS
		);

		m_singleParticlePlaced = false;
		m_tesseract.clearSPPlacement();
	}

	if (previousWorkspace != m_arbiter.getSelectedWorkspace()) {
		m_singleParticlePlaced = false;
		m_tesseract.clearSPPlacement();
	}
	// ---------------------------------------------------------
	// SINGLE_PARTICLE Layer_2 live preview.
	//
	// When the user presses E on SINGLE_PARTICLE from Layer_1,
	// Layer_2 should immediately show a preview particle so
	// color/radius/render-mode changes are visible before the
	// user commits into Layer_3.
	// ---------------------------------------------------------
	if (enteredSingleParticleConfig) {
		startSingleParticleConfigPreview();
	}

	// If the user backs out from the SINGLE_PARTICLE config layer,
	// clear the preview anchor. The actual CAD workspace will be
	// recreated when they enter SINGLE_PARTICLE config again.
	if (returnedFromSingleParticleConfig) {
		m_singleParticlePlaced = false;
		m_tesseract.clearSPPlacement();

		if (m_renderer) {
			m_renderer->setParticleHighlighted(false);
		}
	}

	if (!committedToVoxelBase) {

		if (enteredVolumeRenderSubLayer || result.regenerateVolume) {
			regenerateVolumeField();
		}
		else if (m_arbiter.isVolumeRenderSubLayer() && (volumePrimitiveChanged || volumeScaleChanged || volumeRotationChanged)) {
			regenerateVolumeField();
		}
		else if (volumePrimitiveChanged || volumeScaleChanged || volumeRotationChanged) {
			m_tesseract.markVolumeDirty();
		}
	}

	switch (result.command) {
	case TheArbiter::CMD_EXIT:
		glutDestroyWindow(glutGetWindow());
		return;

	case TheArbiter::CMD_START_PARTICLE_SIMULATION:
		if (!m_arbiter.isParticleSimulationSelected()) break;

		m_tesseract.enterWorkspace(
			TheArbiter::WorkspaceId::PARTICLE_SIMULATION
		);

		m_singleParticlePlaced = false;
		m_tesseract.clearSPPlacement();

		applyParticleSelectionsToSystem();

		if (!m_tesseract.startPSWorkspace())
			break;

		if (m_renderer) {
			m_renderer->setGridMode3D();
			m_renderer->setParticleHighlighted(false);
		}

		syncRenderingWithParticleSystem();

		m_camera.setBehaviorMode(
			CameraProcessor::CAM_STANDARD_3D_ORBIT
		);
		break;

	case TheArbiter::CMD_PLACE_SINGLE_PARTICLE:
		placeSingleParticleAtOrigin();
		m_camera.setBehaviorMode(
			CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
		);
		break;

	case TheArbiter::CMD_PARTICLE_CONFIG_CHANGED:
	case TheArbiter::CMD_PARTICLE_RADIUS_CHANGED:
	case TheArbiter::CMD_PARTICLE_RENDER_MODE_CHANGED:
		applySingleParticleConfigToSystem();
		break;

	case TheArbiter::CMD_TOGGLE_PAUSE:
		m_tesseract.togglePSPause();
		break;

	case TheArbiter::CMD_STEP_SIMULATION:
		m_tesseract.stepPSWorkspace();
		break;

	case TheArbiter::CMD_REDRAW:
	case TheArbiter::CMD_NONE:

	default:
		break;
	}

	// Side-panel action from SUB_LAYER_3.
	if (result.exportObjRequested) {
		exportCurrentMeshOBJ();
	}

	syncCameraBehaviorFromArbiter();

	if (m_singleParticlePlaced &&
		m_arbiter.isParticleConfigLayer() &&
		m_arbiter.isSingleParticleSelected()) {
		applySingleParticleConfigToSystem();
	}

	bool menuContextChanged =
		(previousLayer != m_arbiter.getApplicationLayer()) ||
		(previousWorkspace != m_arbiter.getSelectedWorkspace()) ||
		(previousSubLayer != m_arbiter.getSingleParticleSubLayer());

	if (menuContextChanged || result.rebuildMenu) {
		rebuildMenus();
	}

	if (result.requestRedraw) {
		glutPostRedisplay();
	}
}
void EuclidEngine::onIdle() {
	if (m_exiting || m_cleaned) return;

	advanceObjExportJob();
	glutPostRedisplay();
}
void EuclidEngine::onClose() {
	requestExit();
	glutLeaveMainLoop();
}

// =============================================================================
// MARCHING CUBES / OBJ EXPORT PIPELINE
// =============================================================================
void EuclidEngine::extractMarchingCubesMesh() {
	if (!m_marchingCubes) {
		initMarchingCubes();
	}

	if (!m_marchingCubes) {
		printf("[EuclidEngine] MC extract skipped: MarchingCubes unavailable.\n");
		return;
	}

	if (!m_tesseract.hasVolume()) {
		initVolumeField();
	}

	if (!m_tesseract.hasVolume()) {
		printf("[EuclidEngine] MC extract skipped: volume unavailable.\n");
		return;
	}

	if (m_tesseract.isVolumeDirty()) {
		regenerateVolumeField();
	}

	float* dVolume = m_tesseract.getVolume();

	if (!dVolume) {
		printf("[EuclidEngine] MC extract skipped: null volume pointer.\n");
		return;
	}

	m_marchingCubes->extract(
		dVolume,
		kMarchingCubesIsoValue
	);

	const bool meshGenerated =
		m_marchingCubes->hasTriangleData();

	printf(
		"[EuclidEngine] MC extract complete: verts=%u tris=%u valid=%s\n",
		m_marchingCubes->getTotalVertexCount(),
		m_marchingCubes->getGeneratedTriangleCount(),
		meshGenerated ? "YES" : "NO"
	);
}
void EuclidEngine::exportCurrentMeshOBJ() {
	if (isObjExportWorking()) return;

	m_objExportPanel =
		ViewPort::ObjExportPanelData{};

	m_objExportPanel.mode =
		ViewPort::ObjExportPanelMode::CONFIRM;

	m_objExportPanel.yesSelected = true;
	m_objExportPanel.progressPercent = 0;
	m_objExportPanel.spinnerFrame = 0;

	m_objExportPanel.statusText = "Awaiting confirmation";

	m_objExportStage = ObjExportStage::NONE;

	// Prevent the GLUT right-click menu from appearing over
	// the modal confirmation dialog.
	glutDetachMenu(GLUT_RIGHT_BUTTON);

	glutPostRedisplay();
}

void EuclidEngine::appendObjExportLog(const string& line) {
	m_objExportPanel.logLines.push_back(line);

	// Keep bounded storage even if later checkpoints add
	// many more diagnostic entries.
	const size_t maxStoredLines = 64;

	if (m_objExportPanel.logLines.size() > maxStoredLines) {

		m_objExportPanel.logLines.erase(
			m_objExportPanel.logLines.begin()
		);
	}
}
void EuclidEngine::failObjExportJob(const string& reason) {

	appendObjExportLog(
		"[EuclidEngine] ERROR: " + reason
	);

	m_objExportPanel.mode =
		ViewPort::ObjExportPanelMode::FAILED;

	m_objExportPanel.statusText = reason;
	m_objExportStage = ObjExportStage::NONE;
	m_objExportFutureActive = false;

	glutPostRedisplay();
}
void EuclidEngine::closeObjExportPanel() {

	m_objExportPanel =
		ViewPort::ObjExportPanelData{};

	m_objExportStage = ObjExportStage::NONE;
	m_objExportFutureActive = false;

	rebuildMenus();
	glutPostRedisplay();
}

void EuclidEngine::beginObjExportJob() {

	m_objExportPanel.mode =
		ViewPort::ObjExportPanelMode::WORKING;

	m_objExportPanel.progressPercent = 5;
	m_objExportPanel.spinnerFrame = 0;
	m_objExportPanel.logLines.clear();

	m_objExportPanel.statusText =
		"Preparing Marching Cubes mesh";

	m_objExportStage =
		ObjExportStage::PREPARE_MESH;

	const int now =
		glutGet(GLUT_ELAPSED_TIME);

	m_objExportNextStepMs = now + 120;
	m_objExportLastSpinnerMs = now;

	glutPostRedisplay();
}
void EuclidEngine::advanceObjExportJob() {

	using namespace std::chrono;
	const int now = glutGet(GLUT_ELAPSED_TIME);

	// ---------------------------------------------------------
	// Completion hold: leave 100% visible briefly, then return
	// normal controls to the Arbiter.
	// ---------------------------------------------------------
	if (m_objExportPanel.mode == ViewPort::ObjExportPanelMode::COMPLETE) {

		if (now >= m_objExportCompleteUntilMs)
			closeObjExportPanel();

		return;
	}

	if (!isObjExportWorking()) return;

	// ---------------------------------------------------------
	// Animate spinner independently from the staged operation.
	// ---------------------------------------------------------
	if (now - m_objExportLastSpinnerMs >= 100) {

		m_objExportPanel.spinnerFrame =
			(m_objExportPanel.spinnerFrame + 1) % 4;

		m_objExportLastSpinnerMs = now;
	}

	// File writer is asynchronous. Poll it without blocking GLUT.
	if (m_objExportStage == ObjExportStage::WAIT_FOR_FILE_WRITE) {

		if (!m_objExportFutureActive ||
			!m_objExportFuture.valid()) {

			failObjExportJob("OBJ writer task became invalid.");

			return;
		}

		const std::future_status status =
			m_objExportFuture.wait_for(milliseconds(0));

		if (status != future_status::ready)
			return;

		const bool succeeded =
			m_objExportFuture.get();

		m_objExportFutureActive = false;

		if (!succeeded) {
			failObjExportJob("MarchingCubes::exportOBJ() failed.");
			return;
		}

		char line[256];

		snprintf(
			line,
			sizeof(line),
			"[MarchingCubes3D] exportOBJ success: "
			"'%s' vertices=%u triangles=%u normals=YES",
			m_objExportPath.c_str(),
			m_marchingCubes->getTotalVertexCount(),
			m_marchingCubes->getGeneratedTriangleCount()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 70;
		m_objExportPanel.statusText = "OBJ file written";

		m_objExportStage = ObjExportStage::REPORT_FILE_WRITTEN;
		m_objExportNextStepMs = now + 140;
		return;
	}

	if (now < m_objExportNextStepMs) return;

	char line[512];

	switch (m_objExportStage) {

		// =========================================================
		// 10% — Validate/extract mesh
		// =========================================================
	case ObjExportStage::PREPARE_MESH:

		appendObjExportLog("[EuclidEngine] Marching Cubes initialized.");

		m_objExportPanel.progressPercent = 10;

		if (!m_marchingCubes ||
			!m_marchingCubes->hasTriangleData()) {

			m_objExportPanel.statusText =
				"Extracting Marching Cubes mesh";

			// This remains a main-thread CUDA/OpenGL operation.
			extractMarchingCubesMesh();
		}

		if (!m_marchingCubes || !m_marchingCubes->hasTriangleData()) {

			failObjExportJob("No valid Marching Cubes triangle mesh.");
			return;
		}
		m_objExportStage = ObjExportStage::REPORT_CLASSIFICATION;
		break;

		// =========================================================
		// 20% — Classification
		// =========================================================
	case ObjExportStage::REPORT_CLASSIFICATION:

		snprintf(
			line,
			sizeof(line),
			"[MarchingCubes3D] classifyOnly: "
			"iso=%.4f, activeVoxels=%u, totalVerts=%u",
			kMarchingCubesIsoValue,
			m_marchingCubes->getActiveVoxelCount(),
			m_marchingCubes->getTotalVertexCount()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 20;
		m_objExportPanel.statusText =
			"Voxel classification complete";

		m_objExportStage = ObjExportStage::REPORT_MC_VBO;
		break;

		// =========================================================
		// 25% — Marching Cubes VBO
		// =========================================================
	case ObjExportStage::REPORT_MC_VBO:

		snprintf(
			line,
			sizeof(line),
			"[MarchingCubes3D] Created mesh VBOs: "
			"verts=%u, bytes=%zu each",
			m_marchingCubes
			->getAllocatedMeshVertexCount(),
			m_marchingCubes
			->getMeshVBOBytesEach()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 25;
		m_objExportPanel.statusText =
			"Marching Cubes VBO ready";

		m_objExportStage = ObjExportStage::REPORT_BOUNDS;
		break;

		// =========================================================
		// 30% — Mesh bounds
		// =========================================================
	case ObjExportStage::REPORT_BOUNDS:

		appendObjExportLog("[MarchingCubes3D] mesh bounds:");

		if (m_marchingCubes->hasMeshBounds()) {

			const float3 minValue =
				m_marchingCubes->getMeshMin();

			const float3 maxValue =
				m_marchingCubes->getMeshMax();

			const float3 centerValue =
				m_marchingCubes->getMeshCenter();

			snprintf(
				line,
				sizeof(line),
				"  min    = %.4f %.4f %.4f",
				minValue.x,
				minValue.y,
				minValue.z
			);

			appendObjExportLog(line);

			snprintf(
				line,
				sizeof(line),
				"  max    = %.4f %.4f %.4f",
				maxValue.x,
				maxValue.y,
				maxValue.z
			);

			appendObjExportLog(line);

			snprintf(
				line,
				sizeof(line),
				"  center = %.4f %.4f %.4f",
				centerValue.x,
				centerValue.y,
				centerValue.z
			);

			appendObjExportLog(line);
		}
		else {
			appendObjExportLog("  bounds unavailable");
		}

		m_objExportPanel.progressPercent = 30;
		m_objExportPanel.statusText =
			"Mesh bounds calculated";

		m_objExportStage = ObjExportStage::REPORT_EXTRACTION;

		break;

		// =========================================================
		// 40% — Extract output
		// =========================================================
	case ObjExportStage::REPORT_EXTRACTION:

		snprintf(
			line,
			sizeof(line),
			"[MarchingCubes3D] extract: "
			"activeVoxels=%u, totalVerts=%u, triangles=%u",
			m_marchingCubes->getActiveVoxelCount(),
			m_marchingCubes->getTotalVertexCount(),
			m_marchingCubes->getGeneratedTriangleCount()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 40;
		m_objExportPanel.statusText =
			"Triangle extraction complete";

		m_objExportStage = ObjExportStage::REPORT_ENGINE_EXTRACTION;

		break;

		// =========================================================
		// 50% — Engine extraction confirmation
		// =========================================================
	case ObjExportStage::REPORT_ENGINE_EXTRACTION:

		snprintf(
			line,
			sizeof(line),
			"[EuclidEngine] MC extract complete: "
			"verts=%u tris=%u valid=YES",
			m_marchingCubes->getTotalVertexCount(),
			m_marchingCubes->getGeneratedTriangleCount()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 50;
		m_objExportPanel.statusText =
			"Preparing file writer";

		m_objExportStage = ObjExportStage::BEGIN_FILE_WRITE;

		break;

		// =========================================================
		// 60% — Asynchronous file writer
		// =========================================================
	case ObjExportStage::BEGIN_FILE_WRITE:

#ifdef _WIN32
		_mkdir("SINGLE_PARTICLE_DATA");
#endif

		m_objExportPanel.progressPercent = 60;
		m_objExportPanel.statusText =
			"Writing vertices, normals and faces";

		{
			MarchingCubes* marchingCubes =
				m_marchingCubes;

			const std::string path =
				m_objExportPath;

			m_objExportFuture = async(
				launch::async,
				[marchingCubes, path]() {
					return marchingCubes->exportOBJ(path.c_str(), true);
				}
			);
		}

		m_objExportFutureActive = true;
		m_objExportStage = ObjExportStage::WAIT_FOR_FILE_WRITE;
		return;

		// =========================================================
		// 75% — File is complete
		// =========================================================
	case ObjExportStage::REPORT_FILE_WRITTEN:

		snprintf(
			line,
			sizeof(line),
			"[EuclidEngine] Exported OBJ: %s",
			m_objExportPath.c_str()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 75;
		m_objExportPanel.statusText =
			"Preparing OBJ mesh reload";

		m_objExportStage = ObjExportStage::BEGIN_MESH_RELOAD;

		break;

		// =========================================================
		// 80% — Allow one rendered frame before reload
		// =========================================================
	case ObjExportStage::BEGIN_MESH_RELOAD:

		m_objExportPanel.progressPercent = 80;
		m_objExportPanel.statusText =
			"Uploading exported mesh";

		m_objExportStage = ObjExportStage::RELOAD_MESH;

		break;

		// =========================================================
		// Main-thread OBJ parse and OpenGL upload
		// =========================================================
	case ObjExportStage::RELOAD_MESH:

		if (!m_renderer) {
			failObjExportJob("Renderer unavailable.");
			return;
		}

		m_renderer->clearParticleMeshOBJ();

		if (!m_renderer->loadParticleMeshOBJ(
			m_objExportPath.c_str())) {

			failObjExportJob("Generated OBJ could not be reloaded.");

			return;
		}

		snprintf(
			line,
			sizeof(line),
			"[EuclidRenderer] OBJ mesh uploaded: "
			"vbo=%u vertices=%d bytes=%zu",
			static_cast<unsigned int>(m_renderer->getParticleMeshVBO()),
			static_cast<int>(m_renderer->getParticleMeshVertexCount()),
			m_renderer->getParticleMeshBufferBytes()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 80;
		m_objExportPanel.statusText =
			"GPU mesh upload complete";

		m_objExportStage = ObjExportStage::REPORT_MESH_LOADED;

		break;

		// =========================================================
		// 90% — Renderer load report
		// =========================================================
	case ObjExportStage::REPORT_MESH_LOADED:

		snprintf(
			line,
			sizeof(line),
			"[EuclidRenderer] Loaded particle mesh OBJ: "
			"%s vertices=%d triangles=%d maxExtent=%.6f",
			m_objExportPath.c_str(),
			static_cast<int>(m_renderer->getParticleMeshVertexCount()),
			static_cast<int>(m_renderer->getParticleMeshVertexCount() / 3),
			m_renderer->getParticleMeshMaxExtent()
		);

		appendObjExportLog(line);

		m_objExportPanel.progressPercent = 90;
		m_objExportPanel.statusText =
			"Mesh reload verified";

		m_objExportStage = ObjExportStage::ACTIVATE_MESH_MODE;

		break;

		// =========================================================
		// 99% — Select mesh rendering
		// =========================================================
	case ObjExportStage::ACTIVATE_MESH_MODE:

		m_arbiter.setParticleRenderMode(TheArbiter::PARTICLE_RENDER_MESH);

		appendObjExportLog(
			"[EuclidEngine] SINGLE_PARTICLE render mode "
			"automatically set to MESH."
		);

		m_objExportPanel.progressPercent = 99;
		m_objExportPanel.statusText =
			"Finalizing export";

		m_objExportStage = ObjExportStage::FINISH;

		break;

		// =========================================================
		// 100% — Finished
		// =========================================================
	case ObjExportStage::FINISH:

		m_objExportPanel.progressPercent = 100;
		m_objExportPanel.mode = ViewPort::ObjExportPanelMode::COMPLETE;
		m_objExportPanel.statusText = "Export complete";
		m_objExportCompleteUntilMs = now + 1400;
		m_objExportStage = ObjExportStage::NONE;

		glutPostRedisplay();
		return;

	default:
	case ObjExportStage::NONE:
		return;
	}

	m_objExportNextStepMs = now + 140;

	glutPostRedisplay();
}

bool EuclidEngine::isObjExportModalActive() const {

	return m_objExportPanel.mode !=
		ViewPort::ObjExportPanelMode::HIDDEN;
}
bool EuclidEngine::isObjExportWorking() const {

	return m_objExportPanel.mode ==
		ViewPort::ObjExportPanelMode::WORKING;
}
