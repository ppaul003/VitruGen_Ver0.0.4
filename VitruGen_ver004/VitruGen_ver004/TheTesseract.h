#ifndef _THE_TESSERACT_H_
#define _THE_TESSERACT_H_

#include <GL/glew.h>
#include <cstddef>
#include <vector>

#include <vector_types.h>

#include "TheArbiter.h"
#include "particleSystem.h"
#include "renderer_Euclid.h"

struct cudaGraphicsResource;
class MarchingCubes;

class Tesseract {
public:
	enum TesseractAnimMode {
		ANIM_MODE_IDLE_PREVIEW = 0,
		ANIM_MODE_3D_GRID_STABLE
	};
	enum TesseractAnimTransition {
		ANIM_TRANS_NONE = 0,

		// MEALY
		ANIM_TRANS_IDLE_TO_3D_GRID,
		ANIM_TRANS_3D_GRID_TO_IDLE
	};
	enum TesseractAnimPhase {
		ANIM_PHASE_NONE = 0,

		// Forward transition
		ANIM_PHASE_ORIENT_TO_3D,
		ANIM_PHASE_CAMERA_FOCUS_TO_3D,

		// Reverse transition
		ANIM_PHASE_RETURN_TO_IDLE
	};
	///////////////////////////////////////////////////////////////////////
	using WorkspaceId =
		TheArbiter::WorkspaceId;
	struct WorkspaceUpdateContext {
		bool paused = false;
		float timestep = 0.0f;
		int iterations = 1;

		float damping = 1.0f;
		float gravity = 0.0f;
		float collideSpring = 0.0f;
		float collideAttraction = 0.0f;
		float simBox = 4.0f;

		float* simTime = nullptr;
	};
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

	// -- SINGLE_PARTICLE_MCAD -- ///////////////////////////////////////////////////////////////////
	struct InjectionVector {
		int dx;
		int dy;
		int dz;
		const char* name;
	};
	struct VolumeCadOp {
		enum Type { ADD, SUBTRACT };

		Type type;
		TheArbiter::VolumePrimitive primitive;

		float3 position;
		float3 scale;
		float3 rotationDeg;

		int injectionVectorINdex;
		float injectionT;

	};
	struct SPVolumeBasis {
		float3 xAxis;
		float3 yAxis;
		float3 zAxis;
	};
	struct SPWorkspaceInstance {
		WorkspaceId id =
			WorkspaceId::SINGLE_PARTICLE_MCAD;

		bool initialized = false;
		bool sharedResourcesBound = false;
	};

	// -- LINKED_PARTICLE_MCAD -- ///////////////////////////////////////////
	struct LPWorkspaceInstance {
		WorkspaceId id =
			WorkspaceId::LINKED_PARTICLES_MCAD;

		bool initialized = false;
	};

	///////////////////////////////////////////////////////////////////////
	struct PSRuntimeState {
		bool paused = true;
		float elapsedSimulationTime = 0.0f;
	};
	struct PSSimulationConfig {

		float fixedTimestep = 0.002f;
		int solverIterations = 1;

		float globalDamping = 1.0f;
		float gravityMagnitude = 0.0f;

		float collisionSpring = 0.0f;
		float collisionDamping = 0.02f;
		float collisionShear = 0.1f;
		float collisionAttraction = 0.0f;
		float simulationBoxSize = 4.0f;
	};
	struct PSWorkspaceInstance {
		WorkspaceId id =
			WorkspaceId::PARTICLE_SIMULATION;

		bool initialized = false;
		bool sharedResourcesBound = false;

		PSSimulationConfig config;
		PSRuntimeState runtime;
	};

	Tesseract();
	~Tesseract();

	void captureModelView() { glGetFloatv(GL_MODELVIEW_MATRIX, m_modelView); }
	void bindVolume(float* dVolume) { m_dWorkingVolume = dVolume; m_volumeDirty = true; m_volumeBoundarySensorReady = false; }
	void clearVolumeBinding() { m_dWorkingVolume = nullptr; m_volumeDirty = true; m_volumeBoundarySensorReady = false; }

