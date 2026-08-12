#ifndef _THE_TESSERACT_H_
#define _THE_TESSERACT_H_


#include <GL/glew.h>
#include <cstddef>
#include <vector>
#include <vector_types.h>
#include <filesystem>
#include "TheArbiter.h"
#include "particleSystem.h"
#include "renderer_Euclid.h"
#include "TextureMapWorkspace.h"

struct cudaGraphicsResource;
class MarchingCubes;

class Tesseract {
public:
	// --- GLOBAL SHELL ANIMATION TYPES ---
	enum TesseractAnimMode {
		ANIM_MODE_IDLE_PREVIEW = 0,
		ANIM_MODE_WORKSPACE_STABLE
	};

	enum TesseractAnimTransition {
		ANIM_TRANS_NONE = 0,
		ANIM_TRANS_IDLE_TO_WORKSPACE,
		ANIM_TRANS_WORKSPACE_TO_IDLE
	};

	enum TesseractAnimPhase {
		ANIM_PHASE_NONE = 0,
		ANIM_PHASE_ORIENT_TO_WORKSPACE,
		ANIM_PHASE_CAMERA_FOCUS_TO_WORKSPACE,
		ANIM_PHASE_RETURN_TO_IDLE
	};

	// --- WORKSPACE PIPELINE CONTEXTS ---
	using WorkspaceId = TheArbiter::WorkspaceId;

	struct WorkspaceSpatialDomain {
		float boxSize = 4.0f;
		int collisionCellsPerAxis = 64;

		// show one visual line for every four collision cells
		int visualGridStride = 4;
		bool drawMinorGrid = false;
	};

	struct WorkspaceGridVisualConfig {
		// 64 logical cells / stride 8 = 8 visible divisions.
		int majorStride = 8;
		bool drawBoundary = true;
		bool drawMajor = true;
		bool drawMinor = false;
		bool drawAxes = true;
	};

	struct PSGridVisualState {
		TheArbiter::ParticleGridLayout layout =
			TheArbiter::ParticleGridLayout::Full;

		WorkspaceGridVisualConfig render = {
			8,
			true,
			true,
			true,
			true
		};

		bool dynamicPlaceholder = false;
	};

	// Reserved extension point shared by all workspace update pipelines.
	struct WorkspaceUpdateContext {};

	struct WorkspaceRenderContext {
		const TheArbiter* arbiter = nullptr;
		MarchingCubes* marchingCubes = nullptr;

		EuclidRenderer::DisplayMode displayMode =
			EuclidRenderer::PARTICLE_SPHERES;

		bool displayEnabled = true;
		int viewportW = 1920;
		int viewportH = 1080;

		float thetaRad = 0.0f;
		float phiRad = 0.0f;
		float particleWorkspaceZs = 256.0f;
		float volumeRenderZs = 256.0f;
		float threshold = 0.0f;
		float sliceDistance = 0.0f;
	};

	// --- WORKSPACE INSTANCE STATE ---
	struct PSSimulationConfig {
		float fixedTimestep = 0.025f;
		int solverIterations = 1;

		float globalDamping = 0.9995f;
		float gravityMagnitude = 0.00098f;
		float collisionSpring = 0.5f;
		float collisionDamping = 0.02f;
		float collisionShear = 0.1f;
		float collisionAttraction = 0.0f;
		float simulationBoxSize = 4.0f;
	};

	struct PSRuntimeState {
		bool paused = true;
		float elapsedSimulationTime = 0.0f;
	};

	struct PSWorkspaceInstance {
		WorkspaceId id = WorkspaceId::PARTICLE_SIMULATION;
		bool initialized = false;
		bool resourcesBound = false;
		PSSimulationConfig config;
		PSRuntimeState runtime;
		PSGridVisualState gridVisual;
	};

	struct SPWorkspaceInstance {
		WorkspaceId id = WorkspaceId::SINGLE_PARTICLE_MCAD;
		bool initialized = false;
		bool resourcesBound = false;
	};

	struct LPWorkspaceInstance {
		WorkspaceId id = WorkspaceId::LINKED_PARTICLES_MCAD;
		bool initialized = false;
	};

	struct TextureMapWorkspaceInstance {
		WorkspaceId id = WorkspaceId::TEXTURE_MAP_2D;

		bool initialized = false;
		bool sharedResourcesBound = false;

