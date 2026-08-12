#ifndef __ARBITER_SYS_H__
#define __ARBITER_SYS_H__

#include "Interactions.h"
#include "TextEntry.h"

class TheArbiter {
public:
	// --- CANONICAL APPLICATION NAVIGATION ---
	enum class ApplicationLayer {
		GLOBAL_SHELL = 0,
		DOMAIN_SELECTION,
		WORKSPACE_CONFIGURATION,
		ACTIVE_WORKSPACE,
		COUNT
	};

	enum class GlobalShellSelection {
		IDLE = 0,
		WORKSPACE_DOMAINS,
		COUNT
	};

	enum class WorkspaceDomain {
		NONE = 0,
		GRID_2D,
		GRID_3D,
		SIMCAD_4D,
		COUNT
	};

	enum class WorkspaceId {
		NONE = 0,

		// GRID_2D
		GRAPH_2D,
		TEXTURE_MAP_2D,
		SPRITE_PROJECTION_2D,

		// GRID_3D
		GRAPH_3D,
		SINGLE_PARTICLE_MCAD,
		LINKED_PARTICLES_MCAD,

		// SIMCAD_4D
		PARTICLE_SIMULATION,
		NBODY_SIM,
		FLUID_SIM,
		CUDA_CAD,
		SANDBOX_SIM,

		COUNT
	};

	enum class WorkspaceAvailability {
		AVAILABLE,
		EXPERIMENTAL,
		RESERVED
	};

	enum class ParticleSimLayer1Item {
		Workspace = 0,
		GridLayout,
		ColorMode,
		RadiusMode,
		Configure,
		Count
	};

	enum class SingleParticleLayer1Item {
		Workspace = 0,
		ParticleType,
		Configure,
		Count
	};

	enum class SingleParticleObjectType {
		Static = 0,
		Composite,
		Atomic,
		Count
	};

	enum class TextureMapLayer1Item {
		Workspace = 0,
		TargetStaticParticle,
		LoadTargetAsset,
		Configure,
		Count
	};

	enum class TextureMapLayer2Item {
		PreviewParticleRadius = 0,
		Count
	};

	enum class TextEntryTarget {
		None = 0,

		ParticleDefaultCount,
		ParticleRedCount,
		ParticleGreenCount,
		ParticleBlueCount,

		SingleParticleAssetName
	};

	struct WorkspaceDescriptor {
		WorkspaceId id;
		WorkspaceDomain domain;
		WorkspaceAvailability availability;
		const char* canonicalName;
	};

	struct DomainWorkspaceSelections {
		WorkspaceId grid2D = WorkspaceId::GRAPH_2D;
		WorkspaceId grid3D = WorkspaceId::GRAPH_3D;
		WorkspaceId simcad4D = WorkspaceId::PARTICLE_SIMULATION;
	};

	struct NavigationState {
		ApplicationLayer layer = ApplicationLayer::GLOBAL_SHELL;
		GlobalShellSelection globalShellSelection = GlobalShellSelection::IDLE;
		WorkspaceDomain selectedDomain = WorkspaceDomain::GRID_3D;
		DomainWorkspaceSelections workspaceSelections;
	};

	static const WorkspaceDescriptor& describeWorkspace(WorkspaceId workspace);
	static WorkspaceDomain getWorkspaceDomain(WorkspaceId workspace);
	static WorkspaceAvailability getWorkspaceAvailability(WorkspaceId workspace);
	static bool workspaceBelongsToDomain(
		WorkspaceId workspace,
		WorkspaceDomain domain
	);

	// --- PARTICLE WORKSPACE CONFIGURATION ---
	enum ParticleColorSelection {
		PARTICLE_COLOR_RED = 0,
		PARTICLE_COLOR_BLUE = 1,
		PARTICLE_COLOR_GREEN = 2
	};

	enum ParticleResetMode {
		PARTICLE_RESET_DEFAULT = 0,
		PARTICLE_RESET_RANDOM = 1
	};

	enum class ParticleGridLayout {
		None = 0,
		Minimal,
		Full,
		Dynamic,
		Count
	};

	enum class ParticleColorMode {
		Default = 0,
		RGB,
		Count
	};

	enum class ParticleRadiusMode {
		Uniform = 0,
		Random,
		Count
	};

	enum class ParticleColorChannel {
		Red = 0,
		Green,
		Blue,
		Count
	};

	enum class ParticleSimResetMode {
		Default = 0,
		Random,
		Count
	};

	

	struct ParticleSimDraftConfig {
		ParticleGridLayout gridLayout =
			ParticleGridLayout::Full;

		ParticleColorMode colorMode =
			ParticleColorMode::Default;

		ParticleRadiusMode radiusMode =
			ParticleRadiusMode::Uniform;

		unsigned int defaultParticleCount = 4200;

		unsigned int redCount = 0;
		unsigned int greenCount = 0;
		unsigned int blueCount = 0;

		ParticleColorChannel selectedColorChannel =
			ParticleColorChannel::Red;

		ParticleSimResetMode resetMode =
			ParticleSimResetMode::Default;

		float uniformRadius = 0.0120f;
		float minimumRadius = 0.0098f;
		float maximumRadius = 0.0156f;
	};

	enum ParticleConfigList {
		PARTICLE_LIST_COLOR = 0,

		// In SINGLE_PARTICLE_MCAD, list 2 means radius.
		// In PARTICLE_SIM mode, list 2 still acts as reset mode.
		PARTICLE_LIST_RADIUS = 1,
		PARTICLE_LIST_RESET = PARTICLE_LIST_RADIUS,

