#include "kernel.h"
#include "TheTesseract.h"
#include "marchingCubes.h"

#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>


namespace {
	float smoothStep01(float t) {
		if (t < 0.0f) return 0.0f;
		if (t > 1.0f) return 1.0f;

		return t * t * (3.0f - 2.0f * t);
	}

	float lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}
}

// =============================================================================
// GLOBAL SHELL ANIMATION
// =============================================================================
Tesseract::~Tesseract() {
	releaseSPVolumeBoundarySensor();
}
//
void Tesseract::updateMealyAnimBehavior(float timeS) {
	if (m_transitionDuration <= 0.0f) {
		m_animTransition = ANIM_TRANS_NONE;
		m_animPhase = ANIM_PHASE_NONE;
		return;
	}

	const float rawT =
		(timeS - m_transitionStartTime) / m_transitionDuration;

	const float t = smoothStep01(rawT);

	switch (m_animPhase) {
	case ANIM_PHASE_ORIENT_TO_WORKSPACE:
		m_previewRotation =
			lerp(m_startPreviewRotation, 0.0f, t);

		m_sliceAnimation =
			lerp(m_startSliceAnimation, 0.5f, t);

		if (rawT >= 1.0f) {
			m_previewRotation = 0.0f;
			m_sliceAnimation = 0.5f;

			m_animPhase = ANIM_PHASE_CAMERA_FOCUS_TO_WORKSPACE;
			m_transitionStartTime = timeS;
			m_transitionDuration = 1.25f;

			m_cameraFocusRequested = true;
		}
		break;

	case ANIM_PHASE_CAMERA_FOCUS_TO_WORKSPACE:
		m_previewRotation = 0.0f;
		m_sliceAnimation = 0.5f;

		if (rawT >= 1.0f) {
			m_animMode = ANIM_MODE_WORKSPACE_STABLE;
			m_animPhase = ANIM_PHASE_NONE;
			m_animTransition = ANIM_TRANS_NONE;
		}
		break;

	case ANIM_PHASE_RETURN_TO_IDLE: {
		const float idleRotation =
			timeS * 25.0f;

		const float idleSlice =
			std::fmod(timeS * 0.35f, 3.0f);

		m_previewRotation =
			lerp(m_startPreviewRotation, idleRotation, t);

		m_sliceAnimation =
			lerp(m_startSliceAnimation, idleSlice, t);

		if (rawT >= 1.0f) {
			m_previewRotation = idleRotation;
			m_sliceAnimation = idleSlice;

			m_animMode = ANIM_MODE_IDLE_PREVIEW;
			m_animPhase = ANIM_PHASE_NONE;
			m_animTransition = ANIM_TRANS_NONE;
		}
		break;
	}

	default:
		m_animTransition = ANIM_TRANS_NONE;
		m_animPhase = ANIM_PHASE_NONE;
		break;
	}
}
//
void Tesseract::updateMooreAnimBehavior(float timeS) {
	switch (m_animMode) {
	case ANIM_MODE_IDLE_PREVIEW:
		m_previewRotation = timeS * 25.0f;
		m_sliceAnimation = std::fmod(timeS * 0.35f, 3.0f);
		break;

	case ANIM_MODE_WORKSPACE_STABLE:
		m_previewRotation = 0.0f;
		m_sliceAnimation = 0.5f;
		break;

	default:
		break;
	}
}
//
void Tesseract::updateAnimBehavior(float timeS) {
	if (m_animTransition != ANIM_TRANS_NONE) {
		updateMealyAnimBehavior(timeS);
		return;
	}

	updateMooreAnimBehavior(timeS);
}
//
void Tesseract::beginIdleToWorkspaceTransition(float timeS) {
	m_animTransition = ANIM_TRANS_IDLE_TO_WORKSPACE;
	m_animPhase = ANIM_PHASE_ORIENT_TO_WORKSPACE;

	m_transitionStartTime = timeS;
	m_transitionDuration = 1.25f;

	m_startPreviewRotation = std::fmod(m_previewRotation, 360.0f);
	if (m_startPreviewRotation < 0.0f) {
		m_startPreviewRotation += 360.0f;
	}

	m_startSliceAnimation = m_sliceAnimation;
	m_cameraFocusRequested = false;
}
//
void Tesseract::beginWorkspaceToIdleTransition(float timeS) {
	m_animTransition = ANIM_TRANS_WORKSPACE_TO_IDLE;
	m_animPhase = ANIM_PHASE_RETURN_TO_IDLE;

	m_transitionStartTime = timeS;
	m_transitionDuration = 1.25f;

	m_startPreviewRotation = std::fmod(m_previewRotation, 360.0f);
	if (m_startPreviewRotation < 0.0f) {
		m_startPreviewRotation += 360.0f;
	}

	m_startSliceAnimation = m_sliceAnimation;
	m_cameraFocusRequested = false;
}
//
void Tesseract::beginAnimTransition(TesseractAnimTransition transition, float timeS) {

	switch (transition) {
	case ANIM_TRANS_IDLE_TO_WORKSPACE:
		beginIdleToWorkspaceTransition(timeS);
		break;

	case ANIM_TRANS_WORKSPACE_TO_IDLE:
		beginWorkspaceToIdleTransition(timeS);
		break;

	default:
		m_animTransition = ANIM_TRANS_NONE;
		m_animPhase = ANIM_PHASE_NONE;
		break;
	}
}
//
bool Tesseract::consumeCameraFocusRequest() {
	if (!m_cameraFocusRequested) {
		return false;
	}

	m_cameraFocusRequested = false;

	return true;
}

bool Tesseract::applyWorkspaceBoundaryGridVisual() {
	if (!m_renderer ||
		!m_particleSimSystem) 
		return false;

	const uint3 gridSize =
		m_particleSimSystem->getGridSize();

	const float3 worldOrigin =
		m_particleSimSystem->getWorldOrigin();

	const float3 cellSize =
		m_particleSimSystem->getCellSize();

	m_renderer->setGrid(
		glm::ivec3(
			static_cast<int>(gridSize.x), 
			static_cast<int>(gridSize.y), 
			static_cast<int>(gridSize.z)
		),
		glm::vec3(
			worldOrigin.x, 
			worldOrigin.y, 
			worldOrigin.z
		),
		glm::vec3(
			cellSize.x, 
			cellSize.y, 
			cellSize.z
		)
	);

	m_renderer->setGridStyle(
		m_workspaceGridVisual.majorStride,
		m_workspaceGridVisual.drawMinor
	);

	return true;
}

int Tesseract::getWorkspaceGridHalfSliceRange() const {
	if (!m_particleSimSystem)
		return 0;

	const uint3 gridSize =
		m_particleSimSystem->getGridSize();

	return static_cast<int>(gridSize.x) / 2;
}

// =============================================================================
// ACTIVE WORKSPACE LIFECYCLE / INPUT ROUTING
// =============================================================================
void Tesseract::enterWorkspace(WorkspaceId workspace) {
	if (m_activeWorkspace == workspace) return;

	m_activeWorkspace = workspace;
	switch (m_activeWorkspace) {

	case WorkspaceId::PARTICLE_SIMULATION:
		initializePSWorkspace();
		break;

	case WorkspaceId::SINGLE_PARTICLE_MCAD:
		initializeSPWorkspace();
		break;

	case WorkspaceId::LINKED_PARTICLES_MCAD:
		initializeLPWorkspace();
		break;

	default:
	case WorkspaceId::NONE:
		break;
	}
}

void Tesseract::updateActiveWorkspace(const WorkspaceUpdateContext& ctx) {
	switch (m_activeWorkspace) {

	case WorkspaceId::PARTICLE_SIMULATION:
		updatePSWorkspace(ctx);
		break;

	case WorkspaceId::SINGLE_PARTICLE_MCAD:
		updateSPWorkspace(ctx);
		break;

	case WorkspaceId::LINKED_PARTICLES_MCAD:
		updateLPWorkspace(ctx);
		break;

	default:
	case WorkspaceId::NONE:
		break;
	}
}

void Tesseract::renderActiveWorkspace(const WorkspaceRenderContext& ctx) {
	switch (m_activeWorkspace) {

	case WorkspaceId::PARTICLE_SIMULATION:
		renderPSWorkspace(ctx);
		break;

	case WorkspaceId::SINGLE_PARTICLE_MCAD:
		renderSPWorkspace(ctx);
		break;

	case WorkspaceId::LINKED_PARTICLES_MCAD:
		renderLPWorkspace(ctx);
		break;


	default:
	case WorkspaceId::NONE:
		break;
	}
}

void Tesseract::exitWorkspace() {
	if (m_renderer) {
		m_renderer->setParticleHighlighted(false);
	}
	m_activeWorkspace = WorkspaceId::NONE;
}

