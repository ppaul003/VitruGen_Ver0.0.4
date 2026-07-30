#include "TheArbiter2D.h"
#include <algorithm>
#include <cstdlib>

using namespace std;

namespace {
    static unsigned int parseUnsignedOrZero(const string& text) {
        if (text.empty()) return 0u;
        return static_cast<unsigned int>(strtoul(text.c_str(), nullptr, 10));
    }

    static unsigned int gridCellCountFromIndex(int index) {
        static const unsigned int options[] = { 64u, 256u, 1024u, 4096u };
        index = max(0, min(index, 3));
        return options[index];
    }

    static unsigned int gridCellsPerAxisFromIndex(int index) {
        static const unsigned int options[] = { 8u, 16u, 32u, 64u };
        index = max(0, min(index, 3));
        return options[index];
    }

    static float worldBoxScaleFromIndex(int index) {
        static const float options[] = { 4.0f, 8.0f, 32.0f, 64.0f };
        index = max(0, min(index, 3));
        return options[index];
    }
}

TheArbiter2D::TheArbiter2D() {}
TheArbiter2D::~TheArbiter2D() {}

void TheArbiter2D::setMaxGeneratedParticles(unsigned int maxParticles) {
    m_maxGeneratedParticles = max(1u, maxParticles);
}

unsigned int TheArbiter2D::getTotalGeneratedParticles() const {
    return 
        m_redParticleCount + 
        m_blueParticleCount + 
        m_greenParticleCount +
        m_yellowParticleCount;
}

unsigned int TheArbiter2D::getSelectedParticleCount() const {
    switch (m_particleColorSelection) {
    case PARTICLE_COLOR_BLUE:
        return m_blueParticleCount;

    case PARTICLE_COLOR_GREEN:
        return m_greenParticleCount;

    case PARTICLE_COLOR_YELLOW:
        return m_yellowParticleCount;

    default:
    case PARTICLE_COLOR_RED:
        return m_redParticleCount;
    }
}

unsigned int TheArbiter2D::getGridCellCountSelection() const {
    return gridCellCountFromIndex(m_gridCellOptionIndex);
}

unsigned int TheArbiter2D::getGridCellsPerAxisSelection() const {
    return gridCellsPerAxisFromIndex(m_gridCellOptionIndex);
}

void TheArbiter2D::setSelectedParticleCount(unsigned int count) {
    count = min(count, m_maxGeneratedParticles);

    switch (m_particleColorSelection) {
    case PARTICLE_COLOR_BLUE:
        m_blueParticleCount = count;
        break;

    case PARTICLE_COLOR_GREEN:
        m_greenParticleCount = count;
        break;

    case PARTICLE_COLOR_YELLOW:
        m_yellowParticleCount = count;
        break;

    default:
    case PARTICLE_COLOR_RED:
        m_redParticleCount = count;
        break;
    }
}

void TheArbiter2D::resetParticleCounts() {
    m_redParticleCount = 0;
    m_blueParticleCount = 0;
    m_greenParticleCount = 0;
    m_yellowParticleCount = 0;
    m_particleEntryBuffer.clear();
    m_particleCountEntryActive = false;
}

float TheArbiter2D::getWorldBoxScaleSelection() const {
    return worldBoxScaleFromIndex(m_worldBoxOptionIndex);
}

bool TheArbiter2D::validateParticleCountBudget() {
    const unsigned int total = getTotalGeneratedParticles();

    if (total <= m_maxGeneratedParticles) {
        m_particleConfigMessage =
            "PARTICLE COUNT ACCEPTED: TOTAL " +
            to_string(total) + " / " + to_string(m_maxGeneratedParticles) + ".";
        return true;
    }

    resetParticleCounts();
    m_particleConfigMessage =
        "ERROR: PARTICLE TOTAL EXCEEDED MAX " +
        to_string(m_maxGeneratedParticles) + ". COUNTS RESET TO 0.";
    return false;
}

void TheArbiter2D::backspaceParticleEntry() {
    if (!m_particleEntryBuffer.empty()) {
        m_particleEntryBuffer.pop_back();
    }
}