	const float* getModelView() const { return m_modelView; };
	const int3& getVolumeSize() const { return m_volumeSize; }
	
	float getPreviewRotation() const { return m_previewRotation; }
	float getSliceAnimation() const { return m_sliceAnimation; }

	bool consumeCameraFocusReqest();
	bool isTransitioningTo3D() const { return m_animTransition != ANIM_TRANS_NONE; }
	bool isOrientingTo3D() const { return m_animPhase == ANIM_PHASE_ORIENT_TO_3D; }
	
	void beginAnimTransition(TesseractAnimTransition transition, float timeS);
	void updateAnimBehavior(float timeS);

	size_t getVolumeBytes() const;

	// --- WORKSPACE CONTAINER STUBS ---
	// Checkpoint B:
	// These are intentionally no-op hooks for now.
	// Behavior will be moved into these gradually in Checkpoints C/D.
	WorkspaceId getActiveWorkspace() const { return m_activeWorkspace; }

	void enterWorkspace(WorkspaceId workspace);
	void exitWorkspace();

	void updateActiveWorkspace(const WorkspaceUpdateContext& ctx);
	void renderActiveWorkspace(const WorkspaceRenderContext& ctx);

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

	////////////////////// --- SHARED PARTICLE RESOURCES --- ///////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////
	void bindSharedParticleResources(
		ParticleSystem* psystem,
		EuclidRenderer* renderer,
		std::vector<float>* radiusBuffer
	);

	////////////////////// --- PARTICLE_SIMULATION (PS) --- ////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////
	const PSSimulationConfig& getPSConfig() const { return m_PSWorkspace.config; }
	float getPSSimulationTime() const { return m_PSWorkspace.runtime.elapsedSimulationTime; }

	bool startPSWorkspace();
	bool togglePSPause();
	bool resetPSWorkspace(ParticleSystem::ParticleConfig config);
	bool stepPSWorkspace();

	bool isPSPaused() const { return m_PSWorkspace.runtime.paused; }
	bool isPSWorkspaceInitialized() const { return m_PSWorkspace.initialized; }
	bool isActiveWorkspacePaused() const;

	bool initializePSWorkspace();
	void renderPSWorkspace(const WorkspaceRenderContext& ctx);
	void updatePSWorkspace(const WorkspaceUpdateContext& ctx);

	void applyPSConfig();
	void syncPSRendering();
	////////////////////// --- PARTICLE_SIMULATION (PS) --- ////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////

	////////////////////// --- LINKED_PARTICLES_MCAD (LP) --- //////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////
	bool isLPWorkspaceInitialized() const { return m_LPWorkspace.initialized; }

	bool initializeLPWorkspace();
	void updateLPWorkspace(const WorkspaceUpdateContext& ctx);
	void renderLPWorkspace(const WorkspaceRenderContext& ctx);
	////////////////////// --- LINKED_PARTICLES_MCAD (LP) --- //////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// WORKSPACE BRANCH
	////////////////////// --- SINGLE_PARTICLE --- /////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////
	bool placeSPAnchor(float particleRadius);
	bool isSPWorkspaceIntialized() const { return m_SPWorkspace.initialized; }

	bool initializeSPWorkspace();
	void renderSPWorkspace(const WorkspaceRenderContext& ctx);
	void updateSPWorkspace(const WorkspaceUpdateContext& ctx);

	void applySPConfig(float particleRadius);
	void syncSPRendering();
	
	void clearSPPlacement() { m_placedSP = false; }
	// SP PIPELINE

	bool commitSPWorkingVolume(const TheArbiter& arbiter);
	bool hasVolume() const { return m_dWorkingVolume != nullptr; }
	bool hasCommittedGeometry() const { return m_hasCommittedGeometry; }
	bool hasCommittedVolume() const { return m_dBaseVolume != nullptr; }
	bool hasBrushVolume() const { return m_dBrushVolume != nullptr; }

