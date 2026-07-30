#include "EuclidEngine2D.h"
#include "EucliGen2D_kernel.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

using namespace std;
using namespace glm;

EuclidEngine2D* EuclidEngine2D::s_instance = nullptr;

EuclidEngine2D::EuclidEngine2D() {}

EuclidEngine2D::~EuclidEngine2D() {
    shutdown();
}

bool EuclidEngine2D::init(int argc, char** argv) {
    printf("EucliGen2D Starting...\n\n");
    printf("Welcome to the 2D Grid Tesseract!\n");

    s_instance = this;

    m_numParticles = kNumParticles;
    m_arbiter.setMaxGeneratedParticles(kNumParticles);
    m_gridSizeDim = make_uint2(kGridSize, kGridSize);

    printf("2D grid: %u x %u = %u cells\n",
        m_gridSizeDim.x,
        m_gridSizeDim.y,
        m_gridSizeDim.x * m_gridSizeDim.y);

    printf("particles: %u\n", m_numParticles);

    initGL(&argc, argv);

    // Must be called after GL context creation.
    cudaGLInit(argc, argv);

    setRadii(m_numParticles);
    initParticleSystem(m_numParticles, m_gridSizeDim);

    resetParticleLifeParams();
    initParticleLifeSliders();

    glutDisplayFunc(&EuclidEngine2D::sDisplay);
    glutReshapeFunc(&EuclidEngine2D::sReshape);
    glutMouseFunc(&EuclidEngine2D::sMouse);
    glutMotionFunc(&EuclidEngine2D::sMotion);
    glutPassiveMotionFunc(&EuclidEngine2D::sPassiveMotion);
    glutKeyboardFunc(&EuclidEngine2D::sKeyboard);
    glutSpecialFunc(&EuclidEngine2D::sSpecial);
    glutIdleFunc(&EuclidEngine2D::sIdle);
    glutCloseFunc(&EuclidEngine2D::sClose);

    return true;
}

void EuclidEngine2D::run() {
    glutMainLoop();
}

void EuclidEngine2D::shutdown() {
    if (m_cleaned) return;
    m_cleaned = true;

    destroyParticleLifeSliders();

    if (m_timer) {
        sdkDeleteTimer(&m_timer);
        m_timer = nullptr;
    }

    if (m_renderer) {
        delete m_renderer;
        m_renderer = nullptr;
    }

    if (m_psystem) {
        delete m_psystem;
        m_psystem = nullptr;
    }

    m_rad.clear();
    m_rad.shrink_to_fit();
}

// -----------------------------------------------------------------------------
// GLUT THUNKS
// -----------------------------------------------------------------------------
void EuclidEngine2D::sDisplay() {
    if (s_instance) s_instance->onDisplay();
}

void EuclidEngine2D::sReshape(int w, int h) {
    if (s_instance) s_instance->onReshape(w, h);
}

void EuclidEngine2D::sMouse(int button, int state, int x, int y) {
    if (s_instance) s_instance->onMouse(button, state, x, y);
}

void EuclidEngine2D::sMotion(int x, int y) {
    if (s_instance) s_instance->onMotion(x, y);
}

void EuclidEngine2D::sPassiveMotion(int x, int y) {
    if (s_instance) s_instance->onPassiveMotion(x, y);
}

void EuclidEngine2D::sMainMenu(int value) {
    if (!s_instance) return;
    if (value == MENU_NOP) return;

    s_instance->onKeyboard(static_cast<unsigned char>(value), 0, 0);
}

void EuclidEngine2D::sKeyboard(unsigned char key, int x, int y) {
    if (s_instance) s_instance->onKeyboard(key, x, y);
}

void EuclidEngine2D::sSpecial(int key, int x, int y) {
    if (s_instance) s_instance->onSpecial(key, x, y);
}

void EuclidEngine2D::sIdle() {
    if (s_instance) s_instance->onIdle();
}

void EuclidEngine2D::sClose() {
    if (s_instance) s_instance->onClose();
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void EuclidEngine2D::initGL(int* argc, char** argv) {
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(kWidth, kHeight);
    glutCreateWindow("EucliGen2D Ver0.0.2");

    // Let glutMainLoop return after window close, where supported.
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    initMenus();

    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        printf("GLEW init failed: %s\n", glewGetErrorString(glewErr));
    }

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.03f, 0.07f, 1.0f);

    m_viewport.resize(static_cast<int>(kWidth), static_cast<int>(kHeight));
    m_viewport.applyOrtho2D();

    m_camera.resize(static_cast<int>(kWidth), static_cast<int>(kHeight));
    m_camera.reset(vec2(0.0f, 0.0f), kDefaultPixelsPerWorldUnit);

    sdkCreateTimer(&m_timer);
}

