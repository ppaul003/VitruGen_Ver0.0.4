#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <algorithm>
#include <cassert>
#include <cmath>

#include "TheArbiter.h"

using namespace std;

namespace {
	constexpr TheArbiter::WorkspaceDescriptor kWorkspaceCatalog[] = {
		{
			TheArbiter::WorkspaceId::NONE,
			TheArbiter::WorkspaceDomain::NONE,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"NONE"
		},
		{
			TheArbiter::WorkspaceId::GRAPH_2D,
			TheArbiter::WorkspaceDomain::GRID_2D,
			TheArbiter::WorkspaceAvailability::EXPERIMENTAL,
			"GRAPH_2D"
		},
		{
			TheArbiter::WorkspaceId::TEXTURE_MAP_2D,
			TheArbiter::WorkspaceDomain::GRID_2D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"TEXTURE_MAP_2D"
		},
		{
			TheArbiter::WorkspaceId::SPRITE_PROJECTION_2D,
			TheArbiter::WorkspaceDomain::GRID_2D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"SPRITE_PROJECTION_2D"
		},
		{
			TheArbiter::WorkspaceId::GRAPH_3D,
			TheArbiter::WorkspaceDomain::GRID_3D,
			TheArbiter::WorkspaceAvailability::EXPERIMENTAL,
			"GRAPH_3D"
		},
		{
			TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD,
			TheArbiter::WorkspaceDomain::GRID_3D,
			TheArbiter::WorkspaceAvailability::AVAILABLE,
			"SINGLE_PARTICLE_MCAD"
		},
		{
			TheArbiter::WorkspaceId::LINK_PARTICLES_MCAD,
			TheArbiter::WorkspaceDomain::GRID_3D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"LINK_PARTICLES_MCAD"
		},
		{
			TheArbiter::WorkspaceId::PARTICLE_SIMULATION,
			TheArbiter::WorkspaceDomain::SIMCAD_4D,
			TheArbiter::WorkspaceAvailability::AVAILABLE,
			"PARTICLE_SIMULATION"
		},
		{
			TheArbiter::WorkspaceId::NBODY_SIM,
			TheArbiter::WorkspaceDomain::SIMCAD_4D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"NBODY_SIM"
		},
		{
			TheArbiter::WorkspaceId::FLUID_SIM,
			TheArbiter::WorkspaceDomain::SIMCAD_4D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"FLUID_SIM"
		},
		{
			TheArbiter::WorkspaceId::CUDA_CAD,
			TheArbiter::WorkspaceDomain::SIMCAD_4D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"CUDA_CAD"
		},
		{
			TheArbiter::WorkspaceId::SANDBOX_SIM,
			TheArbiter::WorkspaceDomain::SIMCAD_4D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"SANDBOX_SIM"
		}
	};

	constexpr int kWorkspaceCatalogCount =
		static_cast<int>(sizeof(kWorkspaceCatalog) /
			sizeof(kWorkspaceCatalog[0]));

	static_assert(
		kWorkspaceCatalogCount ==
			static_cast<int>(TheArbiter::WorkspaceId::COUNT),
		"Every WorkspaceId must have exactly one catalog entry."
	);

	constexpr float kOffsetIncrementValues[] = { 
		0.01f, 0.012f, 0.02f, 0.025f, 0.05f, 
		0.10f, 0.12f, 0.20f, 0.25f, 0.50f 
	};

	constexpr const char* kOffsetIncrementNames[] = {
		"0.01", "0.012", "0.02", "0.025", "0.05",
		"0.1", "0.12", "0.2", "0.25", "0.5"
	};

	constexpr int kOffsetIncrementCount =
		static_cast<int>(sizeof(kOffsetIncrementValues) / 
			sizeof(kOffsetIncrementValues[0]));

	constexpr TheArbiter::VolumeInjectionVoxel kInjectionVoxelCycle[] = {
		TheArbiter::INJECTION_VOXEL_NONE,
		TheArbiter::INJECTION_VOXEL_211,
		TheArbiter::INJECTION_VOXEL_121,
		TheArbiter::INJECTION_VOXEL_011,
		TheArbiter::INJECTION_VOXEL_112,
		TheArbiter::INJECTION_VOXEL_101,
		TheArbiter::INJECTION_VOXEL_110,
		// Expanded edge/corner route.
		TheArbiter::INJECTION_VOXEL_120, // +Y/-Z
		TheArbiter::INJECTION_VOXEL_221, // +X/+Y
		TheArbiter::INJECTION_VOXEL_122, // +Y/+Z
		TheArbiter::INJECTION_VOXEL_021, // -X/+Y
		TheArbiter::INJECTION_VOXEL_201, // +X/-Y
		TheArbiter::INJECTION_VOXEL_100, // -Y/-Z
		TheArbiter::INJECTION_VOXEL_001, // -X/-Y
		TheArbiter::INJECTION_VOXEL_102, // -Y/+Z

		// Four missing edge slots needed to complete the 3x3x3 shell.
		TheArbiter::INJECTION_VOXEL_010, // -X/-Z
		TheArbiter::INJECTION_VOXEL_210, // +X/-Z
		TheArbiter::INJECTION_VOXEL_012, // -X/+Z
		TheArbiter::INJECTION_VOXEL_212, // +X/+Z

		// Corners.
		TheArbiter::INJECTION_VOXEL_202, // +X/-Y/+Z
		TheArbiter::INJECTION_VOXEL_020, // -X/+Y/-Z
		TheArbiter::INJECTION_VOXEL_220, // +X/+Y/-Z
		TheArbiter::INJECTION_VOXEL_002, // -X/-Y/+Z
		TheArbiter::INJECTION_VOXEL_200, // +X/-Y/-Z
		TheArbiter::INJECTION_VOXEL_022, // -X/+Y/+Z
		TheArbiter::INJECTION_VOXEL_222, // +X/+Y/+Z
		TheArbiter::INJECTION_VOXEL_000  // -X/-Y/-Z
	};

	constexpr int kInjectionVoxelCycleCount =
		static_cast<int>(sizeof(kInjectionVoxelCycle) /
			sizeof(kInjectionVoxelCycle[0]));
}

const TheArbiter::WorkspaceDescriptor&
TheArbiter::describeWorkspace(WorkspaceId workspace) {
	for (int i = 0; i < kWorkspaceCatalogCount; ++i) {
		if (kWorkspaceCatalog[i].id == workspace) {
			return kWorkspaceCatalog[i];
		}
	}

	return kWorkspaceCatalog[0];
}

TheArbiter::WorkspaceDomain
TheArbiter::getWorkspaceDomain(WorkspaceId workspace) {
	return describeWorkspace(workspace).domain;
}

TheArbiter::WorkspaceAvailability
TheArbiter::getWorkspaceAvailability(WorkspaceId workspace) {
	return describeWorkspace(workspace).availability;
}

const char* TheArbiter::getWorkspaceName(WorkspaceId workspace) {
	return describeWorkspace(workspace).canonicalName;
}

bool TheArbiter::workspaceBelongsToDomain(
	WorkspaceId workspace,
	WorkspaceDomain domain) {
	return getWorkspaceDomain(workspace) == domain;
}

TheArbiter::WorkspaceId
TheArbiter::getWorkspaceSelection(WorkspaceDomain domain) const {
	switch (domain) {
	case WorkspaceDomain::GRID_2D:
		return m_navigation.workspaceSelections.grid2D;

	case WorkspaceDomain::GRID_3D:
		return m_navigation.workspaceSelections.grid3D;

	case WorkspaceDomain::SIMCAD_4D:
		return m_navigation.workspaceSelections.simcad4D;

	default:
	case WorkspaceDomain::NONE:
	case WorkspaceDomain::COUNT:
		return WorkspaceId::NONE;
	}
}

TheArbiter::WorkspaceId TheArbiter::getSelectedWorkspace() const {
	return getWorkspaceSelection(m_navigation.selectedDomain);
}

TheArbiter::AppLayer TheArbiter::getAppLayer() const {
	switch (m_navigation.layer) {
	case ApplicationLayer::GLOBAL_SHELL:
		return LAYER_MENU;

	case ApplicationLayer::DOMAIN_SELECTION:
		return LAYER_ENVIRONMENT_CONFIGURATION;

	case ApplicationLayer::WORKSPACE_CONFIGURATION:
		return LAYER_3D_GRID_MODE_CONFIGURATION;

	case ApplicationLayer::ACTIVE_WORKSPACE:
		return LAYER_SIMULATION_RUN;

	default:
	case ApplicationLayer::COUNT:
		return LAYER_MENU;
	}
}

TheArbiter::EnvironmentSelection
TheArbiter::getEnvironmentSelection() const {
	return isIdleSelected() ? ENV_IDLE : ENV_3D_GRID;
}

TheArbiter::GridSelection TheArbiter::getGridSelection() const {
	switch (getSelectedWorkspace()) {
	case WorkspaceId::SINGLE_PARTICLE_MCAD:
		return GRID_SINGLE_PARTICLE;

	case WorkspaceId::PARTICLE_SIMULATION:
		return GRID_PARTICLES_3D;

	default:
		return GRID_GRAPH_3D;
	}
}

void TheArbiter::resetNavigationState() {
	m_navigation = NavigationState{};
	validateNavigationState();
}

void TheArbiter::setApplicationLayer(ApplicationLayer layer) {
	m_navigation.layer = layer;
	validateNavigationState();
}

void TheArbiter::setWorkspaceSelection(
	WorkspaceDomain domain,
	WorkspaceId workspace) {
	const bool validSelection =
		workspaceBelongsToDomain(workspace, domain);

	assert(validSelection);
	if (!validSelection) return;

	switch (domain) {
	case WorkspaceDomain::GRID_2D:
		m_navigation.workspaceSelections.grid2D = workspace;
		break;

	case WorkspaceDomain::GRID_3D:
		m_navigation.workspaceSelections.grid3D = workspace;
		break;

	case WorkspaceDomain::SIMCAD_4D:
		m_navigation.workspaceSelections.simcad4D = workspace;
		break;

	default:
	case WorkspaceDomain::NONE:
	case WorkspaceDomain::COUNT:
		assert(false && "A workspace selection requires a real domain.");
		return;
	}

	m_navigation.selectedDomain = domain;
	validateNavigationState();
}

void TheArbiter::setLegacyGridSelection(GridSelection selection) {
	switch (selection) {
	case GRID_GRAPH_3D:
		setWorkspaceSelection(
			WorkspaceDomain::GRID_3D,
			WorkspaceId::GRAPH_3D
		);
		break;

	case GRID_SINGLE_PARTICLE:
		setWorkspaceSelection(
			WorkspaceDomain::GRID_3D,
			WorkspaceId::SINGLE_PARTICLE_MCAD
		);
		break;

	case GRID_PARTICLES_3D:
		setWorkspaceSelection(
			WorkspaceDomain::SIMCAD_4D,
			WorkspaceId::PARTICLE_SIMULATION
		);
		break;

	default:
		assert(false && "Unknown legacy grid selection.");
		break;
	}
}

void TheArbiter::validateNavigationState() const {
#ifndef NDEBUG
	assert(
		static_cast<int>(m_navigation.layer) >= 0 &&
		static_cast<int>(m_navigation.layer) <
			static_cast<int>(ApplicationLayer::COUNT)
	);
	assert(
		static_cast<int>(m_navigation.globalShellSelection) >= 0 &&
		static_cast<int>(m_navigation.globalShellSelection) <
			static_cast<int>(GlobalShellSelection::COUNT)
	);
	assert(
		m_navigation.selectedDomain == WorkspaceDomain::GRID_2D ||
		m_navigation.selectedDomain == WorkspaceDomain::GRID_3D ||
		m_navigation.selectedDomain == WorkspaceDomain::SIMCAD_4D
	);
	assert(workspaceBelongsToDomain(
		m_navigation.workspaceSelections.grid2D,
		WorkspaceDomain::GRID_2D
	));
	assert(workspaceBelongsToDomain(
		m_navigation.workspaceSelections.grid3D,
		WorkspaceDomain::GRID_3D
	));
	assert(workspaceBelongsToDomain(
		m_navigation.workspaceSelections.simcad4D,
		WorkspaceDomain::SIMCAD_4D
	));
#endif
}