		vitru::TextureMapWorkspace runtime;
	};

	Tesseract() = default;
	~Tesseract();

	// --- GLOBAL SHELL ANIMATION ---
	float getPreviewRotation() const { return m_previewRotation; }
	float getSliceAnimation() const { return m_sliceAnimation; }

	bool consumeCameraFocusRequest();
	bool isTransitioningToWorkspace() const { return m_animTransition != ANIM_TRANS_NONE; }
	bool isOrientingToWorkspace() const { return m_animPhase == ANIM_PHASE_ORIENT_TO_WORKSPACE; }

	void beginAnimTransition(TesseractAnimTransition transition, float timeS);
	void updateAnimBehavior(float timeS);

	const WorkspaceGridVisualConfig& getWorkspaceGridVisualConfig() const { return m_workspaceGridVisual; }
	const PSGridVisualState& getPSGridVisualState() const { return m_PSWorkspace.gridVisual; }

	bool applyWorkspaceBoundaryGridVisual();
	bool applyPSGridVisual();
	bool setPSGridLayout(TheArbiter::ParticleGridLayout layout);
	int getWorkspaceGridHalfSliceRange() const;

	// --- ACTIVE WORKSPACE LIFECYCLE ---
	WorkspaceId getActiveWorkspace() const { return m_activeWorkspace; }
	void enterWorkspace(WorkspaceId workspace);
	void exitWorkspace();
	void updateActiveWorkspace(const WorkspaceUpdateContext& ctx);
	void renderActiveWorkspace(const WorkspaceRenderContext& ctx);
	bool isActiveWorkspacePaused() const;

	bool handleWorkspaceMouse(
		TheArbiter& arbiter,
		int button,
		int state,
		int x,
		int y,
		int viewportW,
		int viewportH
	);

	bool handleWorkspaceMotion(
		TheArbiter& arbiter,
		int x,
		int y,
		int viewportW,
		int viewportH
	);

	bool handleWorkspacePassiveMotion(
		TheArbiter& arbiter,
		int x,
		int y,
		int viewportW,
		int viewportH
	);

	// --- BIND WORKSPACE RESOURCES ---
	void bindParticleSimulationResources(
		ParticleSystem* particleSystem,
		EuclidRenderer* renderer,
		std::vector<float>* radiusBuffer
	);

	void bindSingleParticleResources(
		ParticleSystem* particleSystem,
		EuclidRenderer* renderer,
		std::vector<float>* radiusBuffer
	);

	void bindTextureMapResources(
		vitru::ProjectAssetRepository* repository,
		const std::filesystem::path& outputStaticParticlesRoot,
		const std::filesystem::path& baseMaterialsRoot
	);

	vitru::TextureMapWorkspace* getTextureMapWorkspaceRuntime() {
		if (!m_textureMapWorkspace.runtime.initialized()) 
			return nullptr;

		return &m_textureMapWorkspace.runtime;
	}

	const vitru::TextureMapWorkspace* getTextureMapWorkspaceRuntime() const {
		if (!m_textureMapWorkspace.runtime.initialized())
			return nullptr;

		return &m_textureMapWorkspace.runtime;
	}

	// --- PARTICLE_SIM CONTROLS ---
	const PSSimulationConfig& getPSConfig() const { return m_PSWorkspace.config; }
	bool startPSWorkspace();
	bool togglePSPause();
	bool resetPSWorkspace(ParticleSystem::ParticleConfig config);
	bool stepPSWorkspace();
	void syncPSRendering();

	// --- SINGLE_PARTICLE_MCAD ANCHOR / PARTICLE CONTROLS ---
	bool placeSPAnchor(float particleRadius);
	void applySPConfig(float particleRadius);
	void syncSPRendering();
	void clearSPPlacement() { m_placedSP = false; }
	bool isPlacedSP() const { return m_placedSP; }

	// --- SINGLE_PARTICLE_MCAD VOLUME RESOURCES ---
	const int3& getVolumeSize() const { return m_volumeSize; }
	size_t getVolumeBytes() const;
	bool hasVolume() const { return m_dWorkingVolume != nullptr; }
	bool hasCommittedVolume() const { return m_dBaseVolume != nullptr; }
	bool hasBrushVolume() const { return m_dBrushVolume != nullptr; }
	bool hasMirrorBrushVolume() const { return m_dMirrorBrushVolume != nullptr; }
	bool isVolumeDirty() const { return m_volumeDirty; }
	bool hasCommittedGeometry() const { return m_hasCommittedGeometry; }

