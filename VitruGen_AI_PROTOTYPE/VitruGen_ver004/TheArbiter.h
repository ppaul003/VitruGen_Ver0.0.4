#ifndef __ARBITER_SYS_H__
#define __ARBITER_SYS_H__

#include "Interactions.h"

#include <GL/freeglut.h>

class TheArbiter {
public:

	enum class WorkspaceDomain {
		NONE = 0,
		GRID_2D,
		GRID_3D,
		SIMCAD_4D,
		COUNT
	};

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
	static const char* getWorkspaceName(WorkspaceId workspace);
	static bool workspaceBelongsToDomain(
		WorkspaceId workspace,
		WorkspaceDomain domain
	);

	enum AppLayer {
		LAYER_MENU = 0,
		LAYER_ENVIRONMENT_CONFIGURATION = 1,
		LAYER_3D_GRID_MODE_CONFIGURATION = 2,
		LAYER_SIMULATION_RUN = 3
	};
	enum EnvironmentSelection {
		ENV_IDLE = 0,
		ENV_3D_GRID = 1
	};
	enum GridSelection {
		GRID_GRAPH_3D = 0,
		GRID_SINGLE_PARTICLE = 1,
		GRID_PARTICLES_3D = 2
	};
	enum ParticleColorSelection {
		PARTICLE_COLOR_RED = 0,
		PARTICLE_COLOR_BLUE = 1,
		PARTICLE_COLOR_GREEN = 2
	};
	enum ParticleResetMode {
		PARTICLE_RESET_DEFAULT = 0,
		PARTICLE_RESET_RANDOM = 1
	};
	enum ParticleConfigList {
		PARTICLE_LIST_COLOR = 0,

		// In SINGLE_PARTICLE mode, list 2 means radius.
		// In PARTICLES_3D mode, list 2 still acts as reset mode.
		PARTICLE_LIST_RADIUS = 1,
		PARTICLE_LIST_RESET = PARTICLE_LIST_RADIUS,

		PARTICLE_LIST_RENDER_MODE = 2,
		PARTICLE_LIST_RUN = 3,
		PARTICLE_LIST_COUNT = 4
	};
	enum ObjectSelection {
		OBJECT_SPHERE = 0,
		OBJECT_TORUS = 1,
		OBJECT_BLOCK = 2
	};
	enum RenderMethod {
		METHOD_VOLUME_RENDER = 0,
		METHOD_SLICE = 1,
		METHOD_RAYCAST = 2
	};
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
	enum SingleParticleSubLayer {
		SP_SUB_LAYER_REFERENCE = 0,
		SP_SUB_LAYER_SHAPE_EDIT,
		SP_SUB_LAYER_VOLUME_RENDER,
		SP_SUB_LAYER_MARCHING_CUBES,
		SP_SUB_LAYER_COUNT
	};
	enum VolumePrimitive {
		VOLUME_PRIMITIVE_BASE = 0,
		VOLUME_PRIMITIVE_SPHERE,
		VOLUME_PRIMITIVE_TORUS,
		VOLUME_PRIMITIVE_BLOCK,
		VOLUME_PRIMITIVE_CYLINDER,
		VOLUME_PRIMITIVE_CAPSULE,
		VOLUME_PRIMITIVE_WEDGE,
		VOLUME_PRIMITIVE_FRUSTUM,
		VOLUME_PRIMITIVE_CONE,
		VOLUME_PRIMITIVE_DELTA_WING,

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
	enum VolumeMirrorMode {
		VOLUME_MIRROR_NONE = 0,
		VOLUME_MIRROR_X,
		VOLUME_MIRROR_Y,
		VOLUME_MIRROR_Z,
		VOLUME_MIRROR_XY,
		VOLUME_MIRROR_XZ,
		VOLUME_MIRROR_YZ,
		VOLUME_MIRROR_XYZ,
		VOLUME_MIRROR_COUNT
	};
	enum VolumeEditTarget {
		VOLUME_EDIT_TARGET_VOXEL_0 = 0,
		VOLUME_EDIT_TARGET_VOXEL_1,
		VOLUME_EDIT_TARGET_COUNT
	};
	enum ApplyToBasePanelItem {
		APPLY_LIST_COMMIT = 0,
		APPLY_LIST_OFFSET_OBJECT,
		APPLY_LIST_CANCEL_TO_PREVIEW,
		APPLY_LIST_COUNT
	};
	enum MarchingCubesPanelItem {
		MC_LIST_SAVE_STATIC_ASSET = 0,
		MC_LIST_OPEN_TEXTURE,
		MC_LIST_EXPORT_OBJ,
		MC_LIST_TO_SUB_LAYER_2,
		MC_LIST_TO_SUB_LAYER_0,
		MC_LIST_COUNT
	};

