#ifdef _WIN32
#include <direct.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <vector>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <string>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <filesystem>

#include "kernel.h"
#include "EuclidEngine.h"
#include "ParticleSimRuntimeConfig.h"
#include "PngImage.h"

using namespace std;
using namespace glm;
namespace fs = std::filesystem;

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
	initTextureMapResources();
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
	// Dedicated reflected brush field. It is never aliased with the primary
	// brush so both members can be classified, previewed, and composed safely.
	if (!m_tesseract.hasMirrorBrushVolume()) {
		float* dMirrorBrushVolume = nullptr;
		allocateArray(
			reinterpret_cast<void**>(&dMirrorBrushVolume),
			volumeBytes);
		m_tesseract.bindMirrorBrushVolume(dMirrorBrushVolume);
		clearVolumeKernelLauncher(dMirrorBrushVolume, v, 1.0e6f);
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
		static_cast<double>(4 * volumeBytes) / (1024.0 * 1024.0)
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

void EuclidEngine::initTextureMapResources() {

	namespace fs = std::filesystem;

	const fs::path applicationRoot =
		fs::current_path();

	const fs::path outputRoot =
		applicationRoot / "OUTPUT";

	const fs::path outputStaticParticlesRoot =
		outputRoot / "STATIC_PARTICLES";
	
	const fs::path baseMaterialsRoot =
		applicationRoot /
		"INPUTS" /
		"TEXTURE_MAP_2D" /
		"BASE_MATERIALS";

	printf(
		"[EuclidEngine] Application working root: %s\n",
		applicationRoot.string().c_str()
	);

	std::error_code error;

	// OUTPUT may legitimately be empty on the first run.
	fs::create_directories(
		outputStaticParticlesRoot,
		error
	);

	if (error) {

		printf(
			"[EuclidEngine] WARNING: "
			"Could not create OUTPUT static-particle directory: %s\n",
			error.message().c_str()
		);

		return;
	}

	error.clear();

	if (!fs::is_directory(
		baseMaterialsRoot,
		error)) {

		printf(
			"[EuclidEngine] WARNING: "
			"Base-material directory was not found: %s\n",
			baseMaterialsRoot.string().c_str()
		);

		return;
	}

	m_tesseract.bindTextureMapResources(
		&m_assetRepository,
		outputStaticParticlesRoot,
		baseMaterialsRoot
	);
	refreshTextureMapBaseMaterialCatalog();
}

void EuclidEngine::refreshTextureMapBaseMaterialCatalog() {
	vitru::TextureMapWorkspace* workspace =
		m_tesseract.getTextureMapWorkspaceRuntime();
	if (!workspace) return;
	const fs::path root = m_inputsRoot / "TEXTURE_MAP_2D" / "BASE_MATERIALS";
	std::error_code error;
	std::vector<vitru::BaseMaterialCatalogEntry> catalog;
	if (!fs::is_directory(root, error) || error) {
		workspace->replaceBaseMaterialCatalog(std::move(catalog));
		return;
	}
	fs::recursive_directory_iterator iterator(
		root, fs::directory_options::skip_permission_denied, error);
	const fs::recursive_directory_iterator end;
	while (!error && iterator != end) {
		const fs::directory_entry entry = *iterator;
		std::error_code entryError;
		std::string extension = entry.path().extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (entry.is_regular_file(entryError) && !entryError && extension == ".png") {
			vitru::BaseMaterialCatalogEntry item;
			item.id = entry.path().lexically_normal().generic_string();
			item.displayName = entry.path().stem().string();
			item.rootPath = entry.path().parent_path();
			item.baseColorPath = entry.path();
			vitru::ImageRGBA8 image;
			std::string imageError;
			item.valid = vitru::loadPngImage(entry.path(), image, &imageError, true);
			item.status = item.valid ? "READY" : imageError;
			if (item.valid) { item.width = image.width; item.height = image.height; }
			catalog.push_back(std::move(item));
		}
		iterator.increment(error);
	}
	std::sort(catalog.begin(), catalog.end(),
		[](const vitru::BaseMaterialCatalogEntry& a,
			const vitru::BaseMaterialCatalogEntry& b) {
			return a.displayName < b.displayName;
		});
	workspace->replaceBaseMaterialCatalog(std::move(catalog));
}

bool EuclidEngine::loadSelectedTextureMapTarget() {

	vitru::TextureMapWorkspace* textureWorkspace =
		m_tesseract.getTextureMapWorkspaceRuntime();

	if (!textureWorkspace) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"workspace runtime unavailable.\n"
		);

		return false;
	}

	const vitru::StaticAssetCatalogEntry* selected =
		textureWorkspace->selectedOutputAsset();

	if (!selected) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"no OUTPUT Static Particle is selected.\n"
		);

		textureWorkspace->target() =
			vitru::TextureMapTargetContext{};

		textureWorkspace->target().readiness =
			vitru::TextureTargetReadiness::Invalid;

		return false;
	}

	if (!selected->valid) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET rejected: "
			"%s is not a valid VSPA target.\n",
			selected->displayName.c_str()
		);

		textureWorkspace->target() =
			vitru::TextureMapTargetContext{};

		textureWorkspace->target().readiness =
			vitru::TextureTargetReadiness::Invalid;

		return false;
	}

	// ---------------------------------------------------------
	// CPU-side VSPA load.
	//
	// Do NOT pass the shared repository here.
	// Do NOT refresh SINGLE_PARTICLE_DATA/p0.obj here.
	//
	// TEXTURE_MAP_2D first loads into a temporary canonical
	// StaticParticleAsset. EuclidEngine commits it afterward.
	// ---------------------------------------------------------
	vitru::StaticParticleAsset loadedAsset;

	vitru::StaticAssetOperationReport report;

	const bool loadSucceeded =
		vitru::loadStaticParticleBundle(
			selected->manifestPath,
			loadedAsset,
			report,
			nullptr,
			{}
	);

	if (!loadSucceeded) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed during phase: %s\n",
			report.phase.c_str()
		);

		for (const string& warning :
			report.warnings) {

			printf(
				"  [WARN] %s\n",
				warning.c_str()
			);
		}

		for (const string& error :
			report.errors) {

			printf(
				"  [ERROR] %s\n",
				error.c_str()
			);
		}

		textureWorkspace->target() =
			vitru::TextureMapTargetContext{};

		textureWorkspace->target().readiness =
			vitru::TextureTargetReadiness::Invalid;

		return false;
	}

	// ---------------------------------------------------------
	// Commit into the shared canonical repository.
	//
	// Repeated E presses on the same loaded target replace the
	// existing repository asset rather than accumulating copies.
	// ---------------------------------------------------------
	vitru::AssetId assetId =
		vitru::INVALID_ASSET_ID;

	const vitru::TextureMapTargetContext previousTarget =
		textureWorkspace->target();

	if (previousTarget.loaded &&
		previousTarget.assetId !=
		vitru::INVALID_ASSET_ID &&
		m_assetRepository.findStaticParticle(
			previousTarget.assetId)) {

		assetId =
			previousTarget.assetId;

		if (!m_assetRepository.replaceStaticParticle(
			assetId,
			move(loadedAsset))) {

			printf(
				"[TEXTURE_MAP_2D] LOAD TARGET failed: "
				"repository replacement failed.\n"
			);

			return false;
		}
	}
	else {

		assetId =
			m_assetRepository.addStaticParticle(
				move(loadedAsset)
			);
	}

	if (assetId == vitru::INVALID_ASSET_ID) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"repository returned an invalid AssetId.\n"
		);

		return false;
	}

	if (!m_assetRepository.setActiveStaticParticle(assetId)) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"could not activate repository asset.\n"
		);

		return false;
	}

	vitru::StaticParticleAsset* active =
		m_assetRepository.findStaticParticle(
			assetId
		);

	if (!active) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"active repository asset could not be resolved.\n"
		);

		return false;
	}

	// ---------------------------------------------------------
	// Upload mesh/material/texture resources to EuclidRenderer.
	//
	// Unlike SINGLE_PARTICLE loading, this does NOT restore the
	// native CUDA SDF. TEXTURE_MAP_2D needs the textured mesh,
	// not the volumetric editing field.
	// ---------------------------------------------------------
	if (!m_renderer || !m_renderer->loadParticleStaticAsset(*active)) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"renderer asset upload failed.\n"
		);

		textureWorkspace->target() =
			vitru::TextureMapTargetContext{};

		textureWorkspace->target().readiness =
			vitru::TextureTargetReadiness::Invalid;

		return false;
	}

	// ---------------------------------------------------------
	// Bind the canonical repository asset as the active
	// TEXTURE_MAP_2D editing target.
	// ---------------------------------------------------------
	if (!textureWorkspace->activateLoadedTarget(assetId)) {

		printf(
			"[TEXTURE_MAP_2D] LOAD TARGET failed: "
			"target context activation failed.\n"
		);

		return false;
	}

	printf(
		"[TEXTURE_MAP_2D] TARGET LOADED: %s\n",
		active->name.c_str()
	);

	printf(
		"  AssetId: %llu\n",
		static_cast<unsigned long long>(
			assetId
			)
	);

	printf(
		"  Materials: %zu\n",
		active->materials.size()
	);

	printf(
		"  Textures: %zu\n",
		active->textures.size()
	);

	printf(
		"  TEXCOORD_0: %s\n",
		active->mesh.uvs.size() ==
		active->mesh.positions.size()
		? "READY"
		: "MISSING"
	);

	return true;
}

bool EuclidEngine::enterTextureMapLayer2Preview() {

	vitru::TextureMapWorkspace* textureWorkspace =
		m_tesseract.getTextureMapWorkspaceRuntime();

	if (!textureWorkspace) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"workspace runtime unavailable.\n"
		);

		return false;
	}

	const vitru::TextureMapTargetContext& target =
		textureWorkspace->target();

	if (!target.loaded ||
		target.assetId ==
		vitru::INVALID_ASSET_ID) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"load Row [3] target first.\n"
		);

		return false;
	}

	if (target.readiness !=
		vitru::TextureTargetReadiness::Ready) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"target is not READY.\n"
		);

		return false;
	}

	vitru::StaticParticleAsset* active =
		m_assetRepository.findStaticParticle(
			target.assetId
		);

	if (!active) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"canonical repository asset is unavailable.\n"
		);

		return false;
	}

	if (!m_renderer) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"renderer unavailable.\n"
		);

		return false;
	}

	// ---------------------------------------------------------
	// Refresh the displayed GPU asset on every Layer 2 entry.
	//
	// Therefore:
	//
	// Layer 2 -> Q -> Layer 1 -> E Row [4]
	//
	// always refreshes the rendered canonical asset.
	// ---------------------------------------------------------
	if (!m_renderer->loadParticleStaticAsset(
		*active)) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"renderer refresh failed.\n"
		);

		return false;
	}

	m_assetRepository.setActiveStaticParticle(
		target.assetId
	);

	// Navigation changes only after all validation succeeds.
	m_arbiter.enterTextureMapLayer2FromMenu();

	if (!m_arbiter.isTextureMapLayer2PanelContext()) {

		printf(
			"[TEXTURE_MAP_2D] CONFIGURE rejected: "
			"Layer 2 transition failed.\n"
		);

		return false;
	}

	m_tesseract.enterWorkspace(
		TheArbiter::WorkspaceId::TEXTURE_MAP_2D
	);

	m_renderer->setParticleHighlighted(false);

	m_camera.setBehaviorMode(
		CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
	);

	printf(
		"[TEXTURE_MAP_2D] Entered Layer 2: %s\n",
		active->name.c_str()
	);

	return true;
}

bool EuclidEngine::enterTextureMapLayer3Runtime() {

	if (!m_arbiter.isTextureMapLayer2PanelContext()) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"Layer 2 configuration is not active.\n"
		);

		return false;
	}

	vitru::TextureMapWorkspace* textureWorkspace =
		m_tesseract.getTextureMapWorkspaceRuntime();

	if (!textureWorkspace) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"workspace runtime unavailable.\n"
		);

		return false;
	}

	const vitru::TextureMapTargetContext& target =
		textureWorkspace->target();

	if (!target.loaded ||
		target.assetId ==
		vitru::INVALID_ASSET_ID ||
		target.readiness !=
		vitru::TextureTargetReadiness::Ready) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"target is not LOADED | READY.\n"
		);

		return false;
	}

	vitru::StaticParticleAsset* active =
		m_assetRepository.findStaticParticle(
			target.assetId
		);

	if (!active) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"canonical repository asset is unavailable.\n"
		);

		return false;
	}

	if (!m_renderer) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"renderer unavailable.\n"
		);

		return false;
	}

	// Refresh the renderer from the shared canonical resource before
	// committing the structural transition.
	if (!m_renderer->loadParticleStaticAsset(
		*active)) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"renderer refresh failed.\n"
		);

		return false;
	}

	if (!m_assetRepository.setActiveStaticParticle(
		target.assetId)) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"canonical target activation failed.\n"
		);

		return false;
	}

	refreshTextureMapBaseMaterialCatalog();

	// Navigation changes only after every engine-owned validation and
	// resource refresh has succeeded.
	m_arbiter.enterTextureMapLayer3Runtime();

	if (!m_arbiter.isTextureMapLayer3RuntimeContext()) {

		printf(
			"[TEXTURE_MAP_2D] RUN WORKSPACE EDIT rejected: "
			"Layer 3 transition failed.\n"
		);

		return false;
	}

	// TEXTURE_MAP_2D remains the same Tesseract workspace across
	// Layer 2 and Layer 3; enterWorkspace is a no-op when already active.
	m_tesseract.enterWorkspace(
		TheArbiter::WorkspaceId::TEXTURE_MAP_2D
	);

	m_renderer->setParticleHighlighted(false);

	m_camera.setBehaviorMode(
		CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
	);

	printf(
		"[TEXTURE_MAP_2D] Entered Layer 3 reference preview: %s\n",
		active->name.c_str()
	);

	return true;
}