	float* getVolume() const { return m_dWorkingVolume; }
	float* getCommittedVolume() const { return m_dBaseVolume; }
	float* getBrushVolume() const { return m_dBrushVolume; }
	float* getMirrorBrushVolume() const { return m_dMirrorBrushVolume; }

	void bindVolume(float* dVolume) {
		m_dWorkingVolume = dVolume;
		m_volumeDirty = true;
		m_volumeBoundarySensorReady = false;
	}

	void clearVolumeBinding() {
		m_dWorkingVolume = nullptr;
		m_volumeDirty = true;
		m_volumeBoundarySensorReady = false;
	}

	void bindCommittedVolume(float* dVolume);

	void bindBrushVolume(float* dVolume) {
		m_dBrushVolume = dVolume;
		m_volumeDirty = true;
	}
	void bindMirrorBrushVolume(float* dVolume) {
		m_dMirrorBrushVolume = dVolume;
		m_volumeDirty = true;
	}

	void clearCommittedVolumeBinding();
	void clearSPCommittedVolume();

	void clearBrushVolumeBinding() {
		m_dBrushVolume = nullptr;
		m_volumeDirty = true;
	}
	void clearMirrorBrushVolumeBinding() {
		m_dMirrorBrushVolume = nullptr;
		m_volumeDirty = true;
	}

	void copyCommittedVolumeToPreview();

	// Copy the current editable CUDA field into CPU memory for VSPA save.
	bool exportWorkingVolumeToHost(std::vector<float>& output) const;

	// Restore a native VSPA scalar field into both BASE and preview buffers.
	bool restoreCommittedVolumeFromHost(const std::vector<float>& input, const int3& sourceSize);

	void markVolumeDirty() {
		m_volumeDirty = true;
		m_volumeBoundarySensorReady = false;
	}

	bool commitSPWorkingVolume(const TheArbiter& arbiter);
	void regenerateSPVolumeField(const TheArbiter& arbiter);

	void bindSPCadVolumeResource(
		struct cudaGraphicsResource** cudaPboResourceSlot
	);

	// --- NODE_2 VOLUME BOUNDARY SENSOR ---
	bool initializeSPVolumeBoundarySensor();
	bool updateSPVolumeBoundarySensor(
		float isoValue = 0.0f,
		float safetyBand = 0.0f
	);

	bool isSPVolumeBoundarySensorReady() const { return m_volumeBoundarySensorReady; }
	unsigned int getSPVolumeBoundaryUnsafeCount() const { return m_volumeBoundaryUnsafeCount; }
	bool isSPOverlapPreviewSensorReady() const { return m_spOverlapPreviewSensorReady; }

	bool isSPOverlapPreviewActive() const {
		return m_spOverlapPreviewSensorReady &&
			m_spOverlapPreviewUnsafeCount == 0 &&
			m_spOverlapPreviewInsideSampleCount > 0 &&
			(!m_spOverlapPreviewMirrorRequired ||
				(m_spMirrorOverlapPreviewSensorReady &&
				m_spMirrorOverlapPreviewUnsafeCount == 0 &&
				m_spMirrorOverlapPreviewInsideSampleCount > 0));
	}

	unsigned int getSPOverlapPreviewUnsafeCount() const {
		return m_spOverlapPreviewUnsafeCount + m_spMirrorOverlapPreviewUnsafeCount;
	}

	unsigned int getSPOverlapPreviewInsideSampleCount() const {
		return m_spOverlapPreviewInsideSampleCount + m_spMirrorOverlapPreviewInsideSampleCount;
	}

	void releaseSPVolumeBoundarySensor();

private:
	bool applyGridVisual(
		const WorkspaceGridVisualConfig& visual
	);

	bool bindRendererToParticleSystem(
		ParticleSystem* particleSystem,
		std::vector<float>* radiusBuffer,
		int drawCount
	);

	// --- WORKSPACE PIPELINES: INITIALIZE -> UPDATE -> RENDER ---
	bool initializePSWorkspace();
	void updatePSWorkspace(const WorkspaceUpdateContext& ctx);
	void renderPSWorkspace(const WorkspaceRenderContext& ctx);