TheArbiter::TheArbiter() {
	validateNavigationState();
}
TheArbiter::~TheArbiter() {}

void TheArbiter::toggleEnvironmentSelection() {
	m_navigation.globalShellSelection =
		isIdleSelected()
		? GlobalShellSelection::WORKSPACE_DOMAINS
		: GlobalShellSelection::IDLE;

	validateNavigationState();
}
void TheArbiter::toggleGridSelection() {
	int v = static_cast<int>(getGridSelection());
	v = (v + 1) % 3;
	setLegacyGridSelection(static_cast<GridSelection>(v));
}
void TheArbiter::toggleParticleColorSelection() {
	switch (m_particleColorSelection) {
	case PARTICLE_COLOR_RED:
		m_particleColorSelection = PARTICLE_COLOR_BLUE;
		break;

	case PARTICLE_COLOR_BLUE:
		m_particleColorSelection = PARTICLE_COLOR_GREEN;
		break;

	case PARTICLE_COLOR_GREEN:
	default:
		m_particleColorSelection = PARTICLE_COLOR_RED;
		break;
	}
}
void TheArbiter::toggleParticleResetMode() {
	m_particleResetMode =
		(m_particleResetMode == PARTICLE_RESET_DEFAULT)
		? PARTICLE_RESET_RANDOM
		: PARTICLE_RESET_DEFAULT;
}
void TheArbiter::toggleParticleRenderMode() {
	m_particleRenderMode =
		(m_particleRenderMode == PARTICLE_RENDER_DEFAULT)
		? PARTICLE_RENDER_MESH
		: PARTICLE_RENDER_DEFAULT;
}
void TheArbiter::toggleVolumePrimitiveSelection() {
	cycleVolumePrimitiveSelection(+1.0f);
}

void TheArbiter::increaseParticleRadius() {
	m_particleRadius += kParticleRadiusStep;

	if (m_particleRadius > kParticleRadiusMax) {
		m_particleRadius = kParticleRadiusMax;
	}
}
void TheArbiter::decreaseParticleRadius() {
	m_particleRadius -= kParticleRadiusStep;

	if (m_particleRadius < kParticleRadiusMin) {
		m_particleRadius = kParticleRadiusMin;
	}
}

void TheArbiter::cycleVolumePrimitiveSelection(float dir) {

	VolumeObjectState& state = activeVolumeState();

	// VOLUME_0:
	//     BASE means the committed anchor volume.
	//
	// VOLUME_1:
	//     BASE means the committed local brush base.
	//     It is a UI/state token, not m_dBaseVolume.
	const int firstPrimitive =
		static_cast<int>(VOLUME_PRIMITIVE_BASE);

	const int lastExclusive =
		static_cast<int>(VOLUME_PRIMITIVE_COUNT);

	const int count = lastExclusive - firstPrimitive;

	if (count <= 0) return;
	int value = static_cast<int>(state.primitive);

	if (value < firstPrimitive || value >= lastExclusive) {
		value = firstPrimitive;
	}

	int local = value - firstPrimitive;

	if (dir < 0.0f)
		local = (local + count - 1) % count;
	else
		local = (local + 1) % count;

	state.primitive =
		static_cast<VolumePrimitive>(firstPrimitive + local);
}

void TheArbiter::cycleRotationAngleIncrement(float dir) {
	static constexpr int kIncrementCount = 5;

	if (dir < 0.0f) {
		m_rotationAngleIncrementIndex =
			(m_rotationAngleIncrementIndex + kIncrementCount - 1) %
			kIncrementCount;
	}
	else {
		m_rotationAngleIncrementIndex =
			(m_rotationAngleIncrementIndex + 1) % kIncrementCount;
	}
}
void TheArbiter::cycleOffsetVectorSelection(float dir) {
	int value = static_cast<int>(m_offsetVectorSelection);
	const int count = static_cast<int>(OFFSET_VECTOR_COUNT);

	if (dir < 0.0f)
		value = (value + count - 1) % count;
	else
		value = (value + 1) % count;

	m_offsetVectorSelection =
		static_cast<OffsetVector>(value);
}
void TheArbiter::cycleOffsetIncrement(float dir) {

	if (dir < 0.0f) {
		m_offsetIncrementIndex =
			(m_offsetIncrementIndex + kOffsetIncrementCount - 1) %
			kOffsetIncrementCount;
	}
	else {
		m_offsetIncrementIndex =
			(m_offsetIncrementIndex + 1) %
			kOffsetIncrementCount;
	}

	m_offsetIncrement =
		kOffsetIncrementValues[m_offsetIncrementIndex];
}
void TheArbiter::cycleInjectionVoxelSelection(float dir) {
	int currentIndex = 0;
	for (int i = 0; i < kInjectionVoxelCycleCount; i++) {
		
		if (kInjectionVoxelCycle[i] == m_volumeInjectionVoxel) {
			
			currentIndex = i;
			break;
		}
	}

	if (dir < 0.0f) {
		currentIndex = (currentIndex + kInjectionVoxelCycleCount - 1) %
			kInjectionVoxelCycleCount;
	}
	else {
		currentIndex = (currentIndex + 1) %
			kInjectionVoxelCycleCount;
	}

	m_volumeInjectionVoxel = 
		kInjectionVoxelCycle[currentIndex];

	// Returning to NONE restores the default Node_1 edit target.
	if (m_volumeInjectionVoxel == INJECTION_VOXEL_NONE) {

		m_volumeEditTarget = 
			VOLUME_EDIT_TARGET_VOXEL_0;
	}

	// A newly selected injection voxel starts with the brush
	// centered in that injection chamber.
	m_injectionRailT = 0.0f;
}
void TheArbiter::cycleVolumeEditTarget(float dir) {
	int value =
		static_cast<int>(m_volumeEditTarget);

	const int count =
		static_cast<int>(VOLUME_EDIT_TARGET_COUNT);

	if (dir < 0.0f) {
		value = (value + count - 1) % count;
	}
	else {
		value = (value + 1) % count;
	}

	m_volumeEditTarget =
		static_cast<VolumeEditTarget>(value);
}
void TheArbiter::cycleVolumeInjectionMode(float dir) {
	(void)dir;

	m_volumeInjectionMode =
		(m_volumeInjectionMode == VOLUME_FUSE)
		? VOLUME_CUT
		: VOLUME_FUSE;
}
void TheArbiter::adjustInjectionRail(float dir) {
	if (dir == 0.0f) return;

	const float delta =
		(dir < 0.0f ? -1.0f : 1.0f) *
		m_offsetIncrement;

	// T = 0.0 -> brush center starts in selected injection voxel.
	// T = 1.0 -> brush center reaches anchor voxel center.
	m_injectionRailT += delta;

	if (m_injectionRailT < 0.0f) {
		m_injectionRailT = 0.0f;
	}

	if (m_injectionRailT > 1.0f) {
		m_injectionRailT = 1.0f;
	}

	
	m_injectionRailT = 
		roundf(m_injectionRailT * 1000000.0f) /
		1000000.0f;
}

TheArbiter::VolumeObjectState&
TheArbiter::activeVolumeState() {
	if (hasInjectionVoxelSelected() && 
		isEditingInjectionVoxel1())
		return m_volume1State;

	return m_volume0State;
}

const TheArbiter::VolumeObjectState&
TheArbiter::activeVolumeState() const {
	if (hasInjectionVoxelSelected() &&
		isEditingInjectionVoxel1())
		return m_volume1State;

	return m_volume0State;
}

const TheArbiter::VolumeObjectState&
TheArbiter::getActiveVolumeState() const {

	return activeVolumeState();
}

bool TheArbiter::isInjectionBrushBaseSelected() const {

	return
		hasInjectionVoxelSelected() &&
		isEditingInjectionVoxel1() &&
		getActiveVolumeState().primitive == VOLUME_PRIMITIVE_BASE;
}
TheArbiter::VolumePrimitive
TheArbiter::getResolvedVolumePrimitiveSelection() const {

	const VolumeObjectState& state = 
		getActiveVolumeState();

	// VOLUME_1 BASE is not the global committed anchor volume.
	// It means "the current brush has been rebased."
	if (hasInjectionVoxelSelected() &&
		isEditingInjectionVoxel1() &&
		state.primitive == VOLUME_PRIMITIVE_BASE) {

		if (!state.brushBaseReady) {
			return VOLUME_PRIMITIVE_SPHERE;
		}

		if (state.brushBasePrimitive == VOLUME_PRIMITIVE_BASE) {
			return VOLUME_PRIMITIVE_SPHERE;
		}

		return state.brushBasePrimitive;
	}

	return state.primitive;
}

void TheArbiter::resetVolumeState(VolumeObjectState& state, VolumePrimitive primitive) {

	state.primitive = primitive;

	state.brushBasePrimitive =
		primitive == VOLUME_PRIMITIVE_BASE
		? VOLUME_PRIMITIVE_SPHERE
		: primitive;

	state.brushBaseReady = false;

	state.basis = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f }
	};

	state.scaleWhole = 1.0f;
	state.scaleX = 1.0f;
	state.scaleY = 1.0f;
	state.scaleZ = 1.0f;

	state.pitchDeg = 0.0f;
	state.yawDeg = 0.0f;
	state.rollDeg = 0.0f;

	state.offsetX = 0.0f;
	state.offsetY = 0.0f;
	state.offsetZ = 0.0f;
}
void TheArbiter::resetAllVolumeStates() {
	resetVolumeState(m_volume0State, VOLUME_PRIMITIVE_SPHERE);
	resetVolumeState(m_volume1State, VOLUME_PRIMITIVE_SPHERE);
}

void TheArbiter::adjustObjectOffset(float dir) {
	if (dir == 0.0f) return;

	const float delta =
		(dir < 0.0f ? -1.0f : 1.0f) * 
		m_offsetIncrement;

	VolumeObjectState& state = 
		activeVolumeState();

	float* selectedOffset = nullptr;

	switch (m_offsetVectorSelection) {

	case OFFSET_VECTOR_Y:
		selectedOffset = &state.offsetY;
		break;

	case OFFSET_VECTOR_Z:
		selectedOffset = &state.offsetZ;
		break;

	default:
	case OFFSET_VECTOR_X:
		selectedOffset = &state.offsetX;
		break;
	}

	*selectedOffset += delta;

	// Keep values stable after repeated additions/subtractions.
	// This prevents values such as 0.029999997 from accumulating.
	*selectedOffset =
		roundf(*selectedOffset * 1000000.0f) /
		1000000.0f;

	if (fabsf(*selectedOffset) < 0.0000005f) {
		*selectedOffset = 0.0f;
	}
}
void TheArbiter::resetObjectOffset() {
	
	VolumeObjectState& state =
		activeVolumeState();

	state.offsetX = 0.0f;
	state.offsetY = 0.0f;
	state.offsetZ = 0.0f;
}

void TheArbiter::setObjectEditMode(ObjectEditMode mode) {
	m_objectEditMode = mode;
	m_objectTransformMode = TRANSFORM_SCALE;
}
void TheArbiter::resetObjectScale() {

	VolumeObjectState& state = activeVolumeState();

	state.scaleWhole = 1.0f;
	state.scaleX = 1.0f;
	state.scaleY = 1.0f;
	state.scaleZ = 1.0f;
}