void EuclidEngine2D::initParticleSystem(unsigned int numParticles, uint2 gridSize) {
    m_psystem = new ParticleSystem2D(numParticles, gridSize);

    const float simBox = m_arbiter.getSimBoxSize();
    const float halfBox = simBox * 0.5f;
    const unsigned int cellsPerAxis = m_arbiter.getGridCellsPerAxisSelection();

    // Force the ParticleSystem2D grid/cell size to match the visible square box.
    m_psystem->setSimBoundary(halfBox);
    m_psystem->setParticleClassCounts(0, 0, 0, 0);
    m_psystem->reset(ParticleSystem2D::CNFG_DEFAULT_RESTART);

    m_renderer = new EuclidRenderer2D();

    const ViewPort2D::Rect r = m_viewport.getGridViewport(true);
    m_renderer->initialize(r.w, r.h);

    m_renderer->setVisibleGridCellsPerAxis(static_cast<int>(cellsPerAxis));
    m_renderer->setParticleRadius(m_psystem->getParticleRadius());
    m_renderer->setGridStyle(1, false);
    m_renderer->setBoundary(-halfBox, halfBox);

    syncRenderingWithParticleSystem();

    m_lastAppliedSimBox = simBox;
    m_lastAppliedCellsPerAxis = cellsPerAxis;
}

void EuclidEngine2D::initMenus() {
    rebuildMenus();
}

void EuclidEngine2D::initParticleLifeSliders() {
    destroyParticleLifeSliders();

    // Create a new parameter list for Particle Life force matrix.
    m_particleLifeSliders = new ParamListGL("Particle Life Forces");

    // RED source row
    m_particleLifeSliders->AddParam(
        new Param<float>(
            "RED x RED",
            m_REDxRED,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_REDxRED)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "RED x BLUE",
            m_REDxBLUE,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_REDxBLUE)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "RED x GREEN",
            m_REDxGREEN,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_REDxGREEN)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "RED x YELLOW",
            m_REDxYELLOW,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_REDxYELLOW)
    );

    // BLUE source row
    m_particleLifeSliders->AddParam(
        new Param<float>(
            "BLUE x RED",
            m_BLUExRED,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_BLUExRED)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "BLUE x BLUE",
            m_BLUExBLUE,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_BLUExBLUE)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "BLUE x GREEN",
            m_BLUExGREEN,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_BLUExGREEN)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "BLUE x YELLOW",
            m_BLUExYELLOW,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_BLUExYELLOW)
    );

    // GREEN source row
    m_particleLifeSliders->AddParam(
        new Param<float>(
            "GREEN x RED",
            m_GREENxRED,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_GREENxRED)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "GREEN x BLUE",
            m_GREENxBLUE,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_GREENxBLUE)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "GREEN x GREEN",
            m_GREENxGREEN,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_GREENxGREEN)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "GREEN x YELLOW",
            m_GREENxYELLOW,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_GREENxYELLOW)
    );

    // YELLOW source row
    m_particleLifeSliders->AddParam(
        new Param<float>(
            "YELLOW x RED",
            m_YELLOWxRED,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_YELLOWxRED)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "YELLOW x BLUE",
            m_YELLOWxBLUE,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_YELLOWxBLUE)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "YELLOW x GREEN",
            m_YELLOWxGREEN,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_YELLOWxGREEN)
    );

    m_particleLifeSliders->AddParam(
        new Param<float>(
            "YELLOW x YELLOW",
            m_YELLOWxYELLOW,
            kParticleLifeForceMin,
            kParticleLifeForceMax,
            kParticleLifeForceStep,
            &m_YELLOWxYELLOW)
    );
}

void EuclidEngine2D::rebuildMenus() {
    glutDetachMenu(GLUT_MIDDLE_BUTTON);

    if (m_menuId != 0) {
        glutDestroyMenu(m_menuId);
        m_menuId = 0;
    }

    m_menuId = glutCreateMenu(&EuclidEngine2D::sMainMenu);

    glutAddMenuEntry("========================================", MENU_NOP);
    glutAddMenuEntry("-  EucliGen2D CUDA GRID -", MENU_NOP);
    glutAddMenuEntry("========================================", MENU_NOP);
    glutAddMenuEntry("Quit (esc)", 27);
    glutAddMenuEntry("========================================", MENU_NOP);

    glutAttachMenu(GLUT_MIDDLE_BUTTON);
}