void TheArbiter2D::adjustParticleEntryValue(int direction) {
    int value = static_cast<int>(
        m_particleEntryBuffer.empty()
        ? getSelectedParticleCount()
        : parseUnsignedOrZero(m_particleEntryBuffer));

    value += direction;
    value = max(0, min(static_cast<int>(m_maxGeneratedParticles), value));
    m_particleEntryBuffer = to_string(value);
}

void TheArbiter2D::commitParticleCountEntry() {
    const unsigned int value = min(
        parseUnsignedOrZero(m_particleEntryBuffer),
        m_maxGeneratedParticles
    );

    setSelectedParticleCount(value);
    m_particleCountEntryActive = false;
    m_particleEntryBuffer.clear();

    m_particleConfigMessage =
        string(getParticleColorName()) +
        " PARTICLES SET TO " + to_string(value) + ".";
}

void TheArbiter2D::handleParticleCountEntry(
    const KeyboardInput2D::KeyEvent& event,
    ArbiterResult& result) {

    result.command = CMD_REDRAW;
    result.requestRedraw = true;

    switch (event.signal) {
    case KeyboardInput2D::KEY_ENTER:
        commitParticleCountEntry();
        break;

    case KeyboardInput2D::KEY_BACKSPACE:
        backspaceParticleEntry();
        break;

    case KeyboardInput2D::KEY_DIGIT:
        appendParticleEntryDigit(static_cast<char>(event.rawKey));
        break;

    case KeyboardInput2D::KEY_A:
        adjustParticleEntryValue(-1);
        break;

    case KeyboardInput2D::KEY_D:
        adjustParticleEntryValue(+1);
        break;

    default:
        // While data entry is active, navigation keys are intentionally locked.
        m_particleConfigMessage = "DATA ENTRY ACTIVE: TYPE 0-9 OR PRESS ENTER TO CONFIRM.";
        break;
    }
}

void TheArbiter2D::appendParticleEntryDigit(char digit) {
    if (digit < '0' || digit > '9') return;

    // Keep the edit buffer small and prevent unsigned overflow.
    if (m_particleEntryBuffer.size() >= 9) return;

    string candidate = m_particleEntryBuffer + digit;
    unsigned int value = parseUnsignedOrZero(candidate);
    value = min(value, m_maxGeneratedParticles);

    m_particleEntryBuffer = to_string(value);
}

void TheArbiter2D::beginParticleCountEntry() {
    m_particleCountEntryActive = true;
    m_particleEntryBuffer.clear();
    m_particleConfigMessage =
        string("ENTER ") + getParticleColorName() +
        " PARTICLE COUNT. ENTER CONFIRMS.";
}

void TheArbiter2D::toggleEnvironmentSelection() {
    m_envSelection =
        (m_envSelection == ENV_IDLE)
        ? ENV_2D_GRID
        : ENV_IDLE;
}


void TheArbiter2D::toggleGridSelection() {
    m_gridSelection =
        (m_gridSelection == GRID_GRAPH)
        ? GRID_PARTICLES
        : GRID_GRAPH;
}

void TheArbiter2D::adjustGridCellCountSelection(int direction) {
    m_gridCellOptionIndex += direction;
    m_gridCellOptionIndex = max(0, min(kGridCellOptionCount - 1, m_gridCellOptionIndex));

    // Enforce the minimum world-box scale allowed by the selected grid density.
    clampWorldBoxScaleToCurrentGrid();

    m_particleConfigMessage =
        "GRID SIZE SET: " +
        to_string(getGridCellCountSelection()) + 
        " CELLS. MAX PARTICLES: " +
        to_string(m_maxGeneratedParticles) + ".";
}

void TheArbiter2D::adjustWorldBoxScaleSelection(int direction) {
    m_worldBoxOptionIndex += direction;

    const int minIndex = getMinimumWorldBoxOptionIndexForCurrentGrid();
    m_worldBoxOptionIndex = max(
        minIndex,
        min(kWorldBoxOptionCount - 1, m_worldBoxOptionIndex)
    );
}

