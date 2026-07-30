#ifndef __ARBITER_SYS_2D_H__
#define __ARBITER_SYS_2D_H__

#include "Keyboard2D.h"
#include <string>

class TheArbiter2D {
public:
	enum AppLayer {
		LAYER_MENU = 0,
		LAYER_ENVIRONMENT_CONFIGURATION = 1,
		LAYER_2D_GRID_MODE_CONFIGURATION = 2,
		LAYER_SIMULATION_RUN = 3
	};

	enum EnvironmentSelection {
		ENV_IDLE = 0,
		ENV_2D_GRID = 1
	};

	enum GridSelection {
		GRID_GRAPH = 0,
		GRID_PARTICLES = 1
	};

	enum EnvironmentConfigList {
		ENV_CONFIG_LIST_GRID_SIZE = 0,
		ENV_CONFIG_SIM_WORLD_SCALE = 1,
		ENV_CONFIG_LIST_GRID_MODE = 2,
		ENV_CONFIG_LIST_CONFIRM = 3,
		ENV_CONFIG_LIST_COUNT = 4
	};

	enum ParticleColorSelection {
		PARTICLE_COLOR_RED = 0,
		PARTICLE_COLOR_BLUE = 1,
		PARTICLE_COLOR_GREEN = 2,
		PARTICLE_COLOR_YELLOW = 3
	};

	enum ParticleResetMode {
		PARTICLE_RESET_DEFAULT = 0,
		PARTICLE_RESET_RANDOM = 1
	};

	enum ParticleConfigList {
		PARTICLE_LIST_GENERATE = 0,
		PARTICLE_LIST_RESET = 1,
		PARTICLE_LIST_RUN = 2,
		PARTICLE_LIST_COUNT = 3
	};

	enum ArbiterCommand {
		CMD_NONE = 0,
		CMD_EXIT,
		CMD_REDRAW,
		CMD_TOGGLE_PAUSE,
		CMD_STEP_SIMULATION,
		CMD_START_PARTICLE_SIMULATION
	};

	struct ArbiterResult {
		ArbiterCommand command = CMD_NONE;
		bool requestRedraw = false;
	};

	TheArbiter2D();
	~TheArbiter2D();

	ArbiterResult processKeyboard(const KeyboardInput2D::KeyEvent& event);

	AppLayer getAppLayer() const { return m_appLayer; }
	EnvironmentSelection getEnvironmentSelection() const { return m_envSelection; }
	GridSelection getGridSelection() const { return m_gridSelection; }
	EnvironmentConfigList getActiveEnvironmentConfigList() const { return m_activeEnvironmentConfigList; }
	ParticleColorSelection getParticleColorSelection() const { return m_particleColorSelection; }
	ParticleResetMode getParticleResetMode() const { return m_particleResetMode; }
	ParticleConfigList getActiveParticleConfigList() const { return m_activeParticleConfigList; }

	unsigned int getRedParticleCount() const { return m_redParticleCount; }
	unsigned int getBlueParticleCount() const { return m_blueParticleCount; }
	unsigned int getGreenParticleCount() const { return m_greenParticleCount; }
	unsigned int getYellowParticleCount() const { return m_yellowParticleCount; }
	unsigned int getMaxGeneratedParticles() const { return m_maxGeneratedParticles; }
	unsigned int getTotalGeneratedParticles() const;
	unsigned int getSelectedParticleCount() const;
	unsigned int getGridCellCountSelection() const;
	unsigned int getGridCellsPerAxisSelection() const;

	const char* getParticleEntryText() const { return m_particleEntryBuffer.c_str(); }
	const char* getParticleConfigMessage() const { return m_particleConfigMessage.c_str(); }

	float getWorldBoxScaleSelection() const;
	float getSimBoxSize() const { return getWorldBoxScaleSelection(); }

	// Optional compatibility helper while old ViewPort/Engine code is being updated.
	float getGridSizeSelection() const {
		return static_cast<float>(getGridCellCountSelection());
	}

	void setMaxGeneratedParticles(unsigned int maxParticles);

	bool isMenuLayer() const { return m_appLayer == LAYER_MENU; }
	bool isEnvironmentConfigLayer() const { return m_appLayer == LAYER_ENVIRONMENT_CONFIGURATION; }
	bool isParticleConfigLayer() const { return m_appLayer == LAYER_2D_GRID_MODE_CONFIGURATION; }
	bool isSimulationRunLayer() const { return m_appLayer == LAYER_SIMULATION_RUN; }
	bool is2DViewLayer() const { return m_appLayer != LAYER_MENU; }
	bool isIdleSelected() const { return m_envSelection == ENV_IDLE; }
	bool is2DVisualizationSelected() const { return m_envSelection == ENV_2D_GRID; }
	bool isGraphSelected() const { return m_gridSelection == GRID_GRAPH; }
	bool isParticlesSelected() const { return m_gridSelection == GRID_PARTICLES; }
	bool isParticleCountEntryActive() const { return m_particleCountEntryActive; }

	const char* getLayerName() const;
	const char* getEnvironmentName() const;
	const char* getGridSelectionName() const;
	const char* getActiveEnvironmentConfigListName() const;
	const char* getParticleColorName() const;
	const char* getParticleResetModeName() const;
	const char* getActiveParticleConfigListName() const;

	void resetToMenu();

private:
	void toggleEnvironmentSelection();
	void toggleGridSelection();
	
	void adjustGridCellCountSelection(int direction);
	void adjustWorldBoxScaleSelection(int direction);

	int getMinimumWorldBoxOptionIndexForCurrentGrid() const;
	void clampWorldBoxScaleToCurrentGrid();

	void toggleParticleColorSelection();
	void toggleParticleResetMode();

	void moveEnvironmentConfigCursorUp();
	void moveEnvironmentConfigCursorDown();
	void moveParticleConfigCursorUp();
	void moveParticleConfigCursorDown();

	void goBackOneLayer(ArbiterResult& result);
	void enterCurrentSelection(ArbiterResult& result);

	void beginParticleCountEntry();
	void handleParticleCountEntry(
		const KeyboardInput2D::KeyEvent& event, 
		ArbiterResult& result
	);

	void commitParticleCountEntry();
	void appendParticleEntryDigit(char digit);
	void backspaceParticleEntry();
	void adjustParticleEntryValue(int direction);
	void setSelectedParticleCount(unsigned int count);
	void resetParticleCounts();
	bool validateParticleCountBudget();

private:

	AppLayer m_appLayer = LAYER_MENU;
	EnvironmentSelection m_envSelection = ENV_IDLE;
	GridSelection m_gridSelection = GRID_PARTICLES;
	EnvironmentConfigList m_activeEnvironmentConfigList = ENV_CONFIG_LIST_GRID_SIZE;
	
	int m_gridCellOptionIndex = 0;
	int m_worldBoxOptionIndex = 0;

	static constexpr int kGridCellOptionCount = 4;
	static constexpr int kWorldBoxOptionCount = 4;

	ParticleColorSelection m_particleColorSelection = PARTICLE_COLOR_RED;
	ParticleResetMode m_particleResetMode = PARTICLE_RESET_DEFAULT;
	ParticleConfigList m_activeParticleConfigList = PARTICLE_LIST_GENERATE;

	unsigned int m_redParticleCount = 0;
	unsigned int m_blueParticleCount = 0;
	unsigned int m_greenParticleCount = 0;
	unsigned int m_yellowParticleCount = 0;
	unsigned int m_maxGeneratedParticles = 4096;

	bool m_particleCountEntryActive = false;
	std::string m_particleEntryBuffer;
	std::string m_particleConfigMessage;
};

#endif