void EuclidEngine2D::destroyParticleLifeSliders() {
    if (m_particleLifeSliders) {
        delete m_particleLifeSliders;
        m_particleLifeSliders = nullptr;
    }

    m_displaySliders = false;
}

// -----------------------------------------------------------------------------
// ENGINE SERVICES
// -----------------------------------------------------------------------------
void EuclidEngine2D::setRadii(unsigned int numParticles) {
    m_rad.assign(numParticles, 0.0f);
}

void EuclidEngine2D::syncRenderingWithParticleSystem() {
    if (!m_psystem || !m_renderer) return;

    m_renderer->setParticleDeviceBuffers(
        m_psystem->getDevicePositionBuffer(),
        m_psystem->getDeviceVelocityBuffer(),
        m_psystem->getNumParticles()
    );

    m_renderer->setParticleColorBuffer(
        m_psystem->getDeviceColorBuffer()
    );

    m_renderer->setVisibleGridCellsPerAxis(
        static_cast<int>(m_arbiter.getGridCellsPerAxisSelection())
    );

    m_renderer->setParticleRadius(
        m_psystem->getParticleRadius()
    );

    m_renderer->setGrid(
        ivec2(
            static_cast<int>(m_psystem->getGridSize().x),
            static_cast<int>(m_psystem->getGridSize().y)
        ),
        vec2(
            m_psystem->getWorldOrigin().x,
            m_psystem->getWorldOrigin().y
        ),
        vec2(
            m_psystem->getCellSize().x,
            m_psystem->getCellSize().y
        )
    );

    m_renderer->setBoundary(-m_psystem->getBoundary(), m_psystem->getBoundary());
}

void EuclidEngine2D::syncParticleLifeParamsToSystem() {
    if (!m_psystem) return;

    m_psystem->setRedParticleLifeForces(m_REDxRED, m_REDxBLUE, m_REDxGREEN, m_REDxYELLOW);
    m_psystem->setBlueParticleLifeForces(m_BLUExRED, m_BLUExBLUE, m_BLUExGREEN, m_BLUExYELLOW);
    m_psystem->setGreenParticleLifeForces(m_GREENxRED, m_GREENxBLUE, m_GREENxGREEN, m_GREENxYELLOW);
    m_psystem->setYellowParticleLifeForces(m_YELLOWxRED, m_YELLOWxBLUE, m_YELLOWxGREEN, m_YELLOWxYELLOW);
    
}

float EuclidEngine2D::computeGridFitPixelsPerWorldUnit(float simBox) const {
    simBox = std::max(1.0f, simBox);

    // Preserve the current 4.0f default view, then scale down as the selected
    // box grows so the outer boundary remains easy to see.
    return std::max(10.0f, kDefaultPixelsPerWorldUnit * (kDefaultSimBox / simBox));

}

void  EuclidEngine2D::applyGridSizeSelectionToSystem(bool frameCameraOnChange) {
    if (!m_psystem || !m_renderer) return;

    const float simBox = m_arbiter.getSimBoxSize();
    const unsigned int cellsPerAxis = m_arbiter.getGridCellsPerAxisSelection();

    const bool simBoxChanged = fabs(simBox - m_lastAppliedSimBox) >= 0.0001f;

    const bool gridResolutionChanged =
        cellsPerAxis != m_lastAppliedCellsPerAxis;

    if (!simBoxChanged && !gridResolutionChanged) {
        return;
    }

    m_lastAppliedSimBox = simBox;
    m_lastAppliedCellsPerAxis = cellsPerAxis;

    const float halfBox = simBox * 0.5f;

    m_psystem->setSimBoundary(halfBox);

    m_renderer->setVisibleGridCellsPerAxis(static_cast<int>(cellsPerAxis));
    m_renderer->setBoundary(-halfBox, halfBox);

    syncRenderingWithParticleSystem();

    if (frameCameraOnChange && simBoxChanged) {
        m_camera.reset(
            vec2(0.0f, 0.0f),
            computeGridFitPixelsPerWorldUnit(simBox)
        );
        syncRendererFromCamera(false);
    }
}