bool Tesseract::handleWorkspaceMouse(
	TheArbiter& arbiter,
	int button,
	int state,
	int x,
	int y,
	int viewportW,
	int viewportH) {

	if (m_activeWorkspace != WorkspaceId::SINGLE_PARTICLE_MCAD)
		return false;

	if (!arbiter.isWorkplaneParticleSelectSubLayer())
		return false;

	if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN)
		return false;

	arbiter.updateHoverFromScreen(x, y, viewportW, viewportH);
	TheArbiter::ArbiterResult result =
		arbiter.trySelectParticleAtCurrentSlice();

	return result.requestRedraw ||
		result.rebuildMenu ||
		result.command != TheArbiter::CMD_NONE;
}

bool Tesseract::handleWorkspaceMotion(
	TheArbiter& arbiter,
	int x,
	int y,
	int viewportW,
	int viewportH) {

	if (m_activeWorkspace != WorkspaceId::SINGLE_PARTICLE_MCAD)
		return false;

	if (!arbiter.isWorkplaneParticleSelectSubLayer())
		return false;


	arbiter.updateHoverFromScreen(
		x,
		y,
		viewportW,
		viewportH
	);

	return true;
}
//
bool Tesseract::handleWorkspacePassiveMotion(
	TheArbiter& arbiter,
	int x,
	int y,
	int viewportW,
	int viewportH) {

	if (m_activeWorkspace != WorkspaceId::SINGLE_PARTICLE_MCAD)
		return false;

	if (!arbiter.isWorkplaneParticleSelectSubLayer())
		return false;

	arbiter.updateHoverFromScreen(
		x,
		y,
		viewportW,
		viewportH
	);

	return true;
}

bool Tesseract::isActiveWorkspacePaused() const {
	if (m_activeWorkspace ==
		WorkspaceId::PARTICLE_SIMULATION)
		return m_PSWorkspace.runtime.paused;

	return true;
}

// =============================================================================
// PARTICLE WORKSPACE RESOURCE BINDING
// =============================================================================
bool Tesseract::bindRendererToParticleSystem(
	ParticleSystem* particleSystem,
	std::vector<float>* radiusBuffer,
	int drawCount) {

	if (!particleSystem ||
		!radiusBuffer ||
		!m_renderer ||
		drawCount <= 0) return false;

	const int systemCapacity =
		particleSystem->getNumParticles();

	if (systemCapacity <= 0) return false;
	drawCount = std::min(drawCount, systemCapacity);

	if (radiusBuffer->size() <
		static_cast<std::size_t>(systemCapacity)) {

		radiusBuffer->resize(
			static_cast<std::size_t>(systemCapacity),
			0.0f
		);
	}

	particleSystem->dumpRadii(radiusBuffer->data());

	// Fully direct the shared renderer
	m_renderer->setParticleSystem(particleSystem);
	m_renderer->setParticleRadius(particleSystem->getParticleRadius());
	m_renderer->setColorBuffer(particleSystem->getColorBuffer());
	m_renderer->setRadiusBuffer(particleSystem->getRadiiBuffer());
	m_renderer->setVertexBuffer(particleSystem->getCurrentReadBuffer(), drawCount);
	m_renderer->setRadius(radiusBuffer->data(), drawCount);

	return true;
}

void Tesseract::bindParticleSimulationResources(
	ParticleSystem* particleSystem,
	EuclidRenderer* renderer,
	std::vector<float>* radiusBuffer) {

	m_particleSimSystem = particleSystem;
	m_particleSimRadii = radiusBuffer;
	m_renderer = renderer;

	m_PSWorkspace.resourcesBound =
		m_particleSimSystem &&
		m_particleSimRadii &&
		m_renderer;

	// Force initialization to run again if resources are rebound.
	m_PSWorkspace.initialized = false;
}

void Tesseract::bindSingleParticleResources(
	ParticleSystem* particleSystem,
	EuclidRenderer* renderer,
	std::vector<float>* radiusBuffer) {

	m_singleParticleSystem = particleSystem;
	m_singleParticleRadii = radiusBuffer;
	m_renderer = renderer;

	m_SPWorkspace.resourcesBound =
		m_singleParticleSystem &&
		m_singleParticleRadii &&
		m_renderer;

	// Force initialization to run again if resources are rebound.
	m_SPWorkspace.initialized = false;
}

// =============================================================================
// PARTICLE_SIM WORKSPACE (SIMCAD_4D)
// =============================================================================
bool Tesseract::initializePSWorkspace() {
	m_PSWorkspace.resourcesBound =
		m_particleSimSystem && m_renderer && m_particleSimRadii;

	if (!m_PSWorkspace.resourcesBound)
		return false;

	m_PSWorkspace.initialized = true;
	syncPSRendering();
	return true;
}
//
void Tesseract::renderPSWorkspace(const WorkspaceRenderContext& ctx) {
	if (!m_PSWorkspace.initialized && !initializePSWorkspace())
		return;

	renderParticleSimulation(
		ctx.displayMode,
		ctx.displayEnabled
	);
}
//
void Tesseract::updatePSWorkspace(const WorkspaceUpdateContext& ctx) {
	if (!m_PSWorkspace.initialized &&
		!initializePSWorkspace())
		return;

	(void)ctx;

	if (m_PSWorkspace.runtime.paused) return;

	advanceParticleSimSTEP();
}
//
void Tesseract::applyPSConfig() {
	if (!m_particleSimSystem) return;

	const PSSimulationConfig& config =
		m_PSWorkspace.config;

	m_particleSimSystem->setIterations(config.solverIterations);
	m_particleSimSystem->setDamping(config.globalDamping);
	m_particleSimSystem->setGravity(-config.gravityMagnitude);
	m_particleSimSystem->setCollideSpring(config.collisionSpring);
	m_particleSimSystem->setCollideDamping(config.collisionDamping);
	m_particleSimSystem->setCollideShear(config.collisionShear);
	m_particleSimSystem->setCollideAttraction(config.collisionAttraction);
	m_particleSimSystem->setSimulationDomain(config.simulationBoxSize);

	// Keep the universal visual boundary aligned with
	// the currently configured simulation boundary.
	m_workspaceGridVisual.majorStride = config.simulationBoxSize;
}
//
void Tesseract::syncPSRendering() {
	if (!m_particleSimSystem ||
		!m_particleSimRadii ||
		!m_renderer) return;

	const int numParticles =
		m_particleSimSystem->getNumParticles();

	if (!bindRendererToParticleSystem(
		m_particleSimSystem,
		m_particleSimRadii,
		numParticles)) return;

	// PARTICLE_SIMULATION owns the original CUDA sample render path.
	m_renderer->setGridMode3D();
	m_renderer->setParticleHighlighted(false);

	applyWorkspaceBoundaryGridVisual();
}
//
void Tesseract::renderParticleSimulation(
	EuclidRenderer::DisplayMode displayMode,
	bool displayEnabled) {

	if (!displayEnabled ||
		!m_renderer ||
		!m_particleSimSystem) return;

	syncPSRendering();

	m_renderer->displayGrid();
	m_renderer->display(displayMode);
}
//
bool Tesseract::advanceParticleSimSTEP() {
	if (!m_PSWorkspace.initialized &&
		!initializePSWorkspace())
		return false;

	if (!m_particleSimSystem) return false;

	applyPSConfig();
	const float timestep =
		m_PSWorkspace.config.fixedTimestep;

	m_particleSimSystem->update(timestep);
	m_PSWorkspace.runtime.elapsedSimulationTime += timestep;

	syncPSRendering();
	return true;
}
//
bool Tesseract::startPSWorkspace() {
	if (m_activeWorkspace !=
		WorkspaceId::PARTICLE_SIMULATION)
		return false;

	if (!m_PSWorkspace.initialized &&
		!initializePSWorkspace())
		return false;

	m_PSWorkspace.runtime.paused = false;
	return true;
}
//
bool Tesseract::togglePSPause() {
	if (m_activeWorkspace !=
		WorkspaceId::PARTICLE_SIMULATION)
		return false;

	m_PSWorkspace.runtime.paused =
		!m_PSWorkspace.runtime.paused;

	return true;
}
//
bool Tesseract::stepPSWorkspace() {
	if (m_activeWorkspace !=
		WorkspaceId::PARTICLE_SIMULATION)
		return false;

	// Does not change the existing paused state.
	return advanceParticleSimSTEP();
}
//
bool Tesseract::resetPSWorkspace(ParticleSystem::ParticleConfig config) {

	if (!m_PSWorkspace.initialized &&
		!initializePSWorkspace()) return false;

	m_particleSimSystem->reset(config);
	m_PSWorkspace.runtime.paused = true;
	m_PSWorkspace.runtime.elapsedSimulationTime = 0.0f;

	syncPSRendering();
	return true;
}