int TheArbiter2D::getMinimumWorldBoxOptionIndexForCurrentGrid() const {
    switch (m_gridCellOptionIndex) {
    default:
    case 0:
        return 0; // 64 cells   -> 4, 8, 32, 64
    case 1:
        return 1; // 256 cells  -> 8, 32, 64
    case 2:
        return 2; // 1024 cells -> 32, 64
    case 3:
        return 3; // 4096 cells -> 64 only
    }
}

void TheArbiter2D::clampWorldBoxScaleToCurrentGrid() {
    const int minIndex = getMinimumWorldBoxOptionIndexForCurrentGrid();

    if (m_worldBoxOptionIndex < minIndex) {
        m_worldBoxOptionIndex = minIndex;
    }

    if (m_worldBoxOptionIndex >= kWorldBoxOptionCount) {
        m_worldBoxOptionIndex = kWorldBoxOptionCount - 1;
    }
}

void TheArbiter2D::toggleParticleColorSelection() {
    switch (m_particleColorSelection) {
    case PARTICLE_COLOR_RED:
        m_particleColorSelection = PARTICLE_COLOR_BLUE;
        break;

    case PARTICLE_COLOR_BLUE:
        m_particleColorSelection = PARTICLE_COLOR_GREEN;
        break;

    case PARTICLE_COLOR_GREEN:
        m_particleColorSelection = PARTICLE_COLOR_YELLOW;
        break;

    case PARTICLE_COLOR_YELLOW:
    default:
        m_particleColorSelection = PARTICLE_COLOR_RED;
        break;
    }
}

void TheArbiter2D::toggleParticleResetMode() {
    m_particleResetMode =
        (m_particleResetMode == PARTICLE_RESET_DEFAULT)
        ? PARTICLE_RESET_RANDOM
        : PARTICLE_RESET_DEFAULT;
}

void TheArbiter2D::moveEnvironmentConfigCursorUp() {
    int v = static_cast<int>(m_activeEnvironmentConfigList);
    v = (v + ENV_CONFIG_LIST_COUNT - 1) % ENV_CONFIG_LIST_COUNT;
    m_activeEnvironmentConfigList = static_cast<EnvironmentConfigList>(v);
}

void TheArbiter2D::moveEnvironmentConfigCursorDown() {
    int v = static_cast<int>(m_activeEnvironmentConfigList);
    v = (v + 1) % ENV_CONFIG_LIST_COUNT;
    m_activeEnvironmentConfigList = static_cast<EnvironmentConfigList>(v);
}

void TheArbiter2D::moveParticleConfigCursorUp() {
    int v = static_cast<int>(m_activeParticleConfigList);
    v = (v + PARTICLE_LIST_COUNT - 1) % PARTICLE_LIST_COUNT;
    m_activeParticleConfigList = static_cast<ParticleConfigList>(v);
}

void TheArbiter2D::moveParticleConfigCursorDown() {
    int v = static_cast<int>(m_activeParticleConfigList);
    v = (v + 1) % PARTICLE_LIST_COUNT;
    m_activeParticleConfigList = static_cast<ParticleConfigList>(v);
}

void TheArbiter2D::goBackOneLayer(ArbiterResult& result) {
    if (m_appLayer == LAYER_MENU) {
        result.requestRedraw = true;
        return;
    }

    if (m_appLayer == LAYER_ENVIRONMENT_CONFIGURATION) {
        m_appLayer = LAYER_MENU;
        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }

    if (m_appLayer == LAYER_2D_GRID_MODE_CONFIGURATION) {
        m_appLayer = LAYER_ENVIRONMENT_CONFIGURATION;
        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }

    if (m_appLayer == LAYER_SIMULATION_RUN) {
        m_appLayer = LAYER_2D_GRID_MODE_CONFIGURATION;
        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }
}