void EuclidEngine2D::resetParticlesFromArbiterConfig() {
    if (!m_psystem) return;

    ParticleSystem2D::ParticleConfig config = ParticleSystem2D::CNFG_DEFAULT_RESTART;

    if (m_arbiter.getParticleResetMode() == TheArbiter2D::PARTICLE_RESET_RANDOM) {
        config = ParticleSystem2D::CNFG_RANDOM_RESTART;
    }

    m_psystem->setParticleClassCounts(
        m_arbiter.getRedParticleCount(),
        m_arbiter.getBlueParticleCount(),
        m_arbiter.getGreenParticleCount(),
        m_arbiter.getYellowParticleCount()
    );

    m_psystem->reset(config);

    syncRenderingWithParticleSystem();
}

void EuclidEngine2D::toggleParticleLifeSliders() {
    if (!m_arbiter.isSimulationRunLayer()) {
        m_displaySliders = false;
        return;
    }

    if (!m_particleLifeSliders) {
        initParticleLifeSliders();
    }

    m_displaySliders = !m_displaySliders;
}

void EuclidEngine2D::drawParticleLifeSliders() {
    if (!m_arbiter.isSimulationRunLayer()) return;
    if (!m_displaySliders) return;
    if (!m_particleLifeSliders) return;

    const int w = glutGet(GLUT_WINDOW_WIDTH);
    const int h = glutGet(GLUT_WINDOW_HEIGHT);

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(
        0.0,
        static_cast<double>(w),
        static_cast<double>(h),
        0.0
    );

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glUseProgram(0);

    // Move it away from the top-left simulation status box.
    m_particleLifeSliders->Render(32, 150);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}

void EuclidEngine2D::resetParticleLifeParams() {
    m_REDxRED = -2.0f;
    m_REDxBLUE = 4.0f;
    m_REDxGREEN = 1.0f;
    m_REDxYELLOW = -1.0f;

    m_BLUExRED = 1.0f;
    m_BLUExBLUE = -2.0f;
    m_BLUExGREEN = 4.0f;
    m_BLUExYELLOW = 1.0f;

    m_GREENxRED = 1.0f;
    m_GREENxBLUE = 1.0f;
    m_GREENxGREEN = -2.0f;
    m_GREENxYELLOW = 4.0f;

    m_YELLOWxRED = 4.0f;
    m_YELLOWxBLUE = -1.0f;
    m_YELLOWxGREEN = 1.0f;
    m_YELLOWxYELLOW = -2.0f;

    syncParticleLifeParamsToSystem();
}

void EuclidEngine2D::updateSimulation() {
    if (!m_psystem) return;

    // CUDA particle sim only runs in Layer 3.
    if (!m_arbiter.isSimulationRunLayer()) return;
    if (m_bPause) return;

    applyCommonSimulationParams();

    m_psystem->update(m_timestep);
    m_simTime += m_timestep;

    syncRenderingWithParticleSystem();
}

void EuclidEngine2D::applyCommonSimulationParams() {
    if (!m_psystem) return;

    const float boxBoundary = m_arbiter.getSimBoxSize() * 0.5f;

    m_psystem->setIterations(m_iterations);
    m_psystem->setDamping(m_damping);
    m_psystem->setGravity(-m_gravity);
    m_psystem->setCollideSpring(m_collideSpring);
    m_psystem->setCollideDamping(m_collideDamping);
    m_psystem->setCollideShear(m_collideShear);
    m_psystem->setCollideAttraction(m_collideAttraction);
    m_psystem->setSimBoundary(boxBoundary);

    syncParticleLifeParamsToSystem();
}

void EuclidEngine2D::configureRendererForCurrentViewport() {
    if (!m_renderer) return;

    const bool menuLayer = m_arbiter.isMenuLayer();
    const ViewPort2D::Rect r = m_viewport.getGridViewport(menuLayer);

    m_renderer->setWindowSize(r.w, r.h);
    m_camera.resize(r.w, r.h);
}

void EuclidEngine2D::syncRendererFromCamera(bool useLaggedCamera) {
    if (!m_renderer) return;

    if (useLaggedCamera) {
        m_camera.applyLaggedToRenderer(m_renderer);
    }
    else {
        m_camera.applyToRenderer(m_renderer);
    }
}

void EuclidEngine2D::syncCameraFromRenderer() {
    if (!m_renderer) return;

    m_camera.setCenterWorld(m_renderer->getCameraCenter());
    m_camera.setPixelsPerWorldUnit(m_renderer->getPixelsPerWorldUnit());
}