// =============================================================================
// SINGLE_PARTICLE_MCAD WORKSPACE (GRID_3D)
// =============================================================================
bool Tesseract::placeSPAnchor(float particleRadius) {
	if (!m_singleParticleSystem) return false;

	m_singleParticleSystem->setParticleRadius(particleRadius);
	if (m_renderer) {
		m_renderer->setParticleRadius(
			m_singleParticleSystem->getParticleRadius()
		);
	}

	float pos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float vel[4] = { 0.0f, 0.0f, 0.0f, m_singleParticleSystem->getParticleRadius() };
	float acc[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	m_singleParticleSystem->setParticle(ParticleSystem::POSITION, 0, pos);
	m_singleParticleSystem->setParticle(ParticleSystem::VELOCITY, 0, vel);
	m_singleParticleSystem->setParticle(ParticleSystem::ACCELERATION, 0, acc);

	m_placedSP = true;
	syncSPRendering();

	return true;
}
//
bool Tesseract::initializeSPWorkspace() {

	m_SPWorkspace.resourcesBound =
		m_singleParticleSystem &&
		m_renderer &&
		m_singleParticleRadii;

	if (!m_SPWorkspace.resourcesBound)
		return false;

	m_SPWorkspace.initialized = true;
	if (m_placedSP) syncSPRendering();

	return true;
}
//
void Tesseract::updateSPWorkspace(const WorkspaceUpdateContext& ctx) {
	if (!m_SPWorkspace.initialized &&
		!initializeSPWorkspace()) return;

	(void)ctx;

	if (!m_placedSP) return;

	updateSingleParticleMCAD();
	// For now, no physics update.
	// SINGLE_PARTICLE_MCAD is a CAD anchor, not a running simulation.
}
//
void Tesseract::renderSPWorkspace(const WorkspaceRenderContext& ctx) {
	if (!m_SPWorkspace.initialized && !initializeSPWorkspace())
		return;

	renderSingleParticleWorkspace(ctx);
}
//
void Tesseract::syncSPRendering() {
	if (!m_singleParticleSystem ||
		!m_singleParticleRadii ||
		!m_renderer) return;

	bindRendererToParticleSystem(
		m_singleParticleSystem,
		m_singleParticleRadii,
		1
	);
}
//
void Tesseract::applySPConfig(float particleRadius) {

	if (!m_singleParticleSystem) return;
	m_singleParticleSystem->setParticleRadius(particleRadius);

	if (m_renderer) {
		m_renderer->setParticleRadius(
			m_singleParticleSystem->getParticleRadius()
		);
	}

	if (m_placedSP) {
		float vel[4] = {
			0.0f, 0.0f, 0.0f,
			m_singleParticleSystem->getParticleRadius()
		};

		m_singleParticleSystem->setParticle(
			ParticleSystem::VELOCITY,
			0,
			vel
		);

		syncSPRendering();
	}
}
//
void Tesseract::renderSingleParticleWorkspace(const WorkspaceRenderContext& ctx) {

	if (!ctx.arbiter) return;
	const TheArbiter& arbiter = *ctx.arbiter;

	if (!m_placedSP) return;
	if (m_renderer) {
		m_renderer->setParticleHighlighted(
			arbiter.isWorkplaneParticleSelectSubLayer() &&
			arbiter.hasSelectedParticle()
		);
	}

	const bool configPreviewActive =
		arbiter.isParticleConfigLayer() &&
		arbiter.isSingleParticleSelected();

	const bool referenceOrEditActive =
		arbiter.isSimulationRunLayer() &&
		arbiter.isSingleParticleSelected() &&
		(arbiter.isSingleParticleReferenceSubLayer() ||
			arbiter.isWorkplaneParticleSelectSubLayer());

	if (configPreviewActive || referenceOrEditActive) {

		renderSingleParticleMCAD(
			arbiter,
			ctx.thetaRad,
			ctx.phiRad,
			ctx.particleWorkspaceZs
		);

		return;
	}

	if (arbiter.isVolumeRenderSubLayer()) {
		renderSPVolumeToPBO(
			arbiter,
			2,
			ctx.viewportW,
			ctx.viewportH,
			ctx.thetaRad,
			ctx.phiRad,
			ctx.volumeRenderZs,
			ctx.threshold,
			ctx.sliceDistance
		);

		renderSPVolumeTexture();

		// ---------------------------------------------------------
		// Node_0 injection-voxel selection preview.
		//
		// The bridge method performs the exact panel/list visibility
		// checks, so calling it here is safe for every assembly node.
		// ---------------------------------------------------------
		renderSPVolumeInjectionVoxelPreview(
			arbiter,
			ctx.thetaRad,
			ctx.phiRad,
			ctx.volumeRenderZs
		);
		// ---------------------------------------------------------
		// Node_1 injection edit-target preview.
		//
		// List [1] now has spatial feedback:
		//     VOXEL_0 -> show VOXEL_1 helper cage
		//     VOXEL_1 -> show VOXEL_0 helper cage
		// ---------------------------------------------------------
		renderSPVolumeInjectionEditTargetPreview(
			arbiter,
			ctx.thetaRad,
			ctx.phiRad,
			ctx.volumeRenderZs
		);

		// ---------------------------------------------------------
		// Node_2-only offset drafting grid and boundary sensor.
		// ---------------------------------------------------------
		if (arbiter.getVolumeAssemblyNode() == TheArbiter::VOLUME_NODE_OFFSET_OBJECT) {

			renderSPVolumeOffsetGrid(
				arbiter,
				ctx.thetaRad,
				ctx.phiRad,
				ctx.volumeRenderZs
			);
		}

		// Draw the world/object axes last so they remain crisp over the grid.
		renderSPVolumeOrientationAxes(
			arbiter,
			ctx.thetaRad,
			ctx.phiRad
		);
		return;
	}

	if (arbiter.isMarchingCubesSubLayer()) {
		renderSPVolumeToPBO(
			arbiter,
			3,
			ctx.viewportW,
			ctx.viewportH,
			ctx.thetaRad,
			ctx.phiRad,
			ctx.volumeRenderZs,
			ctx.threshold,
			ctx.sliceDistance
		);

		renderSPVolumeTexture();

		if (m_renderer) {
			const int volumeDim = m_volumeSize.x;
			const float mcHalfExtent =
				0.5f * static_cast<float>(volumeDim);

			m_renderer->displayMarchingCubesVoxelGrid(
				ctx.thetaRad,
				ctx.phiRad,
				ctx.volumeRenderZs,
				volumeDim,
				16,
				mcHalfExtent
			);
		}

		if (ctx.marchingCubes &&
			ctx.marchingCubes->hasTriangleData()) {

			ctx.marchingCubes->renderWireframe(
				ctx.thetaRad,
				ctx.phiRad,
				ctx.volumeRenderZs
			);
		}

		renderSPVolumeOrientationAxes(
			arbiter,
			ctx.thetaRad,
			ctx.phiRad
		);

		return;
	}
}
//
void Tesseract::renderSingleParticleMCAD(
	const TheArbiter& arbiter,
	float thetaRad,
	float phiRad,
	float zs) {

	if (!m_renderer ||
		!m_singleParticleSystem ||
		!m_placedSP) return;

	const bool showWorkplane =
		arbiter.isWorkplaneParticleSelectSubLayer();

	const bool selected =
		arbiter.hasSelectedParticle();

	const bool useMeshRender =
		arbiter.isParticleRenderMesh();

	m_renderer->setParticleHighlighted(showWorkplane && selected);

	m_renderer->displayParticleWorkspace(
		thetaRad,
		phiRad,
		zs,
		arbiter.getWorkplaneSlice(),
		showWorkplane,
		selected,
		arbiter.hasHover(),
		arbiter.getHoverX(),
		arbiter.getHoverY(),
		useMeshRender
	);
}
//
bool Tesseract::updateSingleParticleMCAD() {
	if (!m_SPWorkspace.initialized &&
		!initializeSPWorkspace())
		return false;

	if (!m_singleParticleSystem) return false;

	syncSPRendering();
	return true;
}
//
size_t Tesseract::getVolumeBytes() const {
	return static_cast<size_t>(m_volumeSize.x) *
		static_cast<size_t>(m_volumeSize.y) *
		static_cast<size_t>(m_volumeSize.z) *
		sizeof(float);
}
//
bool Tesseract::commitSPWorkingVolume(const TheArbiter& arbiter) {
	if (!m_dBaseVolume || !m_dWorkingVolume) return false;

	// ---------------------------------------------------------
	// Injection boolean commit path.
	//
	// This is Checkpoint 4E:
	//
	//     FUSE -> min(anchor, brush)
	//     CUT  -> max(anchor, -brush)
	//
	// It uses the exact same rail-positioned VOLUME_1 brush
	// generated for the 4C visual preview and 4D boundary test.
	// ---------------------------------------------------------
	if (arbiter.hasInjectionVoxelSelected()) {
		return commitSPInjectionBoolean(arbiter);
	}

	// ---------------------------------------------------------
	// Legacy / non-injection commit path.
	//
	// This still handles the initial BASE creation:
	//     selected primitive -> m_dWorkingVolume -> m_dBaseVolume
	// ---------------------------------------------------------
	const bool baseSelected =
		arbiter.getVolumePrimitiveSelection() ==
		TheArbiter::VOLUME_PRIMITIVE_BASE;

	// There is no BASE object to transform yet.
	if (baseSelected &&
		!m_hasCommittedGeometry) {

		return false;
	}

	updateSPVolumePreview(arbiter);

	// Final authoritative safety check.
	if (!updateSPVolumeBoundarySensor(0.0f, 0.0f)) {

		printf(
			"[Tesseract] Commit blocked: "
			"boundary sensor unavailable.\n"
		);

		return false;
	}

	if (!isSPVolumeBoundarySafe()) {

		printf(
			"[Tesseract] Commit blocked: "
			"%u unsafe boundary patches detected.\n",
			m_volumeBoundaryUnsafeCount
		);

		return false;
	}

	cudaMemcpy(
		m_dBaseVolume,
		m_dWorkingVolume,
		getVolumeBytes(),
		cudaMemcpyDeviceToDevice
	);

	threadSync();

	m_committedVolumeReady = true;
	m_hasCommittedGeometry = true;
	m_volumeDirty = false;

	return true;
}
//
bool Tesseract::commitSPInjectionBoolean(const TheArbiter& arbiter) {
	if (!m_dBaseVolume ||
		!m_dWorkingVolume ||
		!m_dBrushVolume) return false;

	if (!arbiter.hasInjectionVoxelSelected()) return false;
	if (!m_hasCommittedGeometry) {

		printf(
			"[Tesseract] Injection commit blocked: "
			"no anchor/base geometry exists.\n"
		);

		return false;
	}

	generateSPVolume1BrushField(arbiter, m_dBrushVolume);
	threadSync();

	// ---------------------------------------------------------
	// Final authoritative safety check.
	//
	// FUSE can expand the anchor, so it must not clip the
	// anchor cage.
	//
	// CUT cannot expand the anchor, so it is allowed.
	// ---------------------------------------------------------
	if (arbiter.getVolumeInjectionMode() ==
		TheArbiter::VOLUME_CUT) {

		markSPVolumeBoundarySafe();
	}
	else {
		if (!classifySPVolumeBoundaryForSource(
			m_dBrushVolume, 0.0f, 0.0f)) {

			printf(
				"[Tesseract] Fuse commit blocked: "
				"boundary sensor unavailable.\n"
			);

			return false;
		}

		if (!isSPVolumeBoundarySafe()) {

			printf(
				"[Tesseract] Fuse commit blocked: "
				"%u unsafe boundary patches detected.\n",
				m_volumeBoundaryUnsafeCount
			);

			return false;
		}
	}

	const int op =
		arbiter.getVolumeInjectionMode() ==
		TheArbiter::VOLUME_CUT
		? 1
		: 0;

	// ---------------------------------------------------------
	// Final Boolean result:
	//
	//     op == 0 -> FUSE -> min(anchor, brush)
	//     op == 1 -> CUT  -> max(anchor, -brush)
	//
	// Output goes to m_dWorkingVolume first.
	// Then it is copied back into m_dBaseVolume.
	// ---------------------------------------------------------
	composeVolumeFieldsLauncher(
		m_dBaseVolume,
		m_dBrushVolume,
		m_dWorkingVolume,
		m_volumeSize,
		op
	);

	threadSync();
	cudaMemcpy(
		m_dBaseVolume,
		m_dWorkingVolume,
		getVolumeBytes(),
		cudaMemcpyDeviceToDevice
	);

	threadSync();

	clearVolumeKernelLauncher(
		m_dBrushVolume,
		m_volumeSize,
		1.0e6f
	);

	threadSync();

	m_committedVolumeReady = true;
	m_hasCommittedGeometry = true;
	m_volumeDirty = false;

	return true;
}
//
void Tesseract::generateSPVolume0Field(const TheArbiter& arbiter, float* dDestination) {
	if (!dDestination) return;

	const TheArbiter::VolumeObjectState& state =
		arbiter.getVolume0State();

	const bool baseSelected =
		state.primitive == TheArbiter::VOLUME_PRIMITIVE_BASE;

	const SPVolumeBasis basis =
		buildSPVolumeBasisFromState(arbiter, state);

	const float3 offset = buildSPVolumeOffsetFromState(state);
	if (baseSelected) {
		if (!m_hasCommittedGeometry || !m_dBaseVolume) {

			clearVolumeKernelLauncher(
				dDestination,
				m_volumeSize,
				1.0e6f
			);
		}
		else {
			transformVolumeFieldLauncher(
				m_dBaseVolume,
				dDestination,
				m_volumeSize,
				make_float3(
					state.scaleWhole * state.scaleX,
					state.scaleWhole * state.scaleY,
					state.scaleWhole * state.scaleZ
				),
				offset,
				basis.xAxis,
				basis.yAxis,
				basis.zAxis
			);
		}

		return;
	}

	volumeKernelLauncher(
		dDestination,
		m_volumeSize,
		getSPVolumePrimitiveIdFromState(state),
		buildSPVolumePrimitiveParamsFromState(state),
		offset,
		basis.xAxis,
		basis.yAxis,
		basis.zAxis
	);
}
//
void Tesseract::generateSPVolume1BrushField(const TheArbiter& arbiter, float* dDestination) {
	if (!dDestination) return;
	if (!arbiter.hasInjectionVoxelSelected()) {
		clearVolumeKernelLauncher(
			dDestination,
			m_volumeSize,
			1.0e6f
		);

		return;
	}

	const TheArbiter::VolumeObjectState& state =
		arbiter.getVolume1State();

	const SPVolumeBasis basis =
		buildSPVolumeBasisFromState(arbiter, state);

	const float3 railBrushOffset =
		buildSPVolumeRailBrushOffset(arbiter);

	volumeKernelLauncher(
		dDestination,
		m_volumeSize,
		getSPVolumePrimitiveIdFromState(state),
		buildSPVolumePrimitiveParamsFromState(state),
		railBrushOffset,
		basis.xAxis,
		basis.yAxis,
		basis.zAxis
	);
}
//
void Tesseract::updateSPVolumePreview(const TheArbiter& arbiter) {
	m_volumeBoundarySensorReady = false;
	if (!m_dWorkingVolume || !m_dBaseVolume) return;

	if (!m_committedVolumeReady) {
		clearSPCommittedVolume();
	}

	const SPVolumeBasis basis = buildSPVolumeBasis(arbiter);

	const bool baseSelected =
		arbiter.getVolumePrimitiveSelection() ==
		TheArbiter::VOLUME_PRIMITIVE_BASE &&
		!arbiter.isInjectionBrushBaseSelected();

	const float3 offset = buildSPVolumeOffset(arbiter);

	// ---------------------------------------------------------
	// BASE: transform/resample the committed working mesh.
	// VOLUME_1 BASE is different. For VOLUME_1, BASE means
	// "the injection brush has been locally rebased."
	// That case is intentionally excluded by:
	//
	//     !arbiter.isInjectionBrushBaseSelected()
	// ---------------------------------------------------------
	if (baseSelected) {

		if (!m_hasCommittedGeometry) {
			clearVolumeKernelLauncher(
				m_dWorkingVolume,
				m_volumeSize,
				1.0e6f
			);
		}
		else {
			transformVolumeFieldLauncher(
				m_dBaseVolume,
				m_dWorkingVolume,
				m_volumeSize,
				make_float3(
					arbiter.getEffectiveVolumeScaleX(),
					arbiter.getEffectiveVolumeScaleY(),
					arbiter.getEffectiveVolumeScaleZ()
				),
				offset,
				basis.xAxis,
				basis.yAxis,
				basis.zAxis
			);
		}

		threadSync();
		m_volumeDirty = false;
		return;
	}

	// ---------------------------------------------------------
	// Editable primitive preview.
	//
	// Checkpoint 4B cleanup:
	//
	// At this stage, updateSPVolumePreview() should NOT perform
	// FUSE/CUT boolean composition.
	//
	// It only generates the currently selected active volume state:
	//
	//     VOLUME_0 -> anchor object preview
	//     VOLUME_1 -> brush object preview
	//
	// FUSE/CUT is still only a render tint / UI mode right now.
	//
	// The rail-positioned two-volume composition path belongs to
	// the next checkpoint after the brush offset follows rail.
	// ---------------------------------------------------------
	volumeKernelLauncher(
		m_dWorkingVolume,
		m_volumeSize,
		getSPVolumePrimitiveId(arbiter),
		buildSPVolumePrimitiveParams(arbiter),
		offset,
		basis.xAxis,
		basis.yAxis,
		basis.zAxis
	);

	threadSync();
	m_volumeDirty = false;
}
//
void Tesseract::copyCommittedVolumeToPreview() {

	m_volumeBoundarySensorReady = false;
	if (!m_dBaseVolume || !m_dWorkingVolume)
		return;

	cudaMemcpy(
		m_dWorkingVolume,
		m_dBaseVolume,
		getVolumeBytes(),
		cudaMemcpyDeviceToDevice
	);

	threadSync();
	m_volumeDirty = false;
}
//
void Tesseract::clearSPCommittedVolume() {
	if (!m_dBaseVolume) return;

	clearVolumeKernelLauncher(
		m_dBaseVolume,
		m_volumeSize,
		1.0e6f
	);

	threadSync();

	m_committedVolumeReady = true;
	m_hasCommittedGeometry = false;
	m_volumeDirty = true;
}
//
// =============================================================================
// SINGLE_PARTICLE_MCAD VOLUME RENDERING
// =============================================================================
bool Tesseract::renderSPVolumeToPBO(
	const TheArbiter& arbiter,
	int renderMethod,
	int viewportW,
	int viewportH,
	float thetaRad,
	float phiRad,
	float zs,
	float threshold,
	float sliceDistance) {

	if (!m_dWorkingVolume) return false;
	if (!m_cudaPboResourceSlot || !(*m_cudaPboResourceSlot)) return false;
	const bool railTwoPassPreview =
		arbiter.hasInjectionVoxelSelected() &&
		m_dBrushVolume != nullptr &&
		arbiter.isVolumeRenderSubLayer() &&
		arbiter.getVolumeAssemblyNode() ==
		TheArbiter::VOLUME_NODE_OFFSET_OBJECT &&
		arbiter.isEditingInjectionVoxel0();

	if (railTwoPassPreview) {
		// pass source 0:
		//		VOLUME_0 anchor field.
		generateSPVolume0Field(arbiter, m_dWorkingVolume);

		// pass source 1:
		//		VOLUME_1 brush field, positioned by railT
		generateSPVolume1BrushField(arbiter, m_dBrushVolume);

		threadSync();

		if (arbiter.getVolumeInjectionMode() ==
			TheArbiter::VOLUME_CUT) {

			markSPVolumeBoundarySafe();
		}
		else {
			classifySPVolumeBoundaryForSource(
				m_dBrushVolume, 0.0f, 0.0f
			);
		}
		m_volumeDirty = false;
	}
	else {
		if (m_volumeDirty)
			regenerateSPVolumeField(arbiter);
	}

	uchar4* dOut =
		reinterpret_cast<uchar4*>(
			mapGLBufferObject(m_cudaPboResourceSlot)
		);

	// ---------------------------------------------------------
	// Pass 1:
	//     Anchor volume.
	//     VOLUME_0 is always rendered red.
	// ---------------------------------------------------------
	float mainTintR = 1.0f;
	float mainTintG = 0.0f;
	float mainTintB = 0.0f;

	const bool standaloneCutBrush =
		!railTwoPassPreview &&
		arbiter.hasInjectionVoxelSelected() &&
		arbiter.isEditingInjectionVoxel1() &&
		arbiter.getVolumeInjectionMode() == TheArbiter::VOLUME_CUT;

	if (standaloneCutBrush) {
		mainTintR = 0.05f;
		mainTintG = 0.28f;
		mainTintB = 1.0f;
	}

	kernelLauncher(
		dOut, m_dWorkingVolume,
		viewportW, viewportH,
		m_volumeSize, renderMethod,
		zs, thetaRad, phiRad,
		threshold, sliceDistance,
		mainTintR, mainTintG, mainTintB
	);

	if (railTwoPassPreview) {
		float brushTintR = 1.0f;
		float brushTintG = 0.0f;
		float brushTintB = 0.0f;

		if (arbiter.getVolumeInjectionMode() ==
			TheArbiter::VOLUME_CUT) {

			brushTintR = 0.05f;
			brushTintG = 0.28f;
			brushTintB = 1.0f;
		}
		// -----------------------------------------------------
		// Pass 2:
		//     Rail-positioned brush overlay.
		//
		//     FUSE -> red
		//     CUT  -> blue
		// -----------------------------------------------------
		kernelOverlayLauncher(
			dOut, m_dBrushVolume,
			viewportW, viewportH,
			m_volumeSize, renderMethod,
			zs, thetaRad, phiRad,
			threshold, sliceDistance,
			brushTintR, brushTintG, brushTintB,
			0.78f
		);
	}

	unmapGLBufferObject(*m_cudaPboResourceSlot);

	return true;
}
//
Tesseract::SPVolumeBasis
Tesseract::buildSPVolumeBasis(const TheArbiter& arbiter) const {

	const TheArbiter::ObjectBasis basis =
		arbiter.getEffectiveObjectBasis();

	SPVolumeBasis result;

	result.xAxis = make_float3(
		basis.xAxis.x,
		basis.xAxis.y,
		basis.xAxis.z
	);

	result.yAxis = make_float3(
		basis.yAxis.x,
		basis.yAxis.y,
		basis.yAxis.z
	);

	result.zAxis = make_float3(
		basis.zAxis.x,
		basis.zAxis.y,
		basis.zAxis.z
	);

	return result;
}
//
Tesseract::SPVolumeBasis
Tesseract::buildSPVolumeBasisFromState(
	const TheArbiter& arbiter,
	const TheArbiter::VolumeObjectState& state) const {

	const TheArbiter::BasisVector localX =
		arbiter.rotateLocalVectorXYZ(
			{ 1.0f, 0.0f, 0.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg
		);
	const TheArbiter::BasisVector localY =
		arbiter.rotateLocalVectorXYZ(
			{ 0.0f, 1.0f, 0.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg
		);
	const TheArbiter::BasisVector localZ =
		arbiter.rotateLocalVectorXYZ(
			{ 0.0f, 0.0f, 1.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg
		);

	TheArbiter::ObjectBasis effectiveBasis{
		arbiter.transformByBasis(state.basis, localX),
		arbiter.transformByBasis(state.basis, localY),
		arbiter.transformByBasis(state.basis, localZ)
	};

	effectiveBasis =
		arbiter.orthonormalizeBasis(effectiveBasis);

	SPVolumeBasis result;
	result.xAxis = make_float3(
		effectiveBasis.xAxis.x,
		effectiveBasis.xAxis.y,
		effectiveBasis.xAxis.z
	);
	result.yAxis = make_float3(
		effectiveBasis.yAxis.x,
		effectiveBasis.yAxis.y,
		effectiveBasis.yAxis.z
	);
	result.zAxis = make_float3(
		effectiveBasis.zAxis.x,
		effectiveBasis.zAxis.y,
		effectiveBasis.zAxis.z
	);

	return result;
}
//
void Tesseract::bindCommittedVolume(float* dVolume) {
	m_dBaseVolume = dVolume;
	m_committedVolumeReady = false;
	m_hasCommittedGeometry = false;
	m_volumeDirty = true;
}
//
void Tesseract::bindSPCadVolumeResource(
	struct cudaGraphicsResource** cudaPboResourceSlot) {

	m_cudaPboResourceSlot = cudaPboResourceSlot;
}
//
void Tesseract::clearCommittedVolumeBinding() {
	m_dBaseVolume = nullptr;
	m_committedVolumeReady = false;
	m_hasCommittedGeometry = false;
	m_volumeDirty = true;
}
//
void Tesseract::regenerateSPVolumeField(const TheArbiter& arbiter) {

	m_volumeBoundarySensorReady = false;
	if (!m_dWorkingVolume) return;

	// ---------------------------------------------------------
	// Defensive standalone-field fallback.
	// ---------------------------------------------------------
	if (!m_dBaseVolume) {

		if (!arbiter.hasEditableVolumePrimitive()) {

			clearVolumeKernelLauncher(
				m_dWorkingVolume,
				m_volumeSize,
				1.0e6f
			);
		}
		else {

			const SPVolumeBasis basis = buildSPVolumeBasis(arbiter);
			const float3 offset = buildSPVolumeOffset(arbiter);

			volumeKernelLauncher(
				m_dWorkingVolume,
				m_volumeSize,
				getSPVolumePrimitiveId(arbiter),
				buildSPVolumePrimitiveParams(arbiter),
				offset,
				basis.xAxis,
				basis.yAxis,
				basis.zAxis
			);
		}

		threadSync();
	}
	else {

		switch (arbiter.getVolumeAssemblyNode()) {

			// -----------------------------------------------------
			// Node_0: committed/base preview.
			// -----------------------------------------------------
		case TheArbiter::VOLUME_NODE_PREVIEW:

			if (m_hasCommittedGeometry) {

				copyCommittedVolumeToPreview();
			}
			else {

				updateSPVolumePreview(arbiter);
			}
			break;

			// -----------------------------------------------------
			// Nodes 1 through 3: transformed editable field.
			// -----------------------------------------------------
		case TheArbiter::VOLUME_NODE_EDIT_OBJECT:
		case TheArbiter::VOLUME_NODE_OFFSET_OBJECT:
		case TheArbiter::VOLUME_NODE_APPLY_TO_BASE:
		default:

			updateSPVolumePreview(arbiter);
			break;
		}
	}

	m_volumeDirty = false;

	// Classify the exact scalar field that was just generated.
	// isosurface
	 // direct-contact safety band
	updateSPVolumeBoundarySensor(0.0f, 0.0f);
}
//
void Tesseract::renderSPVolumeOrientationAxes(
	const TheArbiter& arbiter,
	float thetaRad,
	float phiRad) {

	if (!m_renderer) return;

	const TheArbiter::ObjectBasis baked =
		arbiter.getObjectBasis();

	const TheArbiter::ObjectBasis effective =
		arbiter.getEffectiveObjectBasis();

	EuclidRenderer::VolumeObjectBasis rendererBaked{
		glm::vec3(
			baked.xAxis.x,
			baked.xAxis.y,
			baked.xAxis.z
		),
		glm::vec3(
			baked.yAxis.x,
			baked.yAxis.y,
			baked.yAxis.z
		),
		glm::vec3(
			baked.zAxis.x,
			baked.zAxis.y,
			baked.zAxis.z
		)
	};

	EuclidRenderer::VolumeObjectBasis rendererEffective{
		glm::vec3(
			effective.xAxis.x,
			effective.xAxis.y,
			effective.xAxis.z
		),
		glm::vec3(
			effective.yAxis.x,
			effective.yAxis.y,
			effective.yAxis.z
		),
		glm::vec3(
			effective.zAxis.x,
			effective.zAxis.y,
			effective.zAxis.z
		)
	};

	EuclidRenderer::VolumeAxisGuideMode guideMode =
		EuclidRenderer::VOLUME_AXIS_GUIDE_DEFAULT;

	const bool objectEditNodeActive =
		arbiter.isVolumeRenderSubLayer() &&
		arbiter.getVolumeAssemblyNode() ==
		TheArbiter::VOLUME_NODE_EDIT_OBJECT;


	if (objectEditNodeActive &&
		arbiter.getObjectTransformMode() ==
		TheArbiter::TRANSFORM_SCALE) {

		switch (arbiter.getObjectEditMode()) {
		case TheArbiter::EDIT_SCALE_X:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_X;
			break;

		case TheArbiter::EDIT_SCALE_Y:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_Y;
			break;

		case TheArbiter::EDIT_SCALE_Z:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_Z;
			break;

		default:
		case TheArbiter::EDIT_SCALE_WHOLE:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_DEFAULT;
			break;
		}
	}
	else if (objectEditNodeActive && arbiter.getObjectTransformMode() == TheArbiter::TRANSFORM_ROTATION) {
		switch (arbiter.getObjectRotationMode()) {
		case TheArbiter::ROTATE_YAW:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_YAW;
			break;

		case TheArbiter::ROTATE_ROLL:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_ROLL;
			break;

		default:
		case TheArbiter::ROTATE_PITCH:
			guideMode = EuclidRenderer::VOLUME_AXIS_GUIDE_PITCH;
			break;
		}
	}

	m_renderer->displayVolumeOrientationAxes(
		thetaRad,
		phiRad,
		guideMode,
		rendererBaked,
		rendererEffective,
		arbiter.getRotationPitchDeg(),
		arbiter.getRotationYawDeg(),
		arbiter.getRotationRollDeg()
	);
}
//
void Tesseract::renderSPVolumeInjectionVoxelPreview(
	const TheArbiter& arbiter,
	float thetaRad,
	float phiRad,
	float zs) {
	if (!m_renderer) return;

	// ---------------------------------------------------------
	// Checkpoint 1A visibility rule.
	//
	// Show the central injection-voxel cage only while:
	//
	//     Sub-Layer 2 is active
	//     Node_0 Preview is active
	//     the panel is visible
	//     list [1] Injection Voxels owns the cursor
	//
	// The active-item value persists while the panel is hidden,
	// so checking panel visibility here is important.
	// ---------------------------------------------------------
	const bool injectionVoxelListSelected =
		arbiter.isVolumeRenderSubLayer() &&
		arbiter.getVolumeAssemblyNode() ==
		TheArbiter::VOLUME_NODE_PREVIEW &&
		arbiter.isSubLayerPanelOpen() &&
		arbiter.getActiveSubLayerPanelItem() ==
		TheArbiter::PREVIEW_LIST_INJECTION_MODE;

	if (!injectionVoxelListSelected) return;

	m_renderer->displayVolumeInjectionVoxelPreview(
		thetaRad,
		phiRad,
		zs,
		m_volumeSize.x,
		0.55f,
		arbiter.getInjectionVoxelDX(),
		arbiter.getInjectionVoxelDY(),
		arbiter.getInjectionVoxelDZ()
	);
}
//
void Tesseract::renderSPVolumeInjectionEditTargetPreview(
	const TheArbiter& arbiter,
	float thetaRad,
	float phiRad,
	float zs) {

	if (!m_renderer) return;
	// ---------------------------------------------------------
	// Checkpoint 3B visibility rule.
	//
	// This preview belongs only to:
	//
	//     Sub-Layer 2
	//     Node_1 Edit Object
	//     Injection Voxels != NONE
	//     side panel open
	//     List [1] Edit { VOXEL_0 / VOXEL_1 } selected
	//
	// That keeps the edit-target cages tied directly to List [1].
	// ---------------------------------------------------------
	const bool targetListSelected =
		arbiter.isVolumeRenderSubLayer() &&
		arbiter.getVolumeAssemblyNode() == TheArbiter::VOLUME_NODE_EDIT_OBJECT &&
		arbiter.hasInjectionVoxelSelected() &&
		arbiter.isSubLayerPanelOpen() &&
		arbiter.getActiveSubLayerPanelItem() == TheArbiter::INJECTION_EDIT_LIST_TARGET;

	if (!targetListSelected) return;
	m_renderer->displayVolumeInjectionEditTargetPreview(
		thetaRad,
		phiRad,
		zs,
		m_volumeSize.x,
		0.55f,
		arbiter.getInjectionVoxelDX(),
		arbiter.getInjectionVoxelDY(),
		arbiter.getInjectionVoxelDZ(),
		arbiter.isEditingInjectionVoxel1()
	);

}
//
void Tesseract::renderSPVolumeOffsetGrid(
	const TheArbiter& arbiter,
	float thetaRad,
	float phiRad,
	float zs) {

	if (!m_renderer) return;

	if (!arbiter.isVolumeRenderSubLayer() ||
		arbiter.getVolumeAssemblyNode() !=
		TheArbiter::VOLUME_NODE_OFFSET_OBJECT) {

		return;
	}

	// ---------------------------------------------------------
	// Node_2 Injection List [1] relationship preview.
	//
	// When the user is selecting:
	//
	//     Edit Offset Volume:
	//     > [1] Select { VOLUME_0 / VOLUME_1 }
	//
	// show the voxel relationship instead of the offset grid:
	//
	//     Anchor voxel      -> faint neon orange cage
	//     Injection voxel   -> faint greenish-cyan cage
	//     Injection rail    -> colored direction rail
	//
	// This makes List [1] communicate "which chamber belongs
	// to which volume" before the user edits offsets or rail.
	// ---------------------------------------------------------

	const bool injectionTargetListSelected =
		arbiter.hasInjectionVoxelSelected() &&
		arbiter.isSubLayerPanelOpen() &&
		arbiter.getActiveSubLayerPanelItem() ==
		TheArbiter::INJECTION_OFFSET_LIST_TARGET;

	if (injectionTargetListSelected) {

		m_renderer->displayVolumeInjectionEditTargetPreview(
			thetaRad,
			phiRad,
			zs,
			m_volumeSize.x,
			0.48f,
			arbiter.getInjectionVoxelDX(),
			arbiter.getInjectionVoxelDY(),
			arbiter.getInjectionVoxelDZ(),
			arbiter.isEditingInjectionVoxel1()
		);

		return;
	}

	// ---------------------------------------------------------
	// Normal Node_2 offset grid / boundary-sensor view.
	//
	// This remains active for:
	//
	//     [2] Offset vector
	//     [3] Increment
	//     [4] Injection rail / Inject mode
	//     [5] Apply
	//     [6] Previous
	//
	// and for the non-injection default pipeline.
	// ---------------------------------------------------------
	EuclidRenderer::VolumeOffsetAxis rendererAxis =
		EuclidRenderer::VOLUME_OFFSET_AXIS_X;

	switch (arbiter.getOffsetVectorSelection()) {

	case TheArbiter::OFFSET_VECTOR_Y:
		rendererAxis = EuclidRenderer::VOLUME_OFFSET_AXIS_Y;
		break;

	case TheArbiter::OFFSET_VECTOR_Z:
		rendererAxis = EuclidRenderer::VOLUME_OFFSET_AXIS_Z;
		break;

	default:
	case TheArbiter::OFFSET_VECTOR_X:
		rendererAxis = EuclidRenderer::VOLUME_OFFSET_AXIS_X;
		break;
	}

	m_renderer->displayVolumeOffsetGrid(
		thetaRad,
		phiRad,
		zs,
		m_volumeSize.x,
		rendererAxis,
		arbiter.getOffsetIncrement(),
		arbiter.getOffsetX(),
		arbiter.getOffsetY(),
		arbiter.getOffsetZ(),
		m_volumeBoundaryMaskCPU,
		m_volumeBoundaryFaceStride
	);
	// ---------------------------------------------------------
	// Checkpoint 4A:
	//
	// Node_2 List [4] Injection Vector visual rail.
	//
	// Only active when:
	//     Injection voxel is selected
	//     Node_2 Offset Object is active
	//     VOLUME_0 is selected
	//     side panel is open
	//     List [4] Injection Vector owns the cursor
	//
	// This is visual-only. No CUDA composition happens here.
	// ---------------------------------------------------------
	const bool injectionRailListSelected =
		arbiter.hasInjectionVoxelSelected() &&
		arbiter.isEditingInjectionVoxel0() &&
		arbiter.isSubLayerPanelOpen() &&
		arbiter.getActiveSubLayerPanelItem() ==
		TheArbiter::INJECTION_OFFSET_LIST_RAIL;

	if (injectionRailListSelected) {

		const TheArbiter::VolumeObjectState& brushState =
			arbiter.getVolume1State();

		const float brushOffsetX = brushState.offsetX * 0.5f *
			static_cast<float>(m_volumeSize.x);

		const float brushOffsetY = brushState.offsetY * 0.5f *
			static_cast<float>(m_volumeSize.y);

		const float brushOffsetZ = brushState.offsetZ * 0.5f *
			static_cast<float>(m_volumeSize.z);

		m_renderer->displayVolumeInjectionRailMarker(
			thetaRad,
			phiRad,
			zs,
			m_volumeSize.x,
			arbiter.getInjectionVoxelDX(),
			arbiter.getInjectionVoxelDY(),
			arbiter.getInjectionVoxelDZ(),
			arbiter.getInjectionRailT(),
			1.0f,
			brushOffsetX,
			brushOffsetY,
			brushOffsetZ
		);
	}
}
//
void Tesseract::renderSPVolumeTexture() {
	if (!m_renderer) return;
	m_renderer->displayVolumeTexture();
}
//
// =============================================================================
// SINGLE_PARTICLE_MCAD NODE_2 BOUNDARY SENSOR
// =============================================================================
bool Tesseract::initializeSPVolumeBoundarySensor() {
	if (m_volumeSize.x <= 0 ||
		m_volumeSize.y <= 0 ||
		m_volumeSize.z <= 0) {

		return false;
	}

	const unsigned int requiredFaceStride =
		getVolumeBoundaryFaceStride(m_volumeSize);

	const size_t requiredMaskBytes =
		getVolumeBoundaryMaskBytes(m_volumeSize);

	if (requiredFaceStride == 0 ||
		requiredMaskBytes == 0) {

		return false;
	}

	const bool buffersAlreadyValid =
		m_dVolumeBoundaryMask != nullptr &&
		m_dVolumeBoundaryUnsafeCount != nullptr &&
		m_volumeBoundaryFaceStride ==
		requiredFaceStride &&
		m_volumeBoundaryMaskCPU.size() ==
		requiredMaskBytes;

	if (buffersAlreadyValid) return true;
	releaseSPVolumeBoundarySensor();

	allocateArray(
		reinterpret_cast<void**>(&m_dVolumeBoundaryMask),
		requiredMaskBytes
	);
	allocateArray(
		reinterpret_cast<void**>(&m_dVolumeBoundaryUnsafeCount),
		sizeof(unsigned int)
	);

	if (!m_dVolumeBoundaryMask ||
		!m_dVolumeBoundaryUnsafeCount) {

		releaseSPVolumeBoundarySensor();
		return false;
	}

	m_volumeBoundaryMaskCPU.assign(
		requiredMaskBytes,
		static_cast<unsigned char>(0)
	);

	m_volumeBoundaryFaceStride =
		requiredFaceStride;

	m_volumeBoundaryUnsafeCount = 0;
	m_volumeBoundarySensorReady = false;

	printf(
		"[Tesseract] Volume boundary sensor allocated: "
		"faceStride=%u maskBytes=%zu\n",
		m_volumeBoundaryFaceStride,
		requiredMaskBytes
	);

	return true;
}
//
bool Tesseract::updateSPVolumeBoundarySensor(float isoValue, float safetyBand) {

	return classifySPVolumeBoundaryForSource(
		m_dWorkingVolume,
		isoValue,
		safetyBand
	);
}
//
bool Tesseract::classifySPVolumeBoundaryForSource(
	const float* dSourceVolume,
	float isoValue,
	float safetyBand) {

	m_volumeBoundarySensorReady = false;
	m_volumeBoundaryUnsafeCount = 0;

	if (!dSourceVolume) return false;
	if (!initializeSPVolumeBoundarySensor())
		return false;

	const size_t maskBytes =
		getVolumeBoundaryMaskBytes(m_volumeSize);

	if (maskBytes == 0 ||
		m_volumeBoundaryMaskCPU.size() != maskBytes)
		return false;

	classifyVolumeBoundaryLauncher(
		dSourceVolume,
		m_dVolumeBoundaryMask,
		m_dVolumeBoundaryUnsafeCount,
		m_volumeSize,
		isoValue,
		safetyBand
	);

	threadSync();

	copyArrayFromDevice(
		&m_volumeBoundaryUnsafeCount,
		m_dVolumeBoundaryUnsafeCount,
		nullptr,
		static_cast<int>(sizeof(unsigned int))
	);

	if (m_volumeBoundaryUnsafeCount == 0) {

		std::fill(
			m_volumeBoundaryMaskCPU.begin(),
			m_volumeBoundaryMaskCPU.end(),
			static_cast<unsigned char>(0)
		);
	}
	else {

		copyArrayFromDevice(
			m_volumeBoundaryMaskCPU.data(),
			m_dVolumeBoundaryMask,
			nullptr,
			static_cast<int>(maskBytes)
		);
	}

	m_volumeBoundarySensorReady = true;

	return true;
}
//
void Tesseract::releaseSPVolumeBoundarySensor() {

	if (m_dVolumeBoundaryMask) {
		freeArray(m_dVolumeBoundaryMask);

		m_dVolumeBoundaryMask = nullptr;
	}

	if (m_dVolumeBoundaryUnsafeCount) {
		freeArray(m_dVolumeBoundaryUnsafeCount);

		m_dVolumeBoundaryUnsafeCount = nullptr;
	}

	m_volumeBoundaryMaskCPU.clear();
	m_volumeBoundaryMaskCPU.shrink_to_fit();

	m_volumeBoundaryFaceStride = 0;
	m_volumeBoundaryUnsafeCount = 0;
	m_volumeBoundarySensorReady = false;
}
//
void Tesseract::markSPVolumeBoundarySafe() {
	m_volumeBoundarySensorReady = true;
	m_volumeBoundaryUnsafeCount = 0;

	std::fill(
		m_volumeBoundaryMaskCPU.begin(),
		m_volumeBoundaryMaskCPU.end(),
		static_cast<unsigned char>(0)
	);
}
//
int Tesseract::getSPVolumePrimitiveId(const TheArbiter& arbiter) const {
	switch (arbiter.getResolvedVolumePrimitiveSelection()) {
		case TheArbiter::VOLUME_PRIMITIVE_BASE: return -1;
		case TheArbiter::VOLUME_PRIMITIVE_SPHERE: return 0;
		case TheArbiter::VOLUME_PRIMITIVE_TORUS: return 1;
		case TheArbiter::VOLUME_PRIMITIVE_BLOCK: return 2;
		case TheArbiter::VOLUME_PRIMITIVE_CYLINDER: return 3;
		case TheArbiter::VOLUME_PRIMITIVE_CAPSULE: return 4;
		case TheArbiter::VOLUME_PRIMITIVE_WEDGE: return 5;
		case TheArbiter::VOLUME_PRIMITIVE_FRUSTUM: return 6;
		default: return -1;
	}
}
//
int Tesseract::getSPVolumePrimitiveIdFromState(const TheArbiter::VolumeObjectState& state) const {
	// For VOLUME_1, BASE means "rebased brush".
	// Render the remembered real brush primitive.

	TheArbiter::VolumePrimitive primitive =
		state.primitive;

	if (primitive == TheArbiter::VOLUME_PRIMITIVE_BASE) {
		if (state.brushBaseReady &&
			state.brushBasePrimitive != TheArbiter::VOLUME_PRIMITIVE_BASE) {

			primitive = state.brushBasePrimitive;
		}
		else {
			primitive = TheArbiter::VOLUME_PRIMITIVE_SPHERE;
		}
	}

	switch (primitive) {
		case TheArbiter::VOLUME_PRIMITIVE_SPHERE: return 0;
		case TheArbiter::VOLUME_PRIMITIVE_TORUS: return 1;
		case TheArbiter::VOLUME_PRIMITIVE_BLOCK: return 2;
		case TheArbiter::VOLUME_PRIMITIVE_CYLINDER: return 3;
		case TheArbiter::VOLUME_PRIMITIVE_CAPSULE: return 4;
		case TheArbiter::VOLUME_PRIMITIVE_WEDGE: return 5;
		case TheArbiter::VOLUME_PRIMITIVE_FRUSTUM: return 6;
		default: return 0;
	}
}
//
float3 Tesseract::buildSPVolumeOffset(const TheArbiter& arbiter) const {

	// Node_2 distances use normalized half-volume coordinates.
	//
	// For 128^3:
	//     normalized 1.00 = 64 voxels
	//     normalized 0.01 = 0.64 voxels
	return make_float3(
		arbiter.getOffsetX() *
		0.5f *
		static_cast<float>(m_volumeSize.x),

		arbiter.getOffsetY() *
		0.5f *
		static_cast<float>(m_volumeSize.y),

		arbiter.getOffsetZ() *
		0.5f *
		static_cast<float>(m_volumeSize.z)
	);
}
//
float3 Tesseract::buildSPVolumeOffsetFromState(const TheArbiter::VolumeObjectState& state) const {
	return make_float3(
		state.offsetX *
		0.5f *
		static_cast<float>(m_volumeSize.x),

		state.offsetY *
		0.5f *
		static_cast<float>(m_volumeSize.y),

		state.offsetZ *
		0.5f *
		static_cast<float>(m_volumeSize.z)
	);
}
//
float3 Tesseract::buildSPVolumeRailBrushOffset(const TheArbiter& arbiter) const {
	const TheArbiter::VolumeObjectState& brushState =
		arbiter.getVolume1State();

	float railT = arbiter.getInjectionRailT();
	if (railT < 0.0f) railT = 0.0f;
	if (railT > 1.0f) railT = 1.0f;

	const float3 injectionCenter = make_float3(
		static_cast<float>(arbiter.getInjectionVoxelDX()) *
		static_cast<float>(m_volumeSize.x),
		static_cast<float>(arbiter.getInjectionVoxelDY()) *
		static_cast<float>(m_volumeSize.y),
		static_cast<float>(arbiter.getInjectionVoxelDZ()) *
		static_cast<float>(m_volumeSize.z)
	);

	const float3 railCenter = make_float3(
		injectionCenter.x * (1.0f - railT),
		injectionCenter.y * (1.0f - railT),
		injectionCenter.z * (1.0f - railT)
	);

	const float3 localBrushOffset =
		buildSPVolumeOffsetFromState(brushState);

	return make_float3(
		railCenter.x + localBrushOffset.x,
		railCenter.y + localBrushOffset.y,
		railCenter.z + localBrushOffset.z
	);
}
//
float4 Tesseract::buildSPVolumePrimitiveParams(const TheArbiter& arbiter) const {

	const float minDim =
		static_cast<float>(
			std::min(m_volumeSize.x,
				std::min(m_volumeSize.y, m_volumeSize.z)));

	const float sx = arbiter.getEffectiveVolumeScaleX();
	const float sy = arbiter.getEffectiveVolumeScaleY();
	const float sz = arbiter.getEffectiveVolumeScaleZ();

	float4 param;
	param.x = 0.0f;
	param.y = 0.0f;
	param.z = 0.0f;
	param.w = 0.0f;

	switch (arbiter.getResolvedVolumePrimitiveSelection()) {
	case TheArbiter::VOLUME_PRIMITIVE_TORUS: {
		const float xyScale = 0.5f * (sx + sy);

		// param.x = major radius
		// param.y = minor radius
		param.x = minDim * 0.30f * xyScale;
		param.y = minDim * 0.12f * sz;
		param.z = 0.0f;
		param.w = 0.0f;
		break;
	}

	case TheArbiter::VOLUME_PRIMITIVE_BLOCK:
		// param.xyz = box half-extents
		param.x = minDim * 0.42f * sx;
		param.y = minDim * 0.42f * sy;
		param.z = minDim * 0.42f * sz;
		param.w = 0.0f;
		break;

	case TheArbiter::VOLUME_PRIMITIVE_CYLINDER: {
		const float xyScale = 0.5f * (sx + sy);

		// param.x = radius
		// param.z = half height
		param.x = minDim * 0.32f * xyScale;
		param.y = 0.0f;
		param.z = minDim * 0.42f * sz;
		param.w = 0.0f;
		break;
	}

	case TheArbiter::VOLUME_PRIMITIVE_CAPSULE: {
		const float xyScale = 0.5f * (sx + sy);

		// param.x = radius
		// param.z = half segment length
		param.x = minDim * 0.20f * xyScale;
		param.y = 0.0f;
		param.z = minDim * 0.26f * sz;
		param.w = 0.0f;
		break;
	}

	case TheArbiter::VOLUME_PRIMITIVE_WEDGE:
		// param.xyz = prism half-extents
		param.x = minDim * 0.42f * sx;
		param.y = minDim * 0.42f * sy;
		param.z = minDim * 0.42f * sz;
		param.w = 0.0f;
		break;

	case TheArbiter::VOLUME_PRIMITIVE_FRUSTUM:
		// param.x = bottom half X
		// param.y = bottom half Y
		// param.z = half height
		// param.w = top scale
		param.x = minDim * 0.42f * sx;
		param.y = minDim * 0.42f * sy;
		param.z = minDim * 0.42f * sz;
		param.w = 0.45f;
		break;
	default:
	case TheArbiter::VOLUME_PRIMITIVE_SPHERE:
		// Ellipsoid radii.
		param.x = minDim * 0.46f * sx;
		param.y = minDim * 0.46f * sy;
		param.z = minDim * 0.46f * sz;
		param.w = 0.0f;
		break;
	}

	return param;
}
//
float4 Tesseract::buildSPVolumePrimitiveParamsFromState(const TheArbiter::VolumeObjectState& state) const {

	const float minDim =
		static_cast<float>(
			std::min(m_volumeSize.x,
				std::min(m_volumeSize.y, m_volumeSize.z)));

	const float sx = state.scaleWhole * state.scaleX;
	const float sy = state.scaleWhole * state.scaleY;
	const float sz = state.scaleWhole * state.scaleZ;

	TheArbiter::VolumePrimitive primitive =
		state.primitive;

	if (primitive == TheArbiter::VOLUME_PRIMITIVE_BASE) {
		if (state.brushBaseReady &&
			state.brushBasePrimitive != TheArbiter::VOLUME_PRIMITIVE_BASE) {

			primitive = state.brushBasePrimitive;
		}
		else {
			primitive = TheArbiter::VOLUME_PRIMITIVE_SPHERE;
		}
	}

	float4 param = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

	switch (primitive) {
	case TheArbiter::VOLUME_PRIMITIVE_TORUS: {
		const float xyScale = 0.5f * (sx + sy);

		param.x = minDim * 0.30f * xyScale;
		param.y = minDim * 0.12f * sz;
		break;
	}

	case TheArbiter::VOLUME_PRIMITIVE_BLOCK:
		param.x = minDim * 0.42f * sx;
		param.y = minDim * 0.42f * sy;
		param.z = minDim * 0.42f * sz;
		break;

	case TheArbiter::VOLUME_PRIMITIVE_CYLINDER: {
		const float xyScale = 0.5f * (sx + sy);

		param.x = minDim * 0.32f * xyScale;
		param.z = minDim * 0.42f * sz;
		break;
	}

	case TheArbiter::VOLUME_PRIMITIVE_CAPSULE: {
		const float xyScale = 0.5f * (sx + sy);

		param.x = minDim * 0.20f * xyScale;
		param.z = minDim * 0.26f * sz;
		break;
	}

	case TheArbiter::VOLUME_PRIMITIVE_WEDGE:
		param.x = minDim * 0.42f * sx;
		param.y = minDim * 0.42f * sy;
		param.z = minDim * 0.42f * sz;
		break;

	case TheArbiter::VOLUME_PRIMITIVE_FRUSTUM:
		param.x = minDim * 0.42f * sx;
		param.y = minDim * 0.42f * sy;
		param.z = minDim * 0.42f * sz;
		param.w = 0.45f;
		break;

	default:
	case TheArbiter::VOLUME_PRIMITIVE_SPHERE:
		param.x = minDim * 0.46f * sx;
		param.y = minDim * 0.46f * sy;
		param.z = minDim * 0.46f * sz;
		break;
	}

	return param;
}

// =============================================================================
// LINKED_PARTICLES_MCAD PLACEHOLDER WORKSPACE (GRID_3D)
// =============================================================================
bool Tesseract::initializeLPWorkspace() {
	// Gate 2 placeholder only. LINKED_PARTICLES_MCAD remains RESERVED in
	// TheArbiter and therefore cannot be entered through normal navigation.
	m_LPWorkspace.initialized = true;
	return true;
}
//
void Tesseract::updateLPWorkspace(const WorkspaceUpdateContext& ctx) {
	(void)ctx;
	if (!m_LPWorkspace.initialized &&
		!initializeLPWorkspace())
		return;

	updateLinkedParticlesMCAD();
}
//
void Tesseract::renderLPWorkspace(const WorkspaceRenderContext& ctx) {
	if (!m_LPWorkspace.initialized &&
		!initializeLPWorkspace())
		return;

	renderLinkedParticlesWorkspace(ctx);
}
//
void Tesseract::renderLinkedParticlesWorkspace(const WorkspaceRenderContext& ctx) {

	(void)ctx;
}
//
bool Tesseract::updateLinkedParticlesMCAD() {
	return m_LPWorkspace.initialized;
}
