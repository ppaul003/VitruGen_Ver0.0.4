#include <algorithm>
#include <cassert>
#include <cmath>

#include "TheArbiter.h"

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
			TheArbiter::WorkspaceAvailability::AVAILABLE,
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
			TheArbiter::WorkspaceId::LINKED_PARTICLES_MCAD,
			TheArbiter::WorkspaceDomain::GRID_3D,
			TheArbiter::WorkspaceAvailability::RESERVED,
			"LINKED_PARTICLES_MCAD"
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

	constexpr float kParticleSimRadiusPresets[] = {
		0.0039f,
		0.0046f,
		0.0054f,
		0.0061f,
		0.0068f,
		0.0076f,
		0.0083f,
		0.0091f,
		0.0098f,
		0.0105f,
		0.0113f,
		0.0120f,
		0.0127f,
		0.0135f,
		0.0142f,
		0.0149f,
		0.0156f
	};

	constexpr int kParticleSimRadiusPresetCount =
		static_cast<int>(
			sizeof(kParticleSimRadiusPresets) /
			sizeof(kParticleSimRadiusPresets[0])
			);

	int wrapIndex(int current, int count, int dir) {
		if (count <= 0 || dir == 0) return current;

		const int step = dir < 0 ? -1 : 1;
		return (current + step + count) % count;
	}

	int findClosestParticleRadiusPreset(float value) {
		int closestIndex = 0;
		float closestDistance =
			std::abs(value - kParticleSimRadiusPresets[0]);

		for (int index = 1;
			index < kParticleSimRadiusPresetCount;
			index++) {

			const float distance =
				std::abs(value - kParticleSimRadiusPresets[index]);

			if (distance < closestDistance) {
				closestIndex = index;
				closestDistance = distance;
			}
		}

		return closestIndex;
	}

	void adjustParticleRadiusPreset(float& value, int dir) {
		const int currentIndex =
			findClosestParticleRadiusPreset(value);

		const int nextIndex = (std::max)(
			0,
			(std::min)(
				kParticleSimRadiusPresetCount - 1,
				currentIndex + (dir < 0 ? -1 : 1)
				)
			);

		value = kParticleSimRadiusPresets[nextIndex];
	}

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

	struct InjectionDirection { int x; int y; int z; };
	constexpr InjectionDirection kInjectionDirections[] = {
		{ 0, 0, 0 },
		{ 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 },
		{ 0, 0, 1 }, { 0, -1, 0 }, { 0, 0, -1 },
		{ 0, 1, -1 }, { 1, 1, 0 }, { 0, 1, 1 }, { -1, 1, 0 },
		{ 1, -1, 0 }, { 0, -1, -1 }, { -1, -1, 0 }, { 0, -1, 1 },
		{ -1, 0, -1 }, { 1, 0, -1 }, { -1, 0, 1 }, { 1, 0, 1 },
		{ 1, -1, 1 }, { -1, 1, -1 }, { 1, 1, -1 }, { -1, -1, 1 },
		{ 1, -1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }, { -1, -1, -1 }
	};
	static_assert(
		static_cast<int>(sizeof(kInjectionDirections) / sizeof(kInjectionDirections[0])) ==
		kInjectionVoxelCycleCount,
		"Injection direction table must track the voxel cycle");
}