bool EuclidEngine::saveTextureMapCurrent() {
	vitru::TextureMapWorkspace* workspace =
		m_tesseract.getTextureMapWorkspaceRuntime();
	if (!workspace || workspace->session().dirty) return false;
	const vitru::StaticParticleAsset* asset =
		m_assetRepository.findStaticParticle(workspace->target().assetId);
	if (!asset || asset->name.empty()) return false;
	vitru::StaticAssetOperationReport report;
	const bool saved = vitru::saveStaticParticleBundle(
		*asset, m_outputRoot, asset->name, m_workspaceObj, report);
	printf("[TEXTURE_MAP_2D] SAVE CURRENT SP_ASSET: %s\n",
		saved ? "SUCCESS" : "FAILED");
	for (const std::string& error : report.errors)
		printf("  [ERROR] %s\n", error.c_str());
	if (saved) workspace->refreshOutputCatalog();
	return saved;
}

bool EuclidEngine::saveTextureMapAs(const std::string& assetName) {
	vitru::TextureMapWorkspace* workspace =
		m_tesseract.getTextureMapWorkspaceRuntime();
	if (!workspace) return false;

	vitru::StaticParticleAsset snapshot;
	std::string diagnostic;
	if (!workspace->buildSaveAsSnapshot(
		assetName, snapshot, &diagnostic,
		m_textureMapSaveAsSurfaceTargetName)) {
		printf("[TEXTURE_MAP_2D] SAVE AS rejected: %s\n", diagnostic.c_str());
		return false;
	}

	vitru::StaticAssetOperationReport report;
	if (!vitru::saveStaticParticleBundle(
		snapshot, m_outputRoot, assetName, m_workspaceObj, report)) {
		printf("[TEXTURE_MAP_2D] SAVE AS failed during %s.\n",
			report.phase.c_str());
		for (const std::string& error : report.errors)
			printf("  [ERROR] %s\n", error.c_str());
		return false;
	}

	vitru::StaticParticleAsset reopened;
	vitru::StaticAssetOperationReport loadReport;
	if (!vitru::loadStaticParticleBundle(
		report.manifestPath, reopened, loadReport, nullptr, m_workspaceObj)) {
		printf("[TEXTURE_MAP_2D] SAVE AS reload failed.\n");
		return false;
	}
	const vitru::AssetId id = m_assetRepository.addStaticParticle(
		std::move(reopened));
	m_assetRepository.setActiveStaticParticle(id);
	if (!workspace->adoptSavedTarget(id)) return false;
	if (m_renderer) {
		const vitru::StaticParticleAsset* saved =
			m_assetRepository.findStaticParticle(id);
		if (saved) m_renderer->loadParticleStaticAsset(*saved);
	}
	workspace->refreshOutputCatalog();
	m_textureMapSaveAsSurfaceTargetName.clear();
	printf("[TEXTURE_MAP_2D] SAVE STATIC PARTICLE AS: SUCCESS (%s)\n",
		assetName.c_str());
	return true;
}

void EuclidEngine::handleTextureMapRuntimeResult(
	const TheArbiter::ArbiterResult& result) {

	vitru::TextureMapWorkspace* workspace =
		m_tesseract.getTextureMapWorkspaceRuntime();
	if (!workspace) return;
	if (result.textureMapTextEntryCancelled) {
		m_textureMapSaveAsAwaitingSurfaceName = false;
		m_textureMapSaveAsSurfaceTargetName.clear();
	}

	if (result.textureMapSurfaceTargetNameEntered) {
		std::string diagnostic;
		if (m_textureMapSaveAsAwaitingSurfaceName) {
			if (workspace->validateNewSurfaceTargetName(
				result.textureMapEnteredName, &diagnostic)) {
				m_textureMapSaveAsSurfaceTargetName = result.textureMapEnteredName;
				m_textureMapSaveAsAwaitingSurfaceName = false;
				const vitru::StaticParticleAsset* target =
					m_assetRepository.findStaticParticle(workspace->target().assetId);
				m_arbiter.beginTextureMapSaveAsNameEntry(
					target ? target->name : "Static_Particle");
			}
			else {
				printf("[TEXTURE_MAP_2D] %s\n", diagnostic.c_str());
				m_arbiter.beginTextureMapSurfaceTargetNameEntry();
			}
		}
		else if (!workspace->completeSurfaceTargetName(
			result.textureMapEnteredName, &diagnostic)) {
			printf("[TEXTURE_MAP_2D] %s\n", diagnostic.c_str());
			m_arbiter.beginTextureMapSurfaceTargetNameEntry();
		}
	}

	if (result.textureMapSaveAsNameEntered)
		saveTextureMapAs(result.textureMapEnteredName);

	using Intent = TheArbiter::TextureMapRuntimeIntent;
	switch (result.textureMapRuntimeIntent) {
	case Intent::AdjustPrevious:
		workspace->adjustRuntimeValue(result.textureMapRuntimeRow, -1);
		break;
	case Intent::AdjustNext:
		workspace->adjustRuntimeValue(result.textureMapRuntimeRow, 1);
		break;
	case Intent::TogglePanelOrView:
		if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor)
			workspace->toggleRuntimeView();
		break;
	case Intent::BeginAuthoring:
		if (workspace->beginAuthoringRuntime()) {
			m_arbiter.beginTextureMapAuthoringRuntime();
			printf("[TEXTURE_MAP_2D] Target confirmed; authoring runtime initialized.\n");
		}
		else {
			printf("[TEXTURE_MAP_2D] Authoring gate rejected: %s\n",
				workspace->runtimeStatusMessage().c_str());
		}
		break;
	case Intent::RequestStructuralExit: {
		if (!m_arbiter.isTextureMapRuntimeAuthoring()) {
			m_arbiter.returnTextureMapLayer2Runtime();
		}
		else {
			std::string diagnostic;
			if (workspace->canExitLayer3(&diagnostic))
				m_arbiter.returnTextureMapLayer2Runtime();
			else
				printf("[TEXTURE_MAP_2D] %s\n", diagnostic.c_str());
		}
		break;
	}
	case Intent::Activate: {
		std::string diagnostic;
		const vitru::TextureMapWorkspaceAction action =
			workspace->activateRuntimeRow(result.textureMapRuntimeRow, &diagnostic);
		if (action == vitru::TextureMapWorkspaceAction::RequestSurfaceTargetName)
			m_arbiter.beginTextureMapSurfaceTargetNameEntry();
		else if (action == vitru::TextureMapWorkspaceAction::RequestSaveCurrent)
			saveTextureMapCurrent();
		else if (action == vitru::TextureMapWorkspaceAction::RequestSaveAs) {
			const bool unnamedNewContour = workspace->session().dirty &&
				workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour &&
				workspace->contourAction() == vitru::TextureMapContourAction::New;
			if (unnamedNewContour) {
				m_textureMapSaveAsAwaitingSurfaceName = true;
				m_arbiter.beginTextureMapSurfaceTargetNameEntry();
			}
			else {
				const vitru::StaticParticleAsset* target =
					m_assetRepository.findStaticParticle(workspace->target().assetId);
				m_arbiter.beginTextureMapSaveAsNameEntry(
					target ? target->name : "Static_Particle");
			}
		}
		else if (action == vitru::TextureMapWorkspaceAction::Rejected &&
			!diagnostic.empty())
			printf("[TEXTURE_MAP_2D] %s\n", diagnostic.c_str());
		break;
	}
	default:
		break;
	}

	if (m_arbiter.isTextureMapRuntimeAuthoring()) {
		m_arbiter.syncTextureMapRuntimeNavigation(
			static_cast<int>(workspace->runtimeSubLayer()),
			workspace->runtimeRowCount(),
			workspace->nestedFocus());

		if (m_renderer) {
			vitru::StaticParticleAsset preview = workspace->buildPreviewAsset();
			if (!preview.mesh.empty()) m_renderer->loadParticleStaticAsset(preview);
		}
	}
	if (m_renderer)
		m_renderer->setParticleHighlighted(
			m_arbiter.isTextureMapRuntimeMeshSelected() &&
			!m_arbiter.isTextureMapRuntimeAuthoring());
}