void EuclidEngine2D::draw2DGridScene() {
    if (!m_renderer) return;

    const bool menuLayer = m_arbiter.isMenuLayer();

    configureRendererForCurrentViewport();

    m_camera.updateLag();
    syncRendererFromCamera(true);

    const bool showParticles = m_arbiter.isSimulationRunLayer();
    m_renderer->setShowParticles(showParticles);

    m_viewport.applyGridViewport(menuLayer);

    if (m_displayEnabled) {
        m_renderer->display(m_displayMode);
    }

    // Restore full-window viewport so overlay draws in screen coordinates.
    m_viewport.applyFullViewport();
}

void EuclidEngine2D::computeFPS() {
    if (!m_timer) return;

    m_fpsCount++;

    if (m_fpsCount >= m_fpsLimit) {
        float avgMs = sdkGetAverageTimerValue(&m_timer);
        float ifps = (avgMs > 0.0f) ? (1000.0f / avgMs) : 0.0f;

        char title[256];
        sprintf(
            title,
            "EucliGen2D CUDA Grid: %u / %u particles | %3.1f fps | %s | %s",
            m_psystem ? static_cast<unsigned int>(m_psystem->getNumParticles()) : 0u,
            m_numParticles,
            ifps,
            m_arbiter.getLayerName(),
            m_arbiter.getEnvironmentName()
        );

        glutSetWindowTitle(title);

        m_fpsCount = 0;
        m_fpsLimit = static_cast<int>(std::max(ifps, 1.0f));
        sdkResetTimer(&m_timer);
    }
}

// -----------------------------------------------------------------------------
// DISPLAY
// -----------------------------------------------------------------------------
void EuclidEngine2D::onDisplay() {
    if (m_exiting || m_cleaned) return;

    if (m_timer) sdkStartTimer(&m_timer);

    // Push Layer 1 GRID_SIZE selection into the particle system + renderer.
    // This lets A/D resize the visible grid in real time.
    applyGridSizeSelectionToSystem(true);

    updateSimulation();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    const float timeS = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

    m_tesseract.updatePreviewAnimation(timeS);

    if (m_tesseract.consumeCameraFocusRequest()) {
        const float simBox = m_arbiter.getSimBoxSize();
        m_camera.reset(
            vec2(0.0f, 0.0f),
            computeGridFitPixelsPerWorldUnit(simBox)
        );
        
    }

    draw2DGridScene();

    // Draw the full layered UI, not the old two-state overlay.
    m_viewport.drawOverlay(m_arbiter, m_bPause);

    drawParticleLifeSliders();
    if (m_timer) {
        sdkStopTimer(&m_timer);
        computeFPS();
    }

    glutSwapBuffers();
}

void EuclidEngine2D::onReshape(int w, int h) {
    m_viewport.resize(w, h);

    configureRendererForCurrentViewport();

    if (m_renderer) {
        syncRendererFromCamera(false);
    }

    glutPostRedisplay();
}

// -----------------------------------------------------------------------------
// INPUT
// -----------------------------------------------------------------------------
bool EuclidEngine2D::getMouseGridLocal(
    int sx,
    int sy,
    bool allowOutside,
    int& gx,
    int& gy) const {

    const bool menuLayer = m_arbiter.isMenuLayer();
    const ViewPort2D::Rect r = m_viewport.getGridViewport(menuLayer);

    gx = sx - r.x;

    // GLUT mouse coordinates use top-left origin.
    // OpenGL viewport rectangle uses bottom-left origin.
    const int topY = m_viewport.getHeight() - (r.y + r.h);
    gy = sy - topY;

    if (!allowOutside) {
        if (gx < 0 || gy < 0 || gx >= r.w || gy >= r.h) {
            return false;
        }
    }

    return true;
}

void EuclidEngine2D::onMouse(int button, int state, int x, int y) {
    if (m_arbiter.isSimulationRunLayer() &&
        m_displaySliders &&
        m_particleLifeSliders) {

        m_particleLifeSliders->Mouse(button, state, x, y);
        syncParticleLifeParamsToSystem();
        glutPostRedisplay();
        return;
    }

    if (!m_renderer) return;

    configureRendererForCurrentViewport();
    syncRendererFromCamera(false);

    int gx = 0;
    int gy = 0;

    if (!getMouseGridLocal(x, y, false, gx, gy)) {
        return;
    }

    if (m_mouse.onButton(button, state, gx, gy, m_renderer)) {
        syncCameraFromRenderer();
        glutPostRedisplay();
    }
}