	enum TexturePaintTool {
		TEXTURE_TOOL_BRUSH = 0,
		TEXTURE_TOOL_ERASER,
		TEXTURE_TOOL_FILL
	};

	enum TexturePanelItem {
		TEXTURE_LIST_BRUSH = 0,
		TEXTURE_LIST_COLOR,
		TEXTURE_LIST_BRUSH_SIZE,
		TEXTURE_LIST_ERASER,
		TEXTURE_LIST_FILL,
		TEXTURE_LIST_CLEAR,
		TEXTURE_LIST_IMPORT,
		TEXTURE_LIST_SAVE,
		TEXTURE_LIST_UV_OVERLAY,
		TEXTURE_LIST_APPLY_RETURN,
		TEXTURE_LIST_COUNT
	};

	enum LinkedPanelItem {
		LINKED_LIST_CREATE_ASSEMBLY = 0,
		LINKED_LIST_ADD_MESH,
		LINKED_LIST_ADD_FIXED_JOINT,
		LINKED_LIST_ADD_REVOLUTE_JOINT,
		LINKED_LIST_ADD_INTERACTION,
		LINKED_LIST_ADD_ANIMATION,
		LINKED_LIST_BAKE,
		LINKED_LIST_SAVE_PROJECT,
		LINKED_LIST_OPEN_SANDBOX,
		LINKED_LIST_SELECT_NODE,
		LINKED_LIST_SELECT_PARENT,
		LINKED_LIST_POSITION_X,
		LINKED_LIST_POSITION_Y,
		LINKED_LIST_POSITION_Z,
		LINKED_LIST_ROTATION_X,
		LINKED_LIST_ROTATION_Y,
		LINKED_LIST_ROTATION_Z,
		LINKED_LIST_JOINT_AXIS,
		LINKED_LIST_JOINT_LIMITS,
		LINKED_LIST_LOAD_PROJECT,
		LINKED_LIST_PREVIEW_ANIMATION,
		LINKED_LIST_REMOVE_NODE,
		LINKED_LIST_COUNT
	};

	struct TextureColor {
		unsigned char r = 255;
		unsigned char g = 255;
		unsigned char b = 255;
		unsigned char a = 255;
	};