void TheArbiter2D::enterCurrentSelection(ArbiterResult& result) {
    if (m_appLayer == LAYER_MENU) {
        if (m_envSelection == ENV_2D_GRID) {
            m_appLayer = LAYER_ENVIRONMENT_CONFIGURATION;
            m_activeEnvironmentConfigList = ENV_CONFIG_LIST_GRID_SIZE;
        }
        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }

    if (m_appLayer == LAYER_ENVIRONMENT_CONFIGURATION) {
        if (m_activeEnvironmentConfigList == ENV_CONFIG_LIST_CONFIRM &&
            m_gridSelection == GRID_PARTICLES) {

            m_appLayer = LAYER_2D_GRID_MODE_CONFIGURATION;
            m_activeParticleConfigList = PARTICLE_LIST_GENERATE;
        }

        // GRAPH is intentionally reserved for a later pass.
        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }

    if (m_appLayer == LAYER_2D_GRID_MODE_CONFIGURATION) {
        if (m_activeParticleConfigList == PARTICLE_LIST_RUN) {
            if (validateParticleCountBudget()) {
                m_appLayer = LAYER_SIMULATION_RUN;
                result.command = CMD_START_PARTICLE_SIMULATION;
            }
            else {
                result.command = CMD_REDRAW;
            }

            result.requestRedraw = true;
            return;
        }

        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }

    if (m_appLayer == LAYER_SIMULATION_RUN) {
        result.command = CMD_REDRAW;
        result.requestRedraw = true;
        return;
    }
}

void TheArbiter2D::resetToMenu() {
    m_appLayer = LAYER_MENU;
    m_envSelection = ENV_IDLE;
    m_gridSelection = GRID_PARTICLES;
    m_activeEnvironmentConfigList = ENV_CONFIG_LIST_GRID_SIZE;

    m_gridCellOptionIndex = 0; // 64 cells
    m_worldBoxOptionIndex = 0; // box scale 4

    m_particleColorSelection = PARTICLE_COLOR_RED;
    m_particleResetMode = PARTICLE_RESET_DEFAULT;
    m_activeParticleConfigList = PARTICLE_LIST_GENERATE;

    resetParticleCounts();
    m_particleConfigMessage.clear();
}