		PARTICLE_LIST_RENDER_MODE = 2,
		PARTICLE_LIST_RUN = 3,
		PARTICLE_LIST_COUNT = 4
	};

	// --- SINGLE_PARTICLE_MCAD WORKFLOW ---
	enum ObjectEditMode {
		EDIT_SCALE_WHOLE = 0,
		EDIT_SCALE_Z,
		EDIT_SCALE_Y,
		EDIT_SCALE_X
	};

	enum ObjectRotationMode {
		ROTATE_PITCH = 0,
		ROTATE_YAW,
		ROTATE_ROLL
	};

	enum ObjectTransformMode {
		TRANSFORM_SCALE = 0,
		TRANSFORM_ROTATION
	};

	enum OffsetVector {
		OFFSET_VECTOR_X = 0,
		OFFSET_VECTOR_Y,
		OFFSET_VECTOR_Z,
		OFFSET_VECTOR_COUNT
	};

	enum ParticleRenderMode {
		PARTICLE_RENDER_DEFAULT = 0,
		PARTICLE_RENDER_MESH = 1
	};

	// ---------------------------------------------------------
	// SINGLE_PARTICLE authoring source.
	//
	// ProceduralVolume:
	//     The asset is backed by the CUDA scalar-field workflow.
	//
	// LoadedStaticMesh:
	//     The asset is backed by an imported or saved indexed mesh.
	//     A scalar field may or may not also be available.
	// ---------------------------------------------------------
	enum class SPAuthoringSource {
		ProceduralVolume = 0,
		LoadedStaticMesh
	};

	enum SingleParticleSubLayer {
		SP_SUB_LAYER_REFERENCE = 0,
		SP_SUB_LAYER_SHAPE_EDIT,
		SP_SUB_LAYER_VOLUME_RENDER,
		SP_SUB_LAYER_MARCHING_CUBES,
		SP_SUB_LAYER_COUNT
	};

	// ---------------------------------------------------------
	// SINGLE_PARTICLE collision and rendering setup state.
	// ---------------------------------------------------------
	enum class SPCollisionShape {
		Sphere = 0,
		Block,
		Capsule,
		Cone,
		DeformableSphere,
		Count
	};

	enum class SPMeshBoundMode {
		Default = 0,
		Fill,
		Count
	};

	enum class SPDisplayMode {
		Render = 0,
		RenderAndCollision,
		Wireframe,
		Count
	};

	// ---------------------------------------------------------
	// Sub-Layer 0 panel:
	//     Particle selection and collision setup.
	// ---------------------------------------------------------
	enum SPSubLayer0PanelItem {
		SP0_LIST_COLLISION_SHAPE = 0,
		SP0_LIST_LOAD_STATIC_PARTICLE,
		SP0_LIST_SAVE_ACTIVE_PARTICLE,
		SP0_LIST_RENDERING_SETUP,
		SP0_LIST_COUNT
	};

	// ---------------------------------------------------------
	// Sub-Layer 1 panel:
	//     Rendering setup.
	// ---------------------------------------------------------
	enum SPSubLayer1PanelItem {
		SP1_LIST_RENDER_SOURCE = 0,
		SP1_LIST_MESH_BOUND,
		SP1_LIST_DISPLAY_MODE,
		SP1_LIST_RENDER_CAGE,
		SP1_LIST_MESH_PREVIEW_EDIT,
		SP1_LIST_COLLISION_SETUP,
		SP1_LIST_COUNT
	};

	enum VolumePrimitive {
		VOLUME_PRIMITIVE_BASE = 0,
		VOLUME_PRIMITIVE_SPHERE,
		VOLUME_PRIMITIVE_TORUS,
		VOLUME_PRIMITIVE_BLOCK,
		VOLUME_PRIMITIVE_CYLINDER,
		VOLUME_PRIMITIVE_CONE,
		VOLUME_PRIMITIVE_CAPSULE,
		VOLUME_PRIMITIVE_WEDGE,
		VOLUME_PRIMITIVE_DELTA_WING,
		VOLUME_PRIMITIVE_FRUSTUM,

		VOLUME_PRIMITIVE_COUNT
	};

	enum PreviewPanelItem {
		PREVIEW_LIST_INJECTION_MODE = 0,
		PREVIEW_LIST_EDIT_OBJECT,
		PREVIEW_LIST_RUN_MC,
		PREVIEW_LIST_COUNT
	};

	enum EditObjectPanelItem {
		EDIT_LIST_OBJECT = 0,
		EDIT_LIST_ROTATION_INCREMENT,
		EDIT_LIST_OFFSET_OBJECT,
		EDIT_LIST_PREVIEW_OBJECT,
		EDIT_LIST_COUNT
	};

	enum InjectionEditObjectPanelItem {
		// Dynamic Node_1 panel, used when Injection Voxels != None.
		INJECTION_EDIT_LIST_TARGET = 0,
		INJECTION_EDIT_LIST_OBJECT = 1,

		// VOXEL_0 layout:
		INJECTION_EDIT_LIST_ROTATION_INCREMENT = 2,
		INJECTION_EDIT_LIST_OFFSET_OBJECT = 3,
		INJECTION_EDIT_LIST_PREVIEW_OBJECT = 4,
		INJECTION_EDIT_LIST_VOXEL0_COUNT = 5,

		// VOXEL_1 layout:
		INJECTION_EDIT_LIST_COMMIT_BASE = 2,
		INJECTION_EDIT_LIST_MIRROR = 3,
		INJECTION_EDIT_LIST_VOXEL1_COUNT = 4
	};

	enum OffsetObjectPanelItem {
		OFFSET_LIST_VECTOR = 0,
		OFFSET_LIST_DISTANCE,
		OFFSET_LIST_APPLY_TO_BASE,
		OFFSET_LIST_EDIT_OBJECT,
		OFFSET_LIST_COUNT
	};