// =============================================================================
// WORKSPACE CATALOG / CANONICAL NAVIGATION STATE
// =============================================================================
const TheArbiter::WorkspaceDescriptor&
TheArbiter::describeWorkspace(WorkspaceId workspace) {
	for (int i = 0; i < kWorkspaceCatalogCount; i++) {
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

// =============================================================================
// GLOBAL SHELL / WORKSPACE SELECTION
// =============================================================================
void TheArbiter::cycleGlobalShellSelection(int dir) {
	if (dir == 0) return;

	// User-visible order:
	// IDLE -> GRID_2D -> GRID_3D -> SIMCAD_4D
	int currIndex = 0;
	if (!isIdleSelected()) {
		switch (m_navigation.selectedDomain) {
		case WorkspaceDomain::GRID_2D:
			currIndex = 1;
			break;

		case WorkspaceDomain::GRID_3D:
			currIndex = 2;
			break;

		case WorkspaceDomain::SIMCAD_4D:
			currIndex = 3;
			break;

		default:
		case WorkspaceDomain::NONE:
		case WorkspaceDomain::COUNT:
			currIndex = 2;
			break;
		}
	}

	const int step = dir < 0 ? -1 : 1;
	const int nextIndex =
		(currIndex + step + 4) % 4;

	if (nextIndex == 0) {
		m_navigation.globalShellSelection =
			GlobalShellSelection::IDLE;
	}
	else {
		m_navigation.globalShellSelection =
			GlobalShellSelection::WORKSPACE_DOMAINS;

		switch (nextIndex) {
		case 1:
			m_navigation.selectedDomain =
				WorkspaceDomain::GRID_2D;
			break;

		case 2:
			m_navigation.selectedDomain =
				WorkspaceDomain::GRID_3D;
			break;

		case 3:
			m_navigation.selectedDomain =
				WorkspaceDomain::SIMCAD_4D;
			break;
		}
	}
	validateNavigationState();
}

void TheArbiter::cycleWorkspaceSelection(int dir) {
	if (dir == 0) return;

	static const WorkspaceId grid3DWorkspaces[] = {
		WorkspaceId::GRAPH_3D,
		WorkspaceId::SINGLE_PARTICLE_MCAD,
		WorkspaceId::LINKED_PARTICLES_MCAD
	};

	static const WorkspaceId grid2DWorkspaces[] = {
		WorkspaceId::GRAPH_2D,
		WorkspaceId::TEXTURE_MAP_2D,
		WorkspaceId::SPRITE_PROJECTION_2D
	};

	static const WorkspaceId simcad4DWorkspaces[] = {
		WorkspaceId::PARTICLE_SIMULATION,
		WorkspaceId::NBODY_SIM,
		WorkspaceId::SANDBOX_SIM,
		WorkspaceId::CUDA_CAD
	};

	const WorkspaceId* workspaceCycle = nullptr;
	int workspaceCount = 0;

	switch (m_navigation.selectedDomain) {
	case WorkspaceDomain::GRID_3D:
		workspaceCycle = grid3DWorkspaces;
		workspaceCount = 3;
		break;

	case WorkspaceDomain::GRID_2D:
		workspaceCycle = grid2DWorkspaces;
		workspaceCount = 3;
		break;

	case WorkspaceDomain::SIMCAD_4D:
		workspaceCycle = simcad4DWorkspaces;
		workspaceCount = 4;
		break;

		// GRID_2D receives its own workspace cycle later.

	default:
	case WorkspaceDomain::NONE:
	case WorkspaceDomain::COUNT:
		return;
	}

	const WorkspaceId currentWorkspace =
		getSelectedWorkspace();

	int currIndex = 0;
	for (int index = 0; index < workspaceCount; index++) {
		if (workspaceCycle[index] == currentWorkspace) {
			currIndex = index;
			break;
		}
	}

	const int step = dir < 0 ? -1 : 1;
	const int nextIndex =
		(currIndex + step + workspaceCount) %
		workspaceCount;

	setWorkspaceSelection(
		m_navigation.selectedDomain,
		workspaceCycle[nextIndex]
	);
}

// =============================================================================
// PARTICLE WORKSPACE CONFIGURATION
// =============================================================================
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

bool TheArbiter::isTextureMapLayer1PanelContext() const {

	return
		m_navigation.layer ==
		ApplicationLayer::DOMAIN_SELECTION &&

		m_navigation.selectedDomain ==
		WorkspaceDomain::GRID_2D &&

		getSelectedWorkspace() ==
		WorkspaceId::TEXTURE_MAP_2D;
}

bool TheArbiter::isSingleParticleLayer1PanelContext() const {

	return
		m_navigation.layer ==
		ApplicationLayer::DOMAIN_SELECTION &&

		m_navigation.selectedDomain ==
		WorkspaceDomain::GRID_3D &&

		getSelectedWorkspace() ==
		WorkspaceId::SINGLE_PARTICLE_MCAD;
}

bool TheArbiter::isParticleSimLayer1PanelContext() const {
	if (m_navigation.layer != ApplicationLayer::DOMAIN_SELECTION ||
		m_navigation.selectedDomain != WorkspaceDomain::SIMCAD_4D) {

		return false;
	}

	const WorkspaceId workspace = getSelectedWorkspace();
	return workspace == WorkspaceId::PARTICLE_SIMULATION ||
		workspace == WorkspaceId::SANDBOX_SIM;
}

int TheArbiter::getParticleSimLayer2RowCount() const {
	return m_particleSimDraftConfig.radiusMode ==
		ParticleRadiusMode::Random
		? 5 : 4;
}

unsigned int TheArbiter::getParticleSimRGBTotal() const {
	return m_particleSimDraftConfig.redCount +
		m_particleSimDraftConfig.greenCount +
		m_particleSimDraftConfig.blueCount;
}

bool TheArbiter::isParticleSimLayer2RunSelected() const {
	return m_particleSimLayer2Selection ==
		getParticleSimLayer2RowCount() - 1;
}

void TheArbiter::cycleParticleSimPanelWorkspace(int dir) {
	if (dir == 0) return;

	const WorkspaceId workspace = getSelectedWorkspace();
	const WorkspaceId nextWorkspace =
		workspace == WorkspaceId::PARTICLE_SIMULATION
		? WorkspaceId::SANDBOX_SIM
		: WorkspaceId::PARTICLE_SIMULATION;

	setWorkspaceSelection(
		WorkspaceDomain::SIMCAD_4D,
		nextWorkspace
	);
}

void TheArbiter::moveTextureMapLayer1Cursor(int dir) {
	if (dir == 0) return;

	const int count =
		static_cast<int>(TextureMapLayer1Item::Count);

	const int current =
		static_cast<int>(m_textureMapLayer1Selection);

	m_textureMapLayer1Selection =
		static_cast<TextureMapLayer1Item>(wrapIndex(current, count, dir));
}

void TheArbiter::handleTextureMapLayer1Adjust(
	int dir,
	ArbiterResult& result) {

	if (dir == 0) return;

	switch (m_textureMapLayer1Selection) {

	case TextureMapLayer1Item::Workspace:

		// Cycle through the GRID_2D workspace list:
		//
		// GRAPH_2D
		// TEXTURE_MAP_2D
		// SPRITE_PROJECTION_2D
		cycleWorkspaceSelection(dir);
		break;

	case TextureMapLayer1Item::TargetStaticParticle:

		// EuclidEngine will apply this step to the
		// TextureMapWorkspace OUTPUT catalog.
		result.textureMapCatalogStep =
			dir < 0 ? -1 : +1;
		break;

	case TextureMapLayer1Item::OpenConfiguration:
	case TextureMapLayer1Item::RefreshCatalog:
	case TextureMapLayer1Item::Configure:
	case TextureMapLayer1Item::Count:

		// Action rows have no A/D value.
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}


void TheArbiter::activateTextureMapLayer1Item(ArbiterResult& result) {

	switch (m_textureMapLayer1Selection) {

	case TextureMapLayer1Item::OpenConfiguration:

		// Future Layer 1 catalog/load sprint:
		// load the selected OUTPUT Static Particle target.
		result.loadTextureMapTargetRequested = true;
		break;

	case TextureMapLayer1Item::RefreshCatalog:

		// Future Layer 1 catalog sprint:
		// rescan OUTPUT/STATIC_PARTICLES.
		result.refreshTextureMapCatalogRequested = true;
		break;

	case TextureMapLayer1Item::Configure:
		
		// Enter TEXTURE_MAP_2D Layer 2.
		//
		// Layer2 gets its dedicated target-configuration
		setApplicationLayer(ApplicationLayer::WORKSPACE_CONFIGURATION);
		break;

	case TextureMapLayer1Item::Workspace:
	case TextureMapLayer1Item::TargetStaticParticle:
	case TextureMapLayer1Item::Count:

		// Rows [1] and [2] do not activate with E.
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::moveSingleParticleLayer1Cursor(int dir) {
	if (dir == 0) return;

	const int count =
		static_cast<int>(SingleParticleLayer1Item::Count);

	const int current =
		static_cast<int>(m_singleParticleLayer1Selection);

	m_singleParticleLayer1Selection =
		static_cast<SingleParticleLayer1Item>(
			wrapIndex(current, count, dir));
}

void TheArbiter::cycleSingleParticleObjectType(int dir) {
	if (dir == 0) return;

	const int count =
		static_cast<int>(SingleParticleObjectType::Count);

	const int current =
		static_cast<int>(m_singleParticleObjectType);

	m_singleParticleObjectType =
		static_cast<SingleParticleObjectType>(
			wrapIndex(current,count,dir));
}

void TheArbiter::handleSingleParticleLayer1Adjust(int dir, ArbiterResult& result) {

	switch (m_singleParticleLayer1Selection) {

	case SingleParticleLayer1Item::Workspace:

		// Cycle through the GRID_3D workspace list:
		//
		// GRAPH_3D
		// SINGLE_PARTICLE
		// LINKED_PARTICLES
		cycleWorkspaceSelection(dir);
		break;

	case SingleParticleLayer1Item::ParticleType:

		// Cycle:
		//
		// STATIC
		// COMPOSITE
		// ATOMIC
		cycleSingleParticleObjectType(dir);
		break;

	default:
	case SingleParticleLayer1Item::Configure:
	case SingleParticleLayer1Item::Count:

		// Row [3] is an action row and has no A/D value.
		break;
	}

	result.command = CMD_REDRAW;

	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::activateSingleParticleLayer1Item(ArbiterResult& result) {

	// E only activates the final Configure row.
	if (m_singleParticleLayer1Selection !=
		SingleParticleLayer1Item::Configure) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;

		return;
	}

	// Ver0.0.4 only implements STATIC.
	//
	// COMPOSITE and ATOMIC remain visible selections,
	// but they must not enter an incomplete pipeline.
	if (!isStaticParticleObjectType()) {

		result.command = CMD_REDRAW;

		result.requestRedraw = true;
		result.rebuildMenu = true;

		return;
	}

	// Enter the existing SINGLE_PARTICLE Layer 2
	// particle configuration screen.
	setApplicationLayer(ApplicationLayer::WORKSPACE_CONFIGURATION);

	// Begin Layer 2 at its first row.
	m_activeParticleConfigList = PARTICLE_LIST_COLOR;

	result.command = CMD_REDRAW;

	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::moveParticleSimLayer1Cursor(int dir) {
	const int count =
		static_cast<int>(ParticleSimLayer1Item::Count);

	const int current =
		static_cast<int>(m_particleSimLayer1Selection);

	m_particleSimLayer1Selection =
		static_cast<ParticleSimLayer1Item>(
			wrapIndex(current, count, dir));
}

void TheArbiter::moveParticleSimLayer2Cursor(int dir) {
	m_particleSimLayer2Selection = wrapIndex(
		m_particleSimLayer2Selection,
		getParticleSimLayer2RowCount(),
		dir
	);
}

void TheArbiter::clampParticleSimLayer2Selection() {
	const int lastRow =
		getParticleSimLayer2RowCount() - 1;

	m_particleSimLayer2Selection = (std::max)(
		0,
		(std::min)(m_particleSimLayer2Selection, lastRow)
		);
}

void TheArbiter::adjustParticleSimDefaultCount(int dir) {
	const unsigned int step = 100;
	unsigned int& count =
		m_particleSimDraftConfig.defaultParticleCount;

	if (dir < 0) {
		count = count > step ? count - step : 0;
	}
	else if (dir > 0) {
		count = (std::min)(
			kParticleSimCapacity,
			count + step);
	}
}

void TheArbiter::cycleParticleSimGridLayout(int dir) {
	const int current =
		static_cast<int>(m_particleSimDraftConfig.gridLayout);

	const int count =
		static_cast<int>(ParticleGridLayout::Count);

	m_particleSimDraftConfig.gridLayout =
		static_cast<ParticleGridLayout>(
			wrapIndex(current, count, dir));
}

void TheArbiter::cycleParticleSimColorMode(int dir) {
	const int current =
		static_cast<int>(m_particleSimDraftConfig.colorMode);

	const int count =
		static_cast<int>(ParticleColorMode::Count);

	m_particleSimDraftConfig.colorMode =
		static_cast<ParticleColorMode>(wrapIndex(current, count, dir));

	clampParticleSimLayer2Selection();
}

void TheArbiter::cycleParticleSimRadiusMode(int dir) {
	const int current =
		static_cast<int>(m_particleSimDraftConfig.radiusMode);

	const int count =
		static_cast<int>(ParticleRadiusMode::Count);

	m_particleSimDraftConfig.radiusMode =
		static_cast<ParticleRadiusMode>(wrapIndex(current, count, dir));

	clampParticleSimLayer2Selection();
}

void TheArbiter::cycleParticleSimColorChannel(int dir) {
	const int current =
		static_cast<int>(m_particleSimDraftConfig.selectedColorChannel);

	const int count =
		static_cast<int>(ParticleColorChannel::Count);

	m_particleSimDraftConfig.selectedColorChannel =
		static_cast<ParticleColorChannel>(
			wrapIndex(current, count, dir));
}

void TheArbiter::cycleParticleSimResetMode(int dir) {
	const int current =
		static_cast<int>(m_particleSimDraftConfig.resetMode);

	const int count =
		static_cast<int>(ParticleSimResetMode::Count);

	m_particleSimDraftConfig.resetMode =
		static_cast<ParticleSimResetMode>(wrapIndex(current, count, dir));
}

void TheArbiter::adjustParticleSimUniformRadius(int dir) {
	adjustParticleRadiusPreset(
		m_particleSimDraftConfig.uniformRadius,
		dir
	);
}

void TheArbiter::adjustParticleSimMinimumRadius(int dir) {
	adjustParticleRadiusPreset(
		m_particleSimDraftConfig.minimumRadius,
		dir
	);

	m_particleSimDraftConfig.minimumRadius =
		(std::min)(
			m_particleSimDraftConfig.minimumRadius,
			m_particleSimDraftConfig.maximumRadius);
}

void TheArbiter::adjustParticleSimMaximumRadius(int dir) {
	adjustParticleRadiusPreset(
		m_particleSimDraftConfig.maximumRadius,
		dir
	);

	m_particleSimDraftConfig.maximumRadius =
		(std::max)(
			m_particleSimDraftConfig.maximumRadius,
			m_particleSimDraftConfig.minimumRadius);
}

void TheArbiter::handleParticleSimLayer1Adjust(
	int dir,
	ArbiterResult& result) {

	switch (m_particleSimLayer1Selection) {
	case ParticleSimLayer1Item::Workspace:
		cycleParticleSimPanelWorkspace(dir);
		break;

	case ParticleSimLayer1Item::GridLayout:
		cycleParticleSimGridLayout(dir);
		break;

	case ParticleSimLayer1Item::ColorMode:
		cycleParticleSimColorMode(dir);
		break;

	case ParticleSimLayer1Item::RadiusMode:
		cycleParticleSimRadiusMode(dir);
		break;

	default:
	case ParticleSimLayer1Item::Configure:
	case ParticleSimLayer1Item::Count:
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::handleParticleSimLayer2Adjust(
	int dir,
	ArbiterResult& result) {

	switch (m_particleSimLayer2Selection) {
	case 0:
		if (m_particleSimDraftConfig.colorMode ==
			ParticleColorMode::Default) {

			adjustParticleSimDefaultCount(dir);
		}
		else {
			cycleParticleSimColorChannel(dir);
		}
		break;

	case 1:
		cycleParticleSimResetMode(dir);
		break;

	case 2:
		if (m_particleSimDraftConfig.radiusMode ==
			ParticleRadiusMode::Uniform) {

			adjustParticleSimUniformRadius(dir);
		}
		else {
			adjustParticleSimMinimumRadius(dir);
		}
		break;

	case 3:
		if (m_particleSimDraftConfig.radiusMode ==
			ParticleRadiusMode::Random) {

			adjustParticleSimMaximumRadius(dir);
		}
		break;

	default:
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}

void TheArbiter::requestParticleCountEntry(
	ArbiterResult& result) {

	if (m_particleSimDraftConfig.colorMode ==
		ParticleColorMode::Default) {

		beginDefaultParticleCountEntry(result);
	}
	else {
		beginSelectedRGBCountEntry(result);
	}
}

void TheArbiter::activateParticleSimLayer1Item(
	ArbiterResult& result) {

	if (m_particleSimLayer1Selection ==
		ParticleSimLayer1Item::Configure &&
		isParticleSimulationSelected()) {

		setApplicationLayer(
			ApplicationLayer::WORKSPACE_CONFIGURATION
		);

		clampParticleSimLayer2Selection();
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}

void TheArbiter::activateParticleSimLayer2Item(
	ArbiterResult& result) {

	if (m_particleSimLayer2Selection == 0) {
		requestParticleCountEntry(result);
		return;
	}

	if (isParticleSimLayer2RunSelected()) {
		// TODO(PARTICLE_SIM_DRAFT_APPLICATION):
		// Draft count, RGB and radius values intentionally remain
		// presentation-only until the later runtime integration sprint.
		setApplicationLayer(ApplicationLayer::ACTIVE_WORKSPACE);
		result.command = CMD_START_PARTICLE_SIMULATION;
		result.requestRedraw = true;
		return;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}

// =============================================================================
// SINGLE_PARTICLE_MCAD EDIT SELECTIONS
// =============================================================================
void TheArbiter::cycleVolumePrimitiveSelection(float dir) {
	// A loaded mesh-only StaticParticleAsset is the current BASE.
	// Do not silently replace it with a procedural primitive.
	if (isLoadedStaticMeshOnly() &&
		isEditingInjectionVoxel0()) {

		m_volume0State.primitive =
			VOLUME_PRIMITIVE_BASE;

		return;
	}

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

void TheArbiter::cycleSPMirrorMode(float dir) {
	(void)dir;
	m_spMirrorMode = (m_spMirrorMode == SP_MIRROR_ON)
		? SP_MIRROR_NONE
		: SP_MIRROR_ON;
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

// =============================================================================
// VOLUME OBJECT STATE / TRANSFORMS
// =============================================================================
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
	m_spMirrorMode = SP_MIRROR_NONE;
}

// =============================================================================
// SINGLE_PARTICLE AUTHORING SOURCE
// =============================================================================

void TheArbiter::activateLoadedStaticParticleBase(bool hasEditableVolume) {

	m_spAuthoringSource =
		SPAuthoringSource::LoadedStaticMesh;

	m_loadedStaticMeshHasEditableVolume =
		hasEditableVolume;

	// A loaded StaticParticleAsset is always presented through
	// the mesh renderer unless a later command explicitly changes it.
	m_particleRenderMode =
		PARTICLE_RENDER_MESH;

	// The imported mesh becomes VOLUME_0 / BASE from the user's
	// perspective. It is not a new procedural sphere.
	resetVolumeState(
		m_volume0State,
		VOLUME_PRIMITIVE_BASE
	);

	// VOLUME_1 remains the fresh procedural brush state for a
	// future mesh-to-volume or restored-volume workflow.
	resetVolumeState(
		m_volume1State,
		VOLUME_PRIMITIVE_SPHERE
	);

	m_volumeAssemblyNode =
		VOLUME_NODE_PREVIEW;

	m_volumeInjectionVoxel =
		INJECTION_VOXEL_NONE;

	m_volumeEditTarget =
		VOLUME_EDIT_TARGET_VOXEL_0;

	m_volumeInjectionMode =
		VOLUME_FUSE;

	m_spMirrorMode =
		SP_MIRROR_NONE;

	m_objectEditMode =
		EDIT_SCALE_WHOLE;

	m_objectRotationMode =
		ROTATE_PITCH;

	m_objectTransformMode =
		TRANSFORM_SCALE;

	m_offsetVectorSelection =
		OFFSET_VECTOR_X;

	m_injectionRailT = 0.0f;

	m_volumeBoundarySensorReady = false;
	m_volumeBoundaryUnsafeCount = 0;

	m_spOverlapPreviewStatus =
		SP_OVERLAP_POSITION_IN_NODE_2;
}

void TheArbiter::activateProceduralVolumeAuthoring() {

	m_spAuthoringSource =
		SPAuthoringSource::ProceduralVolume;

	m_loadedStaticMeshHasEditableVolume =
		false;

	resetAllVolumeStates();

	m_volumeAssemblyNode =
		VOLUME_NODE_PREVIEW;

	m_volumeInjectionVoxel =
		INJECTION_VOXEL_NONE;

	m_volumeEditTarget =
		VOLUME_EDIT_TARGET_VOXEL_0;

	m_volumeInjectionMode =
		VOLUME_FUSE;

	m_spMirrorMode =
		SP_MIRROR_NONE;

	m_objectEditMode =
		EDIT_SCALE_WHOLE;

	m_objectRotationMode =
		ROTATE_PITCH;

	m_objectTransformMode =
		TRANSFORM_SCALE;

	m_offsetVectorSelection =
		OFFSET_VECTOR_X;

	m_injectionRailT = 0.0f;

	m_volumeBoundarySensorReady = false;
	m_volumeBoundaryUnsafeCount = 0;

	m_spOverlapPreviewStatus =
		SP_OVERLAP_POSITION_IN_NODE_2;
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
	m_spMirrorMode = SP_MIRROR_NONE;
	m_objectEditMode = EDIT_SCALE_WHOLE;
	m_objectRotationMode = ROTATE_PITCH;
	m_objectTransformMode = TRANSFORM_SCALE;

	resetObjectOffset();
	// The committed result is now the anchor/base.
	resetVolumeState(m_volume0State, VOLUME_PRIMITIVE_BASE);
	// The next injection brush starts fresh.
	resetVolumeState(m_volume1State, VOLUME_PRIMITIVE_SPHERE);

}

void TheArbiter::handleSingleParticlePrimaryAction(
	ArbiterResult& result) {

	if (!isSimulationRunLayer() ||
		!isSingleParticleSelected()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 0 owns selection mode.
	// ---------------------------------------------------------
	if (isSingleParticleReferenceSubLayer()) {

		// Selected particle:
		// E deselects, closes the panel and keeps selection
		// armed for immediate reselection.
		if (m_selectedParticle) {

			m_selectedParticle = false;
			m_spSelectionArmed = true;

			m_subLayerPanelOpen = false;
			m_activeSubLayerPanelItem =
				SP0_LIST_COLLISION_SHAPE;

			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			result.rebuildMenu = true;
			return;
		}

		// Free camera:
		// E arms selection.
		if (!m_spSelectionArmed) {

			m_spSelectionArmed = true;

			m_subLayerPanelOpen = false;
			m_activeSubLayerPanelItem =
				SP0_LIST_COLLISION_SHAPE;

			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			result.rebuildMenu = true;
			return;
		}

		// Selection armed but nothing selected:
		// E cancels selection mode.
		m_spSelectionArmed = false;

		m_hoverValid = false;
		m_hoverX = 0.0f;
		m_hoverY = 0.0f;

		m_subLayerPanelOpen = false;
		m_activeSubLayerPanelItem =
			SP0_LIST_COLLISION_SHAPE;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 1 does not auto-advance with E.
	// Navigation belongs to its panel.
	// ---------------------------------------------------------
	if (isShapeEditSubLayer()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// Preserve established Sub-Layer 2 and 3 E behavior.
	advanceSingleParticleSubLayer(result);
}

void TheArbiter::moveParticleConfigCursorUp() {
	if (isSingleParticleSelected()) {
		int v = static_cast<int>(m_activeParticleConfigList);
		v = (v + PARTICLE_LIST_COUNT - 1) % PARTICLE_LIST_COUNT;
		m_activeParticleConfigList = static_cast<ParticleConfigList>(v);
		return;
	}

	// PARTICLE_SIM visible order:
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

	// PARTICLE_SIM visible order:
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

// =============================================================================
// KEYBOARD COMMAND ROUTING
// =============================================================================
TheArbiter::ArbiterResult
TheArbiter::processKeyboard(const KeyboardInput::KeyEvent& event) {

	// ---------------------------------------------------------
	// GLOBAL MODAL INPUT GATE
	//
	// While TextEntrySession is active, every raw key belongs
	// exclusively to the text-entry session.
	// ---------------------------------------------------------
	if (m_textEntry.isActive())
		return handleTextEntryKeyboard(event);

	ArbiterResult result;

	if (event.signal == KeyboardInput::KEY_ESCAPE) {
		result.command = CMD_EXIT;
		return result;
	}

	// Q is structural backward navigation only.
	// Particle deselection belongs to E in Sub-Layer 0.
	if (event.signal == KeyboardInput::KEY_Q) {
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
					result.exportObjRequested) {

					break;
				}
			}

			switch (event.signal) {

			case KeyboardInput::KEY_E:
				handleSingleParticlePrimaryAction(result);
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

		if (isParticleSimulationSelected()) {
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
		cycleGlobalShellSelection(-1);
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		break;
	case KeyboardInput::KEY_D:
		cycleGlobalShellSelection(+1);
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

	// =========================================================
	// GRID_2D -> TEXTURE_MAP_2D Layer 1 panel
	// =========================================================
	if (isTextureMapLayer1PanelContext()) {

		switch (event.signal) {

		case KeyboardInput::KEY_W:

			moveTextureMapLayer1Cursor(-1);

			result.command = CMD_REDRAW;
			result.requestRedraw = true;

			break;

		case KeyboardInput::KEY_S:

			moveTextureMapLayer1Cursor(+1);

			result.command = CMD_REDRAW;
			result.requestRedraw = true;

			break;

		case KeyboardInput::KEY_A:

			handleTextureMapLayer1Adjust(-1, result);

			break;

		case KeyboardInput::KEY_D:

			handleTextureMapLayer1Adjust(+1, result);

			break;

		case KeyboardInput::KEY_E:
		case KeyboardInput::KEY_ENTER:

			activateTextureMapLayer1Item(result);

			break;

		default:
			break;
		}

		// Prevent the generic workspace selector from processing
		// the same key a second time.
		return;
	}

	// =========================================================
	// GRID_3D -> SINGLE_PARTICLE Layer 1 panel
	// =========================================================
	if (isSingleParticleLayer1PanelContext()) {

		switch (event.signal) {

		case KeyboardInput::KEY_W:

			moveSingleParticleLayer1Cursor(-1);

			result.command = CMD_REDRAW;
			result.requestRedraw = true;

			break;

		case KeyboardInput::KEY_S:

			moveSingleParticleLayer1Cursor(+1);

			result.command = CMD_REDRAW;
			result.requestRedraw = true;

			break;

		case KeyboardInput::KEY_A:

			handleSingleParticleLayer1Adjust(-1, result);

			break;

		case KeyboardInput::KEY_D:

			handleSingleParticleLayer1Adjust(+1, result);

			break;

		case KeyboardInput::KEY_E:
		case KeyboardInput::KEY_ENTER:

			activateSingleParticleLayer1Item(result);

			break;

		default:
			break;
		}

		return;
	}

	// =========================================================
	// SIMCAD_4D -> PARTICLE_SIM Layer 1 panel
	// =========================================================
	if (isParticleSimLayer1PanelContext()) {

		switch (event.signal) {

		case KeyboardInput::KEY_W:
			moveParticleSimLayer1Cursor(-1);
			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			break;

		case KeyboardInput::KEY_S:
			moveParticleSimLayer1Cursor(+1);
			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			break;

		case KeyboardInput::KEY_A:
			handleParticleSimLayer1Adjust(-1, result);
			break;

		case KeyboardInput::KEY_D:
			handleParticleSimLayer1Adjust(+1, result);
			break;

		case KeyboardInput::KEY_E:
		case KeyboardInput::KEY_ENTER:
			activateParticleSimLayer1Item(result);
			break;

		default:
			break;
		}

		return;
	}

	// =========================================================
	// Generic Layer 1 workspace selector
	// =========================================================
	switch (event.signal) {

	case KeyboardInput::KEY_A:

		cycleWorkspaceSelection(-1);

		result.command = CMD_REDRAW;
		result.requestRedraw = true;

		break;

	case KeyboardInput::KEY_D:

		cycleWorkspaceSelection(+1);

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

	if (isParticleSimulationSelected()) {
		switch (event.signal) {
		case KeyboardInput::KEY_W:
			moveParticleSimLayer2Cursor(-1);
			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			break;

		case KeyboardInput::KEY_S:
			moveParticleSimLayer2Cursor(+1);
			result.command = CMD_REDRAW;
			result.requestRedraw = true;
			break;

		case KeyboardInput::KEY_A:
			handleParticleSimLayer2Adjust(-1, result);
			break;

		case KeyboardInput::KEY_D:
			handleParticleSimLayer2Adjust(+1, result);
			break;

		case KeyboardInput::KEY_E:
		case KeyboardInput::KEY_ENTER:
			activateParticleSimLayer2Item(result);
			break;

		default:
			break;
		}

		return;
	}

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

// =============================================================================
// WORKSPACE / PANEL STATUS QUERIES
// =============================================================================
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

bool TheArbiter::setSPOverlapPreviewStatus(
	SPOverlapPreviewStatus status) {

	const bool changed =
		m_spOverlapPreviewStatus != status;

	m_spOverlapPreviewStatus = status;

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

	if (isSingleParticleReferenceSubLayer()) return SP0_LIST_COUNT;
	if (isShapeEditSubLayer()) return SP1_LIST_COUNT;
	if (isMarchingCubesSubLayer()) return MC_LIST_COUNT;

	switch (m_volumeAssemblyNode) {

	case VOLUME_NODE_EDIT_OBJECT:

		if (hasInjectionVoxelSelected()) {

			if (m_volumeEditTarget ==
				VOLUME_EDIT_TARGET_VOXEL_1) {

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

void TheArbiter::cycleSPCollisionShape(float dir) {

	const int count =
		static_cast<int>(SPCollisionShape::Count);

	int value =
		static_cast<int>(m_spCollisionShape);



	if (dir < 0.0f) {
		value =
			(value + count - 1) % count;
	}
	else {
		value =
			(value + 1) % count;
	}

	m_spCollisionShape =
		static_cast<SPCollisionShape>(value);
}

void TheArbiter::cycleSPMeshBoundMode(float dir) {

	(void)dir;

	m_spMeshBoundMode =
		m_spMeshBoundMode ==
		SPMeshBoundMode::Default
		? SPMeshBoundMode::Fill
		: SPMeshBoundMode::Default;
}

void TheArbiter::cycleSPDisplayMode(float dir) {

	const int count =
		static_cast<int>(SPDisplayMode::Count);

	int value =
		static_cast<int>(m_spDisplayMode);

	if (dir < 0.0f) {

		value =
			(value + count - 1) % count;

	}
	else {

		value =
			(value + 1) % count;

	}

	m_spDisplayMode =
		static_cast<SPDisplayMode>(value);

}

void TheArbiter::toggleSPRenderCage() {

	m_spRenderCageVisible =
		!m_spRenderCageVisible;
}

// =============================================================================
// DISPLAY NAMES
// =============================================================================
const char* TheArbiter::getSelectedDomainDisplayName() const {
	if (isIdleSelected()) {
		return "IDLE";
	}

	switch (m_navigation.selectedDomain) {
	case WorkspaceDomain::GRID_2D:
		return "GRID_2D";

	case WorkspaceDomain::GRID_3D:
		return "GRID_3D";

	case WorkspaceDomain::SIMCAD_4D:
		return "SIMCAD_4D";

	default:
	case WorkspaceDomain::NONE:
	case WorkspaceDomain::COUNT:
		return "UNKNOWN_ENVIRONMENT";
	}
}

const char* TheArbiter::getSelectedWorkspaceDisplayName() const {
	switch (getSelectedWorkspace()) {
		// GRID_2D
	case WorkspaceId::GRAPH_2D:
		return "GRAPH_2D";

	case WorkspaceId::TEXTURE_MAP_2D:
		return "TEXTURE_MAP_2D";

	case WorkspaceId::SPRITE_PROJECTION_2D:
		return "SPRITE_PROJECTION_2D";

		// GRID_3D
	case WorkspaceId::GRAPH_3D:
		return "GRAPH_3D";

	case WorkspaceId::SINGLE_PARTICLE_MCAD:
		return "SINGLE_PARTICLE";

	case WorkspaceId::LINKED_PARTICLES_MCAD:
		return "LINK_PARTICLES";

		// SIMCAD_4D
	case WorkspaceId::PARTICLE_SIMULATION:
		return "PARTICLE_SIM";

	case WorkspaceId::NBODY_SIM:
		return "NBODY_SIM";

	case WorkspaceId::FLUID_SIM:
		return "FLUID_SIM";

	case WorkspaceId::SANDBOX_SIM:
		return "SANDBOX_SIM";

	case WorkspaceId::CUDA_CAD:
		return "CUDA_CAD";

	default:
	case WorkspaceId::NONE:
	case WorkspaceId::COUNT:
		return "UNKNOWN_WORKSPACE";
	}
}

const char* TheArbiter::getSingleParticleObjectTypeName() const {
	switch (m_singleParticleObjectType) {

	case SingleParticleObjectType::Static:
		return "STATIC";

	case SingleParticleObjectType::Composite:
		return "COMPOSITE";

	case SingleParticleObjectType::Atomic:
		return "ATOMIC";

	default:
	case SingleParticleObjectType::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getSPCollisionShapeName() const {

	switch (m_spCollisionShape) {

	case SPCollisionShape::Sphere:
		return "SPHERE";

	case SPCollisionShape::Block:
		return "BLOCK";

	case SPCollisionShape::Capsule:
		return "CAPSULE";

	case SPCollisionShape::Cone:
		return "CONE";

	case SPCollisionShape::DeformableSphere:
		return "DEFORMABLE_SPHERE";

	default:
	case SPCollisionShape::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getSPMeshBoundModeName() const {

	switch (m_spMeshBoundMode) {

	case SPMeshBoundMode::Fill:
		return "FILL";

	default:
	case SPMeshBoundMode::Default:
		return "DEFAULT";

	case SPMeshBoundMode::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getSPDisplayModeName() const {

	switch (m_spDisplayMode) {

	case SPDisplayMode::RenderAndCollision:
		return "RENDER_AND_COLLISION";

	case SPDisplayMode::Wireframe:
		return "WIREFRAME";

	default:
	case SPDisplayMode::Render:
		return "RENDER";

	case SPDisplayMode::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getSPRenderSourceName() const {

	return m_particleRenderMode ==
		PARTICLE_RENDER_MESH
		? "MESH"
		: "PARTICLE";
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

const char* TheArbiter::getParticleGridLayoutName() const {
	switch (m_particleSimDraftConfig.gridLayout) {
	case ParticleGridLayout::None:
		return "NONE";

	case ParticleGridLayout::Minimal:
		return "MINIMAL";

	case ParticleGridLayout::Full:
		return "FULL";

	case ParticleGridLayout::Dynamic:
		return "DYNAMIC";

	default:
	case ParticleGridLayout::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getParticleColorModeName() const {
	switch (m_particleSimDraftConfig.colorMode) {
	case ParticleColorMode::Default:
		return "DEFAULT";

	case ParticleColorMode::RGB:
		return "RGB";

	default:
	case ParticleColorMode::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getParticleRadiusModeName() const {
	switch (m_particleSimDraftConfig.radiusMode) {
	case ParticleRadiusMode::Uniform:
		return "UNIFORM";

	case ParticleRadiusMode::Random:
		return "RANDOM";

	default:
	case ParticleRadiusMode::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getParticleColorChannelName() const {
	switch (m_particleSimDraftConfig.selectedColorChannel) {
	case ParticleColorChannel::Red:
		return "RED";

	case ParticleColorChannel::Green:
		return "GREEN";

	case ParticleColorChannel::Blue:
		return "BLUE";

	default:
	case ParticleColorChannel::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getParticleSimResetModeName() const {
	switch (m_particleSimDraftConfig.resetMode) {
	case ParticleSimResetMode::Default:
		return "DEFAULT";

	case ParticleSimResetMode::Random:
		return "RANDOM";

	default:
	case ParticleSimResetMode::Count:
		return "UNKNOWN";
	}
}

const char* TheArbiter::getSingleParticleSubLayerName() const {

	switch (m_singleParticleSubLayer) {

	case SP_SUB_LAYER_REFERENCE:
		return "SUB_LAYER_0 COLLISION SETUP";

	case SP_SUB_LAYER_SHAPE_EDIT:
		return "SUB_LAYER_1 RENDERING SETUP";

	case SP_SUB_LAYER_VOLUME_RENDER:
		return "SUB_LAYER_2 MESH / VOLUME PREVIEW AND EDIT";

	case SP_SUB_LAYER_MARCHING_CUBES:
		return "SUB_LAYER_3 MARCHING CUBES";

	default:
	case SP_SUB_LAYER_COUNT:
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

	case VOLUME_PRIMITIVE_CONE:
		return "CONE";

	case VOLUME_PRIMITIVE_CAPSULE:
		return "CAPSULE";

	case VOLUME_PRIMITIVE_WEDGE:
		return "WEDGE";

	case VOLUME_PRIMITIVE_DELTA_WING:
		return "DELTA_WING";

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
const char* TheArbiter::getSPOverlapPreviewStatusName() const {
	switch (m_spOverlapPreviewStatus) {
	case SP_OVERLAP_ACTIVE:
		return "ACTIVE";

	case SP_OVERLAP_OUTSIDE_CAGE:
		return "OUTSIDE CAGE";

	default:
	case SP_OVERLAP_POSITION_IN_NODE_2:
		return "POSITION IN NODE_2";
	}
}
const char* TheArbiter::getSPMirrorModeName() const {
	return m_spMirrorMode == SP_MIRROR_ON ? "ON" : "NONE";
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

// =============================================================================
// MENU COMMAND ENTRY POINTS
// =============================================================================
TheArbiter::ArbiterResult
TheArbiter::trySelectParticleAtCurrentSlice() {

	ArbiterResult result;

	// This predicate now means:
	//
	//     Sub-Layer 0
	//     selection armed
	//     no selected particle
	//     panel closed
	if (!isWorkplaneParticleSelectSubLayer())
		return result;


	// Particle 0 remains anchored at the origin for this phase.
	const bool sliceNearOrigin =
		abs(m_workplaneSlice) <= 1;

	const float pickRadius = 0.35f;

	const bool hoverNearOrigin =
		m_hoverValid &&
		(m_hoverX * m_hoverX +
			m_hoverY * m_hoverY) <= (pickRadius * pickRadius);

	if (sliceNearOrigin && hoverNearOrigin) {

		// Clicking selects only.
		// Clicking again does not deselect.
		m_selectedParticle = true;

		// Keep selection armed so E deselection can return
		// immediately to an active selection context.
		m_spSelectionArmed = true;
		m_subLayerPanelOpen = false;

		m_activeSubLayerPanelItem =
			SP0_LIST_COLLISION_SHAPE;

		result.command = CMD_REDRAW;

		result.requestRedraw = true;
		result.rebuildMenu = true;

		return result;
	}

	// Empty-space click preserves all state.
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
TheArbiter::setVolumePrimitiveFromMenu(VolumePrimitive primitive) {
	ArbiterResult result;
	const int value = static_cast<int>(primitive);
	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_EDIT_OBJECT ||
		value < static_cast<int>(VOLUME_PRIMITIVE_BASE) ||
		value >= static_cast<int>(VOLUME_PRIMITIVE_COUNT)) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}
	activeVolumeState().primitive = primitive;
	m_activeSubLayerPanelItem = hasInjectionVoxelSelected()
		? INJECTION_EDIT_LIST_OBJECT
		: EDIT_LIST_OBJECT;
	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.regenerateVolume = true;
	result.rebuildMenu = true;
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
TheArbiter::setSPMirrorModeFromMenu(SPMirrorMode mode) {
	ArbiterResult result;
	const bool validMode = mode == SP_MIRROR_NONE || mode == SP_MIRROR_ON;
	if (!isVolumeRenderSubLayer() ||
		m_volumeAssemblyNode != VOLUME_NODE_EDIT_OBJECT ||
		!hasInjectionVoxelSelected() ||
		!isEditingInjectionVoxel1() ||
		!validMode) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_spMirrorMode = mode;
	m_activeSubLayerPanelItem = INJECTION_EDIT_LIST_MIRROR;
	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.regenerateVolume = true;
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
		MC_LIST_SAVE_STATIC_PARTICLE;

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
		static_cast<int>(MC_LIST_SAVE_STATIC_PARTICLE) &&
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

TheArbiter::ArbiterResult
TheArbiter::activateSPPrimaryActionFromMenu() {
	ArbiterResult result;

	if (!isSingleParticleReferenceSubLayer()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	handleSingleParticlePrimaryAction(result);
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setSPCollisionShapeFromMenu(SPCollisionShape shape) {
	ArbiterResult result;
	const int value = static_cast<int>(shape);

	if (!isSingleParticleReferenceSubLayer() ||
		value < 0 ||
		value >= static_cast<int>(SPCollisionShape::Count)) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	// Reserved choices are presentation state only. The runtime collision
	// system remains the operational particle sphere.
	m_spCollisionShape = shape;
	m_activeSubLayerPanelItem = SP0_LIST_COLLISION_SHAPE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::enterSPRenderingSetupFromMenu() {
	ArbiterResult result;

	if (!isSingleParticleReferenceSubLayer() ||
		!m_selectedParticle) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_singleParticleSubLayer = SP_SUB_LAYER_SHAPE_EDIT;
	m_subLayerPanelOpen = true;
	m_activeSubLayerPanelItem = SP1_LIST_RENDER_SOURCE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setSPRenderSourceFromMenu(ParticleRenderMode mode) {
	ArbiterResult result;

	if (!isShapeEditSubLayer() ||
		(mode != PARTICLE_RENDER_DEFAULT &&
			mode != PARTICLE_RENDER_MESH)) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_particleRenderMode = mode;
	m_activeSubLayerPanelItem = SP1_LIST_RENDER_SOURCE;

	result.command = CMD_PARTICLE_RENDER_MODE_CHANGED;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setSPMeshBoundModeFromMenu(SPMeshBoundMode mode) {
	ArbiterResult result;
	const int value = static_cast<int>(mode);

	if (!isShapeEditSubLayer() ||
		value < 0 ||
		value >= static_cast<int>(SPMeshBoundMode::Count)) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_spMeshBoundMode = mode;
	m_activeSubLayerPanelItem = SP1_LIST_MESH_BOUND;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setSPDisplayModeFromMenu(SPDisplayMode mode) {
	ArbiterResult result;
	const int value = static_cast<int>(mode);

	if (!isShapeEditSubLayer() ||
		value < 0 ||
		value >= static_cast<int>(SPDisplayMode::Count)) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_spDisplayMode = mode;
	m_activeSubLayerPanelItem = SP1_LIST_DISPLAY_MODE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::setSPRenderCageVisibleFromMenu(bool visible) {
	ArbiterResult result;

	if (!isShapeEditSubLayer()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_spRenderCageVisible = visible;
	m_activeSubLayerPanelItem = SP1_LIST_RENDER_CAGE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::enterSPVolumePreviewFromMenu() {
	ArbiterResult result;

	if (!isShapeEditSubLayer() ||
		!m_selectedParticle) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_singleParticleSubLayer = SP_SUB_LAYER_VOLUME_RENDER;
	m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
	m_subLayerPanelOpen = true;
	m_activeSubLayerPanelItem = PREVIEW_LIST_INJECTION_MODE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.regenerateVolume = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::returnSPCollisionSetupFromMenu() {
	ArbiterResult result;

	if (!isShapeEditSubLayer()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;
	m_subLayerPanelOpen = true;
	m_activeSubLayerPanelItem = SP0_LIST_COLLISION_SHAPE;

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
	return result;
}

TheArbiter::ArbiterResult
TheArbiter::returnSPToLayer2FromMenu() {
	ArbiterResult result;

	if (!isSingleParticleReferenceSubLayer()) {
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return result;
	}

	retreatSingleParticleSubLayer(result);
	return result;
}

// =============================================================================
// LAYER / SUB-LAYER TRANSITIONS
// =============================================================================
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
		if (isWorkspaceDomainsSelected()) {
			setApplicationLayer(ApplicationLayer::DOMAIN_SELECTION);
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	if (m_navigation.layer == ApplicationLayer::DOMAIN_SELECTION) {
		if (isSingleParticleSelected() ||
			isParticleSimulationSelected()) {

			setApplicationLayer(
				ApplicationLayer::WORKSPACE_CONFIGURATION
			);

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

				setApplicationLayer(ApplicationLayer::ACTIVE_WORKSPACE);

				m_singleParticleSubLayer = SP_SUB_LAYER_REFERENCE;
				m_activeSubLayerPanelItem = SP0_LIST_COLLISION_SHAPE;

				m_spSelectionArmed = false;
				m_selectedParticle = false;
				m_subLayerPanelOpen = false;
				m_hoverValid = false;
				m_hoverX = 0.0f;
				m_hoverY = 0.0f;
				m_workplaneSlice = 0;

				m_volumeAssemblyNode = VOLUME_NODE_PREVIEW;
				result.command = CMD_PLACE_SINGLE_PARTICLE;
			}
			else {
				result.command =
					CMD_REDRAW;
			}

			result.requestRedraw = true;
			return;
		}

		// PARTICLE_SIM path migrated from the legacy PARTICLES_3D mode.
		if (isParticleSimulationSelected()) {
			activateParticleSimLayer2Item(result);
			return;
		}

		result.command = CMD_REDRAW;
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

	if (!isSimulationRunLayer() ||
		!isSingleParticleSelected()) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 3 -> Sub-Layer 2 Preview.
	// Preserve the current MC return behavior.
	// ---------------------------------------------------------
	if (m_singleParticleSubLayer ==
		SP_SUB_LAYER_MARCHING_CUBES) {

		m_singleParticleSubLayer =
			SP_SUB_LAYER_VOLUME_RENDER;

		m_volumeAssemblyNode =
			VOLUME_NODE_PREVIEW;

		m_subLayerPanelOpen = true;

		m_activeSubLayerPanelItem =
			PREVIEW_LIST_INJECTION_MODE;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// ---------------------------------------------------------
	// Walk backward through the existing Sub-Layer 2
	// four-node assembly sequence.
	// ---------------------------------------------------------
	if (m_singleParticleSubLayer ==
		SP_SUB_LAYER_VOLUME_RENDER &&
		m_volumeAssemblyNode !=
		VOLUME_NODE_PREVIEW) {

		m_volumeAssemblyNode =
			static_cast<VolumeAssemblyNode>(
				static_cast<int>(m_volumeAssemblyNode) - 1);

		m_activeSubLayerPanelItem = 0;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 2 Preview -> Sub-Layer 1 Rendering Setup.
	// ---------------------------------------------------------
	if (m_singleParticleSubLayer ==
		SP_SUB_LAYER_VOLUME_RENDER) {

		m_singleParticleSubLayer =
			SP_SUB_LAYER_SHAPE_EDIT;

		m_subLayerPanelOpen = false;

		m_activeSubLayerPanelItem =
			SP1_LIST_RENDER_SOURCE;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 1 -> Sub-Layer 0.
	// Selection remains intact because Q is structural.
	// ---------------------------------------------------------
	if (m_singleParticleSubLayer ==
		SP_SUB_LAYER_SHAPE_EDIT) {

		m_singleParticleSubLayer =
			SP_SUB_LAYER_REFERENCE;

		m_subLayerPanelOpen = false;

		m_activeSubLayerPanelItem =
			SP0_LIST_COLLISION_SHAPE;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 0 -> Layer 2 configuration.
	// Workspace selection state is cleared here.
	// ---------------------------------------------------------
	m_spSelectionArmed = false;
	m_selectedParticle = false;

	m_subLayerPanelOpen = false;
	m_activeSubLayerPanelItem =
		SP0_LIST_COLLISION_SHAPE;

	m_hoverValid = false;
	m_hoverX = 0.0f;
	m_hoverY = 0.0f;
	m_workplaneSlice = 0;

	m_volumeAssemblyNode =
		VOLUME_NODE_PREVIEW;

	setApplicationLayer(
		ApplicationLayer::WORKSPACE_CONFIGURATION
	);

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
		(std::max)(-64, (std::min)(64, m_workplaneSlice + delta));

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
			break;

		case PARTICLE_LIST_RADIUS:
			if (dir < 0.0f) {
				decreaseParticleRadius();
			}
			else {
				increaseParticleRadius();
			}

			result.command = CMD_PARTICLE_RADIUS_CHANGED;
			break;

		case PARTICLE_LIST_RENDER_MODE:
			toggleParticleRenderMode();
			result.command = CMD_PARTICLE_RENDER_MODE_CHANGED;
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
			break;

		case PARTICLE_LIST_RESET:
			toggleParticleResetMode();
			result.command = CMD_REDRAW;
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

// =============================================================================
// SUB-LAYER PANEL NAVIGATION / ACTIONS
// =============================================================================
void TheArbiter::toggleSubLayerPanel(ArbiterResult& result) {

	if (!isSubLayerPanelEligible()) {

		m_subLayerPanelOpen = false;
		m_activeSubLayerPanelItem = 0;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	m_subLayerPanelOpen =
		!m_subLayerPanelOpen;

	if (m_subLayerPanelOpen) {

		if (isSingleParticleReferenceSubLayer()) {

			m_activeSubLayerPanelItem =
				SP0_LIST_COLLISION_SHAPE;
		}
		else if (isShapeEditSubLayer()) {

			m_activeSubLayerPanelItem =
				SP1_LIST_RENDER_SOURCE;
		}
		else {

			m_activeSubLayerPanelItem = 0;
		}
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
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

	// ---------------------------------------------------------
// Sub-Layer 0 — collision setup.
// ---------------------------------------------------------
	if (isSingleParticleReferenceSubLayer()) {

		if (m_activeSubLayerPanelItem ==
			SP0_LIST_COLLISION_SHAPE) {

			cycleSPCollisionShape(dir);
		}

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 1 — rendering setup.
	// ---------------------------------------------------------
	if (isShapeEditSubLayer()) {

		switch (m_activeSubLayerPanelItem) {

		case SP1_LIST_RENDER_SOURCE:

			toggleParticleRenderMode();

			result.command =
				CMD_PARTICLE_RENDER_MODE_CHANGED;
			break;

		case SP1_LIST_MESH_BOUND:

			cycleSPMeshBoundMode(dir);

			result.command =
				CMD_REDRAW;
			break;

		case SP1_LIST_DISPLAY_MODE:

			cycleSPDisplayMode(dir);

			result.command =
				CMD_REDRAW;
			break;

		case SP1_LIST_RENDER_CAGE:

			toggleSPRenderCage();

			result.command =
				CMD_REDRAW;
			break;

		default:
		case SP1_LIST_MESH_PREVIEW_EDIT:
		case SP1_LIST_COLLISION_SETUP:
		case SP1_LIST_COUNT:

			result.command =
				CMD_REDRAW;
			break;
		}

		result.requestRedraw = true;
		result.rebuildMenu = true;
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

			case INJECTION_EDIT_LIST_MIRROR:
				if (isEditingInjectionVoxel1()) {
					cycleSPMirrorMode(dir);
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

	// ---------------------------------------------------------
	// Sub-Layer 0 panel actions.
	// ---------------------------------------------------------
	if (isSingleParticleReferenceSubLayer()) {
		switch (m_activeSubLayerPanelItem) {
		case SP0_LIST_LOAD_STATIC_PARTICLE:
			result.loadStaticParticleRequested = true;
			break;
		case SP0_LIST_SAVE_ACTIVE_PARTICLE:
			result.saveStaticParticleAsRequested = true;
			break;
		case SP0_LIST_RENDERING_SETUP:

			m_singleParticleSubLayer =
				SP_SUB_LAYER_SHAPE_EDIT;

			m_subLayerPanelOpen = true;

			m_activeSubLayerPanelItem =
				SP1_LIST_RENDER_SOURCE;

			result.rebuildMenu = true;
			break;
		default:
			break;
		}

		// Collision shape row is an adjustment row.
		// E does not activate reserved collision implementations.
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	// ---------------------------------------------------------
	// Sub-Layer 1 panel actions.
	// ---------------------------------------------------------
	if (isShapeEditSubLayer()) {

		switch (m_activeSubLayerPanelItem) {

		case SP1_LIST_RENDER_SOURCE:

			toggleParticleRenderMode();

			result.command =
				CMD_PARTICLE_RENDER_MODE_CHANGED;
			break;

		case SP1_LIST_MESH_BOUND:

			cycleSPMeshBoundMode(+1.0f);

			result.command =
				CMD_REDRAW;
			break;

		case SP1_LIST_DISPLAY_MODE:

			cycleSPDisplayMode(+1.0f);

			result.command =
				CMD_REDRAW;
			break;

		case SP1_LIST_RENDER_CAGE:

			toggleSPRenderCage();

			result.command =
				CMD_REDRAW;
			break;

		case SP1_LIST_MESH_PREVIEW_EDIT:

			m_singleParticleSubLayer =
				SP_SUB_LAYER_VOLUME_RENDER;

			m_volumeAssemblyNode =
				VOLUME_NODE_PREVIEW;

			m_subLayerPanelOpen = true;

			m_activeSubLayerPanelItem =
				PREVIEW_LIST_INJECTION_MODE;

			result.command =
				CMD_REDRAW;

			result.regenerateVolume =
				true;

			result.rebuildMenu =
				true;
			break;

		case SP1_LIST_COLLISION_SETUP:

			m_singleParticleSubLayer =
				SP_SUB_LAYER_REFERENCE;

			m_subLayerPanelOpen = true;

			m_activeSubLayerPanelItem =
				SP0_LIST_COLLISION_SHAPE;

			result.command =
				CMD_REDRAW;

			result.rebuildMenu =
				true;
			break;

		default:
		case SP1_LIST_COUNT:

			result.command =
				CMD_REDRAW;
			break;
		}

		result.requestRedraw = true;
		return;
	}

	if (isMarchingCubesSubLayer()) {
		switch (m_activeSubLayerPanelItem) {
		case MC_LIST_SAVE_STATIC_PARTICLE:
			result.saveStaticParticleAsRequested = true;
			break;

		case MC_LIST_SAVE_STATIC_PARTICLE_AS:
			beginSingleParticleAssetNameEntry(result);
			return;

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
			m_activeSubLayerPanelItem = MC_LIST_SAVE_STATIC_PARTICLE;
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

				cycleSPMirrorMode(+1.0f);
				result.regenerateVolume = true;
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

// =============================================================================
// BASIS / GEOMETRY HELPERS
// =============================================================================
TheArbiter::BasisVector
TheArbiter::normalizeBasisVector(const BasisVector& v) const {
	const float lengthSquared = dotBasisVector(v, v);

	if (lengthSquared <= 1.0e-12f)
		return { 0.0f, 0.0f, 0.0f };

	const float inverseLength = 1.0f / std::sqrt(lengthSquared);

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

	const float sp = std::sin(pitch);
	const float cp = std::cos(pitch);

	const float sy = std::sin(yaw);
	const float cy = std::cos(yaw);

	const float sr = std::sin(roll);
	const float cr = std::cos(roll);

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

unsigned int
TheArbiter::getAvailableRGBCountForSelectedChannel() const {

	const unsigned int capacity =
		kParticleSimCapacity;

	const ParticleSimDraftConfig& draft =
		m_particleSimDraftConfig;

	unsigned int occupiedByOtherChannels = 0;

	switch (draft.selectedColorChannel) {

	case ParticleColorChannel::Red:
		occupiedByOtherChannels =
			draft.greenCount +
			draft.blueCount;
		break;

	case ParticleColorChannel::Green:
		occupiedByOtherChannels =
			draft.redCount +
			draft.blueCount;
		break;

	case ParticleColorChannel::Blue:
		occupiedByOtherChannels =
			draft.redCount +
			draft.greenCount;
		break;

	default:
	case ParticleColorChannel::Count:
		return 0;
	}

	if (occupiedByOtherChannels >= capacity)
		return 0;


	return capacity - occupiedByOtherChannels;
}

void TheArbiter::beginSelectedRGBCountEntry(
	ArbiterResult& result) {

	const unsigned int maximum =
		getAvailableRGBCountForSelectedChannel();

	unsigned int currentValue = 0;

	const char* prompt =
		"ENTER PARTICLE AMOUNT";

	switch (m_particleSimDraftConfig.selectedColorChannel) {

	case ParticleColorChannel::Red:

		m_textEntryTarget =
			TextEntryTarget::ParticleRedCount;

		currentValue =
			m_particleSimDraftConfig.redCount;

		prompt = "ENTER RED PARTICLE AMOUNT";
		break;

	case ParticleColorChannel::Green:

		m_textEntryTarget =
			TextEntryTarget::ParticleGreenCount;

		currentValue =
			m_particleSimDraftConfig.greenCount;

		prompt ="ENTER GREEN PARTICLE AMOUNT";
		break;

	case ParticleColorChannel::Blue:

		m_textEntryTarget =
			TextEntryTarget::ParticleBlueCount;

		currentValue =
			m_particleSimDraftConfig.blueCount;

		prompt = "ENTER BLUE PARTICLE AMOUNT";
		break;

	default:
	case ParticleColorChannel::Count:

		m_textEntryTarget =
			TextEntryTarget::None;

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	const bool started =
		m_textEntry.beginUnsignedInteger(
			prompt,
			0,
			maximum,
			currentValue
		);

	if (!started) {
		m_textEntryTarget =
			TextEntryTarget::None;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}

void TheArbiter::beginDefaultParticleCountEntry(
	ArbiterResult& result) {

	m_textEntryTarget =
		TextEntryTarget::ParticleDefaultCount;

	const bool started =
		m_textEntry.beginUnsignedInteger(
			"ENTER PARTICLE AMOUNT",
			0,
			kParticleSimCapacity,
			m_particleSimDraftConfig.defaultParticleCount
		);

	if (!started) {
		m_textEntryTarget =
			TextEntryTarget::None;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}

void TheArbiter::beginSingleParticleAssetNameEntry(
	ArbiterResult& result,
	const std::string& initialName) {
	m_textEntryTarget = TextEntryTarget::SingleParticleAssetName;
	if (!m_textEntry.beginAssetName(
		"ENTER STATIC PARTICLE ASSET NAME",
		initialName,
		96u)) {
		m_textEntryTarget = TextEntryTarget::None;
	}
	result.command = CMD_REDRAW;
	result.requestRedraw = true;
}

void TheArbiter::applyCommittedTextEntry(
	ArbiterResult& result) {
	if (m_textEntryTarget == TextEntryTarget::SingleParticleAssetName) {
		result.staticParticleAssetName = m_textEntry.getNormalizedText();
		result.saveStaticParticleAsRequested =
			!result.staticParticleAssetName.empty();
		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		result.rebuildMenu = true;
		return;
	}

	unsigned int value = 0;

	if (!m_textEntry.tryGetCommittedUnsigned(value)) {

		result.command = CMD_REDRAW;
		result.requestRedraw = true;
		return;
	}

	switch (m_textEntryTarget) {

	case TextEntryTarget::ParticleDefaultCount:

		m_particleSimDraftConfig.defaultParticleCount =
			value;
		break;

	case TextEntryTarget::ParticleRedCount:

		m_particleSimDraftConfig.redCount =
			value;
		break;

	case TextEntryTarget::ParticleGreenCount:

		m_particleSimDraftConfig.greenCount =
			value;
		break;

	case TextEntryTarget::ParticleBlueCount:

		m_particleSimDraftConfig.blueCount =
			value;
		break;

	case TextEntryTarget::SingleParticleAssetName:
		break;

	default:
	case TextEntryTarget::None:
		break;
	}

	result.command = CMD_REDRAW;
	result.requestRedraw = true;
	result.rebuildMenu = true;
}

TheArbiter::ArbiterResult
TheArbiter::handleTextEntryKeyboard(
	const KeyboardInput::KeyEvent& event) {

	ArbiterResult result;

	const TextEntryAction action =
		m_textEntry.handleRawKey(event.rawKey);

	result.command = CMD_REDRAW;
	result.requestRedraw = true;

	switch (action) {

	case TextEntryAction::Changed:
		// ViewPort reads and displays the changed buffer.
		break;

	case TextEntryAction::Rejected:
		// Session remains active.
		// ViewPort displays the rejection status.
		break;

	case TextEntryAction::Cancelled:

		m_textEntryTarget =
			TextEntryTarget::None;
		break;

	case TextEntryAction::Committed:

		applyCommittedTextEntry(result);

		m_textEntryTarget =
			TextEntryTarget::None;
		break;

	default:
	case TextEntryAction::None:
		break;
	}

	return result;
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

void TheArbiter::getMirroredInjectionDirection(
	int& dx, int& dy, int& dz) const {
	dx = -getInjectionVoxelDX();
	dy = -getInjectionVoxelDY();
	dz = -getInjectionVoxelDZ();
}

TheArbiter::VolumeInjectionVoxel
TheArbiter::getMirroredInjectionVoxel() const {
	const int current = static_cast<int>(m_volumeInjectionVoxel);
	if (current <= 0 || current >= kInjectionVoxelCycleCount) {
		return INJECTION_VOXEL_NONE;
	}
	const InjectionDirection source = kInjectionDirections[current];
	for (int i = 1; i < kInjectionVoxelCycleCount; ++i) {
		const InjectionDirection candidate = kInjectionDirections[i];
		if (candidate.x == -source.x &&
			candidate.y == -source.y &&
			candidate.z == -source.z) {
			return kInjectionVoxelCycle[i];
		}
	}
	return INJECTION_VOXEL_NONE;
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