	bool isPlacedSP() const { return m_placedSP; }
	bool isVolumeDirty() const { return m_volumeDirty; }

	float* getVolume() const { return m_dWorkingVolume; }
	float* getCommittedVolume() const { return m_dBaseVolume; }
	float* getBrushVolume() const { return m_dBrushVolume; }
	
	void bindCommittedVolume(float* dVolume);
	void bindBrushVolume(float* dVolume) { m_dBrushVolume = dVolume; m_volumeDirty = true; }

	void clearCommittedVolumeBinding();
	void clearSPCommittedVolume();
	void clearBrushVolumeBinding() { m_dBrushVolume = nullptr; m_volumeDirty = true; }
	
	void copyCommittedVolumeToPreview();

	void markVolumeDirty() { m_volumeDirty = true; m_volumeBoundarySensorReady = false; }
	void markVolumeClean() { m_volumeDirty = false; }
	
	void updateSPVolumePreview(const TheArbiter& arbiter);
	
	// -----------------------------------------------------------------------------
	// NODE_2 VOLUME BOUNDARY SENSOR
	// -----------------------------------------------------------------------------

	bool initializeSPVolumeBoundarySensor();
	bool updateSPVolumeBoundarySensor(float isoValue = 0.0f, float safetyBand = 0.0f);
	bool classifySPVolumeBoundaryForSource(
		const float* dSourceVolume,
		float isoValue,
		float safetyBand
	);
	bool isSPVolumeBoundarySensorReady() const { return m_volumeBoundarySensorReady; }
	bool isSPVolumeBoundarySafe() const { return m_volumeBoundarySensorReady && m_volumeBoundaryUnsafeCount == 0; }

	unsigned int getSPVolumeBoundaryUnsafeCount() const {return m_volumeBoundaryUnsafeCount;}
	unsigned int getSPVolumeBoundaryFaceStride() const {return m_volumeBoundaryFaceStride;}
	const std::vector<unsigned char>& getSPVolumeBoundaryMaskCPU() const {return m_volumeBoundaryMaskCPU;}

	////////////////////// --- SINGLE_PARTICLE MC COMPONENT --- /////////////////////////////////////////////////////
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

	void releaseSPVolumeBoundarySensor();
	void regenerateSPVolumeField(const TheArbiter& arbiter);
	void bindSPCadVolumeResource(struct cudaGraphicsResource** cudaPboResourceSlot);
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
	////////////////////// --- SINGLE_PARTICLE MC COMPONENT --- /////////////////////////////////////////////////////
	////////////////////// --- SINGLE_PARTICLE --- /////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////

private:
	// --- WORKSPACE UPDATE SIGNAL ---
	bool advanceParticleSimSTEP();
	bool updateSingleParticleMCAD();
	bool updateLinkedParticlesMCAD(); // Placeholder

	// --- WORKSPACE RENDERER BRANCH ---
	void renderSingleParticleWorkspace(const WorkspaceRenderContext& ctx);
	void renderLinkedParticlesWorkspace(const WorkspaceRenderContext& ctx); // Placeholder
	void renderParticleSimulation(EuclidRenderer::DisplayMode displayMode, bool displayEnabled);

	void renderSingleParticleMCAD(
		const TheArbiter& arbiter,
		float thetaRad,
		float phiRad,
		float zs
	);
	// --- PS MODE METHODS --- //


	// --- SP MODE METHODS --- //
	bool commitSPInjectionBoolean(const TheArbiter& arbiter);
	int getSPVolumePrimitiveId(const TheArbiter& arbiter) const;
	int getSPVolumePrimitiveIdFromState(const TheArbiter::VolumeObjectState& state) const;

	float3 buildSPVolumeOffset(const TheArbiter& arbiter) const;
	float3 buildSPVolumeOffsetFromState(const TheArbiter::VolumeObjectState& state) const;
	float3 buildSPVolumeRailBrushOffset(const TheArbiter& arbiter) const;