	bool initializeSPWorkspace();
	void updateSPWorkspace(const WorkspaceUpdateContext& ctx);
	void renderSPWorkspace(const WorkspaceRenderContext& ctx);

	bool initializeLPWorkspace();
	void updateLPWorkspace(const WorkspaceUpdateContext& ctx);
	void renderLPWorkspace(const WorkspaceRenderContext& ctx);

	bool initializeTextureMapWorkspace();
	void updateTextureMapWorkspace(const WorkspaceUpdateContext& ctx);
	void renderTextureMapWorkspace(const WorkspaceRenderContext& ctx);

	// --- PARTICLE_SIM PIPELINE ---
	void applyPSConfig();
	bool advanceParticleSimSTEP();

	void renderParticleSimulation(
		EuclidRenderer::DisplayMode displayMode,
		bool displayEnabled
	);

	// --- SINGLE_PARTICLE_MCAD PIPELINE ---
	bool updateSingleParticleMCAD();
	void renderSingleParticleWorkspace(const WorkspaceRenderContext& ctx);

	void renderSingleParticleMCAD(
		const TheArbiter& arbiter,
		float thetaRad,
		float phiRad,
		float zs
	);

	bool commitSPInjectionBoolean(const TheArbiter& arbiter);
	void updateSPVolumePreview(const TheArbiter& arbiter);

	bool renderSPVolumeToPBO(
		const TheArbiter& arbiter,
		int renderMethod,
		int viewportW,
		int viewportH,
		float thetaRad,
		float phiRad,
		float zs,
		float threshold,
		float sliceDistance
	);

	void renderSPVolumeOrientationAxes(
		const TheArbiter& arbiter,
		float thetaRad,
		float phiRad
	);

	void renderSPVolumeInjectionVoxelPreview(
		const TheArbiter& arbiter,
		float thetaRad,
		float phiRad,
		float zs
	);

	void renderSPVolumeInjectionEditTargetPreview(
		const TheArbiter& arbiter,
		float thetaRad,
		float phiRad,
		float zs
	);

	void renderSPVolumeOffsetGrid(
		const TheArbiter& arbiter,
		float thetaRad,
		float phiRad,
		float zs
	);

	void renderSPVolumeTexture();
	int getSPVolumePrimitiveId(const TheArbiter& arbiter) const;

	int getSPVolumePrimitiveIdFromState(
		const TheArbiter::VolumeObjectState& state
	) const;

	float3 buildSPVolumeOffset(const TheArbiter& arbiter) const;

	float3 buildSPVolumeOffsetFromState(
		const TheArbiter::VolumeObjectState& state
	) const;

	float3 buildSPVolumeRailBrushOffset(const TheArbiter& arbiter) const;
	float4 buildSPVolumePrimitiveParams(const TheArbiter& arbiter) const;

	float4 buildSPVolumePrimitiveParamsFromState(
		const TheArbiter::VolumeObjectState& state
	) const;

	struct SPVolumeBasis {
		float3 xAxis;
		float3 yAxis;
		float3 zAxis;
	};

	SPVolumeBasis buildSPVolumeBasis(const TheArbiter& arbiter) const;

	SPVolumeBasis buildSPVolumeBasisFromState(
		const TheArbiter& arbiter,
		const TheArbiter::VolumeObjectState& state
	) const;

	void generateSPVolume0Field(
		const TheArbiter& arbiter,
		float* dDestination
	);

	void generateSPVolume1BrushField(
		const TheArbiter& arbiter,
		float* dDestination
	);

	void generateSPVolume1MirroredBrushField(
		const TheArbiter& arbiter,
		float* dDestination
	);

	bool classifySPVolumeBoundaryForSource(
		const float* dSourceVolume,
		float isoValue,
		float safetyBand
	);

	bool classifySPVolumeBoundaryForSource(
		const float* dSourceVolume,
		float isoValue,
		float safetyBand,
		bool& sensorReady,
		unsigned int& unsafeCount,
		unsigned int* insideSampleCount,
		std::vector<unsigned char>* boundaryMaskCPU
	);

	void updateSPOverlapPreviewStatus(const TheArbiter& arbiter);
	void clearSPOverlapPreviewStatus();