void EuclidEngine::initMenus() {
	glutMenuStatusFunc(&EuclidEngine::sMenuStatus);
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
	if (m_staticAssetFutureActive && m_staticAssetFuture.valid()) {
		m_staticAssetFuture.wait();
		m_staticAssetFutureActive = false;
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

	if (m_arbiter.isTextureMapLayer3RuntimeContext()) {
		vitru::TextureMapWorkspace* workspace =
			m_tesseract.getTextureMapWorkspaceRuntime();
		if (workspace && m_arbiter.isTextureMapRuntimeAuthoring())
			workspace->endAuthoringStroke();
		glutAddMenuEntry(
			m_arbiter.isTextureMapRuntimeAuthoring()
			? "- TEXTURE_MAP_2D Layer 3 Authoring -"
			: "- TEXTURE_MAP_2D Layer 3 Reference Preview -",
			MENU_NOP);
		glutAddMenuEntry("=========================================", MENU_NOP);

		const bool referenceGate =
			!m_arbiter.isTextureMapRuntimeAuthoring();
		const bool hiddenAuthoringPanel = workspace &&
			m_arbiter.isTextureMapRuntimeAuthoring() &&
			workspace->runtimeSubLayer() != vitru::TextureMapSubLayer::PixelEditor &&
			!m_arbiter.isTextureMapRuntimePanelVisible();
		if (referenceGate) {
			const char* primaryAction =
				m_arbiter.isTextureMapRuntimeMeshSelected()
				? "* Deselect Target Mesh"
				: m_arbiter.isTextureMapRuntimeSelectionArmed()
				? "* Cancel Target Selection"
				: "* Arm Target Selection";
			glutAddMenuEntry(primaryAction, MENU_TM_RUNTIME_TOGGLE_MESH);
			if (m_arbiter.isTextureMapRuntimeMeshSelected())
				glutAddMenuEntry("* Show Authoring Panel", MENU_TM_RUNTIME_TOGGLE_VIEW);
			glutAddMenuEntry("=========================================", MENU_NOP);
		}
		else if (hiddenAuthoringPanel) {
			glutAddMenuEntry("* Show Authoring Panel", MENU_TM_RUNTIME_TOGGLE_VIEW);
			glutAddMenuEntry("=========================================", MENU_NOP);
		}

		std::vector<std::string> labels;
		if (referenceGate || hiddenAuthoringPanel) labels.clear();
		else if (!workspace) labels = { "Runtime unavailable" };
		else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::CycleSetup)
			labels = { "Authoring Mode", "Preview Source", "Configure Authoring Pass" };
		else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::BranchSetup) {
			if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour)
				labels = { "Contour Action", "Contour Target", "Draw Contour", "Authoring Cycle Setup" };
			else if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::PanelLines)
				labels = { "Surface Target", "Texture Channel", "Draw Panel Lines", "Authoring Cycle Setup" };
			else labels = { "Surface Target", "Texture Channel / Nested Focus", "Draw Pixel Grid", "Authoring Cycle Setup" };
		}
		else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor) {
			if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Coloring)
				labels = { "Select Face", "Red", "Green", "Blue", "Alpha", "Review / Commit", "Coloring Setup" };
			else if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour)
				labels = { "Select Face", "Close Contour", "Undo Last Point", "Review / Commit", "Contour Setup" };
			else labels = { "Select Face", "Line Thickness", "Line Color", "Review / Commit", "Panel Line Setup" };
		}
		else labels = { "Commit Working Edit To Target", "Save Current SP_Asset",
			"Save Static Particle As", "Return To Pixel Grid", "Return To Authoring Cycle Setup" };

		for (std::size_t i = 0; i < labels.size() && i < 7u; i++) {
			const std::string label =
				(static_cast<int>(i) == m_arbiter.getTextureMapRuntimeRow() ? "* " : "  ") +
				labels[i];
			glutAddMenuEntry(label.c_str(), MENU_TM_RUNTIME_ROW_0 + static_cast<int>(i));
		}
		glutAddMenuEntry("=========================================", MENU_NOP);
		if (m_arbiter.isTextureMapRuntimeAuthoring() && workspace &&
			workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor)
			glutAddMenuEntry("* Toggle Edit / Preview", MENU_TM_RUNTIME_TOGGLE_VIEW);
		else if (m_arbiter.isTextureMapRuntimeAuthoring() && workspace &&
			workspace->runtimeSubLayer() != vitru::TextureMapSubLayer::PixelEditor &&
			!hiddenAuthoringPanel) {
			glutAddMenuEntry(
				"* Hide Panel",
				MENU_TM_RUNTIME_TOGGLE_VIEW);
		}
	}
	else if (m_arbiter.isSingleParticleReferenceSubLayer()) {
		glutAddMenuEntry(
			"- Sub-Layer 0: Particle Selection / Collision Setup",
			MENU_NOP
		);
		glutAddMenuEntry("=========================================", MENU_NOP);

		const char* primaryAction =
			m_arbiter.hasSelectedParticle()
			? "* Deselect particle"
			: m_arbiter.isSPSelectionArmed()
			? "* Cancel particle selection"
			: "* Arm particle selection";

		glutAddMenuEntry(
			primaryAction,
			MENU_SP_PRIMARY_ACTION
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("Collision Proxy:", MENU_NOP);

		const auto collisionShape =
			m_arbiter.getSPCollisionShape();

		glutAddMenuEntry(
			collisionShape == TheArbiter::SPCollisionShape::Sphere
			? "* Sphere { SELECTED / OPERATIONAL }"
			: "* Sphere { OPERATIONAL }",
			MENU_SP_COLLISION_SPHERE
		);
		glutAddMenuEntry(
			collisionShape == TheArbiter::SPCollisionShape::Block
			? "* Block { SELECTED / RESERVED }"
			: "* Block { RESERVED }",
			MENU_SP_COLLISION_BLOCK
		);
		glutAddMenuEntry(
			collisionShape == TheArbiter::SPCollisionShape::Capsule
			? "* Capsule { SELECTED / RESERVED }"
			: "* Capsule { RESERVED }",
			MENU_SP_COLLISION_CAPSULE
		);
		glutAddMenuEntry(
			collisionShape == TheArbiter::SPCollisionShape::Cone
			? "* Cone { SELECTED / RESERVED }"
			: "* Cone { RESERVED }",
			MENU_SP_COLLISION_CONE
		);
		glutAddMenuEntry(
			collisionShape == TheArbiter::SPCollisionShape::DeformableSphere
			? "* Deformable Sphere { SELECTED / RESERVED }"
			: "* Deformable Sphere { RESERVED }",
			MENU_SP_COLLISION_DEFORMABLE_SPHERE
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("Static Particle Asset:", MENU_NOP);
		glutAddMenuEntry("* Load Static Particle", MENU_SP_LOAD_STATIC_PARTICLE);
		glutAddMenuEntry("* Save Active Particle", MENU_SP_SAVE_ACTIVE_PARTICLE);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("Next Sub-Layer:", MENU_NOP);
		glutAddMenuEntry(
			m_arbiter.hasSelectedParticle()
			? "* Rendering Setup"
			: "- Rendering Setup { SELECT PARTICLE FIRST }",
			m_arbiter.hasSelectedParticle()
			? MENU_SP_RENDERING_SETUP
			: MENU_NOP
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("* Return to Layer 2", MENU_SP_RETURN_TO_LAYER_2);
		glutAddMenuEntry("=========================================", MENU_NOP);
	}
	else if (m_arbiter.isShapeEditSubLayer()) {
		glutAddMenuEntry(
			"- Sub-Layer 1: Rendering Setup",
			MENU_NOP
		);
		glutAddMenuEntry("=========================================", MENU_NOP);

		glutAddMenuEntry("Render Source:", MENU_NOP);
		glutAddMenuEntry(
			!m_arbiter.isParticleRenderMesh()
			? "* Particle { SELECTED }"
			: "* Particle",
			MENU_SP_RENDER_SOURCE_PARTICLE
		);
		glutAddMenuEntry(
			m_arbiter.isParticleRenderMesh()
			? "* Mesh { SELECTED }"
			: "* Mesh",
			MENU_SP_RENDER_SOURCE_MESH
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("Mesh Bound:", MENU_NOP);
		glutAddMenuEntry(
			m_arbiter.getSPMeshBoundMode() ==
			TheArbiter::SPMeshBoundMode::Default
			? "* Default { SELECTED }"
			: "* Default",
			MENU_SP_MESH_BOUND_DEFAULT
		);
		glutAddMenuEntry(
			m_arbiter.getSPMeshBoundMode() ==
			TheArbiter::SPMeshBoundMode::Fill
			? "* Fill { SELECTED }"
			: "* Fill",
			MENU_SP_MESH_BOUND_FILL
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("Display Mode:", MENU_NOP);
		glutAddMenuEntry(
			m_arbiter.getSPDisplayMode() ==
			TheArbiter::SPDisplayMode::Render
			? "* Render { SELECTED }"
			: "* Render",
			MENU_SP_DISPLAY_RENDER
		);
		glutAddMenuEntry(
			m_arbiter.getSPDisplayMode() ==
			TheArbiter::SPDisplayMode::RenderAndCollision
			? "* Render + Collision { SELECTED }"
			: "* Render + Collision",
			MENU_SP_DISPLAY_RENDER_COLLISION
		);
		glutAddMenuEntry(
			m_arbiter.getSPDisplayMode() ==
			TheArbiter::SPDisplayMode::Wireframe
			? "* Wireframe { SELECTED }"
			: "* Wireframe",
			MENU_SP_DISPLAY_WIREFRAME
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry("Render Cage:", MENU_NOP);
		glutAddMenuEntry(
			m_arbiter.isSPRenderCageVisible()
			? "* On { SELECTED }"
			: "* On",
			MENU_SP_RENDER_CAGE_ON
		);
		glutAddMenuEntry(
			!m_arbiter.isSPRenderCageVisible()
			? "* Off { SELECTED }"
			: "* Off",
			MENU_SP_RENDER_CAGE_OFF
		);

		glutAddMenuEntry("=========================================", MENU_NOP);
		glutAddMenuEntry(
			"* Mesh / Volume Preview and Edit",
			MENU_SP_VOLUME_PREVIEW
		);
		glutAddMenuEntry(
			"* Return to Collision Setup",
			MENU_SP_COLLISION_SETUP
		);
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
				"Procedural Primitive:",
				MENU_NOP
			);
			const TheArbiter::VolumePrimitive selectedPrimitive =
				m_arbiter.getVolumePrimitiveSelection();
			auto addPrimitiveEntry = [&](const char* name, int command,
				TheArbiter::VolumePrimitive primitive) {
				char label[96];
				snprintf(label, sizeof(label), "%s %s",
					selectedPrimitive == primitive ? "*" : " ", name);
				glutAddMenuEntry(label, command);
			};
			addPrimitiveEntry("BASE", MENU_PRIMITIVE_BASE, TheArbiter::VOLUME_PRIMITIVE_BASE);
			addPrimitiveEntry("SPHERE", MENU_PRIMITIVE_SPHERE, TheArbiter::VOLUME_PRIMITIVE_SPHERE);
			addPrimitiveEntry("TORUS", MENU_PRIMITIVE_TORUS, TheArbiter::VOLUME_PRIMITIVE_TORUS);
			addPrimitiveEntry("BLOCK", MENU_PRIMITIVE_BLOCK, TheArbiter::VOLUME_PRIMITIVE_BLOCK);
			addPrimitiveEntry("CYLINDER", MENU_PRIMITIVE_CYLINDER, TheArbiter::VOLUME_PRIMITIVE_CYLINDER);
			addPrimitiveEntry("CONE", MENU_PRIMITIVE_CONE, TheArbiter::VOLUME_PRIMITIVE_CONE);
			addPrimitiveEntry("CAPSULE", MENU_PRIMITIVE_CAPSULE, TheArbiter::VOLUME_PRIMITIVE_CAPSULE);
			addPrimitiveEntry("WEDGE", MENU_PRIMITIVE_WEDGE, TheArbiter::VOLUME_PRIMITIVE_WEDGE);
			addPrimitiveEntry("DELTA_WING", MENU_PRIMITIVE_DELTA_WING, TheArbiter::VOLUME_PRIMITIVE_DELTA_WING);
			addPrimitiveEntry("FRUSTUM", MENU_PRIMITIVE_FRUSTUM, TheArbiter::VOLUME_PRIMITIVE_FRUSTUM);
			glutAddMenuEntry(
				"=========================================",
				MENU_NOP
			);
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
					"Mirror Injection:",
					MENU_NOP
				);
				glutAddMenuEntry(
					m_arbiter.isSPMirrorEnabled()
					? "  Mirror { NONE }"
					: "* Mirror { NONE }",
					MENU_SP_MIRROR_NONE
				);
				glutAddMenuEntry(
					m_arbiter.isSPMirrorEnabled()
					? "* Mirror { ON }"
					: "  Mirror { ON }",
					MENU_SP_MIRROR_ON
				);
				glutAddMenuEntry(
					"=========================================",
					MENU_NOP
				);

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
			"* Save Static Particle",
			MENU_SAVE_STATIC_PARTICLE
		);

		glutAddMenuEntry(
			"* Save Static Particle As",
			MENU_SAVE_STATIC_PARTICLE_AS
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
		s_instance->handleStaticParticleRequests(result);
		if (s_instance->m_arbiter.isTextureMapLayer3RuntimeContext() ||
			result.textureMapRuntimeIntent != TheArbiter::TextureMapRuntimeIntent::None) {
			if (vitru::TextureMapWorkspace* workspace =
				s_instance->m_tesseract.getTextureMapWorkspaceRuntime())
				workspace->endAuthoringStroke();
			s_instance->handleTextureMapRuntimeResult(result);
		}

		if (result.command ==
			TheArbiter::CMD_PARTICLE_RENDER_MODE_CHANGED) {

			s_instance->applySingleParticleConfigToSystem();
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

	auto selectVolumePrimitive =
		[&](TheArbiter::VolumePrimitive primitive) {
			applyArbiterMenuResult(
				s_instance->m_arbiter.setVolumePrimitiveFromMenu(primitive));
	};

	switch (value) {
	case MENU_TM_RUNTIME_ROW_0:
	case MENU_TM_RUNTIME_ROW_1:
	case MENU_TM_RUNTIME_ROW_2:
	case MENU_TM_RUNTIME_ROW_3:
	case MENU_TM_RUNTIME_ROW_4:
	case MENU_TM_RUNTIME_ROW_5:
	case MENU_TM_RUNTIME_ROW_6:
		applyArbiterMenuResult(
			s_instance->m_arbiter.activateTextureMapRuntimeRowFromMenu(
				value - MENU_TM_RUNTIME_ROW_0));
		return;

	case MENU_TM_RUNTIME_TOGGLE_VIEW:
		applyArbiterMenuResult(
			s_instance->m_arbiter.toggleTextureMapRuntimeViewFromMenu());
		return;

	case MENU_TM_RUNTIME_TOGGLE_MESH:
		s_instance->onKeyboard('e', 0, 0);
		return;

		// =========================================================
		// Sub-Layer 0: selection and collision setup
		// =========================================================
	case MENU_SP_PRIMARY_ACTION:
		applyArbiterMenuResult(
			s_instance->m_arbiter.activateSPPrimaryActionFromMenu()
		);
		return;

	case MENU_SP_COLLISION_SPHERE:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPCollisionShapeFromMenu(
				TheArbiter::SPCollisionShape::Sphere
			)
		);
		return;

	case MENU_SP_COLLISION_BLOCK:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPCollisionShapeFromMenu(
				TheArbiter::SPCollisionShape::Block
			)
		);
		return;

	case MENU_SP_COLLISION_CAPSULE:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPCollisionShapeFromMenu(
				TheArbiter::SPCollisionShape::Capsule
			)
		);
		return;

	case MENU_SP_COLLISION_CONE:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPCollisionShapeFromMenu(
				TheArbiter::SPCollisionShape::Cone
			)
		);
		return;

	case MENU_SP_COLLISION_DEFORMABLE_SPHERE:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPCollisionShapeFromMenu(
				TheArbiter::SPCollisionShape::DeformableSphere
			)
		);
		return;

	case MENU_SP_RENDERING_SETUP:
		applyArbiterMenuResult(
			s_instance->m_arbiter.enterSPRenderingSetupFromMenu()
		);
		return;

	case MENU_SP_RETURN_TO_LAYER_2:
		applyArbiterMenuResult(
			s_instance->m_arbiter.returnSPToLayer2FromMenu()
		);
		return;

		// =========================================================
		// Sub-Layer 1: rendering setup
		// =========================================================
	case MENU_SP_RENDER_SOURCE_PARTICLE:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPRenderSourceFromMenu(
				TheArbiter::PARTICLE_RENDER_DEFAULT
			)
		);
		return;

	case MENU_SP_RENDER_SOURCE_MESH:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPRenderSourceFromMenu(
				TheArbiter::PARTICLE_RENDER_MESH
			)
		);
		return;

	case MENU_SP_MESH_BOUND_DEFAULT:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPMeshBoundModeFromMenu(
				TheArbiter::SPMeshBoundMode::Default
			)
		);
		return;

	case MENU_SP_MESH_BOUND_FILL:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPMeshBoundModeFromMenu(
				TheArbiter::SPMeshBoundMode::Fill
			)
		);
		return;

	case MENU_SP_DISPLAY_RENDER:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPDisplayModeFromMenu(
				TheArbiter::SPDisplayMode::Render
			)
		);
		return;

	case MENU_SP_DISPLAY_RENDER_COLLISION:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPDisplayModeFromMenu(
				TheArbiter::SPDisplayMode::RenderAndCollision
			)
		);
		return;

	case MENU_SP_DISPLAY_WIREFRAME:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPDisplayModeFromMenu(
				TheArbiter::SPDisplayMode::Wireframe
			)
		);
		return;

	case MENU_SP_RENDER_CAGE_ON:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPRenderCageVisibleFromMenu(true)
		);
		return;

	case MENU_SP_RENDER_CAGE_OFF:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPRenderCageVisibleFromMenu(false)
		);
		return;

	case MENU_SP_VOLUME_PREVIEW:
		applyArbiterMenuResult(
			s_instance->m_arbiter.enterSPVolumePreviewFromMenu()
		);
		return;

	case MENU_SP_COLLISION_SETUP:
		applyArbiterMenuResult(
			s_instance->m_arbiter.returnSPCollisionSetupFromMenu()
		);
		return;

	case MENU_PRIMITIVE_BASE:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_BASE); return;
	case MENU_PRIMITIVE_SPHERE:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_SPHERE); return;
	case MENU_PRIMITIVE_TORUS:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_TORUS); return;
	case MENU_PRIMITIVE_BLOCK:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_BLOCK); return;
	case MENU_PRIMITIVE_CYLINDER:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_CYLINDER); return;
	case MENU_PRIMITIVE_CONE:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_CONE); return;
	case MENU_PRIMITIVE_CAPSULE:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_CAPSULE); return;
	case MENU_PRIMITIVE_WEDGE:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_WEDGE); return;
	case MENU_PRIMITIVE_DELTA_WING:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_DELTA_WING); return;
	case MENU_PRIMITIVE_FRUSTUM:
		selectVolumePrimitive(TheArbiter::VOLUME_PRIMITIVE_FRUSTUM); return;

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

	case MENU_SP_MIRROR_NONE:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPMirrorModeFromMenu(
				TheArbiter::SP_MIRROR_NONE));
		return;

	case MENU_SP_MIRROR_ON:
		applyArbiterMenuResult(
			s_instance->m_arbiter.setSPMirrorModeFromMenu(
				TheArbiter::SP_MIRROR_ON));
		return;

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

	case MENU_SAVE_STATIC_PARTICLE:
		s_instance->openStaticParticleSaveConfirm("");
		glutPostRedisplay();
		return;

	case MENU_SAVE_STATIC_PARTICLE_AS: {
		TheArbiter::ArbiterResult result;
		s_instance->m_arbiter.beginSingleParticleAssetNameEntry(result);
		applyArbiterMenuResult(result);
		return;
	}

	case MENU_SP_LOAD_STATIC_PARTICLE:
		s_instance->openStaticParticleLoadPanel();
		glutPostRedisplay();
		return;

	case MENU_SP_SAVE_ACTIVE_PARTICLE:
		s_instance->openStaticParticleSaveConfirm("");
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