	enum ArbiterCommand {
		CMD_NONE = 0,
		CMD_EXIT,
		CMD_REDRAW,
		CMD_TOGGLE_PAUSE,
		CMD_STEP_SIMULATION,
		CMD_START_CUDA_SIMULATION,
		CMD_PLACE_SINGLE_PARTICLE,
		CMD_SELECT_PARTICLE,
		CMD_PARTICLE_CONFIG_CHANGED,
		CMD_PARTICLE_RADIUS_CHANGED,
		CMD_PARTICLE_RENDER_MODE_CHANGED,
		CMD_OPEN_TEXTURE_MAP_2D,
		CMD_OPEN_LINKED_PARTICLES_MCAD,
		CMD_OPEN_SANDBOX_SIM,
		CMD_SAVE_STATIC_PARTICLE,
		CMD_TEXTURE_CLEAR,
		CMD_TEXTURE_IMPORT,
		CMD_TEXTURE_SAVE,
		CMD_TEXTURE_APPLY,
		CMD_LINK_CREATE_ASSEMBLY,
		CMD_LINK_ADD_MESH,
		CMD_LINK_ADD_FIXED_JOINT,
		CMD_LINK_ADD_REVOLUTE_JOINT,
		CMD_LINK_ADD_INTERACTION,
		CMD_LINK_ADD_ANIMATION,
		CMD_LINK_BAKE,
		CMD_LINK_ADJUST,
		CMD_LINK_PREVIEW_ANIMATION,
		CMD_LINK_REMOVE_NODE,
		CMD_PROJECT_SAVE,
		CMD_PROJECT_LOAD,
		CMD_SANDBOX_SPAWN,
		CMD_SANDBOX_FIRE,
		CMD_SANDBOX_RESET,
		CMD_SANDBOX_PLAY_ANIMATION,
		CMD_SANDBOX_DRIVE_FORWARD,
		CMD_SANDBOX_DRIVE_REVERSE,
		CMD_SANDBOX_TURN_LEFT,
		CMD_SANDBOX_TURN_RIGHT,
		CMD_SANDBOX_JOINT_DECREASE,
		CMD_SANDBOX_JOINT_INCREASE,
		CMD_SANDBOX_TOGGLE_FOLLOW_CAMERA,
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
		bool marchingCubesPlaceholderAction = false;
		bool outputObjectPlaceholder = false;
		bool exportObjRequested = false;
		bool particleConfigChanged = false;
		bool particleRadiusChanged = false;
		bool particleRenderModeChanged = false;
		bool goToSubLayer0 = false;

		// New volume CAD actions
		bool commitVolumeFuse = false;
		bool commitVolumeCut = false;
	};

	static constexpr float kParticleWorldBoundary = 1.0f;
	static constexpr float kParticleGridDim = 64.0f;
	static constexpr float kParticleCellSize = (2.0f * kParticleWorldBoundary) / kParticleGridDim;
	static constexpr float kParticleRadiusMax = 0.5f * kParticleCellSize;
	static constexpr float kParticleRadiusMin = 0.25f * kParticleRadiusMax;
	static constexpr float kParticleRadiusDefault = 0.5f * (kParticleRadiusMin + kParticleRadiusMax);
	static constexpr float kParticleRadiusStep = (kParticleRadiusMax - kParticleRadiusMin) / 16.0f;

	TheArbiter();
	~TheArbiter();

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
	float getOffsetDistance() const;
	float getInjectionT() const { return m_injectionRailT; }
	float getInjectionRailT() const { return m_injectionRailT; }
	float getVolumePrimitiveScale() const { return getVolumeScaleWhole(); }
	float getParticleRadius() const { return m_particleRadius; }

	float dotBasisVector(const BasisVector& a, const BasisVector& b) const { return a.x * b.x + a.y * b.y + a.z * b.z; }

	ArbiterResult processKeyboard(const KeyboardInput::KeyEvent& event);
	ArbiterResult setVolumeAssemblyNode(VolumeAssemblyNode node);
	ArbiterResult setOffsetVectorSelection(OffsetVector vector);
	ArbiterResult clearObjectOffsetFromMenu();
	ArbiterResult toggleVolumeInjectionModeFromMenu();
	ArbiterResult commitBrushBaseFromMenu();
	ArbiterResult commitObjectBasisAndReturnToPreview();
	ArbiterResult enterMarchingCubesFromPreview();
	ArbiterResult activateMarchingCubesPanelItemFromMenu(MarchingCubesPanelItem item);
	ArbiterResult trySelectParticleAtCurrentSlice();