void EuclidEngine2D::onMotion(int x, int y) {
    if (m_arbiter.isSimulationRunLayer() &&
        m_displaySliders &&
        m_particleLifeSliders) {

        m_particleLifeSliders->Motion(x, y);
        syncParticleLifeParamsToSystem();
        glutPostRedisplay();
        return;
    }

    if (!m_renderer) return;

    configureRendererForCurrentViewport();
    syncRendererFromCamera(false);

    int gx = 0;
    int gy = 0;

    const bool allowOutside = m_mouse.isDragging();
    if (!getMouseGridLocal(x, y, allowOutside, gx, gy)) {
        return;
    }

    if (m_mouse.onMotion(gx, gy, m_renderer)) {
        syncCameraFromRenderer();
        glutPostRedisplay();
    }
}

void EuclidEngine2D::onPassiveMotion(int x, int y) {
    int gx = 0;
    int gy = 0;

    if (getMouseGridLocal(x, y, false, gx, gy)) {
        if (m_mouse.onPassiveMotion(gx, gy)) {
            glutPostRedisplay();
        }
    }
}

void EuclidEngine2D::onKeyboard(unsigned char key, int x, int y) {

    if (key == '\t' && m_arbiter.isSimulationRunLayer()) {
        toggleParticleLifeSliders();
        glutPostRedisplay();
        return;
    }

    const TheArbiter2D::AppLayer oldLayer = m_arbiter.getAppLayer();

    KeyboardInput2D::KeyEvent event = m_keyboard.onKey(key, x, y);
    TheArbiter2D::ArbiterResult result = m_arbiter.processKeyboard(event);

    const TheArbiter2D::AppLayer newLayer = m_arbiter.getAppLayer();

    const bool entered2DView =
        oldLayer == TheArbiter2D::LAYER_MENU &&
        newLayer != TheArbiter2D::LAYER_MENU;

    const bool returnedToMenu =
        oldLayer != TheArbiter2D::LAYER_MENU &&
        newLayer == TheArbiter2D::LAYER_MENU;

    const float timeS = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

    if (entered2DView) {
        m_tesseract.beginEnter2DTransition(timeS);
    }

    if (returnedToMenu) {
        m_tesseract.beginReturnToMenu(timeS);
    }

    if (newLayer != TheArbiter2D::LAYER_SIMULATION_RUN) {
        m_displaySliders = false;
    }

    switch (result.command) {
    case TheArbiter2D::CMD_EXIT:
        glutDestroyWindow(glutGetWindow());
        return;

    case TheArbiter2D::CMD_TOGGLE_PAUSE:
        m_bPause = !m_bPause;
        break;

    case TheArbiter2D::CMD_STEP_SIMULATION:
        if (m_psystem && m_arbiter.isSimulationRunLayer()) {
            applyCommonSimulationParams();

            m_psystem->update(m_timestep);
            m_simTime += m_timestep;

            syncRenderingWithParticleSystem();
        }
        break;

    case TheArbiter2D::CMD_START_PARTICLE_SIMULATION:
        applyGridSizeSelectionToSystem(true);
        resetParticlesFromArbiterConfig();
        m_bPause = false;
        break;

    case TheArbiter2D::CMD_REDRAW:
    case TheArbiter2D::CMD_NONE:
    default:
        break;
    }

    if (result.requestRedraw) {
        glutPostRedisplay();
    }
}

void EuclidEngine2D::onSpecial(int key, int x, int y) {
    if (m_arbiter.isSimulationRunLayer() &&
        m_displaySliders &&
        m_particleLifeSliders) {

        m_particleLifeSliders->Special(key, x, y);
        syncParticleLifeParamsToSystem();
        glutPostRedisplay();
    }
}

// -----------------------------------------------------------------------------
// LIFETIME / IDLE
// -----------------------------------------------------------------------------
void EuclidEngine2D::requestExit() {
    if (m_exiting) return;

    m_exiting = true;
    glutIdleFunc(nullptr);
    s_instance = nullptr;
}

void EuclidEngine2D::onIdle() {
    if (m_exiting || m_cleaned) return;

    glutPostRedisplay();
}

void EuclidEngine2D::onClose() {
    requestExit();
    glutLeaveMainLoop();
}