	enum InjectionOffsetObjectPanelItem {
		// Dynamic Node_2 panel, used when Injection Voxels != None.
		INJECTION_OFFSET_LIST_TARGET = 0,
		INJECTION_OFFSET_LIST_VECTOR = 1,
		INJECTION_OFFSET_LIST_DISTANCE = 2,

		// VOLUME_0 layout:
		INJECTION_OFFSET_LIST_RAIL = 3,
		INJECTION_OFFSET_LIST_APPLY_TO_BASE = 4,
		INJECTION_OFFSET_LIST_EDIT_OBJECT = 5,
		INJECTION_OFFSET_LIST_VOXEL0_COUNT = 6,

		// VOLUME_1 layout:
		INJECTION_OFFSET_LIST_MODE = 3,
		INJECTION_OFFSET_LIST_VOXEL1_COUNT = 4
	};

	enum VolumeAssemblyNode {
		VOLUME_NODE_PREVIEW = 0,
		VOLUME_NODE_EDIT_OBJECT,
		VOLUME_NODE_OFFSET_OBJECT,
		VOLUME_NODE_APPLY_TO_BASE,
		VOLUME_NODE_COUNT
	};

	enum VolumeInjectionVoxel {
		INJECTION_VOXEL_NONE = 0,
		INJECTION_VOXEL_211, // +X
		INJECTION_VOXEL_121, // +Y
		INJECTION_VOXEL_011, // -X
		INJECTION_VOXEL_112, // +Z
		INJECTION_VOXEL_101, // -Y
		INJECTION_VOXEL_110, // -Z

		// Expanded edge/corner route.
		// Edge-adjacent voxels.
		INJECTION_VOXEL_120, // +Y/-Z
		INJECTION_VOXEL_221, // +X/+Y
		INJECTION_VOXEL_122, // +Y/+Z
		INJECTION_VOXEL_021, // -X/+Y
		INJECTION_VOXEL_201, // +X/-Y
		INJECTION_VOXEL_100, // -Y/-Z
		INJECTION_VOXEL_001, // -X/-Y
		INJECTION_VOXEL_102, // -Y/+Z

		INJECTION_VOXEL_010, // -X/-Z
		INJECTION_VOXEL_210, // +X/-Z
		INJECTION_VOXEL_012, // -X/+Z
		INJECTION_VOXEL_212, // +X/+Z

		// Corner-adjacent voxels.
		INJECTION_VOXEL_202, // +X/-Y/+Z
		INJECTION_VOXEL_020, // -X/+Y/-Z
		INJECTION_VOXEL_220, // +X/+Y/-Z
		INJECTION_VOXEL_002, // -X/-Y/+Z
		INJECTION_VOXEL_200, // +X/-Y/-Z
		INJECTION_VOXEL_022, // -X/+Y/+Z
		INJECTION_VOXEL_222, // +X/+Y/+Z
		INJECTION_VOXEL_000, // -X/-Y/-Z

		INJECTION_VOXEL_COUNT
	};

	enum VolumeInjectionMode {
		VOLUME_FUSE = 0,
		VOLUME_CUT,
		VOLUME_MODE_COUNT
	};

	enum VolumeEditTarget {
		VOLUME_EDIT_TARGET_VOXEL_0 = 0,
		VOLUME_EDIT_TARGET_VOXEL_1,
		VOLUME_EDIT_TARGET_COUNT
	};

	enum SPOverlapPreviewStatus {
		SP_OVERLAP_POSITION_IN_NODE_2 = 0,
		SP_OVERLAP_OUTSIDE_CAGE,
		SP_OVERLAP_ACTIVE
	};

	enum SPMirrorMode {
		SP_MIRROR_NONE = 0,
		SP_MIRROR_ON,
		SP_MIRROR_COUNT
	};

	enum ApplyToBasePanelItem {
		APPLY_LIST_COMMIT = 0,
		APPLY_LIST_OFFSET_OBJECT,
		APPLY_LIST_CANCEL_TO_PREVIEW,
		APPLY_LIST_COUNT
	};

	enum MarchingCubesPanelItem {
		MC_LIST_SAVE_STATIC_PARTICLE = 0,
		MC_LIST_SAVE_STATIC_PARTICLE_AS,
		MC_LIST_EXPORT_OBJ,
		MC_LIST_TO_SUB_LAYER_2,
		MC_LIST_TO_SUB_LAYER_0,
		MC_LIST_COUNT
	};

	enum ArbiterCommand {
		CMD_NONE = 0,
		CMD_EXIT,
		CMD_REDRAW,
		CMD_TOGGLE_PAUSE,
		CMD_STEP_SIMULATION,
		CMD_START_PARTICLE_SIMULATION,
		CMD_PLACE_SINGLE_PARTICLE,
		CMD_PARTICLE_CONFIG_CHANGED,
		CMD_PARTICLE_RADIUS_CHANGED,
		CMD_PARTICLE_RENDER_MODE_CHANGED,
	};

	struct BasisVector {
		float x;
		float y;
		float z;
	};

	struct ObjectBasis {
		BasisVector xAxis;
		BasisVector yAxis;
		BasisVector zAxis;
	};

	struct VolumeObjectState {
		TheArbiter::VolumePrimitive primitive =
			TheArbiter::VOLUME_PRIMITIVE_SPHERE;

		// For VOLUME_1 only:
		// When Commit Brush Base is pressed, primitive becomes BASE for UI,
		// but this remembers the real procedural brush to render.
		TheArbiter::VolumePrimitive brushBasePrimitive =
			TheArbiter::VOLUME_PRIMITIVE_SPHERE;