	float4 buildSPVolumePrimitiveParams(const TheArbiter& arbiter) const;
	float4 buildSPVolumePrimitiveParamsFromState(const TheArbiter::VolumeObjectState& state) const;
	
	SPVolumeBasis buildSPVolumeBasis(const TheArbiter& arbiter) const;
	SPVolumeBasis buildSPVolumeBasisFromState(
		const TheArbiter& arbiter, 
		const TheArbiter::VolumeObjectState& state) const;


	void generateSPVolume0Field(const TheArbiter& arbiter, float* dDestination);
	void generateSPVolume1BrushField(const TheArbiter& arbiter, float* dDestination);
	
	void markSPVolumeBoundarySafe();
	/////////////////////////////////////////////////////////////////

	void updateMealyAnimBehavior(float timeS);
	void updateMooreAnimBehavior(float timeS);
	void beginIdleTo3DGridTransition(float timeS);
	void begin3DGridToIdleTransition(float timeS);

private:
	TesseractAnimTransition m_animTransition = ANIM_TRANS_NONE;
	TesseractAnimMode m_animMode = ANIM_MODE_IDLE_PREVIEW;
	TesseractAnimPhase m_animPhase = ANIM_PHASE_NONE;

	WorkspaceId m_activeWorkspace = WorkspaceId::NONE;

	PSWorkspaceInstance m_PSWorkspace;
	LPWorkspaceInstance m_LPWorkspace;
	SPWorkspaceInstance m_SPWorkspace;

	// --- SINGLE_PARTICLE CAD STATE ---
	bool m_placedSP = false;
	bool m_cameraFocusRequested = false;
	bool m_volumeDirty = true;
	bool m_committedVolumeReady = false;
	// True only after at least one primitive has actually been baked.
	// This is different from m_committedVolumeReady, which only means
	// that the CUDA buffer has been initialized.
	bool m_hasCommittedGeometry = false;

	float m_transitionStartTime = 0.0f;
	float m_transitionDuration = 1.25f;
	float m_startPreviewRotation = 0.0f;
	float m_startSliceAnimation = 0.0f;
	float m_previewRotation = 0.0f;
	float m_sliceAnimation = 0.5f;
	float m_modelView[16]{};

	// --- VOLUME WORKSPACE DATA ---
	float* m_dWorkingVolume = nullptr;
	float* m_dBaseVolume = nullptr;
	float* m_dBrushVolume = nullptr;

	int3 m_volumeSize{ 128, 128, 128 };

	// -----------------------------------------------------------------------------
	// Node_2 boundary-contact sensor.
	//
	// Device mask layout:
	//
	//     face * m_volumeBoundaryFaceStride + localPatch
	//
	// Face order matches VolumeBoundaryFaceId in kernel.h.
	// -----------------------------------------------------------------------------

	unsigned char* m_dVolumeBoundaryMask = nullptr;
	unsigned int* m_dVolumeBoundaryUnsafeCount = nullptr;
	std::vector<unsigned char> m_volumeBoundaryMaskCPU;

	unsigned int m_volumeBoundaryFaceStride = 0;
	unsigned int m_volumeBoundaryUnsafeCount = 0;

	bool m_volumeBoundarySensorReady = false;

	// --- SHARED PARTICLE WORKSPACE RESOURCES ---
	// PS and SP currently consume the same engine-owned ParticleSystem,
	// renderer, and radius buffer. These remain shared services until a
	// later Gate 2 checkpoint defines their final ownership contract.
	ParticleSystem* m_particleSystem = nullptr;
	EuclidRenderer* m_renderer = nullptr;
	std::vector<float>* m_particleRadii = nullptr;

	// --- SINGLE_PARTICLE VOLUME RENDER RESOURCE ---
	struct cudaGraphicsResource** m_cudaPboResourceSlot = nullptr;
};

#endif