void TheArbiter::setObjectRotationMode(ObjectRotationMode mode) {
	m_objectRotationMode = mode;
	m_objectTransformMode = TRANSFORM_ROTATION;
}
void TheArbiter::resetObjectRotation() {
	VolumeObjectState& state = activeVolumeState();

	state.pitchDeg = 0.0f;
	state.yawDeg = 0.0f;
	state.rollDeg = 0.0f;
}

void TheArbiter::increaseObjectRotation() {
	const float step =
		static_cast<float>(getRotationAngleIncrementDeg());

	VolumeObjectState& state = activeVolumeState();

	switch (m_objectRotationMode) {

	case ROTATE_YAW:
		state.yawDeg += step;
		break;

	case ROTATE_ROLL:
		state.rollDeg += step;
		break;

	default:
	case ROTATE_PITCH:
		state.pitchDeg += step;
		break;
	}
}
void TheArbiter::decreaseObjectRotation() {
	const float step =
		static_cast<float>(getRotationAngleIncrementDeg());

	VolumeObjectState& state = activeVolumeState();

	switch (m_objectRotationMode) {

	case ROTATE_YAW:
		state.yawDeg -= step;
		break;

	case ROTATE_ROLL:
		state.rollDeg -= step;
		break;

	default:
	case ROTATE_PITCH:
		state.pitchDeg -= step;
		break;
	}
}

void TheArbiter::increaseVolumePrimitiveScale() {

	constexpr float kScaleStep = 0.05f;

	VolumeObjectState& state = activeVolumeState();

	switch (m_objectEditMode) {

	case EDIT_SCALE_X:
		state.scaleX += kScaleStep;
		break;

	case EDIT_SCALE_Y:
		state.scaleY += kScaleStep;
		break;

	case EDIT_SCALE_Z:
		state.scaleZ += kScaleStep;
		break;

	default:
	case EDIT_SCALE_WHOLE:
		state.scaleWhole += kScaleStep;
		break;
	}
}
void TheArbiter::decreaseVolumePrimitiveScale() {
	constexpr float step = 0.05f;
	constexpr float minScale = 0.10f;

	VolumeObjectState& state = activeVolumeState();

	switch (m_objectEditMode) {

	case EDIT_SCALE_X:
		state.scaleX -= step;
		if (state.scaleX < minScale) {
			state.scaleX = minScale;
		}
		break;

	case EDIT_SCALE_Y:
		state.scaleY -= step;
		if (state.scaleY < minScale) {
			state.scaleY = minScale;
		}
		break;

	case EDIT_SCALE_Z:
		state.scaleZ -= step;
		if (state.scaleZ < minScale) {
			state.scaleZ = minScale;
		}
		break;

	default:
	case EDIT_SCALE_WHOLE:
		state.scaleWhole -= step;
		if (state.scaleWhole < minScale) {
			state.scaleWhole = minScale;
		}
		break;
	}
}

void TheArbiter::resetObjectBasis() {

	VolumeObjectState& state = activeVolumeState();

	state.basis.xAxis = { 1.0f, 0.0f, 0.0f };
	state.basis.yAxis = { 0.0f, 1.0f, 0.0f };
	state.basis.zAxis = { 0.0f, 0.0f, 1.0f };
}