void EuclidEngine::sMenuStatus(int status, int x, int y) {
	(void)x;
	(void)y;
	if (!s_instance || status != GLUT_MENU_IN_USE) return;
	if (vitru::TextureMapWorkspace* workspace =
		s_instance->m_tesseract.getTextureMapWorkspaceRuntime())
		workspace->endAuthoringStroke();
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
	m_arbiter.setSPOverlapPreviewStatus(
		TheArbiter::SP_OVERLAP_POSITION_IN_NODE_2
	);

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

	float* dMirrorBrushVolume = m_tesseract.getMirrorBrushVolume();
	if (dMirrorBrushVolume) {
		freeArray(dMirrorBrushVolume);
		m_tesseract.clearMirrorBrushVolumeBinding();
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

void EuclidEngine::drawTesseractGridAndPlane(
	bool particleSimPreview) {

	const bool gridVisualReady =
		particleSimPreview
		? m_tesseract.applyPSGridVisual()
		: m_tesseract.applyWorkspaceBoundaryGridVisual();

	if (!gridVisualReady ||
		!m_renderer) return;

	// Draw the full 3D Tesseract boundary/grid.
	m_renderer->setGridMode3D();
	m_renderer->displayGrid();

	// ---------------------------------------------------------
	// Menu-layer moving diagnostic slice.
	// ---------------------------------------------------------

	if (!particleSimPreview &&
		(m_arbiter.isMenuLayer() ||
			m_tesseract.isTransitioningToWorkspace())) {

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

void EuclidEngine::applyPSGridLayoutToWorkspace(
	bool forceDiagnostic) {

	const TheArbiter::ParticleGridLayout layout =
		m_arbiter.getParticleSimDraftConfig().gridLayout;

	const bool changed =
		m_tesseract.setPSGridLayout(layout);

	if (!changed && !forceDiagnostic)
		return;

	printf(
		"[PARTICLE_SIM] Grid layout applied: %s%s\n",
		m_arbiter.getParticleGridLayoutName(),
		layout == TheArbiter::ParticleGridLayout::Dynamic
		? " (FULL fallback)"
		: ""
	);
}

// =============================================================================
// PARTICLE_SIM WORKSPACE SUPPORT (SIMCAD_4D)
// =============================================================================
bool EuclidEngine::applyParticleSelectionsToSystem() {
	if (!m_particleSimSystem) return false;

	const TheArbiter::ParticleSimDraftConfig& draft =
		m_arbiter.getParticleSimDraftConfig();

	const uint capacity =
		m_particleSimSystem->getCapacity();

	ParticleSimRuntimeConfig runtimeConfig;
	if (!ParticleSimRuntimeConfig::resolve(
		draft,
		capacity,
		runtimeConfig)) {

		printf(
			"[PARTICLE_SIM] Configuration rejected: count or radius "
			"is outside the supported runtime limits "
			"(capacity %u, maximum radius %.4f).\n",
			capacity,
			ParticleSimRuntimeConfig::kMaximumSupportedRadius
		);

		printf(
			"  requested count = %u\n",
			draft.colorMode == TheArbiter::ParticleColorMode::Default
			? draft.defaultParticleCount
			: m_arbiter.getParticleSimRGBTotal()
		);
		printf(
			"  requested radius mode = %s\n",
			draft.radiusMode == TheArbiter::ParticleRadiusMode::Random
			? "RANDOM"
			: "UNIFORM"
		);
		printf(
			"  requested radius = %.4f / %.4f - %.4f\n",
			draft.uniformRadius,
			draft.minimumRadius,
			draft.maximumRadius
		);

		return false;
	}

	m_particleSimActiveCount =
		runtimeConfig.activeCount;

	if (!m_particleSimSystem->setActiveParticleCount(
		m_particleSimActiveCount)) {

		printf(
			"[PARTICLE_SIM] Configuration rejected: active count %u "
			"could not be applied.\n",
			m_particleSimActiveCount
		);

		return false;
	}

	// Reset placement must use a radius large enough for every active
	// particle. RANDOM mode therefore places with maximumRadius before
	// assigning the individual velocity.w values.
	m_particleSimSystem->setParticleRadius(
		runtimeConfig.placementRadius
	);

	const ParticleSystem::ParticleConfig resetConfig =
		draft.resetMode == TheArbiter::ParticleSimResetMode::Random
		? ParticleSystem::CNFG_RANDOM_RESTART
		: ParticleSystem::CNFG_DEFAULT_RESTART;

	if (!m_tesseract.resetPSWorkspace(resetConfig))
		return false;

	if (!applyPSRadiusModeToSystem())
		return false;

	if (!applyPSColorModeToSystem())
		return false;

	syncRenderingWithParticleSystem();

	printf("[PARTICLE_SIM] Applied radius configuration:\n");
	printf("  capacity     = %u\n", capacity);
	printf("  active       = %u\n", m_particleSimActiveCount);
	printf(
		"  colorMode    = %s\n",
		draft.colorMode == TheArbiter::ParticleColorMode::Default
		? "DEFAULT"
		: "RGB"
	);

	if (draft.colorMode == TheArbiter::ParticleColorMode::RGB) {
		printf("  red          = %u\n", draft.redCount);
		printf("  green        = %u\n", draft.greenCount);
		printf("  blue         = %u\n", draft.blueCount);
	}

	printf(
		"  radiusMode   = %s\n",
		draft.radiusMode == TheArbiter::ParticleRadiusMode::Random
		? "RANDOM"
		: "UNIFORM"
	);

	if (draft.radiusMode == TheArbiter::ParticleRadiusMode::Random) {
		printf("  minimumRadius = %.4f\n", draft.minimumRadius);
		printf("  maximumRadius = %.4f\n", draft.maximumRadius);
		printf("  seed         = %u\n", 1973u);
	}
	else {
		printf("  radius       = %.4f\n", draft.uniformRadius);
	}

	printf(
		"  placementRadius = %.4f\n",
		runtimeConfig.placementRadius
	);

	printf(
		"  resetMode    = %s\n",
		draft.resetMode == TheArbiter::ParticleSimResetMode::Random
		? "RANDOM"
		: "DEFAULT"
	);

	return true;
}

bool EuclidEngine::applyPSRadiusModeToSystem() {
	if (!m_particleSimSystem) return false;

	const TheArbiter::ParticleSimDraftConfig& draft =
		m_arbiter.getParticleSimDraftConfig();

	if (draft.radiusMode == TheArbiter::ParticleRadiusMode::Uniform) {
		return m_particleSimSystem->setUniformActiveRadii(
			draft.uniformRadius
		);
	}

	if (draft.radiusMode == TheArbiter::ParticleRadiusMode::Random) {
		return m_particleSimSystem->setRandomActiveRadii(
			draft.minimumRadius,
			draft.maximumRadius,
			1973u
		);
	}

	printf(
		"[PARTICLE_SIM] Unsupported radius mode was not applied.\n"
	);

	return false;
}

bool EuclidEngine::applyPSColorModeToSystem() {
	if (!m_particleSimSystem) return false;

	const TheArbiter::ParticleSimDraftConfig& draft =
		m_arbiter.getParticleSimDraftConfig();

	if (draft.colorMode == TheArbiter::ParticleColorMode::Default) {

		m_particleSimSystem->setDefaultColorRamp();

		return true;
	}

	return m_particleSimSystem->setRGBParticleCounts(
		draft.redCount,
		draft.greenCount,
		draft.blueCount
	);
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

	const bool particleSimulationRunActive =
		m_arbiter.isSimulationRunLayer() &&
		m_arbiter.isParticleSimulationSelected();

	const bool singleParticleConfigPreviewActive =
		m_arbiter.isParticleConfigLayer() &&
		m_arbiter.isSingleParticleSelected() &&
		m_tesseract.isPlacedSP();

	const bool singleParticleRunActive =
		m_arbiter.isSimulationRunLayer() &&
		m_arbiter.isSingleParticleSelected();

	const bool textureMapActive =
		m_arbiter.isTextureMapLayer2PanelContext() ||
		m_arbiter.isTextureMapLayer3RuntimeContext();

	if (particleSimulationRunActive) {
		targetWorkspace =
			TheArbiter::WorkspaceId::PARTICLE_SIMULATION;
	}
	else if (singleParticleConfigPreviewActive || singleParticleRunActive) {
		targetWorkspace =
			TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD;
	}
	else if (textureMapActive) {
		targetWorkspace =
			TheArbiter::WorkspaceId::TEXTURE_MAP_2D;
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

	if (m_arbiter.isTextureMapLayer2PanelContext()) {

		m_camera.setBehaviorMode(
			CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
		);

		return;
	}

	if (m_arbiter.isTextureMapLayer3RuntimeContext()) {
		m_camera.setBehaviorMode(
			m_arbiter.isTextureMapRuntimeFixedSelectionCamera()
			? CameraProcessor::CAM_SINGLE_PARTICLE_WORKPLANE_LOCKED
			: CameraProcessor::CAM_SINGLE_PARTICLE_ORBIT_CLOSE
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
	const bool boundaryChanged =
		m_arbiter.setVolumeBoundaryStatus(m_tesseract.isSPVolumeBoundarySensorReady(),
			m_tesseract.getSPVolumeBoundaryUnsafeCount());

	TheArbiter::SPOverlapPreviewStatus overlapStatus =
		TheArbiter::SP_OVERLAP_POSITION_IN_NODE_2;

	const bool overlapContext =
		m_arbiter.isVolumeRenderSubLayer() &&
		m_arbiter.hasInjectionVoxelSelected() &&
		(m_arbiter.getVolumeAssemblyNode() ==
			TheArbiter::VOLUME_NODE_EDIT_OBJECT ||
			m_arbiter.getVolumeAssemblyNode() ==
			TheArbiter::VOLUME_NODE_OFFSET_OBJECT);

	if (overlapContext &&
		m_tesseract.isSPOverlapPreviewSensorReady()) {

		if (m_tesseract.isSPOverlapPreviewActive()) {
			overlapStatus =
				TheArbiter::SP_OVERLAP_ACTIVE;
		}
		else if (m_tesseract.getSPOverlapPreviewUnsafeCount() > 0 ||
			m_tesseract.getSPOverlapPreviewInsideSampleCount() > 0) {
			overlapStatus =
				TheArbiter::SP_OVERLAP_OUTSIDE_CAGE;
		}
	}

	const bool overlapChanged =
		m_arbiter.setSPOverlapPreviewStatus(overlapStatus);

	if ((boundaryChanged || overlapChanged) &&
		m_arbiter.isVolumeRenderSubLayer()) {
		rebuildMenus();
	}
}

// =============================================================================
// NAMED STATIC PARTICLE ASSET JOBS
// =============================================================================
bool EuclidEngine::makeCurrentStaticParticleAsset(
	vitru::StaticParticleAsset& output) const {

	auto attachCurrentNativeVolume =
		[this](
			vitru::StaticParticleAsset& asset
			) -> bool {

				std::vector<float> samples;

				if (!m_tesseract.exportWorkingVolumeToHost(
					samples)) {

					return false;
				}

				const int3& dimensions =
					m_tesseract.getVolumeSize();

				asset.volumetricSource.available =
					true;

				asset.volumetricSource.file =
					"volume/base_volume.f32";

				asset.volumetricSource.format =
					"FLOAT32_SDF";

				asset.volumetricSource.dimensions[0] =
					static_cast<std::uint32_t>(
						dimensions.x
						);

				asset.volumetricSource.dimensions[1] =
					static_cast<std::uint32_t>(
						dimensions.y
						);

				asset.volumetricSource.dimensions[2] =
					static_cast<std::uint32_t>(
						dimensions.z
						);

				asset.volumetricSource.isoValue =
					0.0f;

				asset.volumetricSource.samples =
					std::move(samples);

				return true;
	};

	// ---------------------------------------------------------
	// Native SP_MCAD save from Marching Cubes.
	//
	// The canonical mesh and m_dWorkingVolume represent the same
	// currently previewed object.
	// ---------------------------------------------------------
	if (m_arbiter.isMarchingCubesSubLayer() &&
		m_marchingCubes &&
		m_marchingCubes->hasTriangleData() &&
		!m_marchingCubes->getCanonicalMesh().empty()) {

		output =
			vitru::StaticParticleAsset{};

		output.name =
			"Static Particle";

		output.mesh =
			m_marchingCubes->getCanonicalMesh();

		output.materials.emplace_back();

		output.source.kind =
			"NATIVE_SP_MCAD";

		output.anchor.particleIndex =
			0u;

		output.anchor.pivotMode =
			vitru::ParticlePivotMode::GroundCenter;

		output.anchor.fitMode =
			vitru::ParticleFitMode::CollisionSafe;

		output.collision.shape =
			vitru::CollisionProxy::Shape::Sphere;

		output.collision.radius =
			m_arbiter.getParticleRadius();

		output.refreshDerivedData();

		if (!attachCurrentNativeVolume(
			output)) {

			printf(
				"[EuclidEngine] Static asset save failed: "
				"native scalar field could not be captured.\n"
			);

			return false;
		}

		return true;
	}

	// ---------------------------------------------------------
	// Existing active StaticParticleAsset.
	// ---------------------------------------------------------
	const vitru::StaticParticleAsset* active =
		m_assetRepository.activeStaticParticle();

	if (!active) {
		return false;
	}

	output =
		*active;

	// Refresh the persisted field after native-volume editing.
	if (m_tesseract.hasCommittedGeometry()) {

		if (!attachCurrentNativeVolume(
			output)) {

			printf(
				"[EuclidEngine] Static asset save failed: "
				"active native scalar field could not be captured.\n"
			);

			return false;
		}
	}

	return true;
}

void EuclidEngine::openStaticParticleLoadPanel() {
	if (isObjExportModalActive() || m_staticAssetFutureActive) return;
	m_staticAssetCatalog = vitru::enumerateStaticParticleAssets(
		m_inputsRoot,
		m_outputRoot / "STATIC_PARTICLES");
	std::vector<vitru::StaticAssetCatalogEntry> valid;
	for (const vitru::StaticAssetCatalogEntry& entry : m_staticAssetCatalog)
		if (entry.valid) valid.push_back(entry);
	m_staticAssetCatalog.swap(valid);
	m_staticAssetPanel = ViewPort::ObjExportPanelData{};
	m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::SELECT;
	m_staticAssetPanel.titleText = "VITRUGEN STATIC PARTICLE LOAD";
	m_staticAssetPanel.selectedIndex = 0;
	for (const vitru::StaticAssetCatalogEntry& entry : m_staticAssetCatalog) {
		m_staticAssetPanel.selectionLines.push_back(
			entry.displayName + "  |  " + entry.source +
			"  |  VALID  |  " + entry.manifestPath.generic_string());
	}
	m_staticAssetJobKind = StaticAssetJobKind::Load;
	glutPostRedisplay();
}

void EuclidEngine::openStaticParticleSaveConfirm(
	const std::string& displayName) {
	if (isObjExportModalActive() || m_staticAssetFutureActive) return;
	vitru::StaticParticleAsset asset;
	if (!makeCurrentStaticParticleAsset(asset)) {
		m_staticAssetPanel = ViewPort::ObjExportPanelData{};
		m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::FAILED;
		m_staticAssetPanel.titleText = "VITRUGEN STATIC PARTICLE SAVE";
		m_staticAssetPanel.statusText = "No canonical mesh or active asset.";
		m_staticAssetPanel.logLines.push_back("SAVE disabled: no valid StaticParticleAsset is active.");
		return;
	}
	if (displayName.empty()) {
		const vitru::StaticParticleAsset* active =
			m_assetRepository.activeStaticParticle();
		if (!active || active->name.empty()) {
			m_staticAssetPanel = ViewPort::ObjExportPanelData{};
			m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::FAILED;
			m_staticAssetPanel.titleText = "VITRUGEN STATIC PARTICLE SAVE";
			m_staticAssetPanel.statusText = "Use SAVE STATIC PARTICLE AS first.";
			m_staticAssetPanel.logLines.push_back("SAVE disabled: active asset has no named output location.");
			return;
		}
		m_pendingStaticAssetName = active->name;
	}
	else {
		m_pendingStaticAssetName = displayName;
	}
	m_staticAssetPanel = ViewPort::ObjExportPanelData{};
	m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::CONFIRM;
	m_staticAssetPanel.titleText = "VITRUGEN STATIC PARTICLE SAVE";
	m_staticAssetPanel.confirmText = "Save named asset: " + m_pendingStaticAssetName + " ?";
	m_staticAssetPanel.yesSelected = true;
	m_staticAssetJobKind = StaticAssetJobKind::Save;
	glutPostRedisplay();
}

void EuclidEngine::beginStaticParticleAssetJob() {
	if (m_staticAssetFutureActive) return;
	StaticAssetJobKind kind = m_staticAssetJobKind;
	vitru::StaticParticleAsset source;
	fs::path manifest;
	if (kind == StaticAssetJobKind::Save) {
		if (!makeCurrentStaticParticleAsset(source)) {
			m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::FAILED;
			m_staticAssetPanel.statusText = "Active asset validation failed.";
			return;
		}
	}
	else if (kind == StaticAssetJobKind::Load) {
		if (m_staticAssetCatalog.empty() || m_staticAssetPanel.selectedIndex < 0 ||
			m_staticAssetPanel.selectedIndex >= static_cast<int>(m_staticAssetCatalog.size())) return;
		manifest = m_staticAssetCatalog[static_cast<size_t>(m_staticAssetPanel.selectedIndex)].manifestPath;
	}
	else return;

	m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::WORKING;
	m_staticAssetPanel.progressPercent = 10;
	m_staticAssetPanel.spinnerFrame = 0;
	m_staticAssetPanel.statusText = kind == StaticAssetJobKind::Save
		? "validating mesh" : "reading VSPA";
	m_staticAssetPanel.logLines.clear();
	m_staticAssetPanel.logLines.push_back(kind == StaticAssetJobKind::Save
		? "[VSPA] validating mesh and material resources"
		: "[VSPA] reading selected manifest");
	const fs::path outputRoot = m_outputRoot;
	const fs::path workspace = m_workspaceObj;
	const std::string name = m_pendingStaticAssetName;
	m_staticAssetFuture = std::async(std::launch::async,
		[kind, source, manifest, outputRoot, workspace, name]() mutable {
			StaticAssetAsyncResult result;
			if (kind == StaticAssetJobKind::Save) {
				result.success = vitru::saveStaticParticleBundle(
					source, outputRoot, name, workspace, result.report);
				if (result.success) {
					vitru::StaticAssetOperationReport reopened;
					result.success = vitru::loadStaticParticleBundle(
						result.report.manifestPath, result.asset, reopened,
						nullptr, workspace);
					result.report.warnings.insert(result.report.warnings.end(),
						reopened.warnings.begin(), reopened.warnings.end());
					result.report.errors.insert(result.report.errors.end(),
						reopened.errors.begin(), reopened.errors.end());
				}
			}
			else {
				result.success = vitru::loadStaticParticleBundle(
					manifest, result.asset, result.report, nullptr, workspace);
			}
			return result;
		});
	m_staticAssetFutureActive = true;
	m_staticAssetLastSpinnerMs = glutGet(GLUT_ELAPSED_TIME);
}

void EuclidEngine::advanceStaticParticleAssetJob() {
	if (!m_staticAssetFutureActive || !m_staticAssetFuture.valid()) return;
	const int now = glutGet(GLUT_ELAPSED_TIME);
	if (now - m_staticAssetLastSpinnerMs >= 100) {
		m_staticAssetPanel.spinnerFrame =
			(m_staticAssetPanel.spinnerFrame + 1) % 4;
		m_staticAssetPanel.progressPercent =
			(std::min)(90, m_staticAssetPanel.progressPercent + 1);
		m_staticAssetLastSpinnerMs = now;
	}
	if (m_staticAssetFuture.wait_for(std::chrono::milliseconds(0)) !=
		std::future_status::ready) return;
	StaticAssetAsyncResult result = m_staticAssetFuture.get();
	m_staticAssetFutureActive = false;
	for (const std::string& warning : result.report.warnings)
		m_staticAssetPanel.logLines.push_back("[WARN] " + warning);
	for (const std::string& error : result.report.errors)
		m_staticAssetPanel.logLines.push_back("[ERROR] " + error);
	if (!result.success) {
		m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::FAILED;
		m_staticAssetPanel.statusText = result.report.phase;
		return;
	}
	const vitru::AssetId id = m_assetRepository.addStaticParticle(result.asset);
	m_assetRepository.setActiveStaticParticle(id);

	vitru::StaticParticleAsset* active =
		m_assetRepository.activeStaticParticle();

	if (!active || !m_renderer ||
		!m_renderer->loadParticleStaticAsset(*active)) {

		m_staticAssetPanel.mode =
			ViewPort::ObjExportPanelMode::FAILED;

		m_staticAssetPanel.statusText =
			"GPU asset upload failed.";

		m_staticAssetPanel.logLines.push_back(
			"[ERROR] renderer could not upload the loaded asset"
		);

		return;
	}

	// ---------------------------------------------------------
// Restore the optional native scalar field on the main thread.
//
// Bundle parsing and binary file reading happened in the async
// worker. CUDA upload happens here, where the application CUDA/GL
// context is active.
// ---------------------------------------------------------
	bool editableVolumeRestored = false;

	if (active->volumetricSource.available) {

		const int3 sourceDimensions =
			make_int3(
				static_cast<int>(
					active->volumetricSource.dimensions[0]
					),
				static_cast<int>(
					active->volumetricSource.dimensions[1]
					),
				static_cast<int>(
					active->volumetricSource.dimensions[2]
					)
			);

		if (!m_tesseract.restoreCommittedVolumeFromHost(
			active->volumetricSource.samples,
			sourceDimensions)) {

			m_staticAssetPanel.mode =
				ViewPort::ObjExportPanelMode::FAILED;

			m_staticAssetPanel.statusText =
				"Native volume GPU restore failed.";

			m_staticAssetPanel.logLines.push_back(
				"[ERROR] FLOAT32_SDF could not be restored "
				"into the SP_MCAD CUDA volume."
			);

			return;
		}

		editableVolumeRestored = true;

		m_staticAssetPanel.logLines.push_back(
			"[VSPA] native FLOAT32_SDF volume restored"
		);
	}

	// A volume-backed load enters the normal CUDA volume path.
	// A mesh-only load retains the mesh-backed BASE path.
	m_arbiter.activateLoadedStaticParticleBase(
		editableVolumeRestored
	);

	m_arbiter.setParticleRenderMode(
		TheArbiter::PARTICLE_RENDER_MESH
	);

	applySingleParticleConfigToSystem();

	m_staticAssetPanel.mode = ViewPort::ObjExportPanelMode::COMPLETE;
	m_staticAssetPanel.progressPercent = 100;
	m_staticAssetPanel.statusText =
		editableVolumeRestored
		? "geometry, materials, textures, p0 and native volume ready"
		: "geometry, materials, textures and p0 ready";
	m_staticAssetPanel.logLines.push_back("[VSPA] active repository and p0.obj refreshed");
	m_staticAssetPanel.logLines.push_back("[EuclidRenderer] textured material ranges uploaded");
	rebuildMenus();
}

void EuclidEngine::closeStaticParticleAssetPanel() {
	if (m_staticAssetFutureActive) return;
	m_staticAssetPanel = ViewPort::ObjExportPanelData{};
	m_staticAssetCatalog.clear();
	m_staticAssetJobKind = StaticAssetJobKind::None;
	m_pendingStaticAssetName.clear();
	glutPostRedisplay();
}

bool EuclidEngine::isStaticParticleAssetModalActive() const {
	return m_staticAssetPanel.mode != ViewPort::ObjExportPanelMode::HIDDEN;
}

bool EuclidEngine::handleStaticParticleAssetModalKeyboard(
	const KeyboardInput::KeyEvent& event) {
	if (!isStaticParticleAssetModalActive()) return false;
	using Mode = ViewPort::ObjExportPanelMode;
	if (m_staticAssetPanel.mode == Mode::SELECT) {
		if (event.signal == KeyboardInput::KEY_W && !m_staticAssetCatalog.empty())
			m_staticAssetPanel.selectedIndex = (m_staticAssetPanel.selectedIndex - 1 + static_cast<int>(m_staticAssetCatalog.size())) % static_cast<int>(m_staticAssetCatalog.size());
		else if (event.signal == KeyboardInput::KEY_S && !m_staticAssetCatalog.empty())
			m_staticAssetPanel.selectedIndex = (m_staticAssetPanel.selectedIndex + 1) % static_cast<int>(m_staticAssetCatalog.size());
		else if ((event.signal == KeyboardInput::KEY_E || event.signal == KeyboardInput::KEY_ENTER) && !m_staticAssetCatalog.empty()) beginStaticParticleAssetJob();
		else if (event.signal == KeyboardInput::KEY_Q || event.signal == KeyboardInput::KEY_ESCAPE) closeStaticParticleAssetPanel();
		glutPostRedisplay(); return true;
	}
	if (m_staticAssetPanel.mode == Mode::CONFIRM) {
		if (event.signal == KeyboardInput::KEY_W || event.signal == KeyboardInput::KEY_S || event.signal == KeyboardInput::KEY_A || event.signal == KeyboardInput::KEY_D) m_staticAssetPanel.yesSelected = !m_staticAssetPanel.yesSelected;
		else if (event.signal == KeyboardInput::KEY_E || event.signal == KeyboardInput::KEY_ENTER) { if (m_staticAssetPanel.yesSelected) beginStaticParticleAssetJob(); else closeStaticParticleAssetPanel(); }
		else if (event.signal == KeyboardInput::KEY_Q || event.signal == KeyboardInput::KEY_ESCAPE) closeStaticParticleAssetPanel();
		glutPostRedisplay(); return true;
	}
	if (m_staticAssetPanel.mode == Mode::WORKING) return true;
	if (event.signal == KeyboardInput::KEY_E || event.signal == KeyboardInput::KEY_ENTER || event.signal == KeyboardInput::KEY_Q || event.signal == KeyboardInput::KEY_ESCAPE) closeStaticParticleAssetPanel();
	return true;
}

void EuclidEngine::handleStaticParticleRequests(
	const TheArbiter::ArbiterResult& result) {
	if (result.loadStaticParticleRequested) openStaticParticleLoadPanel();
	if (result.saveStaticParticleAsRequested)
		openStaticParticleSaveConfirm(result.staticParticleAssetName);
	else if (result.saveStaticParticleAsRequested)
		openStaticParticleSaveConfirm("");
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

	const bool textureMapWorkspaceActive =
		m_tesseract.getActiveWorkspace() ==
		TheArbiter::WorkspaceId::TEXTURE_MAP_2D;

	const bool singleParticleWorkspaceActive =
		m_tesseract.getActiveWorkspace() ==
		TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD;

	const bool particleSimulationWorkspaceActive =
		m_tesseract.getActiveWorkspace() ==
		TheArbiter::WorkspaceId::PARTICLE_SIMULATION;

	const bool particleSimulationPreviewActive =
		m_arbiter.isParticleSimulationSelected() &&
		(m_arbiter.isParticleSimLayer1PanelContext() ||
			m_arbiter.isParticleConfigLayer());

	// The global diagnostic grid is used by the menu and configuration
	// layers. Active particle workspaces own their local render context.
	if (!singleParticleWorkspaceActive &&
		!particleSimulationWorkspaceActive &&
		!textureMapWorkspaceActive) {

		if (particleSimulationPreviewActive)
			applyPSGridLayoutToWorkspace();

		drawTesseractGridAndPlane(
			particleSimulationPreviewActive
		);
	}

	if (particleSimulationWorkspaceActive ||
		singleParticleWorkspaceActive ||
		textureMapWorkspaceActive) {
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

		if (singleParticleWorkspaceActive) {
			syncVolumeBoundaryStatusFromTesseract();
		}
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
	if (m_staticAssetPanel.mode != ViewPort::ObjExportPanelMode::HIDDEN) {
		exportPanelDataPtr = &m_staticAssetPanel;
	}
	else if (m_objExportPanel.mode != ViewPort::ObjExportPanelMode::HIDDEN) {

		exportPanelDataPtr = &m_objExportPanel;
	}

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 1 presentation bridge.
	//
	// TextureMapWorkspace owns the catalog.
	// EuclidEngine converts catalog state into display-only data.
	// ViewPort only renders the supplied presentation state.
	// ---------------------------------------------------------
	ViewPort::TextureMapLayer1PanelData textureMapPanelData;

	const ViewPort::TextureMapLayer1PanelData*
		textureMapPanelDataPtr = nullptr;

	if (m_arbiter.isTextureMapLayer1PanelContext()) {

		textureMapPanelDataPtr =
			&textureMapPanelData;

		const vitru::TextureMapWorkspace* textureWorkspace =
			m_tesseract.getTextureMapWorkspaceRuntime();

		if (!textureWorkspace) {
			textureMapPanelData.catalogReady = false;

			textureMapPanelData.statusMessage =
				"TEXTURE MAP WORKSPACE RUNTIME UNAVAILABLE.";
		}
		else {

			textureMapPanelData.catalogReady =
				textureWorkspace->outputCatalogReady();

			textureMapPanelData.hasOutputAssets =
				textureWorkspace->hasOutputAssets();

			textureMapPanelData.targetLoaded =
				textureWorkspace->target().loaded;

			const vitru::StaticAssetCatalogEntry* selected =
				textureWorkspace->selectedOutputAsset();

			if (selected) {

				textureMapPanelData.targetName =
					selected->displayName;

				textureMapPanelData.selectedAssetValid =
					selected->valid;

				if (textureWorkspace->target().loaded) {

					textureMapPanelData.statusMessage =
						"TARGET LOADED: " +
						selected->displayName +
						" | TEXTURE_MAP_2D CONFIGURATION READY.";
				}
				else if (
					textureWorkspace->target().readiness ==
					vitru::TextureTargetReadiness::Invalid) {

					textureMapPanelData.statusMessage =
						"TARGET LOAD FAILED: " +
						selected->displayName +
						" | CONFIGURATION LOCKED.";
				}
				else if (selected->valid) {

					textureMapPanelData.statusMessage =
						"SELECTED TARGET: " +
						selected->displayName +
						" | LOAD TARGET ASSET WITH ROW [3].";
				}
				else {

					textureMapPanelData.statusMessage =
						"SELECTED TARGET INVALID: " +
						selected->displayName +
						" | CONFIGURATION LOCKED.";
				}
			}
			else if (textureWorkspace->outputCatalogReady()) {

				textureMapPanelData.statusMessage =
					"NO OUTPUT STATIC PARTICLE ASSETS FOUND.";
			}
			else {

				textureMapPanelData.statusMessage =
					"OUTPUT STATIC PARTICLE CATALOG UNAVAILABLE.";
			}
		}
	}

	ViewPort::TextureMapLayer2PanelData
		textureMapLayer2PanelData;

	const ViewPort::TextureMapLayer2PanelData*
		textureMapLayer2PanelDataPtr = nullptr;

	if (m_arbiter.isTextureMapLayer2PanelContext()) {

		textureMapLayer2PanelDataPtr =
			&textureMapLayer2PanelData;

		const vitru::TextureMapWorkspace* textureWorkspace =
			m_tesseract.getTextureMapWorkspaceRuntime();

		if (textureWorkspace) {

			const vitru::TextureMapTargetContext& target =
				textureWorkspace->target();

			textureMapLayer2PanelData.targetLoaded =
				target.loaded;

			textureMapLayer2PanelData.targetReady =
				target.readiness ==
				vitru::TextureTargetReadiness::Ready;

			textureMapLayer2PanelData.previewParticleRadius =
				target.previewParticleRadius;

			textureMapLayer2PanelData.pixelGridDivisions =
				static_cast<unsigned int>(
					target.pixelGridDivisions
				);

			const vitru::StaticParticleAsset* active =
				m_assetRepository.findStaticParticle(
					target.assetId
				);

			if (active) {

				textureMapLayer2PanelData.targetName =
					active->name;
			}
		}
	}

	ViewPort::TextureMapLayer3PanelData textureMapLayer3PanelData;
	const ViewPort::TextureMapLayer3PanelData* textureMapLayer3PanelDataPtr = nullptr;
	if (m_arbiter.isTextureMapLayer3RuntimeContext()) {
		textureMapLayer3PanelDataPtr = &textureMapLayer3PanelData;
		const vitru::TextureMapWorkspace* workspace =
			m_tesseract.getTextureMapWorkspaceRuntime();
		if (workspace) {
			textureMapLayer3PanelData.panelVisible =
				m_arbiter.isTextureMapRuntimePanelVisible();
			const vitru::StaticParticleAsset* asset =
				m_assetRepository.findStaticParticle(workspace->target().assetId);
			textureMapLayer3PanelData.targetName = asset ? asset->name : "UNAVAILABLE";
			textureMapLayer3PanelData.status = workspace->session().dirty ? "DIRTY" : "CLEAN";
			auto modeName = [&]() -> std::string {
				if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour)
					return "CONTOUR";
				if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::PanelLines)
					return "PANEL_LINES";
				return "COLORING";
			};
			auto channelName = [&]() -> std::string {
				switch (workspace->selectedChannel()) {
				case vitru::TextureMapChannel::EmissiveColor: return "EMISSIVE_COLOR";
				case vitru::TextureMapChannel::AlphaMask: return "ALPHA_MASK";
				case vitru::TextureMapChannel::Height: return "HEIGHT";
				case vitru::TextureMapChannel::Normal: return "NORMAL";
				case vitru::TextureMapChannel::MetallicRoughness: return "METALLIC_ROUGHNESS";
				case vitru::TextureMapChannel::Occlusion: return "OCCLUSION";
				default: return "BASE_COLOR";
				}
			};
			auto surfaceName = [&]() -> std::string {
				if (!asset || workspace->selectedSurfaceTarget() < 0) return "default";
				const std::size_t index = static_cast<std::size_t>(workspace->selectedSurfaceTarget());
				return index < asset->surfaceTargets.size()
					? asset->surfaceTargets[index].name : "default";
			};
			textureMapLayer3PanelData.authoringMode = modeName();
			const std::string grid = std::to_string(workspace->target().pixelGridDivisions);

			if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::CycleSetup) {
				textureMapLayer3PanelData.subLayerLabel = "SUB-LAYER_0 -> AUTHORING CYCLE SETUP";
				textureMapLayer3PanelData.informationLines = {
					"WORKING SESSION | PIXEL GRID { " + grid + " x " + grid + " }"
				};
				textureMapLayer3PanelData.rows = {
					"[1] AUTHORING MODE { " + modeName() + " }",
					"[2] PREVIEW SOURCE { " + std::string(
						workspace->previewSource() == vitru::TextureMapPreviewSource::Working
						? "WORKING" : "COMMITTED") + " }",
					"[3] CONFIGURE AUTHORING PASS"
				};
				textureMapLayer3PanelData.footerLine1 =
					"W/S: SELECT | A/D: CHANGE | E: ACTIVATE | TAB: HIDE PANEL";
			}
			else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::BranchSetup) {
				textureMapLayer3PanelData.subLayerLabel = "SUB-LAYER_1 -> " + modeName() + " SETUP";
				if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour) {
					std::string target = "NEW";
					if (workspace->contourAction() == vitru::TextureMapContourAction::EditExisting) {
						if (!asset || asset->surfaceTargets.empty()) target = "NONE AVAILABLE";
						else target = asset->surfaceTargets[
							static_cast<std::size_t>(std::max(0, workspace->selectedContourTarget())) %
							asset->surfaceTargets.size()].name;
					}
					textureMapLayer3PanelData.rows = {
						"[1] CONTOUR ACTION { " + std::string(
							workspace->contourAction() == vitru::TextureMapContourAction::New
							? "NEW" : "EDIT EXISTING") + " }",
						"[2] CONTOUR TARGET { " + target + " }",
						"[3] DRAW CONTOUR", "[4] AUTHORING CYCLE SETUP"
					};
				}
				else {
					textureMapLayer3PanelData.informationLines = {
						"UV MAP { TEXCOORD_0 } | SOURCE { BOX_ATLAS_6_DIRECTION } [ READY ]"
					};
					if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Coloring) {
						const std::string active = vitru::TextureMapWorkspace::channelActive(
							workspace->selectedChannel()) ? "ACTIVE" : "INACTIVE";
						textureMapLayer3PanelData.rows = {
							"[1] SURFACE TARGET { " + surfaceName() + " }",
							"[2] TEXTURE CHANNEL { " + channelName() + " } [ " + active + " ]",
							"[3] DRAW PIXEL GRID", "[4] AUTHORING CYCLE SETUP"
						};
						if (workspace->nestedFocus()) {
							std::ostringstream nested;
							if (workspace->selectedChannel() == vitru::TextureMapChannel::EmissiveColor)
								nested << "FOCUS > INTENSITY { " << std::fixed << std::setprecision(2)
									<< workspace->emissiveIntensity() << " }";
							else nested << "FOCUS > TEXTURE MAP SOURCE";
							textureMapLayer3PanelData.informationLines.push_back(nested.str());
						}
					}
					else textureMapLayer3PanelData.rows = {
						"[1] SURFACE TARGET { " + surfaceName() + " }",
						"[2] TEXTURE CHANNEL { BASE_COLOR } [ ACTIVE ]",
						"[3] DRAW PANEL LINES", "[4] AUTHORING CYCLE SETUP"
					};
				}
				textureMapLayer3PanelData.footerLine1 =
					"W/S: SELECT | A/D: CHANGE | E: ACTIVATE / FOCUS";
			}
			else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor) {
				textureMapLayer3PanelData.subLayerLabel = "SUB-LAYER_2 -> PIXEL GRID " + modeName();
				textureMapLayer3PanelData.informationLines = {
					"VIEW { " + std::string(workspace->viewMode() == vitru::TextureMapViewMode::Edit
						? "EDIT" : "PREVIEW") + " } | FACE_" +
						std::to_string(workspace->selectedFace()) + " { " +
						vitru::TextureMapWorkspace::boxAtlasFaceAxisName(workspace->selectedFace()) + " }"
				};
				if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Coloring) {
					const auto& c = workspace->paintColor();
					textureMapLayer3PanelData.rows = {
						"[1] SELECT FACE { FACE_" + std::to_string(workspace->selectedFace()) + " }",
						"[2] RED { " + std::to_string(c[0]) + " }",
						"[3] GREEN { " + std::to_string(c[1]) + " }",
						"[4] BLUE { " + std::to_string(c[2]) + " }",
						"[5] ALPHA { " + std::to_string(c[3]) + " }",
						"[6] REVIEW / COMMIT", "[7] COLORING SETUP"
					};
				}
				else if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour) {
					textureMapLayer3PanelData.informationLines.push_back(
						"CONTOUR { " + std::string(workspace->contourClosed() ? "CLOSED | READY" : "OPEN") +
						" } | POINTS { " + std::to_string(workspace->contourPointCount()) + " }");
					textureMapLayer3PanelData.rows = {
						"[1] SELECT FACE { FACE_" + std::to_string(workspace->selectedFace()) + " }",
						"[2] CLOSE CONTOUR", "[3] UNDO LAST POINT",
						"[4] REVIEW / COMMIT", "[5] CONTOUR SETUP"
					};
				}
				else {
					const auto& preset = vitru::TextureMapWorkspace::lineColorPresets()[
						workspace->lineColorPresetIndex()];
					textureMapLayer3PanelData.rows = {
						"[1] SELECT FACE { FACE_" + std::to_string(workspace->selectedFace()) + " }",
						"[2] LINE THICKNESS { " + std::to_string(workspace->lineThickness()) + " }",
						"[3] LINE COLOR { " + std::string(preset.name) + " }",
						"[4] REVIEW / COMMIT", "[5] PANEL LINE SETUP"
					};
				}
				textureMapLayer3PanelData.footerLine1 =
					"W/S: SELECT | A/D: CHANGE | E: ACTIVATE | TAB: EDIT / PREVIEW";
			}
			else {
				textureMapLayer3PanelData.subLayerLabel = "SUB-LAYER_3 -> COMMIT AUTHORING PASS";
				textureMapLayer3PanelData.rows = {
					"[1] COMMIT WORKING EDIT TO TARGET", "[2] SAVE CURRENT SP_ASSET",
					"[3] SAVE STATIC PARTICLE AS", "[4] RETURN TO PIXEL GRID",
					"[5] RETURN TO AUTHORING CYCLE SETUP"
				};
				textureMapLayer3PanelData.footerLine1 = "W/S: SELECT | E: ACTIVATE";
			}

			using PanelLine = ViewPort::TextureMapLayer3PanelLine;
			using PanelSection = ViewPort::TextureMapLayer3PanelSection;
			auto selectableLine = [&](const std::string& text, int row,
				bool subordinate, bool nestedEntry) {
				PanelLine line;
				line.text = text;
				line.selectable = true;
				line.subordinate = subordinate;
				line.selected = m_arbiter.getTextureMapRuntimeRow() == row &&
					(workspace->nestedFocus() == nestedEntry);
				return line;
			};
			auto informationLine = [](const std::string& text,
				bool subordinate, bool emphasized) {
				PanelLine line;
				line.text = text;
				line.subordinate = subordinate;
				line.emphasized = emphasized;
				return line;
			};
			auto addSection = [&](const std::string& heading,
				std::vector<PanelLine> lines) {
				PanelSection section;
				section.heading = heading;
				section.lines = std::move(lines);
				textureMapLayer3PanelData.sections.push_back(std::move(section));
			};

			if (!m_arbiter.isTextureMapRuntimeAuthoring()) {
				const char* gateName = m_arbiter.isTextureMapRuntimeMeshSelected()
					? "TARGET SELECTED"
					: m_arbiter.isTextureMapRuntimeSelectionArmed()
					? "SELECTION ARMED"
					: "REFERENCE PREVIEW";
				textureMapLayer3PanelData.runtimeObjectLine =
					"TARGET: SP { " + textureMapLayer3PanelData.targetName +
					" } | RUNTIME GATE { " + gateName + " }";
			}
			else {
				textureMapLayer3PanelData.runtimeObjectLine =
					"TARGET: SP { " + textureMapLayer3PanelData.targetName +
					" } | AUTHORING { " + modeName() + " } | STATUS { " +
					textureMapLayer3PanelData.status + " }";
			}
			if (!m_arbiter.isTextureMapRuntimeAuthoring()) {
				if (m_arbiter.isTextureMapRuntimeMeshSelected()) {
					textureMapLayer3PanelData.runtimeStateLine =
						"TEXTURE_MAP_2D: TARGET SELECTED";
					textureMapLayer3PanelData.runtimeHelpLine =
						"E: Deselect target    TAB: Authoring cycle panel    Q: Back";
				}
				else if (m_arbiter.isTextureMapRuntimeSelectionArmed()) {
					textureMapLayer3PanelData.runtimeStateLine =
						"TEXTURE_MAP_2D: SELECTION ARMED";
					textureMapLayer3PanelData.runtimeHelpLine =
						"LMB: Select target mesh    E: Cancel selection mode    Q: Back";
				}
				else {
					textureMapLayer3PanelData.runtimeStateLine =
						"TEXTURE_MAP_2D: REFERENCE PREVIEW";
					textureMapLayer3PanelData.runtimeHelpLine =
						"E: Arm target selection    MOUSE: Orbit / Zoom    Q: Back";
				}
			}
			else {
				textureMapLayer3PanelData.runtimeStateLine =
					"TEXTURE_MAP_2D: " + textureMapLayer3PanelData.subLayerLabel;
				textureMapLayer3PanelData.runtimeHelpLine =
					!textureMapLayer3PanelData.panelVisible
					? "TAB: Restore authoring panel    MOUSE: Orbit / Zoom    Q: Back"
					: workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor
					? "TAB: Edit / Preview    W/S: Select item    A/D: Change value    E: Activate    Q: Back"
					: "TAB: Hide panel    W/S: Select item    A/D: Change value    E: Activate    Q: Back";
			}

			const std::string targetObject =
				"SP { " + textureMapLayer3PanelData.targetName + " }";
			const vitru::MaterialSlot* material = asset &&
				workspace->target().materialIndex < asset->materials.size()
				? &asset->materials[workspace->target().materialIndex]
				: nullptr;
			const std::string materialName = material
				? material->name : "Default Material";
			const vitru::TextureMapChannel presentationChannel =
				workspace->authoringMode() == vitru::TextureMapAuthoringMode::PanelLines
				? vitru::TextureMapChannel::BaseColor
				: workspace->selectedChannel();
			const vitru::TextureResource* channelTexture = nullptr;
			if (asset && material) {
				const std::string& textureId =
					presentationChannel == vitru::TextureMapChannel::EmissiveColor
					? material->emissiveTextureId : material->baseColorTextureId;
				channelTexture = asset->findTexture(textureId);
			}
			std::string textureName = channelTexture
				? fs::path(channelTexture->relativePath).filename().string()
				: presentationChannel == vitru::TextureMapChannel::EmissiveColor
				? "blank_working_emissive" : "UNAVAILABLE";
			std::uint32_t textureWidth = channelTexture ? channelTexture->width : 0u;
			std::uint32_t textureHeight = channelTexture ? channelTexture->height : 0u;
			if (presentationChannel == vitru::TextureMapChannel::BaseColor &&
				workspace->hasSelectedBaseMaterialSource()) {
				const vitru::BaseMaterialCatalogEntry* source =
					workspace->selectedBaseMaterial();
				if (source) {
					textureName = source->baseColorPath.filename().string();
					textureWidth = source->width;
					textureHeight = source->height;
				}
			}
			const std::string resolution = textureWidth > 0u && textureHeight > 0u
				? std::to_string(textureWidth) + " x " + std::to_string(textureHeight)
				: "PENDING";

			if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::CycleSetup) {
				addSection("Target Object:", {
					informationLine(targetObject, true, false)
				});
				addSection("Authoring Pass:", {
					selectableLine(textureMapLayer3PanelData.rows[0], 0, false, false),
					selectableLine(textureMapLayer3PanelData.rows[1], 1, false, false)
				});
				addSection("Working Session:", {
					informationLine("STATUS { " + textureMapLayer3PanelData.status + " }", true, false),
					informationLine("PIXEL GRID { " + grid + " x " + grid + " }", true, false)
				});
				addSection("Next Sub-Layer:", {
					selectableLine(textureMapLayer3PanelData.rows[2], 2, false, false),
					informationLine("-> SUB-LAYER 1", true, false)
				});
			}
			else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::BranchSetup) {
				addSection("Target Object:", {
					informationLine(targetObject, true, false)
				});
				if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour) {
					addSection("Surface Target Authoring:", {
						selectableLine(textureMapLayer3PanelData.rows[0], 0, false, false),
						selectableLine(textureMapLayer3PanelData.rows[1], 1, false, false)
					});
				}
				else {
					std::vector<PanelLine> editLines;
					editLines.push_back(selectableLine(
						textureMapLayer3PanelData.rows[0], 0, false, false));
					editLines.push_back(informationLine(
						"MATERIAL SLOT { " + materialName + " }", true, false));
					editLines.push_back(selectableLine(
						textureMapLayer3PanelData.rows[1], 1, false, false));
					if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Coloring &&
						vitru::TextureMapWorkspace::channelActive(workspace->selectedChannel())) {
						if (workspace->selectedChannel() == vitru::TextureMapChannel::EmissiveColor) {
							std::ostringstream intensity;
							intensity << "INTENSITY { " << std::fixed << std::setprecision(2)
								<< workspace->emissiveIntensity() << " }";
							editLines.push_back(selectableLine(
								intensity.str(), 1, true, true));
						}
						else {
							editLines.push_back(selectableLine(
								"TEXTURE MAP { " + textureName + " }", 1, true, true));
						}
					}
					else if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Coloring) {
						editLines.push_back(informationLine(
							"SELECTED CHANNEL IS RESERVED / INACTIVE", true, false));
					}
					else {
						editLines.push_back(informationLine(
							"TEXTURE MAP { " + textureName + " }", true, false));
					}
					editLines.push_back(informationLine(
						"RESOLUTION { " + resolution + " } [ READY ]", true, true));
					editLines.push_back(informationLine(
						"UV MAP { TEXCOORD_0 }", true, false));
					editLines.push_back(informationLine(
						"SOURCE { BOX_ATLAS_6_DIRECTION } [ READY ]", true, true));
					addSection(workspace->authoringMode() == vitru::TextureMapAuthoringMode::PanelLines
						? "Panel Line Surface:" : "Edit Surface Texture:",
						std::move(editLines));
				}
				addSection("Next / Prev Sub-Layers:", {
					selectableLine(textureMapLayer3PanelData.rows[2], 2, false, false),
					informationLine("-> SUB-LAYER 2", true, false),
					selectableLine(textureMapLayer3PanelData.rows[3], 3, false, false),
					informationLine("-> SUB-LAYER 0", true, false)
				});
			}
			else if (workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor) {
				addSection("Target Object:", {
					informationLine(targetObject, true, false)
				});
				addSection("Editing Face:", {
					selectableLine(textureMapLayer3PanelData.rows[0], 0, false, false)
				});
				if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Coloring) {
					addSection("Edit Pixel Color:", {
						selectableLine(textureMapLayer3PanelData.rows[1], 1, false, false),
						selectableLine(textureMapLayer3PanelData.rows[2], 2, false, false),
						selectableLine(textureMapLayer3PanelData.rows[3], 3, false, false),
						selectableLine(textureMapLayer3PanelData.rows[4], 4, false, false)
					});
				}
				else if (workspace->authoringMode() == vitru::TextureMapAuthoringMode::Contour) {
					addSection("Contour Edit:", {
						selectableLine(textureMapLayer3PanelData.rows[1], 1, false, false),
						selectableLine(textureMapLayer3PanelData.rows[2], 2, false, false)
					});
					addSection("Working Contour:", {
						informationLine("STATUS { " + std::string(
							workspace->contourClosed() ? "CLOSED | READY" : "OPEN") + " }", true, false),
						informationLine("POINTS { " + std::to_string(
							workspace->contourPointCount()) + " }", true, false)
					});
				}
				else {
					addSection("Panel Line Tool:", {
						selectableLine(textureMapLayer3PanelData.rows[1], 1, false, false),
						selectableLine(textureMapLayer3PanelData.rows[2], 2, false, false)
					});
					addSection("Working Panel Lines:", {
						informationLine("STATUS { " + textureMapLayer3PanelData.status + " }", true, false)
					});
				}
				const int reviewRow = workspace->authoringMode() ==
					vitru::TextureMapAuthoringMode::Coloring ? 5 : 3;
				const int setupRow = reviewRow + 1;
				addSection("Next / Prev Sub-Layers:", {
					selectableLine(textureMapLayer3PanelData.rows[reviewRow], reviewRow, false, false),
					informationLine("-> SUB-LAYER 3", true, false),
					selectableLine(textureMapLayer3PanelData.rows[setupRow], setupRow, false, false),
					informationLine("-> SUB-LAYER 1", true, false)
				});
			}
			else {
				addSection("Working Output:", {
					informationLine("TARGET { " + textureMapLayer3PanelData.targetName + " }", true, false),
					informationLine("AUTHORING { " + modeName() + " }", true, false),
					informationLine("STATUS { " + textureMapLayer3PanelData.status + " }", true, false)
				});
				addSection("Apply Working Edit:", {
					selectableLine(textureMapLayer3PanelData.rows[0], 0, false, false)
				});
				addSection("Asset Output:", {
					selectableLine(textureMapLayer3PanelData.rows[1], 1, false, false),
					selectableLine(textureMapLayer3PanelData.rows[2], 2, false, false)
				});
				addSection("Previous Sub-Layer:", {
					selectableLine(textureMapLayer3PanelData.rows[3], 3, false, false)
				});
				addSection("Next Cycle:", {
					selectableLine(textureMapLayer3PanelData.rows[4], 4, false, false)
				});
			}

			textureMapLayer3PanelData.footerLine1 =
				workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::CommitSave
				? "W/S: Select    E: Activate    Q: Structural layer navigation"
				: "W/S: Select    A/D: Change value    E: Activate    TAB: Hide / Preview    Q: Back";
			textureMapLayer3PanelData.footerLine2 =
				workspace->runtimeStatusMessage().empty()
				? "Q: STRUCTURAL LAYER NAVIGATION | DIRTY EXIT IS BLOCKED"
				: workspace->runtimeStatusMessage();
		}
	}

	// 8. Draw screen-space overlay.
	m_viewport.drawOverlay(
		m_arbiter,
		mcPanelDataPtr,
		exportPanelDataPtr,
		m_tesseract.isActiveWorkspacePaused(),
		meshAvailable,
		textureMapPanelDataPtr,
		textureMapLayer2PanelDataPtr,
		textureMapLayer3PanelDataPtr);

	// 9. End frame.
	sdkStopTimer(&m_timer);
	computeFPS();
	glutSwapBuffers();
}