TheArbiter2D::ArbiterResult
TheArbiter2D::processKeyboard(const KeyboardInput2D::KeyEvent& event) {
    ArbiterResult result;

    if (event.signal == KeyboardInput2D::KEY_ESCAPE) {
        result.command = CMD_EXIT;
        return result;
    }

    if (m_particleCountEntryActive) {
        handleParticleCountEntry(event, result);
        return result;
    }

    if (event.signal == KeyboardInput2D::KEY_Q) {
        goBackOneLayer(result);
        return result;
    }

    switch (m_appLayer) {
    case LAYER_MENU:
        switch (event.signal) {
            case KeyboardInput2D::KEY_A:
            case KeyboardInput2D::KEY_D:
                toggleEnvironmentSelection();
                result.command = CMD_REDRAW;
                result.requestRedraw = true;
                break;

            case KeyboardInput2D::KEY_E:
                enterCurrentSelection(result);
                break;

            default:
                break;
        }
        break;

    case LAYER_ENVIRONMENT_CONFIGURATION:
        switch (event.signal) {
        case KeyboardInput2D::KEY_W:
            moveEnvironmentConfigCursorUp();
            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_S:
            moveEnvironmentConfigCursorDown();
            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_A:
            if (m_activeEnvironmentConfigList == ENV_CONFIG_LIST_GRID_SIZE) {
                adjustGridCellCountSelection(-1);
            }
            else if (m_activeEnvironmentConfigList == ENV_CONFIG_SIM_WORLD_SCALE) {
                adjustWorldBoxScaleSelection(-1);
            }
            else if (m_activeEnvironmentConfigList == ENV_CONFIG_LIST_GRID_MODE) {
                toggleGridSelection();
            }
            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_D:
            if (m_activeEnvironmentConfigList == ENV_CONFIG_LIST_GRID_SIZE) {
                adjustGridCellCountSelection(+1);
            }
            else if (m_activeEnvironmentConfigList == ENV_CONFIG_SIM_WORLD_SCALE) {
                adjustWorldBoxScaleSelection(+1);
            }
            else if (m_activeEnvironmentConfigList == ENV_CONFIG_LIST_GRID_MODE) {
                toggleGridSelection();
            }
            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_E:
            enterCurrentSelection(result);
            break;

        default:
            break;
        }
        break;

    case LAYER_2D_GRID_MODE_CONFIGURATION:
        switch (event.signal) {
        case KeyboardInput2D::KEY_W:
            moveParticleConfigCursorUp();
            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_S:
            moveParticleConfigCursorDown();
            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_A:
        case KeyboardInput2D::KEY_D:
            if (m_activeParticleConfigList == PARTICLE_LIST_GENERATE) {
                toggleParticleColorSelection();
            }
            else if (m_activeParticleConfigList == PARTICLE_LIST_RESET) {
                toggleParticleResetMode();
            }

            result.command = CMD_REDRAW;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_ENTER:
            if (m_activeParticleConfigList == PARTICLE_LIST_GENERATE) {
                beginParticleCountEntry();
                result.command = CMD_REDRAW;
                result.requestRedraw = true;
            }
            break;

        case KeyboardInput2D::KEY_E:
            enterCurrentSelection(result);
            break;

        default:
            break;
        }
        break;

    case LAYER_SIMULATION_RUN:
        switch (event.signal) {
        case KeyboardInput2D::KEY_SPACE:
            result.command = CMD_TOGGLE_PAUSE;
            result.requestRedraw = true;
            break;

        case KeyboardInput2D::KEY_ENTER:
            result.command = CMD_STEP_SIMULATION;
            result.requestRedraw = true;
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return result;
}

const char* TheArbiter2D::getLayerName() const {
    switch (m_appLayer) {
    case LAYER_MENU:
        return "LAYER_0_MENU";

    case LAYER_ENVIRONMENT_CONFIGURATION:
        return "LAYER_1_ENVIRONMENT_CONFIGURATION";

    case LAYER_2D_GRID_MODE_CONFIGURATION:
        return "LAYER_2_PARTICLE_CONFIGURATION";

    case LAYER_SIMULATION_RUN:
        return "LAYER_3_SIMULATION_RUN";

    default:
        return "UNKNOWN_LAYER";
    }
}

const char* TheArbiter2D::getEnvironmentName() const {
    switch (m_envSelection) {
    case ENV_IDLE:
        return "IDLE";

    case ENV_2D_GRID:
        return "2D_GRID";

    default:
        return "UNKNOWN_ENVIRONMENT";
    }
}

const char* TheArbiter2D::getGridSelectionName() const {
    switch (m_gridSelection) {
    case GRID_GRAPH:
        return "GRAPH";

    case GRID_PARTICLES:
        return "PARTICLES";

    default:
        return "UNKNOWN_GRID_SELECTION";
    }
}

const char* TheArbiter2D::getActiveEnvironmentConfigListName() const {
    switch (m_activeEnvironmentConfigList) {
    case ENV_CONFIG_LIST_GRID_SIZE:
        return "2D GRID SIZE";

    case ENV_CONFIG_SIM_WORLD_SCALE:
        return "2D SIM WORLD BOX SCALE";

    case ENV_CONFIG_LIST_GRID_MODE:
        return "2D GRID SELECTION";

    case ENV_CONFIG_LIST_CONFIRM:
        return "CONFIRM SELECTION";

    default:
        return "UNKNOWN_ENV_CONFIG_LIST";
    }
}

const char* TheArbiter2D::getParticleColorName() const {
    switch (m_particleColorSelection) {
    case PARTICLE_COLOR_RED:
        return "RED";

    case PARTICLE_COLOR_BLUE:
        return "BLUE";

    case PARTICLE_COLOR_GREEN:
        return "GREEN";

    case PARTICLE_COLOR_YELLOW:
        return "YELLOW";

    default:
        return "UNKNOWN_COLOR";
    }
}

const char* TheArbiter2D::getParticleResetModeName() const {
    switch (m_particleResetMode) {
    case PARTICLE_RESET_DEFAULT:
        return "DEFAULT";

    case PARTICLE_RESET_RANDOM:
        return "RANDOM";

    default:
        return "UNKNOWN_RESET_MODE";
    }
}

const char* TheArbiter2D::getActiveParticleConfigListName() const {
    switch (m_activeParticleConfigList) {
    case PARTICLE_LIST_GENERATE:
        return "GENERATE PARTICLE";

    case PARTICLE_LIST_RESET:
        return "PARTICLE RESET MODE";

    case PARTICLE_LIST_RUN:
        return "RUN PARTICLES";

    default:
        return "UNKNOWN_PARTICLE_LIST";
    }
}