		bool brushBaseReady = false;

		TheArbiter::ObjectBasis basis{
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f }
		};

		float scaleWhole = 1.0f;
		float scaleX = 1.0f;
		float scaleY = 1.0f;
		float scaleZ = 1.0f;

		float pitchDeg = 0.0f;
		float yawDeg = 0.0f;
		float rollDeg = 0.0f;

		float offsetX = 0.0f;
		float offsetY = 0.0f;
		float offsetZ = 0.0f;
	};

	struct ArbiterResult {
		ArbiterCommand command = CMD_NONE;
		bool requestRedraw = false;
		bool regenerateVolume = false;
		bool rebuildMenu = false;

		bool enterMarchingCubes = false;
		bool exportObjRequested = false;

		bool saveStaticParticleAsRequested = false;
		bool loadStaticParticleRequested = false;
		std::string staticParticleAssetName;

		// TEXTURE_MAP_2D Layer 1 intents.
		//
		// TheArbiter owns user intent only.
		// Asset loading, validation, catalog handling, and
		// Layer transitions belong to EuclidEngine / workspace logic.
		bool loadTextureMapTargetRequested = false;
		bool configureTextureMapTargetRequested = false;

		// Layer 1 Row [2]:
		// previous / next OUTPUT Static Particle.
		int textureMapCatalogStep = 0;

		// Layer 2 Row [1]:
		// decrease / increase preview radius.
		int textureMapPreviewRadiusStep = 0;

		// Volume CAD action.
		bool commitVolumeFuse = false;
	};

	static constexpr float kParticleWorldBoundary = 1.0f;
	static constexpr float kParticleGridDim = 64.0f;
	static constexpr float kParticleCellSize = (2.0f * kParticleWorldBoundary) / kParticleGridDim;
	static constexpr float kParticleRadiusMax = 0.5f * kParticleCellSize;
	static constexpr float kParticleRadiusMin = 0.25f * kParticleRadiusMax;
	static constexpr float kParticleRadiusDefault = 0.5f * (kParticleRadiusMin + kParticleRadiusMax);
	static constexpr float kParticleRadiusStep = (kParticleRadiusMax - kParticleRadiusMin) / 16.0f;
	static constexpr unsigned int kParticleSimCapacity = 16384;

	TheArbiter();
	~TheArbiter() = default;

	// --- SINGLE_PARTICLE_MCAD STATE QUERIES ---
	ObjectEditMode getObjectEditMode() const { return m_objectEditMode; }
	ObjectRotationMode getObjectRotationMode() const { return m_objectRotationMode; }
	ObjectTransformMode getObjectTransformMode() const { return m_objectTransformMode; }

	const VolumeObjectState& getVolume0State() const { return m_volume0State; }
	const VolumeObjectState& getVolume1State() const { return m_volume1State; }
	const VolumeObjectState& getActiveVolumeState() const;

	float getRotationPitchDeg() const { return getActiveVolumeState().pitchDeg; }
	float getRotationYawDeg() const { return getActiveVolumeState().yawDeg; }
	float getRotationRollDeg() const { return getActiveVolumeState().rollDeg; }
	float getVolumeScaleWhole() const { return getActiveVolumeState().scaleWhole; }
	float getVolumeScaleX() const { return getActiveVolumeState().scaleX; }
	float getVolumeScaleY() const { return getActiveVolumeState().scaleY; }
	float getVolumeScaleZ() const { return getActiveVolumeState().scaleZ; }
	float getEffectiveVolumeScaleX() const;
	float getEffectiveVolumeScaleY() const;
	float getEffectiveVolumeScaleZ() const;
	float getHoverX() const { return m_hoverX; }
	float getHoverY() const { return m_hoverY; }
	float getOffsetIncrement() const { return m_offsetIncrement; }
	float getOffsetX() const { return getActiveVolumeState().offsetX; }
	float getOffsetY() const { return getActiveVolumeState().offsetY; }
	float getOffsetZ() const { return getActiveVolumeState().offsetZ; }
	float getInjectionRailT() const { return m_injectionRailT; }
	float getParticleRadius() const { return m_particleRadius; }

	// --- COMMAND / MENU ENTRY POINTS ---
	ArbiterResult processKeyboard(const KeyboardInput::KeyEvent& event);
	ArbiterResult handleTextEntryKeyboard(const KeyboardInput::KeyEvent& event);

	// --- SP_MCAD / MENU ENTRY POINTS ---
	ArbiterResult enterSPRenderingSetupFromMenu();
	ArbiterResult activateSPPrimaryActionFromMenu();
	ArbiterResult returnSPToLayer2FromMenu();
	ArbiterResult setSPCollisionShapeFromMenu(SPCollisionShape shape);
	ArbiterResult trySelectParticleAtCurrentSlice();
	ArbiterResult returnSPCollisionSetupFromMenu();
	ArbiterResult setSPRenderSourceFromMenu(ParticleRenderMode mode);
	ArbiterResult setSPMeshBoundModeFromMenu(SPMeshBoundMode mode);
	ArbiterResult setSPDisplayModeFromMenu(SPDisplayMode mode);
	ArbiterResult setSPRenderCageVisibleFromMenu(bool visible);
	ArbiterResult enterSPVolumePreviewFromMenu();
	ArbiterResult setVolumeAssemblyNode(VolumeAssemblyNode node);
	ArbiterResult setOffsetVectorSelection(OffsetVector vector);
	ArbiterResult clearObjectOffsetFromMenu();
	ArbiterResult setVolumePrimitiveFromMenu(VolumePrimitive primitive);
	ArbiterResult commitObjectBasisAndReturnToPreview();
	ArbiterResult toggleVolumeInjectionModeFromMenu();
	ArbiterResult setSPMirrorModeFromMenu(SPMirrorMode mode);
	ArbiterResult commitBrushBaseFromMenu();
	ArbiterResult enterMarchingCubesFromPreview();
	ArbiterResult activateMarchingCubesPanelItemFromMenu(MarchingCubesPanelItem item);

	// --- TM_2D / MENU ENTRYPOINTS ---
	ArbiterResult enterTextureMapLayer2FromMenu();

	// --- SINGLE_PARTICLE AUTHORING SOURCE ---
	void activateLoadedStaticParticleBase(bool hasEditableVolume);
	void activateProceduralVolumeAuthoring();

	// --- CANONICAL NAVIGATION QUERIES ---
	ApplicationLayer getApplicationLayer() const { return m_navigation.layer; }
	WorkspaceDomain getSelectedDomain() const { return m_navigation.selectedDomain; }
	WorkspaceId getSelectedWorkspace() const;
	WorkspaceId getWorkspaceSelection(WorkspaceDomain domain) const;

	TextureMapLayer1Item getTextureMapLayer1Selection() const { return m_textureMapLayer1Selection; }
	TextureMapLayer2Item getTextureMapLayer2Selection() const { return m_textureMapLayer2Selection; }

	SingleParticleLayer1Item getSingleParticleLayer1Selection() const { return m_singleParticleLayer1Selection; }
	SingleParticleObjectType getSingleParticleObjectType() const { return m_singleParticleObjectType; }
	ParticleColorSelection getParticleColorSelection() const { return m_particleColorSelection; }
	
	ParticleResetMode getParticleResetMode() const { return m_particleResetMode; }
	ParticleConfigList getActiveParticleConfigList() const { return m_activeParticleConfigList; }
	const ParticleSimDraftConfig& getParticleSimDraftConfig() const { return m_particleSimDraftConfig; }

	ParticleSimLayer1Item getParticleSimLayer1Selection() const { return m_particleSimLayer1Selection; }
	int getParticleSimLayer2Selection() const { return m_particleSimLayer2Selection; }
	int getParticleSimLayer2RowCount() const;
	unsigned int getParticleSimRGBTotal() const;

	VolumeAssemblyNode getVolumeAssemblyNode() const { return m_volumeAssemblyNode; }
	VolumeInjectionMode getVolumeInjectionMode() const { return m_volumeInjectionMode; }

	SingleParticleSubLayer getSingleParticleSubLayer() const { return m_singleParticleSubLayer; }
	VolumePrimitive getVolumePrimitiveSelection() const { return getActiveVolumeState().primitive; }

	BasisVector normalizeBasisVector(const BasisVector& v) const;
	BasisVector transformByBasis(const ObjectBasis& basis, const BasisVector& localVector) const;
	BasisVector rotateLocalVectorXYZ(const BasisVector& vector, float pitchDeg, float yawDeg, float rollDeg) const;

	OffsetVector getOffsetVectorSelection() const { return m_offsetVectorSelection; }

	const ObjectBasis& getObjectBasis() const { return getActiveVolumeState().basis; }

	ObjectBasis getEffectiveObjectBasis() const;
	ObjectBasis orthonormalizeBasis(const ObjectBasis& basis) const;
	VolumePrimitive getResolvedVolumePrimitiveSelection() const;
	VolumeInjectionVoxel getMirroredInjectionVoxel() const;

	int getInjectionVoxelDX() const;
	int getInjectionVoxelDY() const;
	int getInjectionVoxelDZ() const;
	
	void getMirroredInjectionDirection(int& dx, int& dy, int& dz) const;
	int getActiveSubLayerPanelItem() const { return m_activeSubLayerPanelItem; }
	int getWorkplaneSlice() const { return m_workplaneSlice; }
	int getActiveSubLayerPanelItemCount() const;
	int getRotationAngleIncrementDeg() const;

	bool isMenuLayer() const { return m_navigation.layer == ApplicationLayer::GLOBAL_SHELL; }
	bool isEnvironmentConfigLayer() const { return m_navigation.layer == ApplicationLayer::DOMAIN_SELECTION; }
	bool isParticleConfigLayer() const { return m_navigation.layer == ApplicationLayer::WORKSPACE_CONFIGURATION; }
	bool isSimulationRunLayer() const { return m_navigation.layer == ApplicationLayer::ACTIVE_WORKSPACE; }
	bool isIdleSelected() const { return m_navigation.globalShellSelection == GlobalShellSelection::IDLE; }
	bool isWorkspaceDomainsSelected() const { return m_navigation.globalShellSelection == GlobalShellSelection::WORKSPACE_DOMAINS; }
	bool isTextEntryActive() const { return m_textEntry.isActive(); }
	bool isStaticParticleObjectType() const { return m_singleParticleObjectType == SingleParticleObjectType::Static; };
	
	bool isSingleParticleSelected() const { return getSelectedWorkspace() == WorkspaceId::SINGLE_PARTICLE_MCAD; }
	bool isParticleSimulationSelected() const { return getSelectedWorkspace() == WorkspaceId::PARTICLE_SIMULATION; }

	bool isTextureMapLayer1PanelContext() const;
	bool isTextureMapLayer2PanelContext() const;

	bool isSingleParticleLayer1PanelContext() const;

	bool isParticleSimLayer1PanelContext() const;
	bool isParticleSimLayer2RunSelected() const;
	
	bool isVolumeBoundarySensorReady() const { return m_volumeBoundarySensorReady; }
	bool isVolumeBoundarySafe() const { return m_volumeBoundarySensorReady && m_volumeBoundaryUnsafeCount == 0; }
	
	bool isSPOverlapPreviewActive() const { return m_spOverlapPreviewStatus == SP_OVERLAP_ACTIVE; }
	bool isEditingInjectionVoxel0() const { return m_volumeEditTarget == VOLUME_EDIT_TARGET_VOXEL_0; }
	bool isEditingInjectionVoxel1() const { return m_volumeEditTarget == VOLUME_EDIT_TARGET_VOXEL_1; }
	bool setVolumeBoundaryStatus(bool sensorReady, unsigned int unsafeCount);
	bool setSPOverlapPreviewStatus(SPOverlapPreviewStatus status);
	bool isSingleParticleReferenceSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_REFERENCE; }
	bool isShapeEditSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_SHAPE_EDIT; }
	bool isVolumeRenderSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER; }
	bool isMarchingCubesSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_MARCHING_CUBES; }
	bool isParticleRenderMesh() const { return m_particleRenderMode == PARTICLE_RENDER_MESH; }

	bool isProceduralVolumeSource() const { return m_spAuthoringSource == SPAuthoringSource::ProceduralVolume; }
	bool isLoadedStaticMeshSource() const { return m_spAuthoringSource == SPAuthoringSource::LoadedStaticMesh; }
	bool loadedStaticMeshHasEditableVolume() const { return m_loadedStaticMeshHasEditableVolume; }
	bool isLoadedStaticMeshOnly() const { return isLoadedStaticMeshSource() && !m_loadedStaticMeshHasEditableVolume; }
	bool isSubLayerPanelOpen() const { return m_subLayerPanelOpen; }
	bool hasSelectedParticle() const { return m_selectedParticle; }
	bool hasHover() const { return m_hoverValid; }
	bool hasEditableVolumePrimitive() const { return getVolumePrimitiveSelection() != VOLUME_PRIMITIVE_BASE; }
	bool hasInjectionVoxelSelected() const { return m_volumeInjectionVoxel != INJECTION_VOXEL_NONE; }
	
	bool isSPMirrorEnabled() const { return hasInjectionVoxelSelected() && m_spMirrorMode == SP_MIRROR_ON; }
	bool canApplyVolumeToBase() const;
	bool isInjectionBrushBaseSelected() const;

	// The workplane and picking path are active only while:
	//     Sub-Layer 0
	//     selection is armed
	//     no particle has been selected yet
	//     the side panel is closed
	bool isWorkplaneParticleSelectSubLayer() const {
		return isSingleParticleReferenceSubLayer() &&
			m_spSelectionArmed &&
			!m_selectedParticle &&
			!m_subLayerPanelOpen;
	}

	// Every SP sub-layer may own a side panel, but only after
	// the anchor particle has been selected.
	bool isSubLayerPanelEligible() const {
		if (!hasSelectedParticle()) {
			return false;
		}

		return
			isSingleParticleReferenceSubLayer() ||
			isShapeEditSubLayer() ||
			isVolumeRenderSubLayer() ||
			isMarchingCubesSubLayer();
	}

	SPOverlapPreviewStatus getSPOverlapPreviewStatus() const { return m_spOverlapPreviewStatus; }
	SPAuthoringSource getSPAuthoringSource() const { return m_spAuthoringSource; }
	SPMirrorMode getSPMirrorMode() const { return m_spMirrorMode; }
	SPCollisionShape getSPCollisionShape() const {return m_spCollisionShape;}
	SPMeshBoundMode getSPMeshBoundMode() const {return m_spMeshBoundMode;}
	SPDisplayMode getSPDisplayMode() const {return m_spDisplayMode;}

	bool isSPSelectionArmed() const {return m_spSelectionArmed;}
	bool isSPRenderCageVisible() const {return m_spRenderCageVisible;}

	const char* getSelectedDomainDisplayName() const;
	const char* getSelectedWorkspaceDisplayName() const;
	const char* getSingleParticleObjectTypeName() const;
	const char* getSPCollisionShapeName() const;
	const char* getSPMeshBoundModeName() const;
	const char* getSPDisplayModeName() const;
	const char* getSPRenderSourceName() const;
	const char* getParticleColorName() const;
	const char* getParticleResetModeName() const;
	const char* getParticleGridLayoutName() const;
	const char* getParticleColorModeName() const;
	const char* getParticleRadiusModeName() const;
	const char* getParticleColorChannelName() const;
	const char* getParticleSimResetModeName() const;
	const char* getParticleRenderModeName() const;
	const char* getSingleParticleSubLayerName() const;
	const char* getVolumePrimitiveName() const;
	const char* getObjectEditModeName() const;
	const char* getObjectRotationModeName() const;
	const char* getObjectTransformModeName() const;
	const char* getVolumeAssemblyNodeName() const;
	const char* getVolumeInjectionVoxelName() const;
	const char* getVolumeInjectionModeName() const;
	const char* getVolumeEditTargetName() const;
	const char* getSPOverlapPreviewStatusName() const;
	const char* getSPMirrorModeName() const;
	const char* getVolumeEditTargetObjectName() const;
	const char* getOffsetVectorName() const;
	const char* getOffsetIncrementName() const;

	void setParticleRenderMode(ParticleRenderMode mode) { m_particleRenderMode = mode; }
	void updateHoverFromScreen(int x, int y, int w, int h);
	void finalizeVoxelBaseCommit();
	void beginSelectedRGBCountEntry(ArbiterResult& result);
	void beginSingleParticleAssetNameEntry(
		ArbiterResult& result,
		const std::string& initialName = "Static Particle");

	TextEntryMode getTextEntryMode() const { return m_textEntry.getMode(); }
	TextEntryTarget getTextEntryTarget() const { return m_textEntryTarget; }

	const std::string& getTextEntryPrompt() const { return m_textEntry.getPrompt(); }
	const std::string& getTextEntryBuffer() const { return m_textEntry.getBuffer(); }
	const std::string& getTextEntryStatusMessage() const { return m_textEntry.getStatusMessage(); }

	unsigned int getAvailableRGBCountForSelectedChannel() const;
	unsigned int getVolumeBoundaryUnsafeCount() const { return m_volumeBoundaryUnsafeCount; }