	const NavigationState& getNavigationState() const { return m_navigation; }
	ApplicationLayer getApplicationLayer() const { return m_navigation.layer; }
	GlobalShellSelection getGlobalShellSelection() const {
		return m_navigation.globalShellSelection;
	}
	WorkspaceDomain getSelectedDomain() const {
		return m_navigation.selectedDomain;
	}
	WorkspaceId getSelectedWorkspace() const;
	WorkspaceId getWorkspaceSelection(WorkspaceDomain domain) const;

	// Temporary Gate 1 compatibility adapters. These preserve the public
	// contract used by EuclidEngine and ViewPort while NavigationState becomes
	// the sole mutable navigation model.
	AppLayer getAppLayer() const;
	EnvironmentSelection getEnvironmentSelection() const;
	GridSelection getGridSelection() const;
	ParticleColorSelection getParticleColorSelection() const { return m_particleColorSelection; }
	ParticleResetMode getParticleResetMode() const { return m_particleResetMode; }
	ParticleConfigList getActiveParticleConfigList() const { return m_activeParticleConfigList; }

	VolumeAssemblyNode getVolumeAssemblyNode() const { return m_volumeAssemblyNode; }
	VolumeInjectionVoxel getVolumeInjectionVoxel() const { return m_volumeInjectionVoxel; }
	VolumeInjectionMode getVolumeInjectionMode() const { return m_volumeInjectionMode; }
	VolumeMirrorMode getVolumeMirrorMode() const { return m_volumeMirrorMode; }
	int getVolumeMirrorAxisMask() const;
	VolumeEditTarget getVolumeEditTarget() const { return m_volumeEditTarget; }

	SingleParticleSubLayer getSingleParticleSubLayer() const { return m_singleParticleSubLayer; }
	VolumePrimitive getVolumePrimitiveSelection() const { return getActiveVolumeState().primitive; }
	ParticleRenderMode getParticleRenderMode() const { return m_particleRenderMode; }

	BasisVector normalizeBasisVector(const BasisVector& v) const;
	BasisVector transformByBasis(const ObjectBasis& basis, const BasisVector& localVector) const;
	BasisVector rotateLocalVectorXYZ(const BasisVector& vector, float pitchDeg, float yawDeg, float rollDeg) const;