TheArbiter::ObjectBasis
TheArbiter::getEffectiveObjectBasis() const {

	const VolumeObjectState& state = activeVolumeState();

	const BasisVector localX =
		rotateLocalVectorXYZ(
			{ 1.0f, 0.0f, 0.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg
		);

	const BasisVector localY =
		rotateLocalVectorXYZ(
			{ 0.0f, 1.0f, 0.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg
		);

	const BasisVector localZ =
		rotateLocalVectorXYZ(
			{ 0.0f, 0.0f, 1.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg
		);

	ObjectBasis effectiveBasis{
		transformByBasis(state.basis, localX),
		transformByBasis(state.basis, localY),
		transformByBasis(state.basis, localZ)
	};

	return orthonormalizeBasis(effectiveBasis);
}

void TheArbiter::commitObjectRotationToBasis() {
	// ---------------------------------------------------------
	// Commit active volume rotation into its local baked basis.
	//
	// This is intentionally explicit instead of calling
	// getEffectiveObjectBasis(), so the target state is captured
	// once and only once.
	//
	// If Edit { VOLUME_0 } is active:
	//     VOLUME_0 basis is rebased.
	//
	// If Edit { VOLUME_1 } is active:
	//     VOLUME_1 / brush basis is rebased.
	// ---------------------------------------------------------
	VolumeObjectState& state = activeVolumeState();

	const BasisVector localX = 
		rotateLocalVectorXYZ({ 1.0f, 0.0f, 0.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg);

	const BasisVector localY =
		rotateLocalVectorXYZ({ 0.0f, 1.0f, 0.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg);

	const BasisVector localZ =
		rotateLocalVectorXYZ({ 0.0f, 0.0f, 1.0f },
			state.pitchDeg,
			state.yawDeg,
			state.rollDeg);

	const ObjectBasis committedBasis{
		transformByBasis(state.basis, localX),
		transformByBasis(state.basis, localY),
		transformByBasis(state.basis, localZ)
	};

	state.basis = orthonormalizeBasis(committedBasis);
	
	// Editable rotation delta is now baked into the base
	state.pitchDeg = 0.0f;
	state.yawDeg = 0.0f;
	state.rollDeg = 0.0f;

	// After brush-base commit, put the user back into scale mode
	// next W/S test immediately verifies the new local basis
	m_objectTransformMode = TRANSFORM_SCALE;
}
void TheArbiter::commitBrushBase() {
	if (!hasInjectionVoxelSelected() || !isEditingInjectionVoxel1()) {

		commitObjectRotationToBasis();
		return;
	}

	VolumeObjectState& state = m_volume1State;

	// Remember the real procedural primitive before the UI token
	// is changed to BASE.
	if (state.primitive != VOLUME_PRIMITIVE_BASE) {
		state.brushBasePrimitive = state.primitive;
	}
	else if (state.brushBasePrimitive == VOLUME_PRIMITIVE_BASE) {
		state.brushBasePrimitive = VOLUME_PRIMITIVE_SPHERE;
	}

	commitObjectRotationToBasis();

	state.primitive = VOLUME_PRIMITIVE_BASE;
	state.brushBaseReady = true;

	m_objectTransformMode = TRANSFORM_SCALE;
}

TheArbiter::ArbiterResult
TheArbiter::commitObjectBasisAndReturnToPreview() {

	ArbiterResult result;

	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_APPLY_TO_BASE ||
		!canApplyVolumeToBase()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;

		return result;
	}

	// Request that EuclidEngine commit the exact current
	// working field. Remain in Node_3 until the engine
	// confirms success by calling finalizeVoxelBaseCommit().
	result.commitVolumeFuse =
		true;

	result.command =
		CMD_REDRAW;

	result.requestRedraw =
		true;

	result.rebuildMenu =
		true;

	result.regenerateVolume =
		false;

	return result;
}
void TheArbiter::finalizeVoxelBaseCommit() {
	// The engine calls this only after commitSPWorkingVolume()
	// has successfully copied the working field into BASE.
	m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
	m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;
	m_volumeInjectionVoxel = INJECTION_VOXEL_NONE;
	m_volumeEditTarget = VOLUME_EDIT_TARGET_VOXEL_0;
	m_objectEditMode = EDIT_SCALE_WHOLE;
	m_objectRotationMode = ROTATE_PITCH;
	m_objectTransformMode = TRANSFORM_SCALE;

	resetObjectOffset();
	// The committed result is now the anchor/base.
	resetVolumeState(m_volume0State, VOLUME_PRIMITIVE_BASE);
	// The next injection brush starts fresh.
	resetVolumeState(m_volume1State, VOLUME_PRIMITIVE_SPHERE);
	
}

int TheArbiter::getParticleConfigListCount() const {
	return PARTICLE_LIST_COUNT;
}
void TheArbiter::moveParticleConfigCursorUp() {
	if (isSingleParticleSelected()) {
		int v = static_cast<int>(m_activeParticleConfigList);
		v = (v + PARTICLE_LIST_COUNT - 1) % PARTICLE_LIST_COUNT;
		m_activeParticleConfigList = static_cast<ParticleConfigList>(v);
		return;
	}

	// PARTICLES_3D visible order:
	// COLOR -> RESET -> RUN
	if (m_activeParticleConfigList == PARTICLE_LIST_COLOR) {
		m_activeParticleConfigList = PARTICLE_LIST_RUN;
	}
	else if (m_activeParticleConfigList == PARTICLE_LIST_RESET) {
		m_activeParticleConfigList = PARTICLE_LIST_COLOR;
	}
	else {
		m_activeParticleConfigList = PARTICLE_LIST_RESET;
	}
}
void TheArbiter::moveParticleConfigCursorDown() {
	if (isSingleParticleSelected()) {
		int v = static_cast<int>(m_activeParticleConfigList);
		v = (v + 1) % PARTICLE_LIST_COUNT;
		m_activeParticleConfigList = static_cast<ParticleConfigList>(v);
		return;
	}

	// PARTICLES_3D visible order:
	// COLOR -> RESET -> RUN
	if (m_activeParticleConfigList == PARTICLE_LIST_COLOR) {
		m_activeParticleConfigList = PARTICLE_LIST_RESET;
	}
	else if (m_activeParticleConfigList == PARTICLE_LIST_RESET) {
		m_activeParticleConfigList = PARTICLE_LIST_RUN;
	}
	else {
		m_activeParticleConfigList = PARTICLE_LIST_COLOR;
	}
}

void TheArbiter::resetToMenu() {
	resetNavigationState();
	m_particleColorSelection = PARTICLE_COLOR_RED;
	m_particleResetMode = PARTICLE_RESET_DEFAULT;
	m_particleRenderMode = PARTICLE_RENDER_DEFAULT;
	m_activeParticleConfigList = PARTICLE_LIST_COLOR;
	m_activeSubLayerPanelItem = 0;
	m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
	m_volumeInjectionVoxel = INJECTION_VOXEL_NONE;
	m_volumeEditTarget = VOLUME_EDIT_TARGET_VOXEL_0;
	m_volumeInjectionMode = VOLUME_FUSE;
	m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;

	m_volumePrimitiveSelection = VOLUME_PRIMITIVE_SPHERE;
	m_objectEditMode = EDIT_SCALE_WHOLE;
	m_objectRotationMode = ROTATE_PITCH;
	m_objectTransformMode = TRANSFORM_SCALE;
	m_offsetVectorSelection = OFFSET_VECTOR_X;
	
	resetObjectScale();
	resetObjectRotation();
	resetObjectBasis();
	resetObjectOffset();
	resetAllVolumeStates();

	m_particleRadius = kParticleRadiusDefault;
	m_subLayerPanelOpen = false;
	m_selectedParticle = false;
	m_hoverValid = false;
	m_workplaneSlice = 0;
	m_hoverX = 0.0f;
	m_hoverY = 0.0f;
	m_rotationAngleIncrementIndex = 0;
	m_offsetIncrementIndex = 0;

	m_offsetIncrement = 
		kOffsetIncrementValues[m_offsetIncrementIndex];

	m_volumeBoundarySensorReady = false;
	m_volumeBoundaryUnsafeCount = 0;

	//m_offsetDistance = 0.0f;
}
void TheArbiter::updateHoverFromScreen(int x, int y, int w, int h) {
	if (w <= 0 || h <= 0) return;

	const float nx =
		(2.0f * static_cast<float>(x) / static_cast<float>(w)) - 1.0f;

	const float ny =
		1.0f - (2.0f * static_cast<float>(y) / static_cast<float>(h));

	// Placeholder screen-to-workplane mapping.
	// The visual workspace is currently -2..+2 in X/Y.
	const float halfBox = 2.0f;

	m_hoverX = nx * halfBox;
	m_hoverY = ny * halfBox;
	m_hoverValid = true;
}

TheArbiter::ArbiterResult
TheArbiter::processKeyboard(const KeyboardInput::KeyEvent& event) {
	ArbiterResult result;

	if (event.signal == KeyboardInput::KEY_ESCAPE) {
		result.command = CMD_EXIT;
		return result;
	}

	// Global layer retreat. Layer 0 absorbs Q.
	// SINGLE_PARTICLE sub-layer 1 has one extra rule:
	// if particle 0 is selected, Q clears selection first.
	// If nothing is selected, Q goes back one layer/sub-layer as usual.
	if (event.signal == KeyboardInput::KEY_Q) {
		if (isWorkplaneParticleSelectSubLayer() &&
			m_selectedParticle) {

			m_selectedParticle = false;

			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			result.rebuildMenu = true;
			return result;
		}

		goBackOneLayer(result);
		return result;
	}

	switch (m_navigation.layer) {
	case ApplicationLayer::GLOBAL_SHELL:
		handleGlobalShellKeyboard(event, result);
		break;

	case ApplicationLayer::DOMAIN_SELECTION:
		handleDomainSelectionKeyboard(event, result);
		break;

	case ApplicationLayer::WORKSPACE_CONFIGURATION:
		handleWorkspaceConfigurationKeyboard(event, result);
		break;

	case ApplicationLayer::ACTIVE_WORKSPACE:
		if (isSingleParticleSelected()) {
			if (event.signal == KeyboardInput::KEY_TAB) {
				toggleSubLayerPanel(result);
				break;
			}

			if (m_subLayerPanelOpen && isSubLayerPanelEligible()) {
				switch (event.signal) {
				case KeyboardInput::KEY_W:
					moveSubLayerPanelCursorUp(result);
					break;

				case KeyboardInput::KEY_S:
					moveSubLayerPanelCursorDown(result);
					break;

				case KeyboardInput::KEY_A:
					handleSubLayerPanelAdjust(-1.0f, result);
					break;

				case KeyboardInput::KEY_D:
					handleSubLayerPanelAdjust(+1.0f, result);
					break;

				case KeyboardInput::KEY_E:
					activateSubLayerPanelItem(result);
					break;

				default:
					break;
				}

				if (result.command != CMD_NONE ||
					result.requestRedraw ||
					result.regenerateVolume ||
					result.exportObjRequested ||
					result.goToSubLayer0) {

					break;
				}
			}

			switch (event.signal) {
			case KeyboardInput::KEY_E:
				advanceSingleParticleSubLayer(result);
				break;

			case KeyboardInput::KEY_W:
				if (isWorkplaneParticleSelectSubLayer()) {

					adjustWorkplaneSlice(1, result);
				}
				else if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER && 
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {

					increaseVolumePrimitiveScale();

					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;

				}
				else if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_OFFSET_OBJECT &&
					!m_subLayerPanelOpen) {

					// W moves in the positive selected world/volume axis.
					adjustObjectOffset(+1.0f);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				break;

			case KeyboardInput::KEY_S:
				if (isWorkplaneParticleSelectSubLayer()) {

					adjustWorkplaneSlice(-1, result);
				}
				else if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {

					decreaseVolumePrimitiveScale();

					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				else if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_OFFSET_OBJECT &&
					!m_subLayerPanelOpen) {

					// S moves in the negative selected world/volume axis.
					adjustObjectOffset(-1.0f);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				break;

			case KeyboardInput::KEY_A:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {

					decreaseObjectRotation();

					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				break;

			case KeyboardInput::KEY_D:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {

					increaseObjectRotation();

					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				break;

				// Retire the old +/- transform aliases.
				// Scaling is W/S and rotation is A/D.
			case KeyboardInput::KEY_PLUS:
			case KeyboardInput::KEY_MINUS:
				break;

			case KeyboardInput::KEY_1:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectEditMode(EDIT_SCALE_WHOLE);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;

			case KeyboardInput::KEY_2:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectEditMode(EDIT_SCALE_Z);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;

			case KeyboardInput::KEY_3:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectEditMode(EDIT_SCALE_Y);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;

			case KeyboardInput::KEY_4:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectEditMode(EDIT_SCALE_X);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;


			case KeyboardInput::KEY_5:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectRotationMode(ROTATE_PITCH);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;

			case KeyboardInput::KEY_6:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectRotationMode(ROTATE_YAW);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;

			case KeyboardInput::KEY_7:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					setObjectRotationMode(ROTATE_ROLL);
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
				}
				break;

			case KeyboardInput::KEY_8:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					resetObjectRotation();
					m_objectTransformMode = TRANSFORM_ROTATION;
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				break;

			case KeyboardInput::KEY_0:
				if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
					m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {
					m_objectTransformMode = TRANSFORM_SCALE;
					resetObjectScale();
					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.regenerateVolume = true;
				}
				break;

			default:
				break;
			}

			break;
		}

		switch (event.signal) {
		case KeyboardInput::KEY_SPACE:
			result.command = CMD_TOGGLE_PAUSE;
			result.requestRedraw = true;
			break;

		case KeyboardInput::KEY_ENTER:
			result.command = CMD_STEP_SIMULATION;
			result.requestRedraw = true;
			break;

		default:
			break;
		}
		break;
	case ApplicationLayer::COUNT:
		break;
	}

	return result;
}

void TheArbiter::handleGlobalShellKeyboard(
	const KeyboardInput::KeyEvent& event,
	ArbiterResult& result) {
	switch (event.signal) {
	case KeyboardInput::KEY_A:
	case KeyboardInput::KEY_D:
		toggleEnvironmentSelection();
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		break;

	case KeyboardInput::KEY_E:
		enterCurrentSelection(result);
		break;

	default:
		break;
	}
}

void TheArbiter::handleDomainSelectionKeyboard(
	const KeyboardInput::KeyEvent& event,
	ArbiterResult& result) {
	switch (event.signal) {
	case KeyboardInput::KEY_A:
	case KeyboardInput::KEY_D:
		toggleGridSelection();
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		break;

	case KeyboardInput::KEY_E:
		enterCurrentSelection(result);
		break;

	default:
		break;
	}
}

void TheArbiter::handleWorkspaceConfigurationKeyboard(
	const KeyboardInput::KeyEvent& event,
	ArbiterResult& result) {
	switch (event.signal) {
	case KeyboardInput::KEY_W:
		moveParticleConfigCursorUp();
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		break;

	case KeyboardInput::KEY_S:
		moveParticleConfigCursorDown();
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		break;

	case KeyboardInput::KEY_A:
		handleParticleConfigAdjust(-1.0f, result);
		break;

	case KeyboardInput::KEY_D:
		handleParticleConfigAdjust(+1.0f, result);
		break;

	case KeyboardInput::KEY_E:
		enterCurrentSelection(result);
		break;

	default:
		break;
	}
}

bool TheArbiter::setVolumeBoundaryStatus(
	bool sensorReady,
	unsigned int unsafeCount) {
	
	const bool changed =
		m_volumeBoundarySensorReady != sensorReady ||
		m_volumeBoundaryUnsafeCount != unsafeCount;

	m_volumeBoundarySensorReady = sensorReady;

	m_volumeBoundaryUnsafeCount =
		sensorReady
		? unsafeCount : 0;

	return changed;
}
bool TheArbiter::isSubLayerPanelItemSelectable(int item) const {
	if (m_volumeAssemblyNode == VOLUME_NODE_OFFSET_OBJECT) {
		if (hasInjectionVoxelSelected()) {
			if (isEditingInjectionVoxel0() && item == INJECTION_OFFSET_LIST_APPLY_TO_BASE)
				return canApplyVolumeToBase();

			return true;
		}

		if (item == OFFSET_LIST_APPLY_TO_BASE)
			return canApplyVolumeToBase();
	}
		
	return true;

}

int TheArbiter::getActiveSubLayerPanelItemCount() const {
	if (isMarchingCubesSubLayer()) return MC_LIST_COUNT;

	switch (m_volumeAssemblyNode) {

	case VOLUME_NODE_EDIT_OBJECT:
		if (hasInjectionVoxelSelected()) {
			if (m_volumeEditTarget == VOLUME_EDIT_TARGET_VOXEL_1) {

				return INJECTION_EDIT_LIST_VOXEL1_COUNT;
			}

			return INJECTION_EDIT_LIST_VOXEL0_COUNT;
		}

		return EDIT_LIST_COUNT;

	case VOLUME_NODE_OFFSET_OBJECT:
		if (hasInjectionVoxelSelected()) {

			if (isEditingInjectionVoxel1()) {
				return INJECTION_OFFSET_LIST_VOXEL1_COUNT;
			}
			return INJECTION_OFFSET_LIST_VOXEL0_COUNT;
		}
		return OFFSET_LIST_COUNT;

	case VOLUME_NODE_APPLY_TO_BASE:
		return APPLY_LIST_COUNT;

	default:
	case VOLUME_NODE_PREVIEW:
		return PREVIEW_LIST_COUNT;
	}
}

const char* TheArbiter::getLayerName() const {
	switch (m_navigation.layer) {
	case ApplicationLayer::GLOBAL_SHELL:
		return "LAYER_0_MENU";

	case ApplicationLayer::DOMAIN_SELECTION:
		return "LAYER_1_ENVIRONMENT_CONFIGURATION";

	case ApplicationLayer::WORKSPACE_CONFIGURATION:
		return "LAYER_2_3D_GRID_MODE_CONFIGURATION";

	case ApplicationLayer::ACTIVE_WORKSPACE:
		return "LAYER_3_SIMULATION_RUN";

	default:
	case ApplicationLayer::COUNT:
		return "UNKNOWN_LAYER";
	}
}
const char* TheArbiter::getEnvironmentName() const {
	switch (getEnvironmentSelection()) {
	case ENV_IDLE:
		return "IDLE";

	case ENV_3D_GRID:
		return "3D_GRID";

	default:
		return "UNKNOWN_ENVIRONMENT";
	}
}
const char* TheArbiter::getGridSelectionName() const {
	switch (getGridSelection()) {
	case GRID_GRAPH_3D:
		return "GRAPH_3D";

	case GRID_SINGLE_PARTICLE:
		return "SINGLE_PARTICLE";

	case GRID_PARTICLES_3D:
		return "PARTICLES_3D";

	default:
		return "UNKNOWN_GRID_SELECTION";
	}
}
const char* TheArbiter::getParticleColorName() const {
	switch (m_particleColorSelection) {
	case PARTICLE_COLOR_RED:
		return "RED";

	case PARTICLE_COLOR_BLUE:
		return "BLUE";

	case PARTICLE_COLOR_GREEN:
		return "GREEN";

	default:
		return "UNKNOWN_COLOR";
	}
}
const char* TheArbiter::getParticleResetModeName() const {
	switch (m_particleResetMode) {
	case PARTICLE_RESET_DEFAULT:
		return "DEFAULT";

	case PARTICLE_RESET_RANDOM:
		return "RANDOM";

	default:
		return "UNKNOWN_RESET_MODE";
	}
}
const char* TheArbiter::getActiveParticleConfigListName() const {
	switch (m_activeParticleConfigList) {
	case PARTICLE_LIST_COLOR:
		return "PARTICLE COLOR";

	case PARTICLE_LIST_RADIUS:
		return isSingleParticleSelected()
			? "PARTICLE RADIUS"
			: "PARTICLE RESET MODE";

	case PARTICLE_LIST_RENDER_MODE:
		return "PARTICLE RENDER MODE";

	case PARTICLE_LIST_RUN:
		return isSingleParticleSelected()
			? "RUN SIMULATION LAYER"
			: "RUN PARTICLES";

	default:
		return "UNKNOWN_PARTICLE_LIST";
	}
}
const char* TheArbiter::getSingleParticleSubLayerName() const {
	switch (m_singleParticleSubLayer) {
	case SP_SUB_LAYER_REFERENCE:
		return "SUB_LAYER_0_SINGLE_PARTICLE_REFERENCE";

	case SP_SUB_LAYER_SHAPE_EDIT:
		return "SUB_LAYER_1_SHAPE_SELECTION_AND_EDIT";

	case SP_SUB_LAYER_VOLUME_RENDER:
		return "SUB_LAYER_2_VOLUME_RENDER_MODE";

	case SP_SUB_LAYER_MARCHING_CUBES:
		return "SUB_LAYER_3_MARCHING_CUBES_MODE";

	default:
		return "UNKNOWN_SINGLE_PARTICLE_SUB_LAYER";
	}
}
const char* TheArbiter::getVolumePrimitiveName() const {
	switch (getVolumePrimitiveSelection()) {
	case VOLUME_PRIMITIVE_SPHERE:
		return "SPHERE";

	case VOLUME_PRIMITIVE_TORUS:
		return "TORUS";

	case VOLUME_PRIMITIVE_BLOCK:
		return "BLOCK";

	case VOLUME_PRIMITIVE_CYLINDER:
		return "CYLINDER";

	case VOLUME_PRIMITIVE_CAPSULE:
		return "CAPSULE";

	case VOLUME_PRIMITIVE_WEDGE:
		return "WEDGE";

	case VOLUME_PRIMITIVE_FRUSTUM:
		return "FRUSTUM";

	case VOLUME_PRIMITIVE_BASE:
		return "BASE";

	default:
		return "UNKNOWN_VOLUME_PRIMITIVE";
	}
}
const char* TheArbiter::getParticleRenderModeName() const {
	switch (m_particleRenderMode) {
	case PARTICLE_RENDER_DEFAULT:
		return "DEFAULT";

	case PARTICLE_RENDER_MESH:
		return "MESH";

	default:
		return "UNKNOWN_RENDER_MODE";
	}
}
const char* TheArbiter::getObjectEditModeName() const {
	switch (m_objectEditMode) {
	case EDIT_SCALE_WHOLE:
		return "Scale whole object";

	case EDIT_SCALE_Z:
		return "Scale z-axis";

	case EDIT_SCALE_Y:
		return "Scale y-axis";

	case EDIT_SCALE_X:
		return "Scale x-axis";

	default:
		return "Unknown edit mode";
	}
}
const char* TheArbiter::getObjectRotationModeName() const {
	switch (m_objectRotationMode) {
	case ROTATE_YAW:
		return "Yaw";

	case ROTATE_ROLL:
		return "Roll";

	default:
	case ROTATE_PITCH:
		return "Pitch";
	}
}
const char* TheArbiter::getObjectTransformModeName() const {
	switch (m_objectTransformMode) {
	case TRANSFORM_ROTATION:
		return "Rotation";

	default:
	case TRANSFORM_SCALE:
		return "Scale";
	}
}
const char* TheArbiter::getSubLayerPanelListName() const {
	if (isMarchingCubesSubLayer()) {
		switch (m_activeSubLayerPanelItem) {
		case MC_LIST_EXPORT_OBJ: return "Export .OBJ";
		case MC_LIST_TO_SUB_LAYER_2: return "To Sub-Layer 2";
		case MC_LIST_TO_SUB_LAYER_0: return "To Sub-Layer 0";
		default: return "MC panel item";
		}
	}

	switch (m_volumeAssemblyNode) {
	case VOLUME_NODE_EDIT_OBJECT:
		switch (m_activeSubLayerPanelItem) {
		case EDIT_LIST_OBJECT: return "Select object";
		case EDIT_LIST_ROTATION_INCREMENT: return "Rotation increment";
		case EDIT_LIST_OFFSET_OBJECT: return "Offset object";
		case EDIT_LIST_PREVIEW_OBJECT: return "Preview object";
		default: return "Edit panel item";
		}

	case VOLUME_NODE_OFFSET_OBJECT:

		if (hasInjectionVoxelSelected()) {

			switch (m_activeSubLayerPanelItem) {

			case INJECTION_OFFSET_LIST_TARGET:
				return "Offset edit target";

			case INJECTION_OFFSET_LIST_VECTOR:
				return "Offset vector";

			case INJECTION_OFFSET_LIST_DISTANCE:
				return "Offset increment";

			case INJECTION_OFFSET_LIST_RAIL:
				return isEditingInjectionVoxel0()
					? "Injection rail"
					: "Injection mode";

			case INJECTION_OFFSET_LIST_APPLY_TO_BASE:
				return "Apply to base";

			case INJECTION_OFFSET_LIST_EDIT_OBJECT:
				return "Edit volume";

			default:
				return "Injection offset panel item";
			}
		}

		switch (m_activeSubLayerPanelItem) {

		case OFFSET_LIST_VECTOR:
			return "Offset vector";

		case OFFSET_LIST_DISTANCE:
			return "Offset increment";

		case OFFSET_LIST_APPLY_TO_BASE:
			return "Apply to base";

		case OFFSET_LIST_EDIT_OBJECT:
			return "Edit object";

		default:
			return "Offset panel item";
		}

	
	case VOLUME_NODE_APPLY_TO_BASE:
		switch (m_activeSubLayerPanelItem) {
		case APPLY_LIST_COMMIT: return "Commit and preview";
		case APPLY_LIST_OFFSET_OBJECT: return "Offset object";
		case APPLY_LIST_CANCEL_TO_PREVIEW: return "Cancel to preview";
		default: return "Apply panel item";
		}

	default:
	case VOLUME_NODE_PREVIEW:
		switch (m_activeSubLayerPanelItem) {
		case PREVIEW_LIST_INJECTION_MODE: return "Injection mode";
		case PREVIEW_LIST_EDIT_OBJECT: return "Edit object";
		case PREVIEW_LIST_RUN_MC: return "Run MC mode";
		default: return "Preview panel item";
		}
	}
}
const char* TheArbiter::getVolumeAssemblyNodeName() const {
	switch (m_volumeAssemblyNode) {
	case VOLUME_NODE_EDIT_OBJECT:
		return "Node_1: Edit Object";
	case VOLUME_NODE_OFFSET_OBJECT:
		return "Node_2: Offset Object";
	case VOLUME_NODE_APPLY_TO_BASE:
		return "Node_3: Apply To Base";
	default:
	case VOLUME_NODE_PREVIEW:
		return "Node_0: Preview Object";
	}
}
const char* TheArbiter::getVolumeInjectionVoxelName() const {
	switch (m_volumeInjectionVoxel) {
	case INJECTION_VOXEL_211: return "VOXEL_211";
	case INJECTION_VOXEL_121: return "VOXEL_121";
	case INJECTION_VOXEL_011: return "VOXEL_011";
	case INJECTION_VOXEL_112: return "VOXEL_112";
	case INJECTION_VOXEL_101: return "VOXEL_101";
	case INJECTION_VOXEL_110: return "VOXEL_110";
	case INJECTION_VOXEL_120: return "VOXEL_120";
	case INJECTION_VOXEL_221: return "VOXEL_221";
	case INJECTION_VOXEL_122: return "VOXEL_122";
	case INJECTION_VOXEL_021: return "VOXEL_021";
	case INJECTION_VOXEL_201: return "VOXEL_201";
	case INJECTION_VOXEL_100: return "VOXEL_100";
	case INJECTION_VOXEL_001: return "VOXEL_001";
	case INJECTION_VOXEL_102: return "VOXEL_102";
	case INJECTION_VOXEL_010: return "VOXEL_010";
	case INJECTION_VOXEL_210: return "VOXEL_210";
	case INJECTION_VOXEL_012: return "VOXEL_012";
	case INJECTION_VOXEL_212: return "VOXEL_212";
	case INJECTION_VOXEL_202: return "VOXEL_202";
	case INJECTION_VOXEL_020: return "VOXEL_020";
	case INJECTION_VOXEL_220: return "VOXEL_220";
	case INJECTION_VOXEL_002: return "VOXEL_002";
	case INJECTION_VOXEL_200: return "VOXEL_200";
	case INJECTION_VOXEL_022: return "VOXEL_022";
	case INJECTION_VOXEL_222: return "VOXEL_222";
	case INJECTION_VOXEL_000: return "VOXEL_000";

	default:
	case INJECTION_VOXEL_NONE: return "NONE";
	}
}
const char* TheArbiter::getVolumeInjectionModeName() const {
	switch (m_volumeInjectionMode) {
	case VOLUME_CUT:
		return "CUT";

	default:
	case VOLUME_FUSE:
		return "FUSE";
	}
}
const char* TheArbiter::getVolumeEditTargetName() const {
	switch (m_volumeEditTarget) {
	case VOLUME_EDIT_TARGET_VOXEL_1:
		return "VOLUME_1";

	default:
	case VOLUME_EDIT_TARGET_VOXEL_0:
		return "VOLUME_0";
	}
}
const char* TheArbiter::getVolumeEditTargetObjectName() const {
	// Checkpoint 3C:
	//
	// List [2] now reflects the active authoring state.
	//
	// Edit { VOLUME_0 } -> m_volume0State.primitive
	// Edit { VOLUME_1 } -> m_volume1State.primitive

	return getVolumePrimitiveName();
}
const char* TheArbiter::getOffsetVectorName() const {
	switch (m_offsetVectorSelection) {

	case OFFSET_VECTOR_Y:
		return "Y-VECTOR";

	case OFFSET_VECTOR_Z:
		return "Z-VECTOR";

	default:
	case OFFSET_VECTOR_X:
		return "X-VECTOR";
	}
}
const char* TheArbiter::getOffsetIncrementName() const {
	int index = m_offsetIncrementIndex;

	if (index < 0 || index >= kOffsetIncrementCount) {

		index = 0;
	}

	return kOffsetIncrementNames[index];
}

TheArbiter::ArbiterResult
TheArbiter::trySelectParticleAtCurrentSlice() {

	ArbiterResult result;
	if (!isWorkplaneParticleSelectSubLayer()) return result;

	// Particle 0 is anchored at the origin for this phase.
	// A click only toggles selection if the active workplane is near
	// z = 0 and the mouse hover is close to the particle center.
	const bool sliceNearOrigin =
		abs(m_workplaneSlice) <= 1;

	const float pickRadius = 0.35f;

	const bool hoverNearOrigin =
		m_hoverValid &&
		((m_hoverX * m_hoverX + m_hoverY * m_hoverY) <=
			(pickRadius * pickRadius));

	if (sliceNearOrigin && hoverNearOrigin) {
		m_selectedParticle = !m_selectedParticle;

		result.command =
			m_selectedParticle
			? CMD_SELECT_PARTICLE
			: CMD_REDRAW;

		result.requestRedraw = true;
		result.rebuildMenu = true;
		return result;
	}

	// Clicking away from the particle does not toggle.
	result.command = CMD_REDRAW;
	result.requestRedraw = true;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setVolumeAssemblyNode(VolumeAssemblyNode node) {

	ArbiterResult result;
	const int nodeValue = static_cast<int>(node);

	const bool validNode =
		nodeValue >= static_cast<int>(VOLUME_NODE_PREVIEW) &&
		nodeValue < static_cast<int>(VOLUME_NODE_COUNT);

	if (!isVolumeRenderSubLayer() || !validNode) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	if (node == VOLUME_NODE_APPLY_TO_BASE && !canApplyVolumeToBase()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return result;
	}

	m_volumeAssemblyNode = node;

	switch (m_volumeAssemblyNode) {

	case VOLUME_NODE_EDIT_OBJECT:
		m_activeSubLayerPanelItem =
			hasInjectionVoxelSelected()
			? INJECTION_EDIT_LIST_TARGET
			: EDIT_LIST_OBJECT;
		break;

	case VOLUME_NODE_OFFSET_OBJECT:
		m_activeSubLayerPanelItem =
			hasInjectionVoxelSelected()
			? INJECTION_OFFSET_LIST_TARGET
			: OFFSET_LIST_VECTOR;
		break;

	case VOLUME_NODE_APPLY_TO_BASE:
		m_activeSubLayerPanelItem = APPLY_LIST_COMMIT;
		break;

	default:
	case VOLUME_NODE_PREVIEW:
		m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;

	// Preview removes an uncommitted brush.
	// Edit displays the selected editable brush.
	result.regenerateVolume =
		node == VOLUME_NODE_PREVIEW ||
		node == VOLUME_NODE_EDIT_OBJECT;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setOffsetVectorSelection(
	OffsetVector vector) {

	ArbiterResult result;

	const int vectorValue =
		static_cast<int>(vector);

	const bool validVector =
		vectorValue >= static_cast<int>(OFFSET_VECTOR_X) &&
		vectorValue < static_cast<int>(OFFSET_VECTOR_COUNT);

	// Right-click offset-vector commands are only valid while
	// Sub-Layer 2 Node_2 owns the active editing context.
	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_OFFSET_OBJECT ||
		!validVector) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_offsetVectorSelection = vector;

	// Keep the side-panel cursor synchronized with the command
	// selected from the right-click menu.
	m_activeSubLayerPanelItem =
		hasInjectionVoxelSelected()
		? INJECTION_OFFSET_LIST_VECTOR
		: OFFSET_LIST_VECTOR;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;

	// Rebuild so the right-click menu can indicate the newly
	// selected vector.
	result.rebuildMenu = true;

	// Changing the movement axis does not alter the scalar field.
	// It only changes the active grid plane and future W/S motion.
	result.regenerateVolume = false;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::clearObjectOffsetFromMenu() {

	ArbiterResult result;

	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_OFFSET_OBJECT) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	resetObjectOffset();

	m_activeSubLayerPanelItem =
		hasInjectionVoxelSelected()
		? INJECTION_OFFSET_LIST_VECTOR
		: OFFSET_LIST_VECTOR;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;

	// Resetting X/Y/Z changes the scalar-field placement.
	result.regenerateVolume = true;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::toggleVolumeInjectionModeFromMenu() {

	ArbiterResult result;

	// The right-click command is valid only for:
	//
	//     SINGLE_PARTICLE Sub-Layer 2
	//     Node_2 Offset Object
	//     an active Injection Voxel
	//     VOLUME_1 / brush selected
	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_OFFSET_OBJECT ||
		!hasInjectionVoxelSelected() ||
		!isEditingInjectionVoxel1()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;

		return result;
	}

	cycleVolumeInjectionMode(+1.0f);

	// Keep the side-panel cursor synchronized with the
	// right-click menu action.
	m_activeSubLayerPanelItem =
		INJECTION_OFFSET_LIST_MODE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;

	// FUSE/CUT changes the active brush preview.
	result.regenerateVolume = true;

	// Rebuild the menu so its text immediately changes:
	//
	//     {FUSE} <-> {CUT}
	result.rebuildMenu = true;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::commitBrushBaseFromMenu() {

	ArbiterResult result;

	// Commit Brush is valid only while:
	//
	//     Sub-Layer 2 is active
	//     Node_1 owns the workflow
	//     an Injection Voxel is selected
	//     VOLUME_1 is the active edit target
	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_EDIT_OBJECT ||
		!hasInjectionVoxelSelected() ||
		!isEditingInjectionVoxel1()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;

		return result;
	}

	// Performs a brush-local basis commit only.
	// This does not fuse/cut the brush into VOLUME_0.
	commitBrushBase();

	// Keep the side-panel cursor synchronized with the
	// right-click command that was activated.
	m_activeSubLayerPanelItem =
		INJECTION_EDIT_LIST_COMMIT_BASE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.regenerateVolume = true;
	result.rebuildMenu = true;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::enterMarchingCubesFromPreview() {

	ArbiterResult result;

	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode !=
		VOLUME_NODE_PREVIEW) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_singleParticleSubLayer =
		SP_SUB_LAYER_MARCHING_CUBES;

	m_volumeAssemblyNode =
		VOLUME_NODE_PREVIEW;

	m_subLayerPanelOpen = true;

	m_activeSubLayerPanelItem =
		MC_LIST_EXPORT_OBJ;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	result.enterMarchingCubes = true;

	return result;
}

TheArbiter::ArbiterResult
TheArbiter::activateMarchingCubesPanelItemFromMenu(MarchingCubesPanelItem item) {

	ArbiterResult result;

	const int itemValue =
		static_cast<int>(item);

	const bool validItem =
		itemValue >=
		static_cast<int>(MC_LIST_EXPORT_OBJ) &&
		itemValue <
		static_cast<int>(MC_LIST_COUNT);

	if (!isMarchingCubesSubLayer() ||
		!validItem) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;

		return result;
	}

	// Synchronize the side-panel cursor with the menu action.
	m_activeSubLayerPanelItem = item;

	// Reuse the existing tested MC panel transition logic.
	activateSubLayerPanelItem(result);

	return result;
}

void TheArbiter::goBackOneLayer(ArbiterResult& result) {
	if (isMenuLayer()) {
		result.requestRedraw = true;
		return;
	}

	if (m_navigation.layer == ApplicationLayer::DOMAIN_SELECTION) {
		setApplicationLayer(ApplicationLayer::GLOBAL_SHELL);
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	if (m_navigation.layer == ApplicationLayer::WORKSPACE_CONFIGURATION) {
		setApplicationLayer(ApplicationLayer::DOMAIN_SELECTION);
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}
	if (m_navigation.layer == ApplicationLayer::ACTIVE_WORKSPACE) {
		if (isSingleParticleSelected()) {
			retreatSingleParticleSubLayer(result);
			return;
		}

		setApplicationLayer(ApplicationLayer::WORKSPACE_CONFIGURATION);
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}
}
void TheArbiter::enterCurrentSelection(ArbiterResult& result) {
	if (isMenuLayer()) {
		if (is3DGridSelected()) {
			setApplicationLayer(ApplicationLayer::DOMAIN_SELECTION);
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	if (m_navigation.layer == ApplicationLayer::DOMAIN_SELECTION) {
		if (isSingleParticleSelected() ||
			isParticlesSelected()) {

			setApplicationLayer(ApplicationLayer::WORKSPACE_CONFIGURATION);
			m_activeParticleConfigList = PARTICLE_LIST_COLOR;
		}

		// GRAPH_3D is intentionally reserved for a later pass.
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	if (m_navigation.layer == ApplicationLayer::WORKSPACE_CONFIGURATION) {
		if (isSingleParticleSelected()) {
			if (m_activeParticleConfigList == PARTICLE_LIST_RUN) {
				// LIST 4 is the run command for SINGLE_PARTICLE.
				// Place particle 0 at the origin, then enter Layer 3.
				setApplicationLayer(ApplicationLayer::ACTIVE_WORKSPACE);
				m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;

				m_selectedParticle = false;
				m_hoverValid = false;
				m_hoverX = 0.0f;
				m_hoverY = 0.0f;
				m_workplaneSlice = 0;

				result.command = CMD_PLACE_SINGLE_PARTICLE;
			}
			else {
				result.command = CMD_REDRAW;
			}

			result.requestRedraw = true;
			return;
		}

		// PARTICLES_3D path.
		if (m_activeParticleConfigList == PARTICLE_LIST_RUN) {
			setApplicationLayer(ApplicationLayer::ACTIVE_WORKSPACE);
			result.command = CMD_START_CUDA_SIMULATION;
		}
		else {
			result.command = CMD_REDRAW;
		}

		result.requestRedraw = true;
		return;
	}

	if (m_navigation.layer == ApplicationLayer::ACTIVE_WORKSPACE) {
		if (isSingleParticleSelected()) {
			advanceSingleParticleSubLayer(result);
			return;
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}
}
void TheArbiter::advanceSingleParticleSubLayer(ArbiterResult& result) {
	if (!isSimulationRunLayer() || !isSingleParticleSelected()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	if (m_singleParticleSubLayer == SP_SUB_LAYER_SHAPE_EDIT &&
		!m_selectedParticle) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// SUB_LAYER_2 owns its own four-node assembly loop.
	if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER) {
		switch (m_volumeAssemblyNode) {
		case VOLUME_NODE_EDIT_OBJECT:
			m_volumeAssemblyNode = VOLUME_NODE_OFFSET_OBJECT;
			break;

		case VOLUME_NODE_OFFSET_OBJECT:
			if (!canApplyVolumeToBase()) {
				result.command = CMD_REDRAW;
				result.requestRedraw = true;
				result.rebuildMenu = true;

				return;
			}
			
			m_volumeAssemblyNode = VOLUME_NODE_APPLY_TO_BASE;

			break;

		case VOLUME_NODE_APPLY_TO_BASE:
			// Checkpoint placeholder: complete the loop without committing CUDA CSG.
			result = commitObjectBasisAndReturnToPreview();
			break;

		default:
		case VOLUME_NODE_PREVIEW:
			// MC is reachable only from the Preview node.
			result = enterMarchingCubesFromPreview();
			return;
		}

		if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER) {
			m_activeSubLayerPanelItem = 0;
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// Legacy E shortcut from MC returns to the reference node.
	if (m_singleParticleSubLayer == SP_SUB_LAYER_MARCHING_CUBES) {
		m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;
		m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
		m_subLayerPanelOpen = false;
		m_activeSubLayerPanelItem = 0;

		m_selectedParticle = false;
		m_hoverValid = false;
		m_hoverX = 0.0f;
		m_hoverY = 0.0f;
		m_workplaneSlice = 0;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// Normal sub-layer traversal: 0 -> 1 -> 2.
	int v = static_cast<int>(m_singleParticleSubLayer);
	if (v < static_cast<int>(SP_SUB_LAYER_VOLUME_RENDER)) {
		++v;
		m_singleParticleSubLayer = static_cast<SingleParticleSubLayer>(v);

		if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER) {
			m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
			m_activeSubLayerPanelItem = 0;
		}
		else {
			m_subLayerPanelOpen = false;
		}
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::retreatSingleParticleSubLayer(ArbiterResult& result) {
	if (!isSimulationRunLayer() || !isSingleParticleSelected()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// MC retreats to SUB_LAYER_2 Preview rather than directly to Node_3.
	if (m_singleParticleSubLayer == SP_SUB_LAYER_MARCHING_CUBES) {
		m_singleParticleSubLayer = SP_SUB_LAYER_VOLUME_RENDER;
		m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
		m_subLayerPanelOpen = true;
		m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// Q walks backward through the assembly pipeline first.
	if (m_singleParticleSubLayer == SP_SUB_LAYER_VOLUME_RENDER &&
		m_volumeAssemblyNode != VOLUME_NODE_PREVIEW) {

		m_volumeAssemblyNode = static_cast<VolumeAssemblyNode>(
			static_cast<int>(m_volumeAssemblyNode) - 1
			);
		m_activeSubLayerPanelItem = 0;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	int v = static_cast<int>(m_singleParticleSubLayer);
	if (v > 0) {
		--v;
		m_singleParticleSubLayer = static_cast<SingleParticleSubLayer>(v);

		if (!isSubLayerPanelEligible()) {
			m_subLayerPanelOpen = false;
			m_activeSubLayerPanelItem = 0;
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// At sub-layer 0, Q exits back to application Layer 2.
	m_subLayerPanelOpen = false;
	m_activeSubLayerPanelItem = 0;
	m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
	setApplicationLayer(ApplicationLayer::WORKSPACE_CONFIGURATION);

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::adjustWorkplaneSlice(int delta, ArbiterResult& result) {
	if (!isWorkplaneParticleSelectSubLayer()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	m_workplaneSlice =
		std::max(-64, std::min(64, m_workplaneSlice + delta));

	// Particle 0 is at z = 0 for this placeholder picker.
	if (std::abs(m_workplaneSlice) > 1) {
		m_selectedParticle = false;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}
void TheArbiter::handleParticleConfigAdjust(float dir, ArbiterResult& result) {
	if (isSingleParticleSelected()) {
		switch (m_activeParticleConfigList) {
		case PARTICLE_LIST_COLOR:
			toggleParticleColorSelection();
			result.command = CMD_PARTICLE_CONFIG_CHANGED;
			result.particleConfigChanged = true;
			break;

		case PARTICLE_LIST_RADIUS:
			if (dir < 0.0f) {
				decreaseParticleRadius();
			}
			else {
				increaseParticleRadius();
			}

			result.command = CMD_PARTICLE_RADIUS_CHANGED;
			result.particleConfigChanged = true;
			result.particleRadiusChanged = true;
			break;

		case PARTICLE_LIST_RENDER_MODE:
			toggleParticleRenderMode();
			result.command = CMD_PARTICLE_RENDER_MODE_CHANGED;
			result.particleConfigChanged = true;
			result.particleRenderModeChanged = true;
			break;

		case PARTICLE_LIST_RUN:
		default:
			result.command = CMD_REDRAW;
			break;
		}

		result.requestRedraw = true;
		return;
	}
	else {
		switch (m_activeParticleConfigList) {
		case PARTICLE_LIST_COLOR:
			toggleParticleColorSelection();
			result.command = CMD_REDRAW;
			result.particleConfigChanged = true;
			break;

		case PARTICLE_LIST_RESET:
			toggleParticleResetMode();
			result.command = CMD_REDRAW;
			result.particleConfigChanged = true;
			break;

		case PARTICLE_LIST_RUN:
		default:
			result.command = CMD_REDRAW;
			break;
		}
	}

	result.rebuildMenu = true;
	result.requestRedraw = true;
}

void TheArbiter::toggleSubLayerPanel(ArbiterResult& result) {
	if (!isSubLayerPanelEligible()) {
		m_subLayerPanelOpen = false;
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	m_subLayerPanelOpen = !m_subLayerPanelOpen;
	if (m_subLayerPanelOpen) {
		m_activeSubLayerPanelItem = 0;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}
void TheArbiter::moveSubLayerPanelCursorUp(ArbiterResult& result) {
	const int count = getActiveSubLayerPanelItemCount();

	if (count > 0) {

		for (int attempt = 0; attempt < count; attempt++) {

			const int candidate =
				(m_activeSubLayerPanelItem + count - 1) % count;

			m_activeSubLayerPanelItem = candidate;

			if (isSubLayerPanelItemSelectable(candidate)) {

				break;
			}
		}
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}
void TheArbiter::moveSubLayerPanelCursorDown(ArbiterResult& result) {
	const int count = getActiveSubLayerPanelItemCount();

	if (count > 0) {

		for (int attempt = 0; attempt < count; attempt++) {

			const int candidate =
				(m_activeSubLayerPanelItem + 1) % count;

			m_activeSubLayerPanelItem = candidate;

			if (isSubLayerPanelItemSelectable(candidate)) {

				break;
			}
		}
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}
void TheArbiter::handleSubLayerPanelAdjust(float dir, ArbiterResult& result) {
	if (!m_subLayerPanelOpen || !isSubLayerPanelEligible()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// Sub-layer 3 currently contains action rows only.
	if (isMarchingCubesSubLayer()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// ---------------------------------------------------------
	// Node_0: injection voxel carousel.
	//
	// Cycle order:
	//
	//     NONE
	//     VOXEL_211  +X
	//     VOXEL_121  +Y
	//     VOXEL_011  -X
	//     VOXEL_112  +Z
	//     VOXEL_101  -Y
	//     VOXEL_110  -Z
	// ---------------------------------------------------------
	if (m_volumeAssemblyNode == VOLUME_NODE_PREVIEW &&
		m_activeSubLayerPanelItem == PREVIEW_LIST_INJECTION_MODE) {

		cycleInjectionVoxelSelection(dir);

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;

		return;
	}

	// ---------------------------------------------------------
	// Node_1: primitive, edit target, and rotation increment.
	// ---------------------------------------------------------
	if (m_volumeAssemblyNode == VOLUME_NODE_EDIT_OBJECT) {

		if (hasInjectionVoxelSelected()) {

			switch (m_activeSubLayerPanelItem) {

			case INJECTION_EDIT_LIST_TARGET:
				cycleVolumeEditTarget(dir);

				// Switching VOLUME_0 <-> VOLUME_1 changes which
				// authoring state drives m_dWorkingVolume.
				result.regenerateVolume = true;
				result.rebuildMenu = true;
				break;

			case INJECTION_EDIT_LIST_OBJECT:
				// Checkpoint 3C:
				//
				// List [2] now edits whichever volume state is active:
				//
				//     VOLUME_0 -> m_volume0State.primitive
				//     VOLUME_1 -> m_volume1State.primitive
				cycleVolumePrimitiveSelection(dir);

				result.regenerateVolume = true;
				result.rebuildMenu = true;
				break;

			case INJECTION_EDIT_LIST_ROTATION_INCREMENT:

				if (isEditingInjectionVoxel0()) {
					cycleRotationAngleIncrement(dir);
				}
				break;

			default:
				break;
			}

			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			return;
		}

		if (m_activeSubLayerPanelItem == EDIT_LIST_OBJECT) {

			cycleVolumePrimitiveSelection(dir);
			result.regenerateVolume = true;
		}
		else if (m_activeSubLayerPanelItem == EDIT_LIST_ROTATION_INCREMENT) {

			cycleRotationAngleIncrement(dir);
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// ---------------------------------------------------------
	// Node_2: offset / injection rail panel.
	// ---------------------------------------------------------
	if (m_volumeAssemblyNode == VOLUME_NODE_OFFSET_OBJECT) {
		if (hasInjectionVoxelSelected()) {

			switch (m_activeSubLayerPanelItem) {

			case INJECTION_OFFSET_LIST_TARGET:
				cycleVolumeEditTarget(dir);

				// Switching VOLUME_0 <-> VOLUME_1 changes which
				// state drives the working render.
				result.regenerateVolume = true;
				result.rebuildMenu = true;
				break;

			case INJECTION_OFFSET_LIST_VECTOR:
				cycleOffsetVectorSelection(dir);
				break;

			case INJECTION_OFFSET_LIST_DISTANCE:
				cycleOffsetIncrement(dir);
				break;

			case INJECTION_OFFSET_LIST_RAIL:
				if (isEditingInjectionVoxel0()) {
					adjustInjectionRail(dir);
					result.regenerateVolume = true;
				}
				else {
					cycleVolumeInjectionMode(dir);
					result.regenerateVolume = true;
					result.rebuildMenu = true;
				}
				break;

			default:
				break;
			}

			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			return;
		}

		switch (m_activeSubLayerPanelItem) {

		case OFFSET_LIST_VECTOR:
			cycleOffsetVectorSelection(dir);
			break;

		case OFFSET_LIST_DISTANCE:
			cycleOffsetIncrement(dir);
			break;

		default:
			break;
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}
}
void TheArbiter::activateSubLayerPanelItem(ArbiterResult& result) {
	if (!m_subLayerPanelOpen || !isSubLayerPanelEligible()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	if (isMarchingCubesSubLayer()) {
		switch (m_activeSubLayerPanelItem) {
		case MC_LIST_EXPORT_OBJ:
			result.exportObjRequested = true;
			break;

		case MC_LIST_TO_SUB_LAYER_2:
			m_singleParticleSubLayer = SP_SUB_LAYER_VOLUME_RENDER;
			m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
			m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;
			m_subLayerPanelOpen = true;
			result.rebuildMenu = true;
			break;

		case MC_LIST_TO_SUB_LAYER_0:
			m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;
			m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
			m_activeSubLayerPanelItem = 0;
			m_subLayerPanelOpen = false;
			m_selectedParticle = false;
			m_hoverValid = false;
			m_workplaneSlice = 0;
			result.goToSubLayer0 = true;
			result.rebuildMenu = true;
			break;
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	switch (m_volumeAssemblyNode) {
	case VOLUME_NODE_PREVIEW:
		switch (m_activeSubLayerPanelItem) {
		case PREVIEW_LIST_EDIT_OBJECT:

			m_volumeAssemblyNode = VOLUME_NODE_EDIT_OBJECT;
			m_activeSubLayerPanelItem =
				hasInjectionVoxelSelected()
				? INJECTION_EDIT_LIST_TARGET
				: EDIT_LIST_OBJECT;

			result.regenerateVolume = true;
			result.rebuildMenu = true;
			break;

		case PREVIEW_LIST_RUN_MC:
			m_singleParticleSubLayer = SP_SUB_LAYER_MARCHING_CUBES;
			m_activeSubLayerPanelItem = MC_LIST_EXPORT_OBJ;
			m_subLayerPanelOpen = true;
			result.enterMarchingCubes = true;
			result.rebuildMenu = true;
			break;

		default:
			break;
		}
		break;

	case VOLUME_NODE_EDIT_OBJECT:
		if (hasInjectionVoxelSelected()) {
			if (m_activeSubLayerPanelItem == INJECTION_EDIT_LIST_TARGET) {
				// Let E toggle the edit target too, even though
				// A/D remains the primary adjustment control.
				cycleVolumeEditTarget(+1.0f);
				result.regenerateVolume = true;
				result.rebuildMenu = true;
			}
			else if (isEditingInjectionVoxel0() && 
				m_activeSubLayerPanelItem == INJECTION_EDIT_LIST_OFFSET_OBJECT) {

				m_volumeAssemblyNode = VOLUME_NODE_OFFSET_OBJECT;
				m_activeSubLayerPanelItem = OFFSET_LIST_VECTOR;
				result.rebuildMenu = true;
			}
			else if (isEditingInjectionVoxel0() && 
				m_activeSubLayerPanelItem == INJECTION_EDIT_LIST_PREVIEW_OBJECT) {

				m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
				m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;
				result.regenerateVolume = true;
				result.rebuildMenu = true;
			}
			else if (isEditingInjectionVoxel1() && 
				m_activeSubLayerPanelItem == INJECTION_EDIT_LIST_COMMIT_BASE) {

				// Commit Brush Base:
				//
				//     current VOLUME_1 primitive
				//         -> remembered as brushBasePrimitive
				//
				//     current editable rotation
				//         -> baked into VOLUME_1 basis
				//
				//     VOLUME_1 selector
				//         -> BASE
				//
				// This does NOT fuse/cut into VOLUME_0.
				// This does NOT copy m_dWorkingVolume into m_dBaseVolume.
				commitBrushBase();

				result.command = CMD_REDRAW;
				result.requestRedraw = true;
				result.regenerateVolume = true;
				result.rebuildMenu = true;
			}
			else if (isEditingInjectionVoxel1() && 
				m_activeSubLayerPanelItem == INJECTION_EDIT_LIST_MIRROR) {

				// Placeholder for a later checkpoint.
				result.rebuildMenu = true;
			}

			break;
		}

		if (m_activeSubLayerPanelItem == EDIT_LIST_OFFSET_OBJECT) {
			m_volumeAssemblyNode = VOLUME_NODE_OFFSET_OBJECT;
			m_activeSubLayerPanelItem = OFFSET_LIST_VECTOR;
			result.rebuildMenu = true;
		}
		else if (m_activeSubLayerPanelItem == EDIT_LIST_PREVIEW_OBJECT) {
			m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
			m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;
			result.regenerateVolume = true;
			result.rebuildMenu = true;
		}
		break;

	case VOLUME_NODE_OFFSET_OBJECT:
		if (hasInjectionVoxelSelected()) {

			if (m_activeSubLayerPanelItem == INJECTION_OFFSET_LIST_TARGET) {

				cycleVolumeEditTarget(+1.0f);

				result.command = CMD_REDRAW;
				result.requestRedraw = true;
				result.regenerateVolume = true;
				result.rebuildMenu = true;

				return;
			}

			if (isEditingInjectionVoxel1() &&
				m_activeSubLayerPanelItem == INJECTION_OFFSET_LIST_MODE) {

				cycleVolumeInjectionMode(+1.0f);

				result.command = CMD_REDRAW;
				result.requestRedraw = true;
				result.regenerateVolume = true;
				result.rebuildMenu = true;

				return;
			}

			if (isEditingInjectionVoxel0() &&
				m_activeSubLayerPanelItem == INJECTION_OFFSET_LIST_APPLY_TO_BASE) {

				if (!canApplyVolumeToBase()) {

					result.command = CMD_REDRAW;
					result.requestRedraw = true;
					result.rebuildMenu = true;

					return;
				}

				m_volumeAssemblyNode = VOLUME_NODE_APPLY_TO_BASE;
				m_activeSubLayerPanelItem = APPLY_LIST_COMMIT;
				result.rebuildMenu = true;

				break;
			}

			if (isEditingInjectionVoxel0() &&
				m_activeSubLayerPanelItem == INJECTION_OFFSET_LIST_EDIT_OBJECT) {

				m_volumeAssemblyNode = VOLUME_NODE_EDIT_OBJECT;
				m_activeSubLayerPanelItem = INJECTION_EDIT_LIST_TARGET;

				result.rebuildMenu = true;
				break;
			}

			break;
		}

		if (m_activeSubLayerPanelItem == OFFSET_LIST_APPLY_TO_BASE) {

			if (!canApplyVolumeToBase()) {
				result.command = CMD_REDRAW;
				result.requestRedraw = true;
				result.rebuildMenu = true;

				return;
			}

			m_volumeAssemblyNode = VOLUME_NODE_APPLY_TO_BASE;
			m_activeSubLayerPanelItem = APPLY_LIST_COMMIT;
			result.rebuildMenu = true;
		}
		else if (m_activeSubLayerPanelItem == OFFSET_LIST_EDIT_OBJECT) {

			m_volumeAssemblyNode = VOLUME_NODE_EDIT_OBJECT;
			m_activeSubLayerPanelItem =
				hasInjectionVoxelSelected()
				? INJECTION_EDIT_LIST_TARGET
				: EDIT_LIST_OBJECT;

			result.rebuildMenu = true;
		}
		break;

	case VOLUME_NODE_APPLY_TO_BASE:
		switch (m_activeSubLayerPanelItem) {

		case APPLY_LIST_COMMIT:
			result = commitObjectBasisAndReturnToPreview();
			return;

		case APPLY_LIST_OFFSET_OBJECT:
			m_volumeAssemblyNode = VOLUME_NODE_OFFSET_OBJECT;
			m_activeSubLayerPanelItem = OFFSET_LIST_VECTOR;

			result.rebuildMenu = true;
			break;

		default:
		case APPLY_LIST_CANCEL_TO_PREVIEW:
			// Return without committing the editable brush.
			// Preserve its transform state so it can be resumed later,
			// but restore the Node_0 committed-only display.
			m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;

			m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;

			result.regenerateVolume = true;
			result.rebuildMenu = true;
			break;
		}

	default:
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}

TheArbiter::BasisVector
TheArbiter::normalizeBasisVector(const BasisVector& v) const {
	const float lengthSquared = dotBasisVector(v, v);

	if (lengthSquared <= 1.0e-12f) 
		return { 0.0f, 0.0f, 0.0f };

	const float inverseLength =  1.0f / sqrt(lengthSquared);

	return multiplyBasisVector(v, inverseLength);
}

TheArbiter::BasisVector
TheArbiter::transformByBasis(
	const ObjectBasis& basis, 
	const BasisVector& localVector) const {

	return {
		basis.xAxis.x * localVector.x +
		basis.yAxis.x * localVector.y +
		basis.zAxis.x * localVector.z,

		basis.xAxis.y * localVector.x +
		basis.yAxis.y * localVector.y +
		basis.zAxis.y * localVector.z,

		basis.xAxis.z * localVector.x +
		basis.yAxis.z * localVector.y +
		basis.zAxis.z * localVector.z
	};
}

TheArbiter::BasisVector
TheArbiter::rotateLocalVectorXYZ(
	const BasisVector& vector, 
	float pitchDeg, 
	float yawDeg, 
	float rollDeg) const {

	constexpr float kDegToRad =
		0.01745329251994329577f;

	const float pitch = pitchDeg * kDegToRad;
	const float yaw = yawDeg * kDegToRad;
	const float roll = rollDeg * kDegToRad;

	const float sp = sin(pitch);
	const float cp = cos(pitch);

	const float sy = sin(yaw);
	const float cy = cos(yaw);

	const float sr = sin(roll);
	const float cr = cos(roll);

	// Rx(pitch)
	const BasisVector afterPitch{
		vector.x,
		cp * vector.y - sp * vector.z,
		sp * vector.y + cp * vector.z
	};

	// Ry(yaw)
	const BasisVector afterYaw{
		cy * afterPitch.x + sy * afterPitch.z,
		afterPitch.y,
		-sy * afterPitch.x + cy * afterPitch.z
	};

	// Rz(roll)
	return {
		cr * afterYaw.x - sr * afterYaw.y,
		sr * afterYaw.x + cr * afterYaw.y,
		afterYaw.z
	};
}

TheArbiter::ObjectBasis
TheArbiter::orthonormalizeBasis(const ObjectBasis& basis) const {
	BasisVector x =
		normalizeBasisVector(basis.xAxis);

	BasisVector yProjection =
		multiplyBasisVector(
			x,
			dotBasisVector(basis.yAxis, x)
		);

	BasisVector y =
		normalizeBasisVector(
			subtractBasisVector(
				basis.yAxis,
				yProjection
			)
		);

	BasisVector z =
		normalizeBasisVector(
			crossBasisVector(x, y)
		);

	// Recalculate Y to maintain a clean right-handed basis.
	y = normalizeBasisVector(
		crossBasisVector(z, x)
	);

	return { x, y, z };
}

bool TheArbiter::canApplyVolumeToBase() const {
	// CUT cannot expand the anchor volume, so it is allowed
	// through the panel once the injection pipeline is active.
	if (hasInjectionVoxelSelected() && 
		m_volumeInjectionMode == VOLUME_CUT) {

		return true;
	}

	return isVolumeBoundarySafe();
}

int TheArbiter::getInjectionVoxelDX() const {
	switch (m_volumeInjectionVoxel) {

	case INJECTION_VOXEL_211:
	case INJECTION_VOXEL_221:
	case INJECTION_VOXEL_201:
	case INJECTION_VOXEL_210:
	case INJECTION_VOXEL_212:
	case INJECTION_VOXEL_202:
	case INJECTION_VOXEL_220:
	case INJECTION_VOXEL_200:
	case INJECTION_VOXEL_222:
		return 1;

	case INJECTION_VOXEL_011:
	case INJECTION_VOXEL_021:
	case INJECTION_VOXEL_001:
	case INJECTION_VOXEL_010:
	case INJECTION_VOXEL_012:
	case INJECTION_VOXEL_020:
	case INJECTION_VOXEL_002:
	case INJECTION_VOXEL_022:
	case INJECTION_VOXEL_000:
		return -1;

	default:
		return 0;
	}
}
int TheArbiter::getInjectionVoxelDY() const {
	switch (m_volumeInjectionVoxel) {

	case INJECTION_VOXEL_121:
	case INJECTION_VOXEL_120:
	case INJECTION_VOXEL_221:
	case INJECTION_VOXEL_122:
	case INJECTION_VOXEL_021:
	case INJECTION_VOXEL_020:
	case INJECTION_VOXEL_220:
	case INJECTION_VOXEL_022:
	case INJECTION_VOXEL_222:
		return 1;

	case INJECTION_VOXEL_101:
	case INJECTION_VOXEL_201:
	case INJECTION_VOXEL_100:
	case INJECTION_VOXEL_001:
	case INJECTION_VOXEL_102:
	case INJECTION_VOXEL_202:
	case INJECTION_VOXEL_002:
	case INJECTION_VOXEL_200:
	case INJECTION_VOXEL_000:
		return -1;

	default:
		return 0;
	}
}
int TheArbiter::getInjectionVoxelDZ() const {
	switch (m_volumeInjectionVoxel) {

	case INJECTION_VOXEL_112:
	case INJECTION_VOXEL_122:
	case INJECTION_VOXEL_102:
	case INJECTION_VOXEL_012:
	case INJECTION_VOXEL_212:
	case INJECTION_VOXEL_202:
	case INJECTION_VOXEL_002:
	case INJECTION_VOXEL_022:
	case INJECTION_VOXEL_222:
		return 1;

	case INJECTION_VOXEL_110:
	case INJECTION_VOXEL_120:
	case INJECTION_VOXEL_100:
	case INJECTION_VOXEL_010:
	case INJECTION_VOXEL_210:
	case INJECTION_VOXEL_020:
	case INJECTION_VOXEL_220:
	case INJECTION_VOXEL_200:
	case INJECTION_VOXEL_000:
		return -1;

	default:
		return 0;
	}
}
int TheArbiter::getRotationAngleIncrementDeg() const {
	switch (m_rotationAngleIncrementIndex) {
	case 1: return 20;
	case 2: return 45;
	case 3: return 90;
	case 4: return 180;
	default:
	case 0: return 1;
	}
}

float TheArbiter::getEffectiveVolumeScaleX() const {
	const VolumeObjectState& s = getActiveVolumeState();
	return s.scaleWhole * s.scaleX;
}
float TheArbiter::getEffectiveVolumeScaleY() const {
	const VolumeObjectState& s = getActiveVolumeState();
	return s.scaleWhole * s.scaleY;
}
float TheArbiter::getEffectiveVolumeScaleZ() const {
	const VolumeObjectState& s = getActiveVolumeState();
	return s.scaleWhole * s.scaleZ;
}

float TheArbiter::getOffsetDistance() const {
	const TheArbiter::VolumeObjectState& state =
		getActiveVolumeState();

	switch (m_offsetVectorSelection) {
	case OFFSET_VECTOR_Y:
		return state.offsetY;

	case OFFSET_VECTOR_Z:
		return state.offsetZ;

	default:
	case OFFSET_VECTOR_X:
		return state.offsetX;
	}
}