private:
	// --- NAVIGATION TRANSITIONS / INPUT ROUTING ---
	void setApplicationLayer(ApplicationLayer layer);

	void setWorkspaceSelection(
		WorkspaceDomain domain,
		WorkspaceId workspace
	);

	void validateNavigationState() const;

	void handleGlobalShellKeyboard(
		const KeyboardInput::KeyEvent& event,
		ArbiterResult& result
	);

	void handleDomainSelectionKeyboard(
		const KeyboardInput::KeyEvent& event,
		ArbiterResult& result
	);

	void handleWorkspaceConfigurationKeyboard(
		const KeyboardInput::KeyEvent& event,
		ArbiterResult& result
	);

	void applyCommittedTextEntry(ArbiterResult& result);
	void beginDefaultParticleCountEntry(ArbiterResult& result);

	// --- PARTICLE / WORKSPACE SELECTION ---
	void cycleGlobalShellSelection(int dir);
	void cycleWorkspaceSelection(int dir);
	void cycleParticleSimPanelWorkspace(int dir);
	void toggleParticleColorSelection();
	void toggleParticleResetMode();

	void moveParticleConfigCursorUp();
	void moveParticleConfigCursorDown();

	// TEXTURE_MAP_2D Layer 1 configuration.
	void moveTextureMapLayer1Cursor(int dir);
	void handleTextureMapLayer1Adjust(int dir, ArbiterResult& result);
	void activateTextureMapLayer1Item(ArbiterResult& result);
	// TEXTURE_MAP_2D Layer 2 configuration.
	void moveTextureMapLayer2Cursor(int dir);
	void handleTextureMapLayer2Adjust(int dir, ArbiterResult& result);

	// SINGLE_PARTICLE Layer 1 configuration.
	void moveSingleParticleLayer1Cursor(int dir);
	void cycleSingleParticleObjectType(int dir);
	void handleSingleParticleLayer1Adjust(int dir, ArbiterResult& result);
	void activateSingleParticleLayer1Item(ArbiterResult& result);

	// --- SINGLE_PARTICLE SETUP CONTROLS ---
	void handleSingleParticlePrimaryAction(ArbiterResult& result);

	void cycleSPCollisionShape(float dir);
	void cycleSPMeshBoundMode(float dir);
	void cycleSPDisplayMode(float dir);
	void toggleSPRenderCage();

	// PARTICLE_SIM layer config
	void moveParticleSimLayer1Cursor(int dir);
	void moveParticleSimLayer2Cursor(int dir);
	void handleParticleSimLayer1Adjust(int dir, ArbiterResult& result);
	void handleParticleSimLayer2Adjust(int dir, ArbiterResult& result);
	void activateParticleSimLayer1Item(ArbiterResult& result);
	void activateParticleSimLayer2Item(ArbiterResult& result);
	void requestParticleCountEntry(ArbiterResult& result);
	void clampParticleSimLayer2Selection();
	void adjustParticleSimDefaultCount(int dir);
	void cycleParticleSimGridLayout(int dir);
	void cycleParticleSimColorMode(int dir);
	void cycleParticleSimRadiusMode(int dir);
	void cycleParticleSimColorChannel(int dir);
	void cycleParticleSimResetMode(int dir);
	void adjustParticleSimUniformRadius(int dir);
	void adjustParticleSimMinimumRadius(int dir);
	void adjustParticleSimMaximumRadius(int dir);

	void toggleParticleRenderMode();

	// --- SINGLE_PARTICLE_MCAD VOLUME EDITING ---
	void increaseVolumePrimitiveScale();
	void decreaseVolumePrimitiveScale();

	void increaseParticleRadius();
	void decreaseParticleRadius();

	void cycleVolumePrimitiveSelection(float dir);
	void cycleRotationAngleIncrement(float dir);
	void cycleOffsetVectorSelection(float dir);
	void cycleOffsetIncrement(float dir);
	void cycleInjectionVoxelSelection(float dir);
	void cycleVolumeEditTarget(float dir);
	void cycleVolumeInjectionMode(float dir);
	void cycleSPMirrorMode(float dir);
	void adjustInjectionRail(float dir);

	VolumeObjectState& activeVolumeState();
	const VolumeObjectState& activeVolumeState() const;

	void resetVolumeState(
		VolumeObjectState& state,
		VolumePrimitive primitive = VOLUME_PRIMITIVE_SPHERE
	);

	void resetAllVolumeStates();

	void adjustObjectOffset(float dir);
	void resetObjectOffset();

	void setObjectEditMode(ObjectEditMode mode);
	void resetObjectScale();

	void setObjectRotationMode(ObjectRotationMode mode);
	void resetObjectRotation();

	void increaseObjectRotation();
	void decreaseObjectRotation();
	void resetObjectBasis();
	void commitObjectRotationToBasis();
	void commitBrushBase();

	// --- LAYER / SUB-LAYER TRANSITIONS ---
	void goBackOneLayer(ArbiterResult& result);
	void enterCurrentSelection(ArbiterResult& result);
	void advanceSingleParticleSubLayer(ArbiterResult& result);
	void retreatSingleParticleSubLayer(ArbiterResult& result);
	void adjustWorkplaneSlice(int delta, ArbiterResult& result);
	void handleParticleConfigAdjust(float dir, ArbiterResult& result);

	void toggleSubLayerPanel(ArbiterResult& result);
	void moveSubLayerPanelCursorUp(ArbiterResult& result);
	void moveSubLayerPanelCursorDown(ArbiterResult& result);
	void handleSubLayerPanelAdjust(float dir, ArbiterResult& result);
	void activateSubLayerPanelItem(ArbiterResult& result);

	bool isSubLayerPanelItemSelectable(int item) const;

	// Basis-vector primitives are implementation details used by the CAD
	// orientation and orthonormalization helpers.
	static float dotBasisVector(
		const BasisVector& a,
		const BasisVector& b) {

		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	static BasisVector subtractBasisVector(
		const BasisVector& a,
		const BasisVector& b) {

		return {
			a.x - b.x,
			a.y - b.y,
			a.z - b.z
		};
	}

	static BasisVector multiplyBasisVector(
		const BasisVector& vector,
		float scalar) {

		return {
			vector.x * scalar,
			vector.y * scalar,
			vector.z * scalar
		};
	}

	static BasisVector crossBasisVector(
		const BasisVector& a,
		const BasisVector& b) {

		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	// --- NAVIGATION ---
	NavigationState m_navigation;

	//TEXTURE_MAP_2D Layer 1 menu cursor
	TextureMapLayer1Item m_textureMapLayer1Selection =
		TextureMapLayer1Item::Workspace;
	//TEXTURE_MAP_2D Layer 2 menu cursor
	TextureMapLayer2Item m_textureMapLayer2Selection =
		TextureMapLayer2Item::PreviewParticleRadius;

	TextEntrySession m_textEntry;
	TextEntryTarget m_textEntryTarget = TextEntryTarget::None;

	// --- PARTICLE WORKSPACE CONFIGURATION ---
	ParticleSimLayer1Item m_particleSimLayer1Selection =
		ParticleSimLayer1Item::Workspace;

	ParticleSimDraftConfig m_particleSimDraftConfig;
	ParticleColorSelection m_particleColorSelection = PARTICLE_COLOR_RED;
	ParticleResetMode m_particleResetMode = PARTICLE_RESET_DEFAULT;
	ParticleConfigList m_activeParticleConfigList = PARTICLE_LIST_COLOR;

	ParticleRenderMode m_particleRenderMode = PARTICLE_RENDER_DEFAULT;

	// Determines whether Sub-Layer 2 is editing a CUDA scalar field
	// or presenting an imported/static indexed mesh.
	SPAuthoringSource m_spAuthoringSource = SPAuthoringSource::ProceduralVolume;

	// True only when a loaded mesh bundle also restored an editable
	// volumetric source into the CAD volume pipeline.
	bool m_loadedStaticMeshHasEditableVolume = false;

	int m_particleSimLayer2Selection = 0;

	// --- SINGLE_PARTICLE_MCAD WORKFLOW ---
	SPDisplayMode m_spDisplayMode = SPDisplayMode::Render;

	SingleParticleLayer1Item m_singleParticleLayer1Selection =
		SingleParticleLayer1Item::Workspace;

	SingleParticleObjectType m_singleParticleObjectType =
		SingleParticleObjectType::Static;

	SingleParticleSubLayer m_singleParticleSubLayer =
		SP_SUB_LAYER_REFERENCE;

	// Collision and visual setup state.
	SPCollisionShape m_spCollisionShape = SPCollisionShape::Sphere;
	SPMeshBoundMode m_spMeshBoundMode = SPMeshBoundMode::Default;
	VolumeAssemblyNode m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;

	// Sub-Layer 0 selection state.
	bool m_hoverValid = false;
	bool m_spSelectionArmed = false;
	bool m_selectedParticle = false;
	bool m_spRenderCageVisible = true;

	int m_workplaneSlice = 0;

	float m_hoverX = 0.0f;
	float m_hoverY = 0.0f;
	float m_particleRadius = kParticleRadiusDefault;

	// --- VOLUME OBJECT EDITING ---
	ObjectEditMode m_objectEditMode = EDIT_SCALE_WHOLE;
	ObjectRotationMode m_objectRotationMode = ROTATE_PITCH;
	ObjectTransformMode m_objectTransformMode = TRANSFORM_SCALE;
	VolumeInjectionVoxel m_volumeInjectionVoxel = INJECTION_VOXEL_NONE;
	VolumeEditTarget m_volumeEditTarget = VOLUME_EDIT_TARGET_VOXEL_0;
	VolumeInjectionMode m_volumeInjectionMode = VOLUME_FUSE;
	SPMirrorMode m_spMirrorMode = SP_MIRROR_NONE;
	OffsetVector m_offsetVectorSelection = OFFSET_VECTOR_X;

	int m_rotationAngleIncrementIndex = 0;
	int m_offsetIncrementIndex = 0;
	float m_offsetIncrement = 0.01f;
	float m_injectionRailT = 0.0f;

	VolumeObjectState m_volume0State;
	VolumeObjectState m_volume1State;

	// --- SUB-LAYER PANEL / BOUNDARY STATUS ---
	bool m_subLayerPanelOpen = false;
	bool m_volumeBoundarySensorReady = false;
	SPOverlapPreviewStatus m_spOverlapPreviewStatus =
		SP_OVERLAP_POSITION_IN_NODE_2;
	int m_activeSubLayerPanelItem = 0;
	unsigned int m_volumeBoundaryUnsafeCount = 0;
};

#endif