void EuclidEngine::onMouse(int button, int state, int x, int y) {
	if (isObjExportModalActive() || 
		isStaticParticleAssetModalActive() ||
		m_arbiter.isTextEntryActive()) return;

	const bool isWheel = (button == 3 || button == 4);

	const bool textureMapPreviewActive =
		m_arbiter.isTextureMapLayer2PanelContext() ||
		m_arbiter.isTextureMapLayer3RuntimeContext();

	const bool singleParticleConfigPreviewActive =
		m_arbiter.isParticleConfigLayer() &&
		m_arbiter.isSingleParticleSelected() &&
		m_tesseract.isPlacedSP();

	const bool singleParticleOpenGLCameraActive =
		singleParticleConfigPreviewActive ||
		textureMapPreviewActive ||
		(m_arbiter.isSimulationRunLayer() &&
			m_arbiter.isSingleParticleSelected() &&
			(m_arbiter.isSingleParticleReferenceSubLayer() ||
				m_arbiter.isShapeEditSubLayer()));

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

		if (m_arbiter.isTextureMapRuntimeAuthoring()) {
			vitru::TextureMapWorkspace* workspace =
				m_tesseract.getTextureMapWorkspaceRuntime();
			if (workspace &&
				workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor &&
				workspace->viewMode() == vitru::TextureMapViewMode::Edit) {
				workspace->adjustEditorZoom(button == 3 ? 1 : -1);
				glutPostRedisplay();
				return;
			}
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

	if (m_arbiter.isTextureMapRuntimeAuthoring()) {
		vitru::TextureMapWorkspace* workspace =
			m_tesseract.getTextureMapWorkspaceRuntime();
		if (workspace &&
			workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor &&
			workspace->viewMode() == vitru::TextureMapViewMode::Edit &&
			button == GLUT_LEFT_BUTTON) {

			const int width = m_viewport.getWidth();
			const int height = m_viewport.getHeight();
			const int canvas = static_cast<int>(std::min(
				static_cast<float>(height) * 0.72f,
				static_cast<float>(width) * 0.55f) * workspace->editorZoom());
			const int left = static_cast<int>(static_cast<float>(width) * 0.62f) - canvas / 2;
			const int top = (height - canvas) / 2;
			const int divisions = static_cast<int>(workspace->target().pixelGridDivisions);
			const int cellX = canvas > 0
				? (x - left) * divisions / canvas : -1;
			const int cellY = canvas > 0
				? (top + canvas - 1 - y) * divisions / canvas : -1;
			const bool inside = x >= left && x < left + canvas &&
				y >= top && y < top + canvas;

			if (state == GLUT_DOWN && inside)
				workspace->beginAuthoringStroke(cellX, cellY);
			else if (state == GLUT_UP)
				workspace->endAuthoringStroke();
			glutPostRedisplay();
			return;
		}
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
	if (isObjExportModalActive() ||
		isStaticParticleAssetModalActive() ||
		m_arbiter.isTextEntryActive()) return;

	if (m_arbiter.isTextureMapRuntimeAuthoring()) {
		vitru::TextureMapWorkspace* workspace =
			m_tesseract.getTextureMapWorkspaceRuntime();
		if (workspace &&
			workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor &&
			workspace->viewMode() == vitru::TextureMapViewMode::Edit) {
			const int width = m_viewport.getWidth();
			const int height = m_viewport.getHeight();
			const int canvas = static_cast<int>(std::min(
				static_cast<float>(height) * 0.72f,
				static_cast<float>(width) * 0.55f) * workspace->editorZoom());
			const int left = static_cast<int>(static_cast<float>(width) * 0.62f) - canvas / 2;
			const int top = (height - canvas) / 2;
			const int divisions = static_cast<int>(workspace->target().pixelGridDivisions);
			const int cellX = canvas > 0 ? (x - left) * divisions / canvas : -1;
			const int cellY = canvas > 0 ? (top + canvas - 1 - y) * divisions / canvas : -1;
			workspace->setCursorCell(cellX, cellY);
			workspace->continueAuthoringStroke(cellX, cellY);
			glutPostRedisplay();
			return;
		}
	}

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
	if (isObjExportModalActive() ||
		isStaticParticleAssetModalActive() ||
		m_arbiter.isTextEntryActive()) return;

	if (m_arbiter.isTextureMapRuntimeAuthoring()) {
		vitru::TextureMapWorkspace* workspace =
			m_tesseract.getTextureMapWorkspaceRuntime();
		if (workspace &&
			workspace->runtimeSubLayer() == vitru::TextureMapSubLayer::PixelEditor &&
			workspace->viewMode() == vitru::TextureMapViewMode::Edit) {
			const int width = m_viewport.getWidth();
			const int height = m_viewport.getHeight();
			const int canvas = static_cast<int>(std::min(
				static_cast<float>(height) * 0.72f,
				static_cast<float>(width) * 0.55f) * workspace->editorZoom());
			const int left = static_cast<int>(static_cast<float>(width) * 0.62f) - canvas / 2;
			const int top = (height - canvas) / 2;
			const int divisions = static_cast<int>(workspace->target().pixelGridDivisions);
			const int cellX = canvas > 0 ? (x - left) * divisions / canvas : -1;
			const int cellY = canvas > 0 ? (top + canvas - 1 - y) * divisions / canvas : -1;
			workspace->setCursorCell(cellX, cellY);
			glutPostRedisplay();
			return;
		}
	}

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
	if (handleStaticParticleAssetModalKeyboard(event)) return;
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

	TheArbiter::ArbiterResult result =
		m_arbiter.processKeyboard(event);

	if (result.textureMapCatalogStep != 0) {

		vitru::TextureMapWorkspace* textureWorkspace =
			m_tesseract.getTextureMapWorkspaceRuntime();

		if (!textureWorkspace) {

			std::printf(
				"[TEXTURE_MAP_2D] TARGET SP selection skipped: "
				"workspace runtime unavailable.\n"
			);
		}
		else {

			textureWorkspace->selectOutputAsset(
				result.textureMapCatalogStep
			);

			if (const vitru::StaticAssetCatalogEntry* selected =
				textureWorkspace->selectedOutputAsset()) {

				std::printf(
					"[TEXTURE_MAP_2D] TARGET SP: %s [%s]\n",
					selected->displayName.c_str(),
					selected->valid
					? "READY"
					: "INVALID"
				);
			}
			else {

				std::printf(
					"[TEXTURE_MAP_2D] TARGET SP: "
					"no OUTPUT assets available.\n"
				);
			}
		}
	}

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 1 Row [3].
	//
	// Load the currently selected OUTPUT Static Particle into
	// the shared canonical repository and renderer.
	// ---------------------------------------------------------
	if (result.loadTextureMapTargetRequested) {

		const bool loaded =
			loadSelectedTextureMapTarget();

		std::printf(
			"[TEXTURE_MAP_2D] Row [3] LOAD TARGET: %s\n",
			loaded
			? "SUCCESS"
			: "FAILED"
		);
	}

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 1 Row [4].
	//
	// Enter Layer 2 only after Row [3] has produced a
	// loaded and READY canonical target.
	// ---------------------------------------------------------
	if (result.configureTextureMapTargetRequested) {

		const bool entered =
			enterTextureMapLayer2Preview();

		printf(
			"[TEXTURE_MAP_2D] Row [4] CONFIGURE: %s\n",
			entered
			? "SUCCESS"
			: "LOCKED"
		);
	}

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 2 Row [1].
	// Apply the previous / next discrete preview-radius preset.
	// ---------------------------------------------------------
	if (result.textureMapPreviewRadiusStep != 0) {

		vitru::TextureMapWorkspace* textureWorkspace =
			m_tesseract.getTextureMapWorkspaceRuntime();

		if (textureWorkspace) {

			textureWorkspace->adjustPreviewParticleRadius(
				result.textureMapPreviewRadiusStep
			);
		}
	}

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 2 Row [2].
	// Apply the previous / next logical pixel-grid preset.
	// ---------------------------------------------------------
	if (result.textureMapPixelGridStep != 0) {

		vitru::TextureMapWorkspace* textureWorkspace =
			m_tesseract.getTextureMapWorkspaceRuntime();

		if (textureWorkspace) {

			textureWorkspace->adjustPixelGridDivisions(
				result.textureMapPixelGridStep
			);
		}
	}

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 2 Row [3].
	// Validate and execute the Layer 2 -> Layer 3 transition.
	// ---------------------------------------------------------
	if (result.runTextureMapWorkspaceRequested) {

		const bool entered =
			enterTextureMapLayer3Runtime();

		printf(
			"[TEXTURE_MAP_2D] Row [3] RUN WORKSPACE EDIT: %s\n",
			entered
			? "SUCCESS"
			: "LOCKED"
		);
	}

	if (previousWorkspace == TheArbiter::WorkspaceId::TEXTURE_MAP_2D &&
		(previousLayer == TheArbiter::ApplicationLayer::ACTIVE_WORKSPACE ||
			m_arbiter.isTextureMapLayer3RuntimeContext() ||
			result.textureMapSurfaceTargetNameEntered ||
			result.textureMapSaveAsNameEntered ||
			result.textureMapTextEntryCancelled)) {
		handleTextureMapRuntimeResult(result);
	}

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

		applyPSGridLayoutToWorkspace(true);

		if (!applyParticleSelectionsToSystem()) {
			printf(
				"[PARTICLE_SIM] Runtime configuration failed; "
				"simulation was not started.\n"
			);

			m_tesseract.exitWorkspace();

			KeyboardInput::KeyEvent returnToConfig;
			returnToConfig.signal = KeyboardInput::KEY_Q;
			returnToConfig.rawKey = 'q';
			m_arbiter.processKeyboard(returnToConfig);

			syncTesseractWorkspaceFromArbiter();
			break;
		}

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
	handleStaticParticleRequests(result);

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
	advanceStaticParticleAssetJob();
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
			"'%s' vertices=%zu triangles=%zu normals=YES indexed=YES",
			m_objExportPath.c_str(),
			m_marchingCubes->getCanonicalMesh().positions.size(),
			m_marchingCubes->getCanonicalMesh().triangleCount()
		);

		appendObjExportLog(line);
		{
			const vitru::MeshProcessingReport& report =
				m_marchingCubes->getMeshProcessingReport();
			snprintf(
				line, sizeof(line),
				"[MeshProcessor] rawV=%zu rawT=%zu finalV=%zu finalT=%zu removed=%zu welded=%zu radius=%.6f",
				report.rawVertexCount, report.rawTriangleCount,
				report.finalVertexCount, report.finalTriangleCount,
				report.removedNonFiniteTriangles +
				report.removedDegenerateTriangles +
				report.removedDuplicateTriangles,
				report.weldedVertexInstances,
				report.bounds.radius);
			appendObjExportLog(line);
		}

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
		if (m_marchingCubes->getCanonicalMesh().bounds.valid) {
			const vitru::MeshBounds& exported =
				m_marchingCubes->getCanonicalMesh().bounds;
			const glm::vec3 loadedMin = m_renderer->getParticleMeshMin();
			const glm::vec3 loadedMax = m_renderer->getParticleMeshMax();
			const glm::vec3 loadedCenter = m_renderer->getParticleMeshCenter();
			const float loadedExtent = m_renderer->getParticleMeshMaxExtent();
			float delta = fabsf(loadedExtent - exported.maxAxisExtent);
			delta = (std::max)(delta, fabsf(loadedMin.x - exported.min.x));
			delta = (std::max)(delta, fabsf(loadedMin.y - exported.min.y));
			delta = (std::max)(delta, fabsf(loadedMin.z - exported.min.z));
			delta = (std::max)(delta, fabsf(loadedMax.x - exported.max.x));
			delta = (std::max)(delta, fabsf(loadedMax.y - exported.max.y));
			delta = (std::max)(delta, fabsf(loadedMax.z - exported.max.z));
			delta = (std::max)(delta, fabsf(loadedCenter.x - exported.center.x));
			delta = (std::max)(delta, fabsf(loadedCenter.y - exported.center.y));
			delta = (std::max)(delta, fabsf(loadedCenter.z - exported.center.z));
			const size_t loadedTriangles = static_cast<size_t>(
				m_renderer->getParticleMeshVertexCount()) / 3u;
			const bool roundtripPass =
				loadedTriangles == m_marchingCubes->getCanonicalMesh().triangleCount() &&
				delta <= 1.0e-5f;
			snprintf(
				line, sizeof(line),
				"[OBJ Roundtrip] triangles=%zu/%zu maxBoundsDelta=%.8f extent=%.6f/%.6f %s",
				loadedTriangles,
				m_marchingCubes->getCanonicalMesh().triangleCount(),
				delta, loadedExtent, exported.maxAxisExtent,
				roundtripPass ? "PASS" : "MISMATCH");
			appendObjExportLog(line);
		}

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