	BasisVector makeBasisVector(float x, float y, float z) const { return { x, y, z }; }
	BasisVector addBasisVector(
		const BasisVector& a,
		const BasisVector& b) const {

		return {
			a.x + b.x,
			a.y + b.y,
			a.z + b.z
		};
	}
	BasisVector subtractBasisVector(
		const BasisVector& a,
		const BasisVector& b) const {

		return {
			a.x - b.x,
			a.y - b.y,
			a.z - b.z
		};
	}
	BasisVector multiplyBasisVector(
		const BasisVector& v,
		float scalar) const {

		return {
			v.x * scalar,
			v.y * scalar,
			v.z * scalar
		};
	}
	BasisVector crossBasisVector(
		const BasisVector& a,
		const BasisVector& b) const {

		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	OffsetVector getOffsetVectorSelection() const { return m_offsetVectorSelection; }

	const ObjectBasis& getObjectBasis() const { return getActiveVolumeState().basis; }

	ObjectBasis getEffectiveObjectBasis() const;
	ObjectBasis orthonormalizeBasis(const ObjectBasis& basis) const;
	VolumePrimitive getResolvedVolumePrimitiveSelection() const;
	
	int getInjectionVoxelDX() const;
	int getInjectionVoxelDY() const;
	int getInjectionVoxelDZ() const;
	int getActiveSubLayerPanelItem() const { return m_activeSubLayerPanelItem; }
	int getInjectionVectorIndex() const { return static_cast<int>(m_offsetVectorSelection); }
	int getWorkplaneSlice() const { return m_workplaneSlice; }
	int getActiveSubLayerPanelItemCount() const;
	int getRotationAngleIncrementDeg() const;
	int getActiveTexturePanelItem() const { return m_activeTexturePanelItem; }
	int getTextureBrushSize() const { return m_textureBrushSize; }
	int getTextureColorIndex() const { return m_textureColorIndex; }
	TexturePaintTool getTexturePaintTool() const { return m_texturePaintTool; }
	TextureColor getTexturePaintColor() const;
	bool isTextureUvOverlayVisible() const { return m_textureUvOverlayVisible; }
	int getActiveLinkedPanelItem() const { return m_activeLinkedPanelItem; }
	int getLinkedInteractionRoleIndex() const { return m_linkedInteractionRoleIndex; }
	int getLinkedAdjustmentDirection() const { return m_linkedAdjustmentDirection; }

	bool isMenuLayer() const { return m_navigation.layer == ApplicationLayer::GLOBAL_SHELL; }
	bool isEnvironmentConfigLayer() const { return m_navigation.layer == ApplicationLayer::DOMAIN_SELECTION; }
	bool isParticleConfigLayer() const { return m_navigation.layer == ApplicationLayer::WORKSPACE_CONFIGURATION; }
	bool isSimulationRunLayer() const { return m_navigation.layer == ApplicationLayer::ACTIVE_WORKSPACE; }
	bool is3DViewLayer() const { return !isMenuLayer(); }
	bool isIdleSelected() const { return m_navigation.globalShellSelection == GlobalShellSelection::IDLE; }
	bool is3DGridSelected() const { return m_navigation.globalShellSelection == GlobalShellSelection::WORKSPACE_DOMAINS; }
	bool is3DVisualizationSelected() const { return is3DGridSelected(); }
	bool isGraphSelected() const { return getSelectedWorkspace() == WorkspaceId::GRAPH_3D; }
	bool isSingleParticleSelected() const { return getSelectedWorkspace() == WorkspaceId::SINGLE_PARTICLE_MCAD; }
	bool isParticlesSelected() const { return getSelectedWorkspace() == WorkspaceId::PARTICLE_SIMULATION; }
	bool isTextureMapSelected() const { return getSelectedWorkspace() == WorkspaceId::TEXTURE_MAP_2D; }
	bool isLinkedParticlesSelected() const { return getSelectedWorkspace() == WorkspaceId::LINKED_PARTICLES_MCAD; }
	bool isSandboxSelected() const { return getSelectedWorkspace() == WorkspaceId::SANDBOX_SIM; }
	bool isMvpPipelineWorkspaceSelected() const {
		return isSingleParticleSelected() || isTextureMapSelected() ||
			isLinkedParticlesSelected() || isSandboxSelected();
	}
	bool isWorkParticleSelectSubLayer() const { return isShapeEditSubLayer(); }
	bool isBaseVolumeSelected() const { return getVolumePrimitiveSelection() == VOLUME_PRIMITIVE_BASE; }
	bool isVolumeBoundarySensorReady() const { return m_volumeBoundarySensorReady; }
	bool isVolumeBoundarySafe() const { return m_volumeBoundarySensorReady && m_volumeBoundaryUnsafeCount == 0; }
	bool isEditingInjectionVoxel0() const { return m_volumeEditTarget == VOLUME_EDIT_TARGET_VOXEL_0; }
	bool isEditingInjectionVoxel1() const { return m_volumeEditTarget == VOLUME_EDIT_TARGET_VOXEL_1; }
	bool isInjectionEditPanelActive() const { return m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT && hasInjectionVoxelSelected(); }

	bool setVolumeBoundaryStatus(bool sensorReady, unsigned int unsafeCount);
	
	bool isSingleParticleReferenceSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_REFERENCE; }
	bool isShapeEditSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_SHAPE_EDIT; }
	bool isVolumeRenderSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER; }
	bool isMarchingCubesSubLayer() const { return isSimulationRunLayer() && isSingleParticleSelected() && m_singleParticleSubLayer == SP_SUB_LAYER_MARCHING_CUBES; }
	bool isWorkplaneParticleSelectSubLayer() const { return isShapeEditSubLayer(); }
	bool isParticleRenderDefault() const { return m_particleRenderMode == PARTICLE_RENDER_DEFAULT; }
	bool isParticleRenderMesh() const { return m_particleRenderMode == PARTICLE_RENDER_MESH; }
	bool isMcAnimationEnabled() const { return m_mcAnimationEnabled; }
	bool isMcRenderingEnabled() const { return m_mcRenderingEnabled; }
	bool isMcLightingEnabled() const { return m_mcLightingEnabled; }
	bool isMcWireframeEnabled() const { return m_mcWireframeEnabled; }
	bool isSubLayerPanelOpen() const { return m_subLayerPanelOpen; }
	bool isSubLayerPanelEligible() const { return isVolumeRenderSubLayer() || isMarchingCubesSubLayer(); }

	bool hasSelectedParticle() const { return m_selectedParticle; }
	bool hasHover() const { return m_hoverValid; }
	bool hasEditableVolumePrimitive() const { return getVolumePrimitiveSelection() != VOLUME_PRIMITIVE_BASE; }
	bool hasInjectionVoxelSelected() const { return m_volumeInjectionVoxel != INJECTION_VOXEL_NONE; }
	bool hasPrimitiveBrushSelected() const { return getVolumePrimitiveSelection() != VOLUME_PRIMITIVE_BASE; }
	
	bool canApplyVolumeToBase() const;
	bool isInjectionBrushBaseSelected() const;

	unsigned int getVolumeBoundaryUnsafeCount() const { return m_volumeBoundaryUnsafeCount; }

	const char* getLayerName() const;
	const char* getEnvironmentName() const;
	const char* getGridSelectionName() const;
	const char* getParticleColorName() const;
	const char* getParticleResetModeName() const;
	const char* getActiveParticleConfigListName() const;
	const char* getParticleRenderModeName() const;
	const char* getSingleParticleSubLayerName() const;
	const char* getVolumePrimitiveName() const;
	const char* getObjectEditModeName() const;
	const char* getObjectRotationModeName() const;
	const char* getObjectTransformModeName() const;
	const char* getVolumeAssemblyNodeName() const;
	const char* getVolumeInjectionVoxelName() const;
	const char* getVolumeInjectionModeName() const;
	const char* getVolumeMirrorModeName() const;
	const char* getVolumeEditTargetName() const;
	const char* getVolumeEditTargetObjectName() const;
	const char* getSubLayerPanelListName() const;
	const char* getOffsetVectorName() const;
	const char* getOffsetIncrementName() const;
	const char* getTexturePaintToolName() const;
	const char* getTextureColorName() const;
	const char* getLinkedInteractionRoleName() const;

	void setParticleRenderMode(ParticleRenderMode mode) { m_particleRenderMode = mode; }
	void updateHoverFromScreen(int x, int y, int w, int h);
	void finalizeVoxelBaseCommit();
	void resetToMenu();