	bool isSPVolumeBoundarySafe() const { return m_volumeBoundarySensorReady && m_volumeBoundaryUnsafeCount == 0; }
	void markSPVolumeBoundarySafe();
	//
	// --- LINKED_PARTICLES_MCAD PLACEHOLDER PIPELINE ---
	bool updateLinkedParticlesMCAD();
	void renderLinkedParticlesWorkspace(const WorkspaceRenderContext& ctx);
	//
	// --- GLOBAL SHELL ANIMATION IMPLEMENTATION ---
	void updateMealyAnimBehavior(float timeS);
	void updateMooreAnimBehavior(float timeS);
	void beginIdleToWorkspaceTransition(float timeS);
	void beginWorkspaceToIdleTransition(float timeS);
	//
	// --- GLOBAL SHELL ANIMATION STATE ---
	TesseractAnimTransition m_animTransition = ANIM_TRANS_NONE;
	TesseractAnimMode m_animMode = ANIM_MODE_IDLE_PREVIEW;
	TesseractAnimPhase m_animPhase = ANIM_PHASE_NONE;
	bool m_cameraFocusRequested = false;
	float m_transitionStartTime = 0.0f;
	float m_transitionDuration = 1.25f;
	float m_startPreviewRotation = 0.0f;
	float m_startSliceAnimation = 0.0f;
	float m_previewRotation = 0.0f;
	float m_sliceAnimation = 0.5f;

	WorkspaceGridVisualConfig m_workspaceGridVisual;

	// --- WORKSPACE INSTANCES ---
	WorkspaceId m_activeWorkspace = WorkspaceId::NONE;

	PSWorkspaceInstance m_PSWorkspace;

	SPWorkspaceInstance m_SPWorkspace;
	LPWorkspaceInstance m_LPWorkspace;

	TextureMapWorkspaceInstance m_textureMapWorkspace;

	// --- ASSIGN PARTICLE WORKSPACE RESOURCES ---
	ParticleSystem* m_particleSimSystem = nullptr;
	ParticleSystem* m_singleParticleSystem = nullptr;

	EuclidRenderer* m_renderer = nullptr;

	//std::vector<float>* m_particleRadii = nullptr;
	std::vector<float>* m_particleSimRadii = nullptr;
	std::vector<float>* m_singleParticleRadii = nullptr;

	// --- SINGLE_PARTICLE_MCAD STATE / VOLUME RESOURCES ---
	bool m_placedSP = false;
	bool m_volumeDirty = true;
	bool m_committedVolumeReady = false;
	// True only after geometry has been baked; committedVolumeReady only
	// means that the backing CUDA buffer has been initialized.
	bool m_hasCommittedGeometry = false;
	float* m_dWorkingVolume = nullptr;
	float* m_dBaseVolume = nullptr;
	float* m_dBrushVolume = nullptr;
	float* m_dMirrorBrushVolume = nullptr;
	int3 m_volumeSize{ 128, 128, 128 };
	struct cudaGraphicsResource** m_cudaPboResourceSlot = nullptr;

	// --- NODE_2 VOLUME BOUNDARY SENSOR STATE ---
	// Device mask: face * m_volumeBoundaryFaceStride + localPatch.
	// Face order matches VolumeBoundaryFaceId in kernel.h.
	unsigned char* m_dVolumeBoundaryMask = nullptr;
	unsigned int* m_dVolumeBoundaryUnsafeCount = nullptr;
	unsigned int* m_dVolumeInsideSampleCount = nullptr;
	std::vector<unsigned char> m_volumeBoundaryMaskCPU;
	unsigned int m_volumeBoundaryFaceStride = 0;
	unsigned int m_volumeBoundaryUnsafeCount = 0;
	bool m_volumeBoundarySensorReady = false;

	// Shared overlap eligibility is intentionally independent from
	// Node_2/Node_3 commit boundary safety.
	unsigned int m_spOverlapPreviewUnsafeCount = 0;
	unsigned int m_spOverlapPreviewInsideSampleCount = 0;
	bool m_spOverlapPreviewSensorReady = false;
	unsigned int m_spMirrorOverlapPreviewUnsafeCount = 0;
	unsigned int m_spMirrorOverlapPreviewInsideSampleCount = 0;
	bool m_spMirrorOverlapPreviewSensorReady = false;
	bool m_spOverlapPreviewMirrorRequired = false;
};

#endif