private:
	void resetNavigationState();
	void setApplicationLayer(ApplicationLayer layer);
	void setWorkspaceSelection(
		WorkspaceDomain domain,
		WorkspaceId workspace
	);
	void setLegacyGridSelection(GridSelection selection);
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
	void handleTextureWorkspaceKeyboard(
		const KeyboardInput::KeyEvent& event,
		ArbiterResult& result
	);
	void handleLinkedWorkspaceKeyboard(
		const KeyboardInput::KeyEvent& event,
		ArbiterResult& result
	);
	void handleSandboxWorkspaceKeyboard(
		const KeyboardInput::KeyEvent& event,
		ArbiterResult& result
	);

	void toggleEnvironmentSelection();
	void toggleGridSelection();
	void cycleWorkspaceSelection(float dir);
	void toggleParticleColorSelection();
	void toggleParticleResetMode();

	void moveParticleConfigCursorUp();
	void moveParticleConfigCursorDown();

	void toggleParticleRenderMode();
	void toggleVolumePrimitiveSelection();

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
	void cycleVolumeMirrorMode(float dir);
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

	int getParticleConfigListCount() const;

private:
	NavigationState m_navigation;

	ParticleColorSelection m_particleColorSelection = PARTICLE_COLOR_RED;
	ParticleResetMode m_particleResetMode = PARTICLE_RESET_DEFAULT;
	ParticleConfigList m_activeParticleConfigList = PARTICLE_LIST_COLOR;
	ObjectSelection m_objectSelection = OBJECT_SPHERE;
	RenderMethod m_renderMethod = METHOD_RAYCAST;
	ObjectEditMode m_objectEditMode = EDIT_SCALE_WHOLE;
	ObjectRotationMode m_objectRotationMode = ROTATE_PITCH;
	ObjectTransformMode m_objectTransformMode = TRANSFORM_SCALE;
	VolumeAssemblyNode m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
	VolumeInjectionVoxel m_volumeInjectionVoxel = INJECTION_VOXEL_NONE;
	VolumeEditTarget m_volumeEditTarget = VOLUME_EDIT_TARGET_VOXEL_0;
	VolumeInjectionMode m_volumeInjectionMode = VOLUME_FUSE;
	VolumeMirrorMode m_volumeMirrorMode = VOLUME_MIRROR_NONE;
	ParticleRenderMode m_particleRenderMode = PARTICLE_RENDER_DEFAULT;
	
	SingleParticleSubLayer m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;
	VolumePrimitive m_volumePrimitiveSelection = VOLUME_PRIMITIVE_SPHERE;

	OffsetVector m_offsetVectorSelection = OFFSET_VECTOR_X;

	bool m_mcAnimationEnabled = false;
	bool m_mcRenderingEnabled = true;
	bool m_mcLightingEnabled = false;
	bool m_mcWireframeEnabled = true;
	bool m_selectedParticle = false;
	bool m_subLayerPanelOpen = false;
	bool m_hoverValid = false;
	bool m_volumeBoundarySensorReady = false;

	int m_workplaneSlice = 0;
	int m_activeSubLayerPanelItem = 0;
	int m_rotationAngleIncrementIndex = 0;
	int m_offsetIncrementIndex = 0;
	int m_activeTexturePanelItem = TEXTURE_LIST_BRUSH;
	int m_textureBrushSize = 12;
	int m_textureColorIndex = 0;
	int m_activeLinkedPanelItem = LINKED_LIST_CREATE_ASSEMBLY;
	int m_linkedInteractionRoleIndex = 0;
	int m_linkedAdjustmentDirection = 0;
	TexturePaintTool m_texturePaintTool = TEXTURE_TOOL_BRUSH;
	bool m_textureUvOverlayVisible = true;

	unsigned int m_volumeBoundaryUnsafeCount = 0;

	float m_hoverX = 0.0f;
	float m_hoverY = 0.0f;

	float m_volumePrimitiveScale = 1.0f;
	float m_offsetIncrement = 0.01f;
	// Normalized volume-domain translation.
	//
	// For a 128^3 volume:
	//     normalized 1.00 = 64 voxel units
	//     normalized 0.01 = 0.64 voxel units
	float m_offsetX = 0.0f;
	float m_offsetY = 0.0f;
	float m_offsetZ = 0.0f;

	float m_mcIsoValue = 0.0f;

	float m_threshold = 0.0f;
	float m_sliceDistance = 0.0f;

	float m_particleRadius = kParticleRadiusDefault;

	float m_scaleWhole = 1.0f;
	float m_scaleX = 1.0f;
	float m_scaleY = 1.0f;
	float m_scaleZ = 1.0f;

	float m_rotationPitchDeg = 0.0f;
	float m_rotationYawDeg = 0.0f;
	float m_rotationRollDeg = 0.0f;

	float m_injectionRailT = 0.0f;

	ObjectBasis m_objectBasis{
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f }
	};

	VolumeObjectState m_volume0State;
	VolumeObjectState m_volume1State;
};

#endif
