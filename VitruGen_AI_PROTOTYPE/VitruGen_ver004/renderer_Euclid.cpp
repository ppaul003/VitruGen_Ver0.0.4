#include <GL/glew.h>
#include <GL/freeglut.h>

#include <stdio.h>
#include <vector>
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <functional>

#include "renderer_Euclid.h"
#include "render_utils.h"

using namespace std;
using namespace glm;

static void parseOBJFaceToken(
    const string& token,
    int& vertexIndex,
    int& normalIndex) {

    vertexIndex = 0;
    normalIndex = 0;

    // Supports:
    // f v
    // f v//n
    // f v/t/n
    // f v/t
    const size_t slash1 = token.find('/');

    if (slash1 == string::npos) {
        vertexIndex = atoi(token.c_str());
        return;
    }

    vertexIndex = atoi(token.substr(0, slash1).c_str());

    const size_t slash2 = token.find('/', slash1 + 1);

    if (slash2 == string::npos) {
        return;
    }

    if (slash2 + 1 < token.size()) {
        normalIndex = atoi(token.substr(slash2 + 1).c_str());
    }
}

static float wrapGuideAngleDeg(float angleDeg) {
    float wrapped = fmodf(angleDeg, 360.0f);

    if (wrapped > 180.0f) wrapped -= 360.0f;
    if (wrapped < -180.0f) wrapped += 360.0f;

    return wrapped;
}

static int getOffsetGridMajorEvery(float normalizedIncrement) {
    if (normalizedIncrement <= 0.0f) return 1;

    const float epsilon = 0.000001f;

    const bool baseTwelveFamily =
        fabsf(normalizedIncrement - 0.012f) < epsilon ||
        fabsf(normalizedIncrement - 0.12f) < epsilon;

    float normalizedMajorStep = 1.0f;

    if (baseTwelveFamily) {

        // 0.012 -> major every 0.12
        // 0.12  -> major every 1.20
        normalizedMajorStep = normalizedIncrement * 10.0f;
    }
    else if (normalizedIncrement < 0.1f) {

        // Decimal-hundredth family:
        //
        // 0.01, 0.02, 0.025, 0.05
        //
        // major lines occur every 0.10.
        normalizedMajorStep = 0.1f;
    }
    else {

        // Decimal-tenth family:
        //
        // 0.1, 0.2, 0.25, 0.5
        //
        // major lines occur every 1.0.
        normalizedMajorStep = 1.0f;
    }

    const int majorEvery =
        static_cast<int>(lroundf(normalizedMajorStep / 
            normalizedIncrement));

    return std::max(1, majorEvery);
}

static vec3 rotationArcPoint(
    const EuclidRenderer::VolumeObjectBasis& bakedBasis,
    EuclidRenderer::VolumeAxisGuideMode guideMode,
    float angleRad,
    float radius) {

    const float c = cosf(angleRad);
    const float s = sinf(angleRad);

    switch (guideMode) {
    case EuclidRenderer::VOLUME_AXIS_GUIDE_PITCH:
        // YZ plane, beginning on +Y.
        return radius * (
            c * bakedBasis.yAxis +
            s * bakedBasis.zAxis
            );

    case EuclidRenderer::VOLUME_AXIS_GUIDE_YAW:
        // XZ plane, beginning on +X.
        return radius * (
		c * bakedBasis.xAxis -
		s * bakedBasis.zAxis
	);

    default:
    case EuclidRenderer::VOLUME_AXIS_GUIDE_ROLL:
        // XY plane, beginning on +X.
        return radius * (
            c * bakedBasis.xAxis +
            s * bakedBasis.yAxis
            );
    }
}

static void drawRotationDeltaGuide(
    const EuclidRenderer::VolumeObjectBasis& bakedBasis,
    EuclidRenderer::VolumeAxisGuideMode guideMode,
    float activeAngleDeg,
    float axisLen) {

    const float visualAngleDeg = wrapGuideAngleDeg(activeAngleDeg);
    const float visualAngleRad = radians(visualAngleDeg);
    const float radius = axisLen * 0.62f;

    const int segments = std::max(
        8,
        std::min(96, static_cast<int>(fabsf(visualAngleDeg) / 4.0f) + 8)
    );

    // Angular sweep on the selected rotation plane.
    glLineWidth(2.0f);
    glColor4f(1.0f, 0.78f, 0.12f, 0.95f);
    glBegin(GL_LINE_STRIP);

    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const vec3 p = rotationArcPoint(
            bakedBasis,
            guideMode,
            visualAngleRad * t,
            radius
        );

        glVertex3f(p.x, p.y, p.z);
    }

    glEnd();

    // Delta vector/chord from the original +vector to its rotated endpoint.
    const vec3 start = rotationArcPoint(bakedBasis, guideMode, 0.0f, radius);
    const vec3 end = rotationArcPoint(bakedBasis, guideMode, visualAngleRad, radius);

    glLineWidth(1.8f);
    glColor4f(1.0f, 0.92f, 0.48f, 0.85f);
    glBegin(GL_LINES);
    glVertex3f(start.x, start.y, start.z);
    glVertex3f(end.x, end.y, end.z);
    glEnd();
}

EuclidRenderer::EuclidRenderer() :
    m_bInitialized(false),
    m_rad(0),
    m_pos(0),
    m_radCapacity(0),
    m_numParticles(0),
    m_pointSize(1.0f),
    m_particleRadius(0.125f * 0.5f),
    m_fov(60.0f),
    m_window_w(0),
    m_window_h(0),
    m_program0(0),
    m_program1(0),
    m_meshProgram(0),
    m_meshColorLocation(-1),
    m_meshLightDirLocation(-1),
    m_meshAmbientLocation(-1),
    m_vbo(0),
    m_radVBO(0),
    m_colorVBO(0) {

    _initGL();
    _initialize();
}
EuclidRenderer::~EuclidRenderer() {
    m_pos = 0;

    delete[] m_rad;
    m_rad = nullptr;

    // Deletes the OBJ VBO and clears the associated CPU data.
    clearParticleMeshOBJ();
    releaseMvpTextures();

    if (m_program0) {
        glDeleteProgram(m_program0);
        m_program0 = 0;
    }

    if (m_program1) {
        glDeleteProgram(m_program1);
        m_program1 = 0;
    }

    if (m_meshProgram) {
        glDeleteProgram(m_meshProgram);
        m_meshProgram = 0;
    }

    if (m_particlePosVBO) {
        glDeleteBuffers(
            1,
            &m_particlePosVBO
        );

        m_particlePosVBO = 0;
    }

    if (m_particleRadVBO) {
        glDeleteBuffers(
            1,
            &m_particleRadVBO
        );

        m_particleRadVBO = 0;
    }

    if (m_particleColorVBO) {
        glDeleteBuffers(
            1,
            &m_particleColorVBO
        );

        m_particleColorVBO = 0;
    }
}

void EuclidRenderer::setPositions(float* pos, int numParticles) {
    m_pos = pos;
    m_numParticles = numParticles;
}
void EuclidRenderer::setVertexBuffer(unsigned int vbo, int numParticles) {
    m_vbo = vbo;
    m_numParticles = numParticles;
}
void EuclidRenderer::setRadius(float* r, int numParticles) {
    assert(m_bInitialized);

    if (m_radCapacity != numParticles) {
        delete[] m_rad;
        m_rad = new float[numParticles];
        m_radCapacity = numParticles;
    }

    for (int i = 0; i < numParticles; i++) {
        m_rad[i] = r[i];
    }

    m_numParticles = numParticles;
}

void EuclidRenderer::setGrid(
    const ivec3& gridDim,
    const vec3& worldOrigin,
    const vec3& cellSize) {

    m_gridDim = gridDim;
    m_gridOrigin = worldOrigin;
    m_cellSize = cellSize;
}

void EuclidRenderer::setGridStyle(int majorEvery, bool drawMinor) {
    m_gridMajorEvery = std::max(1, majorEvery);
    m_drawMinorGrid = drawMinor;
}
void EuclidRenderer::setGridMode3D() {
    m_gridMode = GRID_3D;
}
void EuclidRenderer::setGridMode2D(WorkPlane plane, int sliceOffset) {
    m_gridMode = GRID_2D;
    m_workPlane = plane;
    m_sliceOffset = sliceOffset;
}

void EuclidRenderer::drawAxes() {
    const float axisLen = 0.35f;

    glLineWidth(2.0f);
    glBegin(GL_LINES);

    glColor4f(1, 0, 0, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(axisLen, 0, 0);

    glColor4f(0, 1, 0, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(0, axisLen, 0);

    glColor4f(0, 0, 1, 1);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, axisLen);

    glEnd();
    glLineWidth(1.0f);
}
void EuclidRenderer::drawWorkspaceBox() {
    const float s = kWorkspaceHalfBox;

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Outer bounding cube.
    glLineWidth(1.5f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.55f);

    glBegin(GL_LINES);

    // bottom square
    glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s);
    glVertex3f(s, -s, -s); glVertex3f(s, -s, s);
    glVertex3f(s, -s, s); glVertex3f(-s, -s, s);
    glVertex3f(-s, -s, s); glVertex3f(-s, -s, -s);

    // top square
    glVertex3f(-s, s, -s); glVertex3f(s, s, -s);
    glVertex3f(s, s, -s); glVertex3f(s, s, s);
    glVertex3f(s, s, s); glVertex3f(-s, s, s);
    glVertex3f(-s, s, s); glVertex3f(-s, s, -s);

    // verticals
    glVertex3f(-s, -s, -s); glVertex3f(-s, s, -s);
    glVertex3f(s, -s, -s); glVertex3f(s, s, -s);
    glVertex3f(s, -s, s); glVertex3f(s, s, s);
    glVertex3f(-s, -s, s); glVertex3f(-s, s, s);

    glEnd();

    // Sparse internal major grid, similar to EucliGen_ver002.
    glLineWidth(1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.12f);

    glBegin(GL_LINES);

    const int majorEvery = 4;

    for (int i = 0; i <= kWorkspaceGridDim; ++i) {
        if ((i % majorEvery) != 0) continue;

        const float v = -s + i * kWorkspaceCellSize;

        // XY plane lines at z = 0
        glVertex3f(-s, v, 0.0f); glVertex3f(s, v, 0.0f);
        glVertex3f(v, -s, 0.0f); glVertex3f(v, s, 0.0f);

        // XZ plane lines at y = 0
        glVertex3f(-s, 0.0f, v); glVertex3f(s, 0.0f, v);
        glVertex3f(v, 0.0f, -s); glVertex3f(v, 0.0f, s);

        // YZ plane lines at x = 0
        glVertex3f(0.0f, -s, v); glVertex3f(0.0f, s, v);
        glVertex3f(0.0f, v, -s); glVertex3f(0.0f, v, s);
    }

    glEnd();

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}
void EuclidRenderer::drawVolumeAxes(
    VolumeAxisGuideMode guideMode,
    const VolumeObjectBasis& bakedBasis,
    const VolumeObjectBasis& effectiveBasis,
    float pitchDeg,
    float yawDeg,
    float rollDeg) {

    const float axisLen = 1.20f;

    const bool scaleGuide =
        guideMode == VOLUME_AXIS_GUIDE_X ||
        guideMode == VOLUME_AXIS_GUIDE_Y ||
        guideMode == VOLUME_AXIS_GUIDE_Z;

    const bool rotationGuide =
        guideMode == VOLUME_AXIS_GUIDE_PITCH ||
        guideMode == VOLUME_AXIS_GUIDE_YAW ||
        guideMode == VOLUME_AXIS_GUIDE_ROLL;
    // Scale editing dims the world-reference axes more strongly.
    // Rotation guides remain overlaid on visible world axes.

    const float defaultAlpha =
        scaleGuide
        ? 0.38f
        : (rotationGuide ? 0.82f : 0.95f);

    auto emitVector = [](
        const vec3& vector) {

            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(vector.x, vector.y, vector.z);
    };

    // -----------------------------------------------------------------
    // Default positive world XYZ axes.
    //
    // These remain stationary even after an object-basis commit.
    // -----------------------------------------------------------------
    glLineWidth(3.0f);
    glBegin(GL_LINES);

    // World +X: red.
    glColor4f(1.0f, 0.0f, 0.0f, defaultAlpha);

    emitVector(vec3(axisLen, 0.0f, 0.0f));

    // World +Y: green.
    glColor4f(0.0f, 1.0f, 0.0f, defaultAlpha);
    emitVector(vec3(0.0f, axisLen, 0.0f));

    // World +Z: blue.
    glColor4f(0.0f, 0.0f, 1.0f, defaultAlpha);
    emitVector(vec3(0.0f, 0.0f, axisLen));

    glEnd();

    // Scale Whole and non-editing states use only the world axes.
    if (guideMode == VOLUME_AXIS_GUIDE_DEFAULT) {
        glLineWidth(1.0f);
        return;
    }

    // -----------------------------------------------------------------
    // Scale-axis guides.
    //
    // Scaling follows the effective local object basis:
    //
    //     effective basis =
    //         baked basis × editable rotation
    // -----------------------------------------------------------------
    if (scaleGuide) {

        vec3 selectedAxis(1.0f, 0.0f, 0.0f);
        vec4 positiveColor(1.0f, 0.0f, 0.0f, 1.0f);
        vec4 negativeColor(1.0f, 0.25f, 0.65f, 1.0f);

        switch (guideMode) {

        case VOLUME_AXIS_GUIDE_X:
            selectedAxis = effectiveBasis.xAxis;

            // +X red / -X pink.
            positiveColor =
                vec4(1.0f, 0.0f, 0.0f, 1.0f);

            negativeColor =
                vec4(1.0f,0.25f, 0.65f, 1.0f);

            break;

        case VOLUME_AXIS_GUIDE_Y:
            selectedAxis = effectiveBasis.yAxis;

            // +Y green / -Y lime.
            positiveColor =
                vec4(0.0f, 1.0f, 0.0f, 1.0f);

            negativeColor =
                vec4(0.62f, 1.0f, 0.12f, 1.0f);
            break;

        default:
        case VOLUME_AXIS_GUIDE_Z:
            selectedAxis = effectiveBasis.zAxis;

            // +Z blue / -Z cyan.
            positiveColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);
            negativeColor = vec4(0.0f, 1.0f, 1.0f, 1.0f);

            break;
        }

        const vec3 positive =
            selectedAxis * axisLen;

        const vec3 negative =
            -selectedAxis * axisLen;

        glLineWidth(5.0f);
        glBegin(GL_LINES);

        glColor4f(
            negativeColor.r,
            negativeColor.g,
            negativeColor.b,
            negativeColor.a
        );

        emitVector(negative);

        glColor4f(
            positiveColor.r,
            positiveColor.g,
            positiveColor.b,
            positiveColor.a
        );

        emitVector(positive);

        glEnd();

        glLineWidth(1.0f);
        return;
    }

    // -----------------------------------------------------------------
    // Rotation guides.
    //
    // Rotation axis:
    //     baked basis
    //
    // Rotated object vectors:
    //     effective basis
    // -----------------------------------------------------------------
    const glm::vec3 bakedX =
        bakedBasis.xAxis * axisLen;

    const glm::vec3 bakedY =
        bakedBasis.yAxis * axisLen;

    const glm::vec3 bakedZ =
        bakedBasis.zAxis * axisLen;

    const glm::vec3 effectiveX =
        effectiveBasis.xAxis * axisLen;

    const glm::vec3 effectiveY =
        effectiveBasis.yAxis * axisLen;

    const glm::vec3 effectiveZ =
        effectiveBasis.zAxis * axisLen;

    float activeAngleDeg = 0.0f;

    glLineWidth(5.0f);
    glBegin(GL_LINES);

    switch (guideMode) {

    // -------------------------------------------------------------
    // Pitch: rotate around baked local X.
    // -------------------------------------------------------------
    case VOLUME_AXIS_GUIDE_PITCH:
        activeAngleDeg = pitchDeg;

        // -X: pink.
        glColor4f(1.0f, 0.25f, 0.65f, 1.0f);
        emitVector(-bakedX);

        // +X: red.
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        emitVector(bakedX);

        // Effective +Y: dark green.
        glColor4f(0.0f, 0.48f, 0.12f, 1.0f);
        emitVector(effectiveY);

        // Effective +Z: cyan.
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
        emitVector(effectiveZ);

        break;

    // -------------------------------------------------------------
    // Yaw: rotate around baked local Y.
    // -------------------------------------------------------------
    case VOLUME_AXIS_GUIDE_YAW:
        activeAngleDeg = yawDeg;

        // -Y: dark lime.
        glColor4f(0.18f, 0.62f, 0.08f,1.0f);
        emitVector(-bakedY);

        // +Y: green.
        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        emitVector(bakedY);

        // Effective +X: pink.
        glColor4f(1.0f, 0.25f, 0.65f, 1.0f);
        emitVector(effectiveX);

        // Effective +Z: cyan.
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
        emitVector(effectiveZ);

        break;

    // -------------------------------------------------------------
    // Roll: rotate around baked local Z.
    // -------------------------------------------------------------
    default:
    case VOLUME_AXIS_GUIDE_ROLL:
        activeAngleDeg = rollDeg;

        // -Z: cyan.
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
        emitVector(-bakedZ);

        // +Z: blue.
        glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
        emitVector(bakedZ);

        // Effective +X: pink.
        glColor4f(1.0f, 0.25f, 0.65f, 1.0f);
        emitVector(effectiveX);

        // Effective +Y: dark green.
        glColor4f(0.0f, 0.48f, 0.12f, 1.0f);
        emitVector(effectiveY);

        break;
    }

    glEnd();

    // Draw the editable angular delta relative to the baked basis.
    drawRotationDeltaGuide(
        bakedBasis,
        guideMode,
        activeAngleDeg,
        axisLen
    );

    glLineWidth(1.0f);
}
void EuclidRenderer::drawVolumeBoundaryCage(
    int volumeDim,
    int majorEvery,
    float alphaScale,
    const vec4& majorColor,
    const vec4& minorColor,
    const vec4& edgeColor) {

    if (volumeDim <= 0) return;
    alphaScale = std::max(0.0f, std::min(1.0f, alphaScale));

    const int divisions =
        std::max(1, volumeDim);

    majorEvery = std::max(1, majorEvery);

    const float halfExtent = 
        0.5f * static_cast<float>(volumeDim);

    // For a 128^3 volume:
    //
    //     halfExtent = 64
    //     divisions  = 128
    //     cellStep   = 1 voxel
    //
    // Each visible face therefore contains a 128 x 128
    // square lattice. Later, contact patches can be drawn
    // over individual squares without rebuilding the base cage.
    const float cellStep =
        (2.0f * halfExtent) / static_cast<float>(divisions);

    // ---------------------------------------------------------
    // Six face grids.
    //
    // Only interior lines are drawn here. The twelve outer
    // cube edges are drawn separately afterward so shared
    // edges are not repeatedly blended by neighboring faces.
    // ---------------------------------------------------------
    glLineWidth(1.0f);
    glBegin(GL_LINES);

    for (int i = 1; i < divisions; i++) {

        const float position =
            -halfExtent + static_cast<float>(i) * cellStep;

        const bool major =
            (i % majorEvery) == 0;

        if (major) {
            // Major boundary-grid line:
            // brighter greenish cyan.
            glColor4f(
                majorColor.r, 
                majorColor.g, 
                majorColor.b, 
                majorColor.a * alphaScale
            );
        }
        else {
            // Minor voxel line:
            // deliberately faint because all six faces remain
            // visible through the screen-space volume render.
            glColor4f(
                minorColor.r,
                minorColor.g,
                minorColor.b,
                minorColor.a * alphaScale
            );
        }

        // =====================================================
        // -Z FACE
        // =====================================================

        // X-parallel line.
        glVertex3f(-halfExtent, position, -halfExtent);
        glVertex3f(halfExtent, position, -halfExtent);

        // Y-parallel line.
        glVertex3f(position, -halfExtent, -halfExtent);
        glVertex3f(position, halfExtent, -halfExtent);

        // =====================================================
        // +Z FACE
        // =====================================================

        // X-parallel line.
        glVertex3f(-halfExtent, position, halfExtent);
        glVertex3f(halfExtent, position, halfExtent);

        // Y-parallel line.
        glVertex3f(position, -halfExtent, halfExtent);
        glVertex3f(position, halfExtent, halfExtent);

        // =====================================================
        // -Y FACE
        // =====================================================

        // X-parallel line.
        glVertex3f(-halfExtent,-halfExtent,position);
        glVertex3f(halfExtent, -halfExtent, position);

        // Z-parallel line.
        glVertex3f(position,-halfExtent,-halfExtent);
        glVertex3f(position, -halfExtent, halfExtent);

        // =====================================================
        // +Y FACE
        // =====================================================

        // X-parallel line.
        glVertex3f(-halfExtent, halfExtent, position);
        glVertex3f(halfExtent, halfExtent, position);

        // Z-parallel line.
        glVertex3f(position, halfExtent, -halfExtent);
        glVertex3f(position, halfExtent, halfExtent);

        // =====================================================
        // -X FACE
        // =====================================================

        // Y-parallel line.
        glVertex3f(-halfExtent, -halfExtent, position);
        glVertex3f(-halfExtent, halfExtent, position);

        // Z-parallel line.
        glVertex3f(-halfExtent, position, -halfExtent);
        glVertex3f(-halfExtent, position, halfExtent);

        // =====================================================
        // +X FACE
        // =====================================================

        // Y-parallel line.
        glVertex3f(halfExtent, -halfExtent, position);
        glVertex3f(halfExtent, halfExtent, position);

        // Z-parallel line.
        glVertex3f(halfExtent, position, -halfExtent);
        glVertex3f(halfExtent, position, halfExtent);
    }

    glEnd();

    // ---------------------------------------------------------
    // Twelve outer cube edges.
    // ---------------------------------------------------------
    glLineWidth(2.0f);
    glColor4f(
        edgeColor.r,
        edgeColor.g,
        edgeColor.b,
        edgeColor.a* alphaScale
    );

    glBegin(GL_LINES);

    // -Z square.
    glVertex3f(-halfExtent, -halfExtent, -halfExtent);
    glVertex3f(halfExtent, -halfExtent, -halfExtent);

    glVertex3f(halfExtent, -halfExtent, -halfExtent);
    glVertex3f(halfExtent, halfExtent, -halfExtent);

    glVertex3f(halfExtent, halfExtent, -halfExtent);
    glVertex3f(-halfExtent, halfExtent, -halfExtent);

    glVertex3f(-halfExtent, halfExtent, -halfExtent);
    glVertex3f(-halfExtent, -halfExtent, -halfExtent);

    // +Z square.
    glVertex3f(-halfExtent, -halfExtent, halfExtent);
    glVertex3f(halfExtent, -halfExtent, halfExtent);

    glVertex3f(halfExtent, -halfExtent, halfExtent);
    glVertex3f(halfExtent, halfExtent, halfExtent);

    glVertex3f(halfExtent, halfExtent, halfExtent);
    glVertex3f(-halfExtent, halfExtent, halfExtent);

    glVertex3f(-halfExtent, halfExtent, halfExtent);
    glVertex3f(-halfExtent, -halfExtent, halfExtent);

    // Four depth edges.
    glVertex3f(-halfExtent, -halfExtent, -halfExtent);
    glVertex3f(-halfExtent, -halfExtent, halfExtent);

    glVertex3f(halfExtent, -halfExtent, -halfExtent);
    glVertex3f(halfExtent, -halfExtent, halfExtent);

    glVertex3f(halfExtent, halfExtent, -halfExtent);
    glVertex3f(halfExtent, halfExtent, halfExtent);

    glVertex3f(-halfExtent, halfExtent, -halfExtent);
    glVertex3f(-halfExtent, halfExtent, halfExtent);

    glEnd();

    glLineWidth(1.0f);
}
void EuclidRenderer::drawVolumeBoundaryContactPatches(
    int volumeDim,
    const std::vector<unsigned char>& boundaryMask,
    unsigned int boundaryFaceStride) {

    if (volumeDim <= 0 ||
        boundaryFaceStride == 0 ||
        boundaryMask.empty()) {

        return;
    }

    constexpr unsigned int kFaceCount = 6;
    const size_t requiredEntries =
        static_cast<size_t>(boundaryFaceStride) *
        static_cast<size_t>(kFaceCount);

    if (boundaryMask.size() < requiredEntries) return;

    const unsigned int patchesPerFace =
        static_cast<unsigned int>(volumeDim) * 
        static_cast<unsigned int>(volumeDim);

    if (boundaryFaceStride < patchesPerFace) return;


    const float halfExtent =
        0.5f * static_cast<float>(volumeDim);

    const float patchStep =
        (2.0f * halfExtent) / static_cast<float>(volumeDim);

    // Move the alarm wire a tiny distance outward from the
    // base cage so the cyan and orange lines do not compete.
    const float wallOffset = 0.025f;

    const float negativeWall = -halfExtent - wallOffset;
    const float positiveWall = halfExtent + wallOffset;

    glLineWidth(2.25f);
    glColor4f(1.0f, 0.24f, 0.0f, 0.96f);
    glBegin(GL_LINES);

    for (unsigned int face = 0; face < kFaceCount; face++) {
        
        const size_t faceBase =
            static_cast<size_t>(face) *
            static_cast<size_t>(boundaryFaceStride);

        for (unsigned int localPatch = 0; localPatch < patchesPerFace; localPatch++) {
            if (boundaryMask[faceBase + localPatch] == 0) continue;

            const unsigned int u =
                localPatch %
                static_cast<unsigned int>(volumeDim);

            const unsigned int v =
                localPatch /
                static_cast<unsigned int>(volumeDim);

            const float u0 = -halfExtent + static_cast<float>(u) * patchStep;
            const float u1 = u0 + patchStep;
            const float v0 = -halfExtent + static_cast<float>(v) * patchStep;

            const float v1 = v0 + patchStep;

            vec3 a;
            vec3 b;
            vec3 c;
            vec3 d;

            switch (face) {

                // -------------------------------------------------
                // -X / +X:
                //
                //     u = Y
                //     v = Z
                // -------------------------------------------------
            case 0:
                a = vec3(negativeWall, u0, v0);
                b = vec3(negativeWall, u1, v0);
                c = vec3(negativeWall, u1, v1);
                d = vec3(negativeWall, u0, v1);
                
                break;

            case 1:
                a = vec3(positiveWall, u0, v0);
                b = vec3(positiveWall, u1, v0);
                c = vec3(positiveWall, u1, v1);
                d = vec3(positiveWall, u0, v1);

                break;

                // -------------------------------------------------
                // -Y / +Y:
                //
                //     u = X
                //     v = Z
                // -------------------------------------------------
            case 2:
                a = vec3(u0, negativeWall, v0);
                b = vec3(u1, negativeWall, v0);
                c = vec3(u1, negativeWall, v1);
                d = vec3(u0, negativeWall, v1);

                break;

            case 3:
                a = vec3(u0, positiveWall, v0);
                b = vec3(u1, positiveWall, v0);
                c = vec3(u1, positiveWall, v1);
                d = vec3(u0, positiveWall, v1);

                break;

                // -------------------------------------------------
                // -Z / +Z:
                //
                //     u = X
                //     v = Y
                // -------------------------------------------------
            default:
            case 4:
                a = vec3(u0, v0, negativeWall);
                b = vec3(u1, v0, negativeWall);
                c = vec3(u1, v1, negativeWall);
                d = vec3(u0, v1, negativeWall);

                break;

            case 5:
                a = vec3(u0, v0, positiveWall);
                b = vec3(u1, v0, positiveWall);
                c = vec3(u1, v1, positiveWall);
                d = vec3(u0, v1, positiveWall);

                break;
            }

            // Square edge A -> B.
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b.x, b.y, b.z);
           

            // B -> C.
            glVertex3f(b.x, b.y, b.z);
            glVertex3f(c.x, c.y, c.z);
     

            // C -> D.
            glVertex3f(c.x, c.y, c.z);
            glVertex3f(d.x, d.y, d.z);

            // D -> A.
            glVertex3f(d.x, d.y, d.z);
            glVertex3f(a.x, a.y, a.z);
        }
    }

    glEnd();

    glLineWidth(1.0f);
}
void EuclidRenderer::drawVolumeOffsetMarkers(
    int volumeDim,
    VolumeOffsetAxis offsetAxis,
    float normalizedOffsetX,
    float normalizedOffsetY,
    float normalizedOffsetZ) {

    if (volumeDim <= 0) return;

    const float epsilon = 0.000001f;
    const bool hasOffset =
        fabsf(normalizedOffsetX) > epsilon ||
        fabsf(normalizedOffsetY) > epsilon ||
        fabsf(normalizedOffsetZ) > epsilon;

    if (!hasOffset) return;


    const float halfExtent = 0.5f * static_cast<float>(volumeDim);
    const vec3 offsetCenter(
        normalizedOffsetX * halfExtent,
        normalizedOffsetY * halfExtent,
        normalizedOffsetZ * halfExtent
    );

   vec4 markerColor(1.0f, 0.25f, 0.65f, 1.0f);

    switch (offsetAxis) {

    case VOLUME_OFFSET_AXIS_Y:
        // Selected Y vector: dark green.
        markerColor =
            vec4(0.0f, 0.58f, 0.18f, 1.0f);
        break;

    case VOLUME_OFFSET_AXIS_Z:
        // Selected Z vector: cyan.
        markerColor =
            vec4(0.0f, 1.0f, 1.0f, 1.0f);
        break;

    default:
    case VOLUME_OFFSET_AXIS_X:
        // Selected X vector: pink.
        markerColor =
            vec4(1.0f, 0.25f, 0.65f, 1.0f);
        break;
    }

    // ---------------------------------------------------------
    // Displacement segment.
    // ---------------------------------------------------------
    glLineWidth(2.5f);

    glColor4f(
        markerColor.r,
        markerColor.g,
        markerColor.b,
        0.72f
    );

    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(
        offsetCenter.x,
        offsetCenter.y,
        offsetCenter.z
    );

    glEnd();

    // ---------------------------------------------------------
    // Original center: white.
    // Offset center: selected-vector color.
    // ---------------------------------------------------------
    glPointSize(11.0f);
    glBegin(GL_POINTS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);

    glColor4f(
        markerColor.r,
        markerColor.g,
        markerColor.b,
        markerColor.a
    );

    glVertex3f(
        offsetCenter.x,
        offsetCenter.y,
        offsetCenter.z
    );

    glEnd();

    glPointSize(1.0f);
    glLineWidth(1.0f);
}
void EuclidRenderer::drawVolumeOffsetGridPlane(
    int volumeDim,
    VolumeOffsetAxis offsetAxis,
    float normalizedIncrement) {

    if (volumeDim <= 0 || normalizedIncrement <= 0.0f) return;

    const float halfExtent =
        0.5f * static_cast<float>(volumeDim);

    // Offset increments are expressed in normalized volume units.
    //
    // For a 128^3 field:
    //
    //     normalized 1.00 = 64 voxel units
    //     normalized 0.01 = 0.64 voxel units
    const float minorStep =
        normalizedIncrement * halfExtent;

    if (minorStep <= 0.0f) {
        return;
    }

    const int halfStepCount =
        std::max(1, static_cast<int>(floorf(halfExtent / minorStep)));

    // With increment 0.01:
    //
    //     minor line every 0.01
    //     major line every 0.10
    const int majorEvery = 
        getOffsetGridMajorEvery(normalizedIncrement);

    const bool drawXZ = 
        offsetAxis == VOLUME_OFFSET_AXIS_Z;

    glLineWidth(1.0f);
    glBegin(GL_LINES);

    for (int i = -halfStepCount; i <= halfStepCount; i++) {

        const float position =
            static_cast<float>(i) * minorStep;

        if (position < -halfExtent || position > halfExtent) 
            continue;

        const bool major = (i % majorEvery) == 0;

        if (major) {
            glColor4f(0.32f, 1.00f, 0.82f, 0.34f);
        }
        else {
            glColor4f(0.18f, 0.82f, 0.72f, 0.11f);
        }

        if (drawXZ) {
            // X-parallel line at Z = position.
            glVertex3f(-halfExtent, 0.0f, position);
            glVertex3f(halfExtent, 0.0f, position);

            // Z-parallel line at X = position.
            glVertex3f(position, 0.0f, -halfExtent);
            glVertex3f(position, 0.0f, halfExtent);
        }
        else {
            // X-parallel line at Y = position.
            glVertex3f(-halfExtent, position, 0.0f);
            glVertex3f(halfExtent, position, 0.0f);

            // Y-parallel line at X = position.
            glVertex3f(position, -halfExtent, 0.0f);
            glVertex3f(position, halfExtent, 0.0f);
        }
    }

    glEnd();

    // ---------------------------------------------------------
    // Plane boundary.
    // ---------------------------------------------------------
    glLineWidth(1.8f);
    glColor4f(0.42f, 1.00f, 0.86f, 0.72f);

    glBegin(GL_LINE_LOOP);
    if (drawXZ) {
        glVertex3f(-halfExtent, 0.0f, -halfExtent);
        glVertex3f(halfExtent, 0.0f, -halfExtent);
        glVertex3f(halfExtent, 0.0f, halfExtent);
        glVertex3f(-halfExtent, 0.0f, halfExtent);
    }
    else {
        glVertex3f(-halfExtent, -halfExtent, 0.0f);
        glVertex3f(halfExtent, -halfExtent, 0.0f);
        glVertex3f(halfExtent, halfExtent, 0.0f);
        glVertex3f(-halfExtent, halfExtent, 0.0f);
    }

    glEnd();

    // ---------------------------------------------------------
    // Highlight the currently selected offset vector.
    // ---------------------------------------------------------
    glLineWidth(3.5f);
    glBegin(GL_LINES);

    switch (offsetAxis) {
    case VOLUME_OFFSET_AXIS_Y:
        // Y vector: dark green.
        glColor4f(0.0f, 0.58f, 0.18f, 0.95f);

        glVertex3f(0.0f, -halfExtent, 0.0f);
        glVertex3f(0.0f, halfExtent, 0.0f);
        break;

    case VOLUME_OFFSET_AXIS_Z:
        // Z vector: cyan.
        glColor4f(0.0f, 1.0f, 1.0f, 0.95f);

        glVertex3f(0.0f, 0.0f, -halfExtent);
        glVertex3f(0.0f, 0.0f, halfExtent);
        break;

    default:
    case VOLUME_OFFSET_AXIS_X:
        // X vector: pink.
        glColor4f(1.0f, 0.25f, 0.65f, 0.95f);

        glVertex3f(-halfExtent, 0.0f, 0.0f);
        glVertex3f(halfExtent, 0.0f, 0.0f);
        break;
    }

    glEnd();
    glLineWidth(1.0f);
}

void EuclidRenderer::drawXYWorkplane(
    int slice,
    bool hoverValid,
    float hoverX,
    float hoverY) {
    const float s = kWorkspaceHalfBox;      // should be 2.0f
    const int sliceRange = 64;

    // slice = -64 -> z = -2
    // slice =   0 -> z =  0
    // slice =  64 -> z = +2
    const float z =
        (static_cast<float>(slice) / static_cast<float>(sliceRange)) * s;

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // The scanner plane should not write into the depth buffer.
    // Otherwise it can accidentally occlude the particle/axes.
    glDepthMask(GL_FALSE);

    // Workplane outer border.
    glLineWidth(2.0f);
    glColor4f(0.82f, 0.90f, 1.0f, 0.75f);

    glBegin(GL_LINE_LOOP);
    glVertex3f(-s, -s, z);
    glVertex3f(s, -s, z);
    glVertex3f(s, s, z);
    glVertex3f(-s, s, z);
    glEnd();

    // Dense XY grid lines.
    const int divisions = 16;

    glLineWidth(1.0f);
    glBegin(GL_LINES);

    for (int i = -divisions; i <= divisions; ++i) {
        const float t =
            static_cast<float>(i) / static_cast<float>(divisions);

        const float x = t * s;
        const float y = t * s;

        // Major lines every 4 divisions.
        if ((i % 4) == 0) {
            glColor4f(0.75f, 0.85f, 1.0f, 0.45f);
        }
        else {
            glColor4f(0.55f, 0.65f, 0.90f, 0.20f);
        }

        // Lines parallel to Y.
        glVertex3f(x, -s, z);
        glVertex3f(x, s, z);

        // Lines parallel to X.
        glVertex3f(-s, y, z);
        glVertex3f(s, y, z);
    }

    glEnd();

    // Center cross on the active plane.
    glLineWidth(1.5f);
    glColor4f(0.90f, 0.95f, 1.0f, 0.55f);

    glBegin(GL_LINES);
    glVertex3f(-s, 0.0f, z);
    glVertex3f(s, 0.0f, z);

    glVertex3f(0.0f, -s, z);
    glVertex3f(0.0f, s, z);
    glEnd();

    // Mouse hover crosshair on the workplane.
    if (hoverValid) {
        const float h = 0.08f;

        glLineWidth(2.0f);
        glColor4f(1.0f, 1.0f, 0.15f, 0.95f);

        glBegin(GL_LINES);
        glVertex3f(hoverX - h, hoverY, z + 0.01f);
        glVertex3f(hoverX + h, hoverY, z + 0.01f);

        glVertex3f(hoverX, hoverY - h, z + 0.01f);
        glVertex3f(hoverX, hoverY + h, z + 0.01f);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
}
void EuclidRenderer::drawMarchingCubesVoxelGridWire(
    int volumeDim,
    int majorEvery,
    float halfExtent) {
    if (volumeDim <= 0) return;

    const float s = halfExtent;

    if (majorEvery <= 0) {
        majorEvery = 16;
    }

    glDepthMask(GL_FALSE);

    glLineWidth(1.0f);
    glColor4f(0.75f, 0.75f, 1.0f, 0.07f);

    const float step =
        (2.0f * s) / static_cast<float>(volumeDim);

    glBegin(GL_LINES);

    // Draw major XY sheets along Z.
    for (int z = 0; z <= volumeDim; z += majorEvery) {
        const float zz = -s + z * step;

        for (int y = 0; y <= volumeDim; y += majorEvery) {
            const float yy = -s + y * step;
            glVertex3f(-s, yy, zz);
            glVertex3f(s, yy, zz);
        }

        for (int x = 0; x <= volumeDim; x += majorEvery) {
            const float xx = -s + x * step;
            glVertex3f(xx, -s, zz);
            glVertex3f(xx, s, zz);
        }
    }

    // Draw vertical Z lines.
    for (int x = 0; x <= volumeDim; x += majorEvery) {
        const float xx = -s + x * step;

        for (int y = 0; y <= volumeDim; y += majorEvery) {
            const float yy = -s + y * step;

            glVertex3f(xx, yy, -s);
            glVertex3f(xx, yy, s);
        }
    }

    glEnd();

    // Draw a clearer outer volume cube boundary.
    glLineWidth(1.6f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.32f);

    glBegin(GL_LINES);

    // bottom square
    glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s);
    glVertex3f(s, -s, -s);  glVertex3f(s, -s, s);
    glVertex3f(s, -s, s);   glVertex3f(-s, -s, s);
    glVertex3f(-s, -s, s);  glVertex3f(-s, -s, -s);

    // top square
    glVertex3f(-s, s, -s); glVertex3f(s, s, -s);
    glVertex3f(s, s, -s);  glVertex3f(s, s, s);
    glVertex3f(s, s, s);   glVertex3f(-s, s, s);
    glVertex3f(-s, s, s);  glVertex3f(-s, s, -s);

    // verticals
    glVertex3f(-s, -s, -s); glVertex3f(-s, s, -s);
    glVertex3f(s, -s, -s);  glVertex3f(s, s, -s);
    glVertex3f(s, -s, s);   glVertex3f(s, s, s);
    glVertex3f(-s, -s, s);  glVertex3f(-s, s, s);

    glEnd();

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
}
void EuclidRenderer::drawActiveVoxelCellWire(
    uint voxelID,
    int gridX,
    int gridY,
    int gridZ) {

    const uint gx = static_cast<uint>(gridX);
    const uint gy = static_cast<uint>(gridY);

    const uint x = voxelID % gx;
    const uint y = (voxelID / gx) % gy;
    const uint z = voxelID / (gx * gy);

    const float sx = static_cast<float>(gridX) * 0.5f;
    const float sy = static_cast<float>(gridY) * 0.5f;
    const float sz = static_cast<float>(gridZ) * 0.5f;

    const float x0 = -sx + static_cast<float>(x);
    const float y0 = -sy + static_cast<float>(y);
    const float z0 = -sz + static_cast<float>(z);

    const float x1 = x0 + 1.0f;
    const float y1 = y0 + 1.0f;
    const float z1 = z0 + 1.0f;

    glBegin(GL_LINES);

    // bottom
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y0, z0);

    // top
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x0, y0, z1);

    // verticals
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1);

    glEnd();
}

void EuclidRenderer::displayGrid() {
    if (!m_gridEnabled) return;
    if (m_gridDim.x <= 0 || m_gridDim.y <= 0 || m_gridDim.z <= 0) return;

    glUseProgram(0);
    glDisable(GL_POINT_SPRITE_ARB);
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const vec3 mn = m_gridOrigin;
    const vec3 mx = m_gridOrigin + glm::vec3(
        m_cellSize.x * (float)m_gridDim.x,
        m_cellSize.y * (float)m_gridDim.y,
        m_cellSize.z * (float)m_gridDim.z
    );

    // Axes at origin
    if (m_drawAxes) {
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glColor4f(1, 0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0.35f, 0, 0);
        glColor4f(0, 1, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0.35f, 0);
        glColor4f(0, 0, 1, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 0.35f);
        glEnd();
    }

    const int major = std::max(1, m_gridMajorEvery);

    if (m_gridMode == GRID_3D) {
        // bounding box
        glLineWidth(2.0f);
        glColor4f(1, 1, 1, 0.35f);
        RenderUtils::draw_aabb_wire(mn, mx);

        // major lattice (sparse)
        glLineWidth(1.0f);
        glColor4f(1, 1, 1, 0.12f);
        glBegin(GL_LINES);

        // X-parallel
        for (int yi = 0; yi <= m_gridDim.y; ++yi) {
            if (!RenderUtils::is_major(yi, major)) continue;
            float y = mn.y + yi * m_cellSize.y;
            for (int zi = 0; zi <= m_gridDim.z; ++zi) {
                if (!RenderUtils::is_major(zi, major)) continue;
                float z = mn.z + zi * m_cellSize.z;
                glVertex3f(mn.x, y, z); glVertex3f(mx.x, y, z);
            }
        }
        // Y-parallel
        for (int xi = 0; xi <= m_gridDim.x; ++xi) {
            if (!RenderUtils::is_major(xi, major)) continue;
            float x = mn.x + xi * m_cellSize.x;
            for (int zi = 0; zi <= m_gridDim.z; ++zi) {
                if (!RenderUtils::is_major(zi, major)) continue;
                float z = mn.z + zi * m_cellSize.z;
                glVertex3f(x, mn.y, z); glVertex3f(x, mx.y, z);
            }
        }
        // Z-parallel
        for (int xi = 0; xi <= m_gridDim.x; ++xi) {
            if (!RenderUtils::is_major(xi, major)) continue;
            float x = mn.x + xi * m_cellSize.x;
            for (int yi = 0; yi <= m_gridDim.y; ++yi) {
                if (!RenderUtils::is_major(yi, major)) continue;
                float y = mn.y + yi * m_cellSize.y;
                glVertex3f(x, y, mn.z); glVertex3f(x, y, mx.z);
            }
        }
        glEnd();

        // optional minor grid (dense)
        if (m_drawMinorGrid) {
            glColor4f(1, 1, 1, 0.04f);
            glBegin(GL_LINES);
            for (int yi = 0; yi <= m_gridDim.y; ++yi) {
                float y = mn.y + yi * m_cellSize.y;
                for (int zi = 0; zi <= m_gridDim.z; ++zi) {
                    float z = mn.z + zi * m_cellSize.z;
                    glVertex3f(mn.x, y, z); glVertex3f(mx.x, y, z);
                }
            }
            for (int xi = 0; xi <= m_gridDim.x; ++xi) {
                float x = mn.x + xi * m_cellSize.x;
                for (int zi = 0; zi <= m_gridDim.z; ++zi) {
                    float z = mn.z + zi * m_cellSize.z;
                    glVertex3f(x, mn.y, z); glVertex3f(x, mx.y, z);
                }
            }
            for (int xi = 0; xi <= m_gridDim.x; ++xi) {
                float x = mn.x + xi * m_cellSize.x;
                for (int yi = 0; yi <= m_gridDim.y; ++yi) {
                    float y = mn.y + yi * m_cellSize.y;
                    glVertex3f(x, y, mn.z); glVertex3f(x, y, mx.z);
                }
            }
            glEnd();
        }
    }
    else {
        // 2D slice: thick boundary + thick major + thin cell lines
        if (m_workPlane == PLANE_XY) {
            const int mid = m_gridDim.z / 2;
            const int sliceIndex = RenderUtils::clampi(mid + m_sliceOffset, 0, m_gridDim.z);
            const float z = mn.z + sliceIndex * m_cellSize.z;

            glLineWidth(3.0f);
            glColor4f(1, 1, 1, 0.9f);
            RenderUtils::draw_rect_wire_xy(mn.x, mn.y, mx.x, mx.y, z);

            glLineWidth(2.0f);
            glColor4f(1, 1, 1, 0.35f);
            glBegin(GL_LINES);
            for (int xi = 0; xi <= m_gridDim.x; ++xi) if (RenderUtils::is_major(xi, major)) {
                float x = mn.x + xi * m_cellSize.x;
                glVertex3f(x, mn.y, z); glVertex3f(x, mx.y, z);
            }
            for (int yi = 0; yi <= m_gridDim.y; ++yi) if (RenderUtils::is_major(yi, major)) {
                float y = mn.y + yi * m_cellSize.y;
                glVertex3f(mn.x, y, z); glVertex3f(mx.x, y, z);
            }
            glEnd();

            glLineWidth(1.0f);
            glColor4f(1, 1, 1, 0.18f);
            glBegin(GL_LINES);
            for (int xi = 0; xi <= m_gridDim.x; ++xi) {
                float x = mn.x + xi * m_cellSize.x;
                glVertex3f(x, mn.y, z); glVertex3f(x, mx.y, z);
            }
            for (int yi = 0; yi <= m_gridDim.y; ++yi) {
                float y = mn.y + yi * m_cellSize.y;
                glVertex3f(mn.x, y, z); glVertex3f(mx.x, y, z);
            }
            glEnd();
        }

        else if (m_workPlane == PLANE_XZ) {
            const int mid = m_gridDim.y / 2;
            const int sliceIndex = RenderUtils::clampi(mid + m_sliceOffset, 0, m_gridDim.y);
            const float y = mn.y + sliceIndex * m_cellSize.y;

            glLineWidth(3.0f);
            glColor4f(1, 1, 1, 0.9f);
            RenderUtils::draw_rect_wire_xz(mn.x, mn.z, mx.x, mx.z, y);

            glLineWidth(2.0f);
            glColor4f(1, 1, 1, 0.35f);
            glBegin(GL_LINES);
            for (int xi = 0; xi <= m_gridDim.x; ++xi) if (RenderUtils::is_major(xi, major)) {
                float x = mn.x + xi * m_cellSize.x;
                glVertex3f(x, y, mn.z); glVertex3f(x, y, mx.z);
            }
            for (int zi = 0; zi <= m_gridDim.z; ++zi) if (RenderUtils::is_major(zi, major)) {
                float z = mn.z + zi * m_cellSize.z;
                glVertex3f(mn.x, y, z); glVertex3f(mx.x, y, z);
            }
            glEnd();

            glLineWidth(1.0f);
            glColor4f(1, 1, 1, 0.18f);
            glBegin(GL_LINES);
            for (int xi = 0; xi <= m_gridDim.x; ++xi) {
                float x = mn.x + xi * m_cellSize.x;
                glVertex3f(x, y, mn.z); glVertex3f(x, y, mx.z);
            }
            for (int zi = 0; zi <= m_gridDim.z; ++zi) {
                float z = mn.z + zi * m_cellSize.z;
                glVertex3f(mn.x, y, z); glVertex3f(mx.x, y, z);
            }
            glEnd();
        }

        else if (m_workPlane == PLANE_YZ) {
            const int mid = m_gridDim.x / 2;
            const int sliceIndex = RenderUtils::clampi(mid + m_sliceOffset, 0, m_gridDim.x);
            const float x = mn.x + sliceIndex * m_cellSize.x;

            glLineWidth(3.0f);
            glColor4f(1, 1, 1, 0.9f);
            RenderUtils::draw_rect_wire_yz(mn.y, mn.z, mx.y, mx.z, x);

            glLineWidth(2.0f);
            glColor4f(1, 1, 1, 0.35f);
            glBegin(GL_LINES);
            for (int yi = 0; yi <= m_gridDim.y; ++yi) if (RenderUtils::is_major(yi, major)) {
                float y = mn.y + yi * m_cellSize.y;
                glVertex3f(x, y, mn.z); glVertex3f(x, y, mx.z);
            }
            for (int zi = 0; zi <= m_gridDim.z; ++zi) if (RenderUtils::is_major(zi, major)) {
                float z = mn.z + zi * m_cellSize.z;
                glVertex3f(x, mn.y, z); glVertex3f(x, mx.y, z);
            }
            glEnd();

            glLineWidth(1.0f);
            glColor4f(1, 1, 1, 0.18f);
            glBegin(GL_LINES);
            for (int yi = 0; yi <= m_gridDim.y; ++yi) {
                float y = mn.y + yi * m_cellSize.y;
                glVertex3f(x, y, mn.z); glVertex3f(x, y, mx.z);
            }
            for (int zi = 0; zi <= m_gridDim.z; ++zi) {
                float z = mn.z + zi * m_cellSize.z;
                glVertex3f(x, mn.y, z); glVertex3f(x, mx.y, z);
            }
            glEnd();
        }
    }

    glDisable(GL_BLEND);
    glLineWidth(1.0f);
}

void EuclidRenderer::displayParticleWorkspace(
    float thetaRad,
    float phiRad,
    float zs,
    int workplaneSlice,
    bool showWorkplane,
    bool selected,
    bool hoverValid,
    float hoverX,
    float hoverY,
    bool useMeshRender) {

    if (!m_psystem) return;

    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const float aspect = (m_window_h > 0)
        ? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const float safeZs = std::max(zs, 1.0f);
    const float zoomScale = safeZs / 256.0f;

    // Match SINGLE_PARTICLE CAD default framing to CameraProcessor camDist.
    const float spCadBaseDistance = 1.25f;

    glTranslatef(0.0f, 0.0f, -spCadBaseDistance * zoomScale);

    glRotatef(
        phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f,
        0.0f,
        0.0f
    );

    glRotatef(
        thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f,
        1.0f,
        0.0f
    );

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glUseProgram(0);

    drawWorkspaceBox();

    if (showWorkplane) {
        drawXYWorkplane(
            workplaneSlice,
            hoverValid,
            hoverX,
            hoverY
        );
    }

    drawAxes();
    const ParticleProxy3D& p = m_psystem->getActiveParticle();

    const bool particleVisible =
        !showWorkplane || particleIntersectsSlice(p, workplaneSlice);

    const float4 c = m_psystem->getUniformParticleColor();

    float pos[4] = {
        p.position.x,
        p.position.y,
        p.position.z,
        1.0f
    };

    float color[4] = {
        c.x,
        c.y,
        c.z,
        c.w
    };

    const float rad = p.radius;

    if (particleVisible) {
        if (useMeshRender && hasParticleMeshOBJ()) {
            drawParticleMeshOBJ(
                p,
                c,
                selected
            );
        }
        else {
            // Normal shaded sphere pass.
            drawParticleSphere(
                m_program0,
                pos,
                rad,
                color,
                false
            );

            // Highlight / selected emissive pass.
            if (selected) {
                float highlightColor[4] = {
                    1.0f,
                    0.35f,
                    0.05f,
                    0.85f
                };

                drawParticleSphere(
                    m_program1,
                    pos,
                    rad * 1.35f,
                    highlightColor,
                    true
                );
            }
        }
    }
    glUseProgram(0);
}
void EuclidRenderer::displayXYWorkplane(
    int slice,
    bool hoverValid,
    float hoverX,
    float hoverY,
    bool selected) {

    const float s = 2.0f;
    const int sliceRange = 64;

    const float z =
        (static_cast<float>(slice) / static_cast<float>(sliceRange)) * s;

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Do not let the scanner plane occlude particle/axes.
    glDepthMask(GL_FALSE);

    // Workplane border.
    glLineWidth(2.0f);
    glColor4f(0.82f, 0.90f, 1.0f, 0.75f);

    glBegin(GL_LINE_LOOP);
    glVertex3f(-s, -s, z);
    glVertex3f(s, -s, z);
    glVertex3f(s, s, z);
    glVertex3f(-s, s, z);
    glEnd();

    // Workplane grid.
    const int divisions = 16;

    glLineWidth(1.0f);
    glBegin(GL_LINES);

    for (int i = -divisions; i <= divisions; ++i) {
        const float t =
            static_cast<float>(i) / static_cast<float>(divisions);

        const float x = t * s;
        const float y = t * s;

        if ((i % 4) == 0) {
            glColor4f(0.75f, 0.85f, 1.0f, 0.45f);
        }
        else {
            glColor4f(0.55f, 0.65f, 0.90f, 0.20f);
        }

        glVertex3f(x, -s, z);
        glVertex3f(x, s, z);

        glVertex3f(-s, y, z);
        glVertex3f(s, y, z);
    }

    glEnd();

    // Center cross.
    glLineWidth(1.5f);
    glColor4f(0.90f, 0.95f, 1.0f, 0.55f);

    glBegin(GL_LINES);
    glVertex3f(-s, 0.0f, z);
    glVertex3f(s, 0.0f, z);

    glVertex3f(0.0f, -s, z);
    glVertex3f(0.0f, s, z);
    glEnd();

    // Hover crosshair.
    if (hoverValid) {
        const float h = selected ? 0.12f : 0.08f;

        glLineWidth(selected ? 3.0f : 2.0f);

        if (selected) {
            glColor4f(1.0f, 0.45f, 0.05f, 0.95f);
        }
        else {
            glColor4f(1.0f, 1.0f, 0.15f, 0.95f);
        }

        glBegin(GL_LINES);
        glVertex3f(hoverX - h, hoverY, z + 0.01f);
        glVertex3f(hoverX + h, hoverY, z + 0.01f);

        glVertex3f(hoverX, hoverY - h, z + 0.01f);
        glVertex3f(hoverX, hoverY + h, z + 0.01f);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
}
void EuclidRenderer::displayMarchingCubesVoxelGrid(
    float thetaRad,
    float phiRad,
    float zs,
    int volumeDim,
    int majorEvery,
    float halfExtent) {

    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect = (m_window_h > 0)
        ? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
        : 1.0f;

    // Match MarchingCubes::renderWireframe().
    gluPerspective(m_fov, aspect, 1.0, 2000.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Match MarchingCubes::renderWireframe().
    // This makes the MC volume box zoom together with the generated mesh.
    glTranslatef(0.0f, 0.0f, -zs);

    glRotatef(
        phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f,
        0.0f,
        0.0f
    );

    glRotatef(
        thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f,
        1.0f,
        0.0f
    );

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // The CUDA raycast is screen-space, so keep this as overlay geometry.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawMarchingCubesVoxelGridWire(
        volumeDim,
        majorEvery,
        halfExtent
    );

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}
void EuclidRenderer::displayActiveVoxelWireFrame(
    float thetaRad,
    float phiRad,
    float zs,
    const std::vector<uint>& activeVoxelIds,
    int gridX, int gridY, int gridZ,
    int maxBoxes,
    float alpha) {

    if (activeVoxelIds.empty()) return;
    if (gridX <= 0 || gridY <= 0 || gridZ <= 0) return;

    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect = (m_window_h > 0)
        ? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 1.0, 2000.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -zs);
    glRotatef(
        phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f, 0.0f, 0.0f
    );

    glRotatef(
        thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f, 1.0f, 0.0f
    );

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // this debug overlay over the CUDA raycast texture
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(1.1f);
    glColor4f(1.0f, 0.82f, 0.22f, alpha);

    const int count = static_cast<int>(activeVoxelIds.size());
    const int stride =
        (count > maxBoxes && maxBoxes > 0)
        ? std::max(1, count / maxBoxes)
        : 1;

    for (int n = 0; n < count; n += stride) {
        drawActiveVoxelCellWire(
            activeVoxelIds[n],
            gridX, gridY, gridZ
        );
    }

    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}
void EuclidRenderer::displayMarchingCubesTriangleWireFrame(
    float thetaRad,
    float phiRad,
    float zs,
    const vector<float4>& verts,
    float alpha,
    int maxTriangles) {

    if (verts.empty()) return;

    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect = (m_window_h > 0)
        ? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 1.0, 2000.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Use the same volume-space camera convention as the rest of the OpenGL overlays.
    glTranslatef(0.0f, 0.0f, -zs);
    glRotatef(phiRad * 180.0f / static_cast<float>(M_PI), 1.0f, 0.0f, 0.0f);
    glRotatef(thetaRad * 180.0f / static_cast<float>(M_PI), 0.0f, 1.0f, 0.0f);

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_LINE_BIT |
        GL_POLYGON_BIT |
        GL_DEPTH_BUFFER_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_CURRENT_BIT
    );

    GLint oldProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.15f);
    glColor4f(1.0f, 0.96f, 0.92f, alpha);

    const int totalVerts = static_cast<int>(verts.size());
    const int totalTriangles = totalVerts / 3;
    const int drawTriangles = std::min(totalTriangles, maxTriangles);
    const int drawVerts = drawTriangles * 3;

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < drawVerts; i++) {
        glVertex3f(verts[i].x, verts[i].y, verts[i].z);
    }
    glEnd();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(static_cast<GLuint>(oldProgram));
    glPopAttrib();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}

void EuclidRenderer::clearParticleMeshOBJ() {
    if (m_particleMeshVBO) {
        glDeleteBuffers(1, &m_particleMeshVBO);
        m_particleMeshVBO = 0;
    }

    m_particleMeshVertexCount = 0;

    m_particleMeshVerts.clear();
    m_particleMeshNorms.clear();

    m_particleMeshMin = vec3(0.0f);
    m_particleMeshMax = vec3(0.0f);
    m_particleMeshCenter = vec3(0.0f);
        
    m_particleMeshMaxExtent = 1.0f;
    m_particleMeshLoaded = false;
    m_particleMeshPath.clear();
}
bool EuclidRenderer::uploadParticleMeshVBO() {
    m_particleMeshVertexCount = 0;

    if (m_particleMeshVerts.empty()) return false;
    

    if (m_particleMeshNorms.size() !=
        m_particleMeshVerts.size()) {

        printf(
            "[EuclidRenderer] OBJ VBO upload failed: "
            "position/normal count mismatch. "
            "positions=%zu normals=%zu\n",
            m_particleMeshVerts.size(),
            m_particleMeshNorms.size()
        );

        return false;
    }

    vector<ParticleMeshVertex> packedVertices;
    packedVertices.resize(m_particleMeshVerts.size());

    for (size_t i = 0; i < m_particleMeshVerts.size(); i++) {

        vec3 normal = m_particleMeshNorms[i];
        const float normalLengthSquared = dot(normal, normal);

        if (normalLengthSquared > 1.0e-12f) {
            normal /= sqrtf(normalLengthSquared);
        }
        else {
            normal = vec3(0.0f, 1.0f, 0.0f);
        }

        packedVertices[i].position = m_particleMeshVerts[i];
        packedVertices[i].normal = normal;
    }

    if (!m_particleMeshVBO) {
        glGenBuffers(1, &m_particleMeshVBO);
    }

    if (!m_particleMeshVBO) {
        printf(
            "[EuclidRenderer] OBJ VBO upload failed: "
            "glGenBuffers returned zero.\n"
        );

        return false;
    }

    GLint previousArrayBuffer = 0;

    glGetIntegerv(
        GL_ARRAY_BUFFER_BINDING,
        &previousArrayBuffer
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_particleMeshVBO
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        packedVertices.size() *
        sizeof(ParticleMeshVertex),
        packedVertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        static_cast<GLuint>(previousArrayBuffer)
    );

    m_particleMeshVertexCount =
        static_cast<GLsizei>(packedVertices.size());

    printf(
        "[EuclidRenderer] OBJ mesh uploaded: "
        "vbo=%u vertices=%d bytes=%zu\n",
        static_cast<unsigned int>(m_particleMeshVBO),
        static_cast<int>(m_particleMeshVertexCount),
        packedVertices.size() *
        sizeof(ParticleMeshVertex)
    );

    return m_particleMeshVertexCount > 0;
}
void EuclidRenderer::displayVolumeOrientationAxes(
    float thetaRad,
    float phiRad,
    VolumeAxisGuideMode guideMode,
    const VolumeObjectBasis& bakedBasis,
    const VolumeObjectBasis& effectiveBasis,
    float pitchDeg,
    float yawDeg,
    float rollDeg) {
    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect = (m_window_h > 0)
        ? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Stable overlay size, but same orientation as the volume ray camera.
    glTranslatef(0.0f, 0.0f, -4.0f);

    glRotatef(
        phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f,
        0.0f,
        0.0f
    );

    glRotatef(
        thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f,
        1.0f,
        0.0f
    );

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawVolumeAxes(
        guideMode,
        bakedBasis,
        effectiveBasis,
        pitchDeg,
        yawDeg,
        rollDeg
    );

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}
void EuclidRenderer::displayVolumeInjectionVoxelPreview(
    float thetaRad, float phiRad, float zs,
    int volumeDim, float alphaScale,
    int injectionDx, int injectionDy, int injectionDz) {
    if (volumeDim <= 0) return;

    alphaScale = std::max(0.0f, std::min(1.0f, alphaScale));

    const bool hasInjectionVoxel =
        injectionDx != 0 ||
        injectionDy != 0 ||
        injectionDz != 0;

    glViewport(0, 0, m_window_w, m_window_h);
    // ---------------------------------------------------------
    // Projection matching the existing volume overlays.
    // ---------------------------------------------------------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect = (m_window_h > 0)
        ? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 1.0f, 2000.0f);

    // ---------------------------------------------------------
    // Match the CUDA volume camera orientation.
    // ---------------------------------------------------------
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -zs);

    glRotatef(phiRad * 180.0f / 
        static_cast<float>(M_PI),
        1.0f, 0.0f, 0.0f);

    glRotatef(thetaRad * 180.0f /
        static_cast<float>(M_PI),
        0.0f, 1.0f, 0.0f);

    // ---------------------------------------------------------
    // Transparent overlay state.
    //
    // The scalar field has already been rendered into the
    // screen-space texture, so this cage is intentionally drawn
    // over it without depth rejection.
    // ---------------------------------------------------------
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const int majorEvery = std::max(1, volumeDim / 16);

    const vec4 cyanMajor(0.24f, 1.00f, 0.78f, 0.16f);
    const vec4 cyanMinor(0.10f, 0.82f, 0.66f, 0.45f);
    const vec4 cyanEdge(0.38f, 1.00f, 0.84f, 0.72f);

    const vec4 orangeMajor(1.00f, 0.46f, 0.04f, 0.18f);
    const vec4 orangeMinor(1.00f, 0.25f, 0.00f, 0.055f);
    const vec4 orangeEdge(1.00f, 0.58f, 0.08f, 0.78f);
    // ---------------------------------------------------------
    // VOXEL_111 anchor chamber.
    //
    // NONE:
    //     center cage remains cyan.
    //
    // Selected injection voxel:
    //     center cage becomes orange to mark BASE anchor.
    // ---------------------------------------------------------
    if (hasInjectionVoxel) {
        
        drawVolumeBoundaryCage(
            volumeDim,
            majorEvery,
            alphaScale,
            orangeMajor,
            orangeMinor,
            orangeEdge
        );
    }
    else {
        drawVolumeBoundaryCage(
            volumeDim,
            majorEvery,
            alphaScale,
            cyanMajor,
            cyanMinor,
            cyanEdge
        );
    }
    // ---------------------------------------------------------
    // Selected neighboring injection chamber.
    // ---------------------------------------------------------
    if (hasInjectionVoxel) {
        
        const float chamberStep = 
            static_cast<float>(volumeDim);

        const float targetX = 
            static_cast<float>(injectionDx) * chamberStep;

        const float targetY =
            static_cast<float>(injectionDy) * chamberStep;

        const float targetZ =
            static_cast<float>(injectionDz) * chamberStep;

        glPushMatrix();
        glTranslatef(targetX, targetY, targetZ);

        drawVolumeBoundaryCage(
            volumeDim,
            majorEvery,
            alphaScale,
            cyanMajor,
            cyanMinor,
            cyanEdge
        );

        glPopMatrix();
        // -----------------------------------------------------
        // Multi-axis injection rail.
        //
        // The rail is drawn as a colored stair-step path:
        //
        //     +X : red
        //     -X : pink
        //     +Y : green
        //     -Y : dark green
        //     +Z : blue
        //     -Z : cyan
        //
        // This gives edge/corner voxels readable compound rails:
        //
        //     120  +Y/-Z       green + cyan
        //     222  +X/+Y/+Z    red + green + blue
        //     000  -X/-Y/-Z    pink + dark green + cyan
        // -----------------------------------------------------
        auto colorForAxis = [](int axis, int sign) -> vec4 {
            // axis 0 = X
            if (axis == 0) {
                if (sign > 0) {
                    return vec4(1.0f, 0.0f, 0.0f, 0.95f);
                }
                return vec4(1.0f, 0.25f, 0.65f, 0.95f);
            }
            // axis 0 = Y
            if (axis == 1) {
                if (sign > 0) {
                    return vec4(0.0f, 1.0, 0.0f, 0.95f);
                }
                return vec4(0.0f, 0.48f, 0.12f, 0.95f);
            }
            // axis 2 = Z
            if (sign > 0) {
                return vec4(0.0f, 0.0f, 1.0f, 0.95f);
            }
            return vec4(0.0f, 1.0f, 1.0f, 0.95f);
        };

        auto drawRailSegment =
            [](const vec3& a, const vec3& b, const vec4& c) {

            glLineWidth(4.0f);
            glColor4f(c.r, c.g, c.b, c.a);
            glBegin(GL_LINES);
            glVertex3f(a.x, a.y, a.z);
            glVertex3f(b.x, b.y, b.z);
            glEnd();
        };

        vec3 cursor(0.0f, 0.0f, 0.0f);
        vec4 lastRailColor(1.0f, 1.0f, 1.0f, 0.95f);

        if (injectionDx != 0) {

            const vec3 next = cursor +
                vec3(static_cast<float>(injectionDx) * chamberStep, 0.0f, 0.0f);

            lastRailColor = colorForAxis(0, injectionDx);
            drawRailSegment(cursor, next, lastRailColor);
            cursor = next;
        }

        if (injectionDy != 0) {

            const vec3 next = cursor +
                vec3(0.0f, static_cast<float>(injectionDy) * chamberStep, 0.0f);

            lastRailColor = colorForAxis(1, injectionDy);
            drawRailSegment(cursor, next, lastRailColor);
            cursor = next;
        }

        if (injectionDz != 0) {

            const vec3 next = cursor +
                vec3(0.0f, 0.0f, static_cast<float>(injectionDz) * chamberStep);

            lastRailColor = colorForAxis(2, injectionDz);
            drawRailSegment(cursor, next, lastRailColor);
            cursor = next;
        }
        // Endpoint tick at the selected voxel center.
        const float tick =
            static_cast<float>(volumeDim) * 0.035f;

        glLineWidth(2.5f);
        glColor4f(
            lastRailColor.r, 
            lastRailColor.g, 
            lastRailColor.b, 
            lastRailColor.a
        );

        glBegin(GL_LINES);
        glVertex3f(targetX - tick, targetY, targetZ);
        glVertex3f(targetX + tick, targetY, targetZ);
        glVertex3f(targetX, targetY - tick, targetZ);
        glVertex3f(targetX, targetY + tick, targetZ);
        glVertex3f(targetX, targetY, targetZ - tick);
        glVertex3f(targetX, targetY, targetZ + tick);
        glEnd();
    }

    // ---------------------------------------------------------
    // Restore normal rendering state.
    // ---------------------------------------------------------
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
void EuclidRenderer::displayVolumeInjectionEditTargetPreview(
    float thetaRad,
    float phiRad,
    float zs,
    int volumeDim,
    float alphaScale,
    int injectionDx,
    int injectionDy,
    int injectionDz,
    bool editingVoxel1) {
    if (volumeDim <= 0) return;

    const bool hasInjectionVoxel =
        injectionDx != 0 ||
        injectionDy != 0 ||
        injectionDz != 0;

    if (!hasInjectionVoxel) return;


    alphaScale = 
        std::max(0.0f, std::min(1.0f, alphaScale));

    glViewport(0, 0, m_window_w, m_window_h);

    // ---------------------------------------------------------
    // Projection matching the existing volume overlays.
    // ---------------------------------------------------------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect =
        (m_window_h > 0)
        ? static_cast<float>(m_window_w) /
        static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 1.0f, 2000.0f);

    // ---------------------------------------------------------
    // Match the CUDA volume camera orientation.
    // ---------------------------------------------------------
    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -zs);

    glRotatef(phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f, 0.0f, 0.0f);

    glRotatef(thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f, 1.0f, 0.0f);

    glUseProgram(0);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const int majorEvery = std::max(1, volumeDim / 16);
    const float chamberStep = static_cast<float>(volumeDim);
    const vec3 anchorCenter(0.0f, 0.0f, 0.0f);

    const vec3 injectionCenter(
        static_cast<float>(injectionDx) * chamberStep,
        static_cast<float>(injectionDy) * chamberStep,
        static_cast<float>(injectionDz) * chamberStep
    );
    // ---------------------------------------------------------
    // Local preview-origin shift.
    //
    // This creates the illusion that the selected edit volume is
    // centered without moving the CUDA raycaster camera.
    //
    // Edit { VOLUME_0 }:
    //     previewOrigin = anchor center
    //     anchor remains at origin
    //     injection cage appears at selected voxel position
    //
    // Edit { VOLUME_1 }:
    //     previewOrigin = injection center
    //     injection brush remains visually centered
    //     anchor cage shifts to the opposite side
    // ---------------------------------------------------------
    const vec3 previewOrigin =
        editingVoxel1
        ? injectionCenter
        : anchorCenter;

    const vec4 cyanMajor(0.24f, 1.00f, 0.78f, 0.16f);
    const vec4 cyanMinor(0.10f, 0.82f, 0.66f, 0.045f);
    const vec4 cyanEdge(0.38f, 1.00f, 0.84f, 0.72f);

    const vec4 orangeMajor(1.00f,0.46f,0.04f,0.18f);
    const vec4 orangeMinor(1.00f, 0.25f, 0.00f, 0.055f);
    const vec4 orangeEdge(1.00f, 0.58f, 0.08f, 0.78f);

    // Shift only this overlay's local coordinate frame.
    // The surrounding glPushMatrix/glPopMatrix keeps this isolated.
    glTranslatef(
        -previewOrigin.x,
        -previewOrigin.y,
        -previewOrigin.z
    );

    // ---------------------------------------------------------
    // List [1] behavior:
    //
    // Edit { VOXEL_0 }:
    //     show VOXEL_1 as greenish-cyan helper cage.
    //
    // Edit { VOXEL_1 }:
    //     show VOXEL_0 as neon-orange helper cage.
    // ---------------------------------------------------------
    if (!editingVoxel1) {

        glPushMatrix();

        glTranslatef(
            injectionCenter.x,
            injectionCenter.y,
            injectionCenter.z
        );

        drawVolumeBoundaryCage(
            volumeDim,
            majorEvery,
            alphaScale,
            cyanMajor,
            cyanMinor,
            cyanEdge
        );

        glPopMatrix();
    }
    else {

        drawVolumeBoundaryCage(
            volumeDim,
            majorEvery,
            alphaScale,
            orangeMajor,
            orangeMinor,
            orangeEdge
        );
    }

    // ---------------------------------------------------------
    // Focus marker.
    //
    // This is the checkpoint-3B visual focus target. It does
    // not move the CUDA raycaster yet.
    // ---------------------------------------------------------
    const vec3 focusCenter =
        editingVoxel1
        ? injectionCenter
        : anchorCenter;

    const vec4 focusColor =
        editingVoxel1
        ? vec4(0.38f, 1.00f, 0.84f, 0.95f)
        : vec4(1.00f, 0.95f, 0.72f, 0.95f);

    const float focusTick = 
        static_cast<float>(volumeDim) * 0.055f;

    glLineWidth(4.0f);

    glColor4f(
        focusColor.r,
        focusColor.g,
        focusColor.b,
        focusColor.a
    );

    glBegin(GL_LINES);
    glVertex3f(focusCenter.x - focusTick, focusCenter.y, focusCenter.z);
    glVertex3f(focusCenter.x + focusTick, focusCenter.y, focusCenter.z);
    glVertex3f(focusCenter.x, focusCenter.y - focusTick, focusCenter.z);
    glVertex3f(focusCenter.x, focusCenter.y + focusTick, focusCenter.z);
    glVertex3f(focusCenter.x, focusCenter.y, focusCenter.z - focusTick);
    glVertex3f(focusCenter.x, focusCenter.y, focusCenter.z + focusTick);
    glEnd();

    // ---------------------------------------------------------
    // Docking relationship line.
    //
    // VOXEL_0 edit:
    //     faint cyan rail points toward injection chamber.
    //
    // VOXEL_1 edit:
    //     faint orange rail points back toward anchor chamber.
    // ---------------------------------------------------------
    const vec4 railColor =
        editingVoxel1
        ? vec4(1.00f, 0.58f, 0.08f, 0.72f)
        : vec4(0.38f, 1.00f, 0.84f, 0.72f);

    glLineWidth(2.5f);

    glColor4f(
        railColor.r,
        railColor.g,
        railColor.b,
        railColor.a
    );

    glBegin(GL_LINES);
    glVertex3f(anchorCenter.x, anchorCenter.y, anchorCenter.z);
    glVertex3f(injectionCenter.x, injectionCenter.y, injectionCenter.z);
    glEnd();

    // ---------------------------------------------------------
    // Restore normal rendering state.
    // ---------------------------------------------------------
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}
void EuclidRenderer::displayVolumeInjectionRailMarker(
    float thetaRad,
    float phiRad,
    float zs,
    int volumeDim,
    int injectionDx,
    int injectionDy,
    int injectionDz,
    float railT,
    float alphaScale,
    float brushOffsetX,
    float brushOffsetY,
    float brushOffsetZ) {

    if (volumeDim <= 0) return;

    const bool hasInjectionVoxel =
        injectionDx != 0 ||
        injectionDy != 0 ||
        injectionDz != 0;

    if (!hasInjectionVoxel) return;

    railT = std::max(0.0f, std::min(1.0f, railT));
    alphaScale = std::max(0.0f, std::min(1.0f, alphaScale));

    const float chamberStep = static_cast<float>(volumeDim);
    const vec3 anchorCenter(0.0f, 0.0f, 0.0f);

    const vec3 injectionCenter(
        static_cast<float>(injectionDx) * chamberStep,
        static_cast<float>(injectionDy) * chamberStep,
        static_cast<float>(injectionDz) * chamberStep
    );

    const int nonZeroAxes =
        (injectionDx != 0 ? 1 : 0) +
        (injectionDy != 0 ? 1 : 0) +
        (injectionDz != 0 ? 1 : 0);

    const bool straightRail = nonZeroAxes == 1;
    auto straightRailColor = [&]() -> vec4 {

        // +X / -X
        if (injectionDx > 0) {
            return vec4(1.0f, 0.0f, 0.0f, 0.95f);       // red
        }
        if (injectionDx < 0) {
            return vec4(1.0f, 0.25f, 0.65f, 0.95f);     // pink
        }

        // +Y / -Y
        if (injectionDy > 0) {
            return vec4(0.0f, 1.0f, 0.0f, 0.95f);       // green
        }
        if (injectionDy < 0) {
            return vec4(0.0f, 0.48f, 0.12f, 0.95f);    // dark green
        }

        // +Z / -Z
        if (injectionDz > 0) {
            return vec4(0.0f, 0.0f, 1.0f, 0.95f);       // blue
        }

        return vec4(0.0f, 1.0f, 1.0f, 0.95f);           // cyan
    };

    const vec4 railColor =
        straightRail
        ? straightRailColor()
        : vec4(1.0f, 1.0f, 1.0f, 0.95f);               // diagonal = white

    // railT convention:
    //
    //     railT = 0.0 -> rail base starts at injection voxel center
    //     railT = 1.0 -> rail base reaches anchor center
    //
    // VOLUME_1 local offset is applied after the rail placement.
    // This means the brush can ride the rail, then drift locally
    // inside its own injection volume.
    const vec3 railCenter =
        injectionCenter + (anchorCenter - injectionCenter) * railT;

    const vec3 brushLocalOffset(
        brushOffsetX,
        brushOffsetY,
        brushOffsetZ
    );

    const vec3 markerCenter = railCenter + brushLocalOffset;

    glViewport(0, 0, m_window_w, m_window_h);

    // ---------------------------------------------------------
    // Projection matching the existing volume overlays.
    // ---------------------------------------------------------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect =
        (m_window_h > 0)
        ? static_cast<float>(m_window_w) /
        static_cast<float>(m_window_h)
        : 1.0f;

    gluPerspective(m_fov, aspect, 1.0, 2000.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -zs);

    glRotatef(
        phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f, 0.0f, 0.0f
    );

    glRotatef(
        thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f, 1.0f, 0.0f
    );

    // ---------------------------------------------------------
    // Overlay state.
    // ---------------------------------------------------------
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---------------------------------------------------------
    // Full rail: anchor center -> injection voxel center.
    // ---------------------------------------------------------
    glLineWidth(3.0f);
    glColor4f(
        railColor.r,
        railColor.g,
        railColor.b,
        0.32f * alphaScale
    );

    glBegin(GL_LINES);
    glVertex3f(anchorCenter.x, anchorCenter.y, anchorCenter.z);
    glVertex3f(injectionCenter.x, injectionCenter.y, injectionCenter.z);
    glEnd();

    // ---------------------------------------------------------
    // Active rail-depth segment:
    //
    // Shows how far the brush has travelled from the injection
    // voxel toward the anchor.
    // ---------------------------------------------------------
    glLineWidth(5.0f);
    glColor4f(
        railColor.r,
        railColor.g,
        railColor.b,
        0.92f * alphaScale
    );

    glBegin(GL_LINES);
    glVertex3f(injectionCenter.x, injectionCenter.y, injectionCenter.z);
    glVertex3f(markerCenter.x, markerCenter.y, markerCenter.z);
    glEnd();

    // ---------------------------------------------------------
    // Anchor center tick.
    // ---------------------------------------------------------
    const float anchorTick =
        static_cast<float>(volumeDim) * 0.030f;

    glLineWidth(3.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.72f * alphaScale);

    glBegin(GL_LINES);
    glVertex3f(-anchorTick, 0.0f, 0.0f);
    glVertex3f(anchorTick, 0.0f, 0.0f);

    glVertex3f(0.0f, -anchorTick, 0.0f);
    glVertex3f(0.0f, anchorTick, 0.0f);

    glVertex3f(0.0f, 0.0f, -anchorTick);
    glVertex3f(0.0f, 0.0f, anchorTick);
    glEnd();

    // ---------------------------------------------------------
    // Moving rail marker.
    // 
    // Brush local-offset tether.
    //
    // White mini-cross = rail-positioned brush base.
    // Colored large cross = final brush center after local offset.
    // ---------------------------------------------------------

    const bool hasBrushLocalOffset =
        dot(brushLocalOffset, brushLocalOffset) > 0.0001f;

    if (hasBrushLocalOffset) {
        const float baseTick =
            static_cast<float>(volumeDim) * 0.025f;

        glLineWidth(2.5f);
        glColor4f(1.0f, 1.0f, 1.0f, 0.86f * alphaScale);

        glBegin(GL_LINES);

        // Small white cross at railCenter.
        glVertex3f(railCenter.x - baseTick, railCenter.y, railCenter.z);
        glVertex3f(railCenter.x + baseTick, railCenter.y, railCenter.z);

        glVertex3f(railCenter.x, railCenter.y - baseTick, railCenter.z);
        glVertex3f(railCenter.x, railCenter.y + baseTick, railCenter.z);

        glVertex3f(railCenter.x, railCenter.y, railCenter.z - baseTick);
        glVertex3f(railCenter.x, railCenter.y, railCenter.z + baseTick);

        // Tether from rail center to final brush marker.
        glVertex3f(railCenter.x, railCenter.y, railCenter.z);
        glVertex3f(markerCenter.x, markerCenter.y, markerCenter.z);

        glEnd();
    }

    const float markerTick =
        static_cast<float>(volumeDim) * 0.055f;

    glLineWidth(5.0f);
    glColor4f(
        railColor.r,
        railColor.g,
        railColor.b,
        1.0f * alphaScale
    );

    glBegin(GL_LINES);

    // X tick.
    glVertex3f(markerCenter.x - markerTick, markerCenter.y, markerCenter.z);
    glVertex3f(markerCenter.x + markerTick, markerCenter.y, markerCenter.z);

    // Y tick.
    glVertex3f(markerCenter.x, markerCenter.y - markerTick, markerCenter.z);
    glVertex3f(markerCenter.x, markerCenter.y + markerTick, markerCenter.z);

    // Z tick.
    glVertex3f(markerCenter.x, markerCenter.y, markerCenter.z - markerTick);
    glVertex3f(markerCenter.x, markerCenter.y, markerCenter.z + markerTick);

    glEnd();

    // ---------------------------------------------------------
    // Restore state.
    // ---------------------------------------------------------
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}
void EuclidRenderer::displayVolumeOffsetGrid(
    float thetaRad,
    float phiRad,
    float zs,
    int volumeDim,
    VolumeOffsetAxis offsetAxis,
    float normalizedIncrement,
    float normalizedOffsetX,
    float normalizedOffsetY,
    float normalizedOffsetZ,
    const vector<unsigned char>& boundaryMask,
    unsigned int boundaryFaceStride) {

    if (volumeDim <= 0) return;

    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    const float aspect =
        (m_window_h > 0)
        ? static_cast<float>(m_window_w) /
        static_cast<float>(m_window_h)
        : 1.0f;

    // Match the existing volume-space OpenGL overlays.
    gluPerspective(m_fov, aspect, 1.0, 2000.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -zs);
    glRotatef(
        phiRad * 180.0f / static_cast<float>(M_PI),
        1.0f, 0.0f, 0.0f
    );
    glRotatef(
        thetaRad * 180.0f / static_cast<float>(M_PI),
        0.0f, 1.0f, 0.0f
    );

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // The CUDA volume is screen-space, so the grid is rendered
    // as transparent overlay geometry.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // The permanent six-sided voxel-domain cage.
    //
    // Sixteen major sections per dimension gives:
    //     128-volume -> major line every 8 voxels
    //      64-volume -> major line every 4 voxels
    const int boundaryMajorEvery = std::max(1, volumeDim / 16);

    // Draw the faint fixed volume-domain cage first.
    drawVolumeBoundaryCage(
        volumeDim,
        boundaryMajorEvery
    );

    drawVolumeOffsetGridPlane(
        volumeDim,
        offsetAxis,
        normalizedIncrement
    );

    // Redraw only unsafe cage patches in neon orange.
    drawVolumeBoundaryContactPatches(
        volumeDim,
        boundaryMask,
        boundaryFaceStride
    );

    drawVolumeOffsetMarkers(
        volumeDim,
        offsetAxis,
        normalizedOffsetX,
        normalizedOffsetY,
        normalizedOffsetZ
    );

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}

GLuint EuclidRenderer::_compileProgram(
    const char* vsource, const char* fsource,
    const char* secondaryAttributeName, const char* programLabel) {

    const char* safeLabel =
        (programLabel && programLabel[0] != '\0')
        ? programLabel
        : "unnamed program";

    if (!vsource || !fsource) {
        printf(
            "[EuclidRenderer] Cannot compile %s: "
            "shader source is null.\n",
            safeLabel
        );

        return 0;
    }

    GLuint vertexObject =
        glCreateShader(
            GL_VERTEX_SHADER
        );

    GLuint fragmentObject =
        glCreateShader(
            GL_FRAGMENT_SHADER
        );

    if (!vertexObject || !fragmentObject) {
        printf(
            "[EuclidRenderer] Cannot compile %s: "
            "glCreateShader failed.\n",
            safeLabel
        );

        if (vertexObject) {
            glDeleteShader(vertexObject);
        }

        if (fragmentObject) {
            glDeleteShader(fragmentObject);
        }

        return 0;
    }

    glShaderSource(
        vertexObject,
        1,
        &vsource,
        nullptr
    );

    glShaderSource(
        fragmentObject,
        1,
        &fsource,
        nullptr
    );

    glCompileShader(vertexObject);
    glCompileShader(fragmentObject);

    char vertexLabel[128];
    char fragmentLabel[128];

    snprintf(
        vertexLabel,
        sizeof(vertexLabel),
        "%s vertex",
        safeLabel
    );

    snprintf(
        fragmentLabel,
        sizeof(fragmentLabel),
        "%s fragment",
        safeLabel
    );

    const bool vertexCompiled =
        checkShader(
            vertexObject,
            vertexLabel
        );

    const bool fragmentCompiled =
        checkShader(
            fragmentObject,
            fragmentLabel
        );

    if (!vertexCompiled ||
        !fragmentCompiled) {

        glDeleteShader(vertexObject);
        glDeleteShader(fragmentObject);

        return 0;
    }

    GLuint program =
        glCreateProgram();

    if (!program) {
        printf(
            "[EuclidRenderer] Cannot link %s: "
            "glCreateProgram failed.\n",
            safeLabel
        );

        glDeleteShader(vertexObject);
        glDeleteShader(fragmentObject);

        return 0;
    }

    glAttachShader(
        program,
        vertexObject
    );

    glAttachShader(
        program,
        fragmentObject
    );

    // Attribute 0 is consistently reserved for vertex position.
    glBindAttribLocation(
        program,
        0,
        "position"
    );

    // Particle shaders use "radius".
    // The OBJ shader uses "normal".
    if (secondaryAttributeName &&
        secondaryAttributeName[0] != '\0') {

        glBindAttribLocation(
            program,
            1,
            secondaryAttributeName
        );
    }

    glLinkProgram(program);

    GLint linkSuccess = GL_FALSE;

    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &linkSuccess
    );

    if (linkSuccess != GL_TRUE) {
        GLint logLength = 0;

        glGetProgramiv(
            program,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (logLength < 1) {
            logLength = 1;
        }

        std::vector<char> log(
            static_cast<size_t>(logLength) +
            1u,
            '\0'
        );

        glGetProgramInfoLog(
            program,
            logLength,
            nullptr,
            log.data()
        );

        printf(
            "[EuclidRenderer] Failed to link %s:\n%s\n",
            safeLabel,
            log.data()
        );

        glDetachShader(
            program,
            vertexObject
        );

        glDetachShader(
            program,
            fragmentObject
        );

        glDeleteShader(vertexObject);
        glDeleteShader(fragmentObject);
        glDeleteProgram(program);

        return 0;
    }

    // The linked program retains the compiled shader code.
    // The standalone shader objects are no longer needed.
    glDetachShader(
        program,
        vertexObject
    );

    glDetachShader(
        program,
        fragmentObject
    );

    glDeleteShader(vertexObject);
    glDeleteShader(fragmentObject);

    printf(
        "[EuclidRenderer] Shader program ready: %s "
        "(program=%u, attribute1=%s)\n",
        safeLabel,
        static_cast<unsigned int>(program),
        secondaryAttributeName
        ? secondaryAttributeName
        : "none"
    );

    return program;
}

void EuclidRenderer::_initialize() {
    assert(!m_bInitialized);

    if (!m_radVBO) {
        glGenBuffers(1, &m_radVBO);
    }

    if (!m_particlePosVBO) {
        glGenBuffers(1, &m_particlePosVBO);
    }

    if (!m_particleRadVBO) {
        glGenBuffers(1, &m_particleRadVBO);
    }

    if (!m_particleColorVBO) {
        glGenBuffers(1, &m_particleColorVBO);
    }

    m_bInitialized = true;
}
void EuclidRenderer::_initGL() {

    // ---------------------------------------------------------
    // Particle sphere shader.
    // ---------------------------------------------------------
    m_program0 =
        _compileProgram(
            vertexShader,
            spherePixelShader,
            "radius",
            "particle sphere"
        );

    // ---------------------------------------------------------
    // Particle selection/highlight shader.
    // ---------------------------------------------------------
    m_program1 =
        _compileProgram(
            lightVertShader,
            lightFragShader,
            "radius",
            "particle highlight"
        );

    // ---------------------------------------------------------
    // OBJ triangle mesh shader.
    // ---------------------------------------------------------
    m_meshProgram =
        _compileProgram(
            modelVertexShader,
            modelFragmentShader,
            "normal",
            "OBJ mesh"
        );

    m_meshColorLocation = -1;
    m_meshLightDirLocation = -1;
    m_meshAmbientLocation = -1;

    if (m_meshProgram) {

        m_meshColorLocation =
            glGetUniformLocation(
                m_meshProgram,
                "uColor"
            );

        m_meshLightDirLocation =
            glGetUniformLocation(
                m_meshProgram,
                "uLightDir"
            );

        m_meshAmbientLocation =
            glGetUniformLocation(
                m_meshProgram,
                "uAmbient"
            );

        if (m_meshColorLocation < 0) {
            printf(
                "[EuclidRenderer] WARNING: OBJ mesh shader "
                "uniform uColor was not found.\n"
            );
        }

        if (m_meshLightDirLocation < 0) {
            printf(
                "[EuclidRenderer] WARNING: OBJ mesh shader "
                "uniform uLightDir was not found.\n"
            );
        }

        if (m_meshAmbientLocation < 0) {
            printf(
                "[EuclidRenderer] WARNING: OBJ mesh shader "
                "uniform uAmbient was not found.\n"
            );
        }

        // Establish safe defaults. These values remain stored
        // in the program until the future mesh draw path changes them.
        glUseProgram(
            m_meshProgram
        );

        if (m_meshColorLocation >= 0) {
            glUniform4f(
                m_meshColorLocation,
                1.0f,
                0.0f,
                0.0f,
                1.0f
            );
        }

        if (m_meshLightDirLocation >= 0) {
            glUniform3f(
                m_meshLightDirLocation,
                0.577f,
                0.577f,
                0.577f
            );
        }

        if (m_meshAmbientLocation >= 0) {
            glUniform1f(
                m_meshAmbientLocation,
                0.15f
            );
        }

        glUseProgram(0);

        printf(
            "[EuclidRenderer] OBJ mesh shader uniforms: "
            "color=%d lightDir=%d ambient=%d\n",
            m_meshColorLocation,
            m_meshLightDirLocation,
            m_meshAmbientLocation
        );
    }

    glClampColorARB(
        GL_CLAMP_VERTEX_COLOR_ARB,
        GL_FALSE
    );

    glClampColorARB(
        GL_CLAMP_FRAGMENT_COLOR_ARB,
        GL_FALSE
    );
}
void EuclidRenderer::_drawPoints(bool useColorBuffer) {
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, m_vbo);

    glVertexPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_VERTEX_ARRAY);

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, m_radVBO);

    glEnableVertexAttribArrayARB(1);
    glVertexAttribPointerARB(1, 1, GL_FLOAT, GL_FALSE, 0, 0);

    const bool useColors = useColorBuffer && (m_colorVBO != 0);

    if (useColors) {
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, m_colorVBO);

        glColorPointer(4, GL_FLOAT, 0, 0);
        glEnableClientState(GL_COLOR_ARRAY);
    }

    glDrawArrays(GL_POINTS, 0, m_numParticles);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableVertexAttribArrayARB(1);

    if (useColors) {
        glDisableClientState(GL_COLOR_ARRAY);
    }

    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void EuclidRenderer::display(DisplayMode mode) {
    switch (mode) {

    case PARTICLE_POINTS: {
        glColor3f(1, 1, 1);
        glPointSize(m_pointSize);
        _drawPoints();
        break;
    }
    default:
    case PARTICLE_SPHERES: {
        glEnable(GL_POINT_SPRITE_ARB);
        glTexEnvi(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, GL_TRUE);

        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_NV);
        glDepthMask(GL_TRUE);

        glEnable(GL_DEPTH_TEST);

        glUseProgram(m_program0);
        glUniform1f(glGetUniformLocation(m_program0, "pointScale"),
            m_window_h / tanf(m_fov * 0.5f * (float)M_PI / 180.0f));

        vector<float> radii(m_numParticles);
        for (int i = 0; i < m_numParticles; i++) {

            radii[i] = m_rad[i];
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_radVBO);

        glBufferData(GL_ARRAY_BUFFER,
            m_numParticles * sizeof(float),
            radii.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glColor3f(1, 1, 1);
        _drawPoints();

        if (m_particleHighlighted && m_program1) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glUseProgram(m_program1);
            glUniform1f(
                glGetUniformLocation(m_program1, "pointScale"),
                m_window_h / tanf(m_fov * 0.5f * (float)M_PI / 180.0f)
            );

            vector<float> highlightRadii(m_numParticles);
            for (int i = 0; i < m_numParticles; i++) {
                highlightRadii[i] = m_rad[i] * m_particleHighlightScale;
            }

            glBindBuffer(GL_ARRAY_BUFFER, m_radVBO);
            glBufferData(
                GL_ARRAY_BUFFER,
                m_numParticles * sizeof(float),
                highlightRadii.data(),
                GL_STATIC_DRAW
            );
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glColor4f(1.0f, 0.35f, 0.05f, 0.85f);
            _drawPoints(false);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glUseProgram(0);
        glDisable(GL_POINT_SPRITE_ARB);

        break;
    }

    }
}
void EuclidRenderer::displayVolumeTexture() {
    if (!m_tex) return;

    glViewport(0, 0, m_window_w, m_window_h);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, m_window_w, m_window_h, 0.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);

    if (m_pbo) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo);
    }

    glBindTexture(GL_TEXTURE_2D, m_tex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        m_window_w,
        m_window_h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glEnable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, static_cast<float>(m_window_h));

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(static_cast<float>(m_window_w), static_cast<float>(m_window_h));

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(static_cast<float>(m_window_w), 0.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    if (m_pbo) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
}

bool EuclidRenderer::loadParticleMeshOBJ(const char* filename) {
    if (!filename || filename[0] == '\0') {
        clearParticleMeshOBJ();
        return false;
    }

    if (m_particleMeshLoaded && m_particleMeshPath == filename) {
        return true;
    }

    ifstream in(filename);

    if (!in.is_open()) {
        clearParticleMeshOBJ();
        printf("[EuclidRenderer3D] Mesh OBJ unavailable: %s\n", filename);
        return false;
    }

    vector<vec3> sourceVerts;
    vector<vec3> sourceNorms;

    vector<vec3> outVerts;
    vector<vec3> outNorms;

    string line;

    while (getline(in, line)) {
        if (line.empty()) continue;

        istringstream ss(line);
        string tag;
        ss >> tag;

        if (tag == "v") {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            if (!(ss >> x >> y >> z))
                continue;

            sourceVerts.push_back(vec3(x, y, z));
        }
        else if (tag == "vn") {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            if (!(ss >> x >> y >> z))
                continue;

            vec3 normal(x, y, z);
            const float lengthSquared = dot(normal, normal);
            if (lengthSquared > 1.0e-12f) {
                normal /= sqrtf(lengthSquared);
            }
            else {
                normal = vec3(0.0f);
            }

            sourceNorms.push_back(normal);
        }
        else if (tag == "f") {
            string tok[3];
            if (!(ss >> tok[0] >> tok[1] >> tok[2]))
                continue;

            vec3 triangleVerts[3];
            vec3 triangleNorms[3];

            bool triangleValid = true;

            for (int i = 0; i < 3; i++) {
                int vIdx = 0;
                int nIdx = 0;

                parseOBJFaceToken(tok[i], vIdx, nIdx);

                if (vIdx <= 0 || vIdx > static_cast<int>(sourceVerts.size())) {
                    triangleValid = false;
                    break;
                }

                triangleVerts[i] = sourceVerts[static_cast<size_t>(vIdx - 1)];

                if (nIdx > 0 && nIdx <= static_cast<int>(sourceNorms.size())) {
                    triangleNorms[i] = sourceNorms[static_cast<size_t>(nIdx - 1)];
                }
                else {
                    triangleNorms[i] = vec3(0.0f);
                }
            }

            if (!triangleValid)
                continue;

            vec3 faceNormal = cross(
                triangleVerts[1] - triangleVerts[0],
                triangleVerts[2] - triangleVerts[0]
            );

            const float faceLengthSquared = dot(faceNormal, faceNormal);
            if (faceLengthSquared > 1.0e-12f) {
                faceNormal /= sqrtf(faceLengthSquared);
            }
            else {
                faceNormal = vec3(0.0f, 1.0f, 0.0f);
            }

            for (int i = 0; i < 3; i++) {
                vec3 normal = triangleNorms[i];
                const float normalLengthSquared = dot(normal, normal);

                if (normalLengthSquared > 1.0e-12f) {
                    normal /= sqrtf(normalLengthSquared);
                }
                else {
                    normal = faceNormal;
                }
                outVerts.push_back(triangleVerts[i]);

                outNorms.push_back(normal);
            }
        }
    }

    in.close();

    if (outVerts.empty() ||
        outVerts.size() != outNorms.size() ||
        (outVerts.size() % 3u) != 0u) {

        printf("[EuclidRenderer] Mesh OBJ load failed: "
            "no valid triangle stream in %s\n",
            filename
        );

        return false;
    }

    // Parsing succeeded. The old mesh can now be safely replaced.
    clearParticleMeshOBJ();

    m_particleMeshVerts = move(outVerts);
    m_particleMeshNorms = move(outNorms);

    computeParticleMeshBounds();

    if (!uploadParticleMeshVBO()) {
        printf(
            "[EuclidRenderer] Mesh OBJ load failed: "
            "GPU upload failed for %s\n",
            filename
        );

        clearParticleMeshOBJ();
        return false;
    }

    m_particleMeshPath =filename;

    m_particleMeshLoaded = true;

    printf(
        "[EuclidRenderer] Loaded particle mesh OBJ: "
        "%s vertices=%d triangles=%d "
        "maxExtent=%.6f\n",
        filename,
        static_cast<int>(
            m_particleMeshVertexCount
            ),
        static_cast<int>(
            m_particleMeshVertexCount / 3
            ),
        m_particleMeshMaxExtent
    );

    return true;
}
bool EuclidRenderer::particleIntersectsSlice(const ParticleProxy3D& p, int slice) const {
    const float s = kWorkspaceHalfBox;
    const int sliceRange = 64;

    const float z =
        (static_cast<float>(slice) / static_cast<float>(sliceRange)) * s;

    const float sliceThickness = s / static_cast<float>(sliceRange);

    return fabs(p.position.z - z) <= sliceThickness;
}
bool EuclidRenderer::checkShader(
    GLuint shader,
    const char* label) {
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        printf("%s shader compile failed:\n%s\n", label, log);
        return false;
    }

    return true;
}

void EuclidRenderer::drawParticleMeshOBJ(
    const ParticleProxy3D& p,
    const float4& color,
    bool selected) {

    if (!hasParticleMeshOBJ()) return;
    if (!m_meshProgram) return;
    if (m_particleMeshMaxExtent <= 0.0f ||  p.radius <= 0.0f) return;

    GLint oldProgram = 0;

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &oldProgram
    );

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_POLYGON_BIT |
        GL_DEPTH_BUFFER_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_CURRENT_BIT |
        GL_LIGHTING_BIT |
        GL_LINE_BIT
    );

    glPushClientAttrib(
        GL_CLIENT_VERTEX_ARRAY_BIT
    );

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POINT_SPRITE_ARB);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float targetDiameter = 2.0f *p.radius;
    const float meshScale = targetDiameter / m_particleMeshMaxExtent;

    glPushMatrix();

    // Attach the OBJ to the SINGLE_PARTICLE anchor.
    glTranslatef(
        p.position.x,
        p.position.y,
        p.position.z
    );

    // Preserve the authored OBJ offset. Do not subtract
    // m_particleMeshCenter from the vertices.
    glScalef(
        meshScale,
        meshScale,
        meshScale
    );

    glUseProgram(m_meshProgram);

    if (m_meshColorLocation >= 0) {
        glUniform4f(
            m_meshColorLocation,
            color.x,
            color.y,
            color.z,
            color.w
        );
    }

    if (m_meshLightDirLocation >= 0) {
        // Same diagonal lighting direction used by
        // the particle sphere shader.
        glUniform3f(
            m_meshLightDirLocation,
            0.577f,
            0.577f,
            0.577f
        );
    }

    if (m_meshAmbientLocation >= 0) {
        // Zero matches the particle sphere's pure
        // directional diffuse shading.
        glUniform1f(m_meshAmbientLocation, 0.0f);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_particleMeshVBO);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT,GL_FALSE, sizeof(ParticleMeshVertex),
        reinterpret_cast<const GLvoid*>(offsetof( ParticleMeshVertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleMeshVertex),
        reinterpret_cast<const GLvoid*>(offsetof(ParticleMeshVertex, normal)));

    // ---------------------------------------------------------
    // Filled shaded mesh pass.
    // ---------------------------------------------------------
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glDrawArrays(GL_TRIANGLES, 0, m_particleMeshVertexCount);

    // ---------------------------------------------------------
    // Selected mesh outline.
    // ---------------------------------------------------------
    if (selected) {

        if (m_meshColorLocation >= 0) {
            glUniform4f(
                m_meshColorLocation,
                1.0f,
                0.55f,
                0.06f,
                0.95f
            );
        }

        // An ambient value of 1 makes the outline unlit,
        // preserving a consistent neon-orange selection color.
        if (m_meshAmbientLocation >= 0) {
            glUniform1f(m_meshAmbientLocation, 1.0f);
        }

        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glLineWidth(1.5f);
        glDepthFunc(GL_LEQUAL);
        glDrawArrays(GL_TRIANGLES, 0, m_particleMeshVertexCount);

        glDepthFunc(GL_LESS);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(static_cast<GLuint>(oldProgram));

    glPopMatrix();

    glPopClientAttrib();
    glPopAttrib();
}
void EuclidRenderer::drawParticleSphere(
    GLuint program,
    const float pos[4],
    float radius,
    const float color[4],
    bool emissiveBlend) {

    if (!program) return;

    glUseProgram(program);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SPRITE_ARB);
    glTexEnvi(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, GL_TRUE);

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_NV);

    if (emissiveBlend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
    }
    else {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    const float pointScale =
        m_window_h / tanf(
            m_fov * 0.5f *
            static_cast<float>(M_PI) / 180.0f
        );

    GLint pointScaleLoc =
        glGetUniformLocation(program, "pointScale");

    if (pointScaleLoc >= 0) {
        glUniform1f(pointScaleLoc, pointScale);
    }

    // Position VBO.
    glBindBuffer(GL_ARRAY_BUFFER, m_particlePosVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float) * 4,
        pos,
        GL_DYNAMIC_DRAW
    );

    glVertexPointer(4, GL_FLOAT, 0, nullptr);
    glEnableClientState(GL_VERTEX_ARRAY);

    // Radius VBO.
    glBindBuffer(GL_ARRAY_BUFFER, m_particleRadVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float),
        &radius,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArrayARB(1);
    glVertexAttribPointerARB(
        1,
        1,
        GL_FLOAT,
        GL_FALSE,
        0,
        nullptr
    );

    // Color VBO.
    glBindBuffer(GL_ARRAY_BUFFER, m_particleColorVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float) * 4,
        color,
        GL_DYNAMIC_DRAW
    );

    glColorPointer(4, GL_FLOAT, 0, nullptr);
    glEnableClientState(GL_COLOR_ARRAY);

    glDrawArrays(GL_POINTS, 0, 1);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableVertexAttribArrayARB(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_POINT_SPRITE_ARB);

    glUseProgram(0);
}
void EuclidRenderer::computeParticleMeshBounds() {
    if (m_particleMeshVerts.empty()) {
        m_particleMeshMin = vec3(0.0f);
        m_particleMeshMax = vec3(0.0f);
        m_particleMeshCenter = vec3(0.0f);
        m_particleMeshMaxExtent = 1.0f;
        return;
    }

    vec3 mn = m_particleMeshVerts[0];
    vec3 mx = m_particleMeshVerts[0];

    for (const vec3& v : m_particleMeshVerts) {
        mn.x = std::min(mn.x, v.x);
        mn.y = std::min(mn.y, v.y);
        mn.z = std::min(mn.z, v.z);

        mx.x = std::max(mx.x, v.x);
        mx.y = std::max(mx.y, v.y);
        mx.z = std::max(mx.z, v.z);
    }

    m_particleMeshMin = mn;
    m_particleMeshMax = mx;
    m_particleMeshCenter = 0.5f * (mn + mx);

    const glm::vec3 extent = mx - mn;

    m_particleMeshMaxExtent = std::max(
        0.000001f,
        std::max(extent.x, std::max(extent.y, extent.z))
    );
}

GLuint EuclidRenderer::ensureMvpTexture(
    const vitru::StaticParticleAsset& asset) {
    if (!asset.texture.valid()) return 0;

    std::uint64_t hash = 1469598103934665603ull;
    for (const vitru::ColorRGBA8& pixel : asset.texture.pixels) {
        const std::uint32_t packed =
            static_cast<std::uint32_t>(pixel.r) |
            (static_cast<std::uint32_t>(pixel.g) << 8u) |
            (static_cast<std::uint32_t>(pixel.b) << 16u) |
            (static_cast<std::uint32_t>(pixel.a) << 24u);
        hash ^= packed;
        hash *= 1099511628211ull;
    }

    MvpTextureResource& resource = m_mvpTextures[asset.id];
    const bool upload = resource.texture == 0 ||
        resource.width != asset.texture.width ||
        resource.height != asset.texture.height ||
        resource.contentHash != hash;

    if (!resource.texture) glGenTextures(1, &resource.texture);
    if (!upload) return resource.texture;

    glBindTexture(GL_TEXTURE_2D, resource.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(asset.texture.width),
        static_cast<GLsizei>(asset.texture.height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        asset.texture.pixels.data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);

    resource.width = asset.texture.width;
    resource.height = asset.texture.height;
    resource.contentHash = hash;
    return resource.texture;
}

void EuclidRenderer::releaseMvpTextures() {
    for (auto& pair : m_mvpTextures) {
        if (pair.second.texture) glDeleteTextures(1, &pair.second.texture);
    }
    m_mvpTextures.clear();
}

void EuclidRenderer::drawMvpStaticMesh(
    const vitru::StaticParticleAsset& asset) {
    if (asset.mesh.positions.empty()) return;

    const GLuint texture = ensureMvpTexture(asset);
    if (texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else {
        glDisable(GL_TEXTURE_2D);
        glColor4ub(
            asset.material.baseColor.r,
            asset.material.baseColor.g,
            asset.material.baseColor.b,
            asset.material.baseColor.a
        );
    }

    auto emitVertex = [&](std::uint32_t index) {
        if (index >= asset.mesh.positions.size()) return;
        if (index < asset.mesh.normals.size()) {
            const vitru::Vec3& normal = asset.mesh.normals[index];
            glNormal3f(normal.x, normal.y, normal.z);
        }
        if (index < asset.mesh.uvs.size()) {
            const vitru::Vec2& uv = asset.mesh.uvs[index];
            glTexCoord2f(uv.x, 1.0f - uv.y);
        }
        const vitru::Vec3& position = asset.mesh.positions[index];
        glVertex3f(position.x, position.y, position.z);
    };

    glBegin(GL_TRIANGLES);
    if (!asset.mesh.indices.empty()) {
        for (std::uint32_t index : asset.mesh.indices) emitVertex(index);
    }
    else {
        for (std::uint32_t index = 0;
            index < static_cast<std::uint32_t>(asset.mesh.positions.size());
            ++index) emitVertex(index);
    }
    glEnd();

    if (texture) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
}

void EuclidRenderer::displayTextureMap2D(
    const vitru::StaticParticleAsset& asset,
    bool showUvOverlay,
    int viewportW,
    int viewportH) {
    if (viewportW <= 0 || viewportH <= 0 || !asset.texture.valid()) return;

    const float availableLeft = static_cast<float>(viewportW) * 0.34f;
    const float availableRight = static_cast<float>(viewportW) * 0.95f;
    const float availableTop = static_cast<float>(viewportH) * 0.10f;
    const float availableBottom = static_cast<float>(viewportH) * 0.90f;
    const float canvasSize = std::min(
        availableRight - availableLeft,
        availableBottom - availableTop
    );
    const float left = availableLeft +
        ((availableRight - availableLeft) - canvasSize) * 0.5f;
    const float top = availableTop +
        ((availableBottom - availableTop) - canvasSize) * 0.5f;
    const float right = left + canvasSize;
    const float bottom = top + canvasSize;

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, viewportW, viewportH, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float checker = canvasSize / 16.0f;
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (int row = 0; row < 16; ++row) {
        for (int column = 0; column < 16; ++column) {
            const float shade = ((row + column) & 1) ? 0.16f : 0.23f;
            glColor4f(shade, shade, shade, 1.0f);
            const float x0 = left + column * checker;
            const float y0 = top + row * checker;
            glVertex2f(x0, y0);
            glVertex2f(x0 + checker, y0);
            glVertex2f(x0 + checker, y0 + checker);
            glVertex2f(x0, y0 + checker);
        }
    }
    glEnd();

    const GLuint texture = ensureMvpTexture(asset);
    if (texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(left, top);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(right, top);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(right, bottom);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(left, bottom);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    if (showUvOverlay && !asset.mesh.uvs.empty()) {
        glColor4f(0.2f, 1.0f, 0.82f, 0.72f);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        const std::vector<std::uint32_t>& indices = asset.mesh.indices;
        const std::size_t count = indices.empty()
            ? asset.mesh.positions.size() : indices.size();
        for (std::size_t triangle = 0; triangle + 2u < count; triangle += 3u) {
            std::uint32_t corners[3] = {
                indices.empty() ? static_cast<std::uint32_t>(triangle) : indices[triangle],
                indices.empty() ? static_cast<std::uint32_t>(triangle + 1u) : indices[triangle + 1u],
                indices.empty() ? static_cast<std::uint32_t>(triangle + 2u) : indices[triangle + 2u]
            };
            for (int edge = 0; edge < 3; ++edge) {
                const vitru::Vec2& a = asset.mesh.uvs[corners[edge]];
                const vitru::Vec2& b = asset.mesh.uvs[corners[(edge + 1) % 3]];
                glVertex2f(left + a.x * canvasSize, top + a.y * canvasSize);
                glVertex2f(left + b.x * canvasSize, top + b.y * canvasSize);
            }
        }
        glEnd();
    }

    glColor4f(0.4f, 1.0f, 0.82f, 0.9f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void EuclidRenderer::displayStaticParticlePreview(
    const vitru::StaticParticleAsset& asset,
    float rotationDegrees) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();
    glRotatef(rotationDegrees, 0.0f, 1.0f, 0.0f);
    drawMvpStaticMesh(asset);
    glPopMatrix();
    glPopAttrib();
}

void EuclidRenderer::displayKinematicParticle(
    const vitru::KinematicParticleAsset& asset,
    const vitru::SandboxInstance* instance,
    bool showNodes) {
    if (!asset.runtimeReady || asset.nodes.empty()) return;

    auto findStatic = [&](vitru::AssetId id) -> const vitru::StaticParticleAsset* {
        for (const vitru::StaticParticleAsset& candidate : asset.embeddedStaticAssets) {
            if (candidate.id == id) return &candidate;
        }
        return nullptr;
    };

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();

    if (instance) {
        glTranslatef(
            instance->rootTransform.position.x,
            instance->rootTransform.position.y,
            instance->rootTransform.position.z
        );
        glRotatef(instance->rootTransform.rotationDegrees.z, 0.0f, 0.0f, 1.0f);
        glRotatef(instance->rootTransform.rotationDegrees.y, 0.0f, 1.0f, 0.0f);
        glRotatef(instance->rootTransform.rotationDegrees.x, 1.0f, 0.0f, 0.0f);
    }

    std::function<void(vitru::NodeId)> drawNode = [&](vitru::NodeId nodeId) {
        std::size_t nodeIndex = 0;
        const vitru::CompiledNode* compiled = nullptr;
        for (; nodeIndex < asset.nodes.size(); ++nodeIndex) {
            if (asset.nodes[nodeIndex].source.id == nodeId) {
                compiled = &asset.nodes[nodeIndex];
                break;
            }
        }
        if (!compiled) return;

        const vitru::AssemblyNode& node = compiled->source;
        glPushMatrix();
        glTranslatef(
            node.localTransform.position.x,
            node.localTransform.position.y,
            node.localTransform.position.z
        );
        glRotatef(node.localTransform.rotationDegrees.z, 0.0f, 0.0f, 1.0f);
        glRotatef(node.localTransform.rotationDegrees.y, 0.0f, 1.0f, 0.0f);
        glRotatef(node.localTransform.rotationDegrees.x, 1.0f, 0.0f, 0.0f);
        glScalef(
            node.localTransform.scale.x,
            node.localTransform.scale.y,
            node.localTransform.scale.z
        );

        if (node.type == vitru::AssemblyNodeType::JOINT && instance &&
            nodeIndex < instance->jointValuesDegrees.size()) {
            glRotatef(
                instance->jointValuesDegrees[nodeIndex],
                node.jointAxis.x,
                node.jointAxis.y,
                node.jointAxis.z
            );
        }

        if (node.type == vitru::AssemblyNodeType::MESH) {
            const vitru::StaticParticleAsset* staticAsset = findStatic(node.staticAssetId);
            if (staticAsset) drawMvpStaticMesh(*staticAsset);
        }
        else if (showNodes && node.type == vitru::AssemblyNodeType::JOINT) {
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.15f, 0.45f, 1.0f, 0.95f);
            glutWireSphere(0.16, 12, 8);
            glBegin(GL_LINES);
            glVertex3f(0.0f, 0.0f, 0.0f);
            glVertex3f(
                node.jointAxis.x * 0.5f,
                node.jointAxis.y * 0.5f,
                node.jointAxis.z * 0.5f
            );
            glEnd();
        }
        else if (showNodes && node.type == vitru::AssemblyNodeType::INTERACTION) {
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.15f, 1.0f, 0.35f, 0.95f);
            glutSolidSphere(0.12, 10, 8);
        }

        for (const vitru::CompiledNode& child : asset.nodes) {
            if (child.source.parentId == node.id) drawNode(child.source.id);
        }
        glPopMatrix();
    };

    drawNode(asset.rootNodeId);
    glPopMatrix();
    glPopAttrib();
}

void EuclidRenderer::displayLinkedAssembly(
    const vitru::ProjectAssetRepository& repository,
    const vitru::LinkedAssembly& assembly,
    float previewTimeSeconds,
    bool previewAnimation) {
    const vitru::BakeResult preview =
        repository.validateAndBake(assembly.id, assembly.name + " Preview");
    if (preview.success) {
        vitru::SandboxInstance previewInstance;
        const vitru::SandboxInstance* instance = nullptr;
        if (previewAnimation) {
            previewInstance.spawned = true;
            previewInstance.assetId = preview.asset.id;
            previewInstance.jointValuesDegrees.assign(preview.asset.nodes.size(), 0.0f);
            const vitru::AnimationClip* clip = nullptr;
            for (const vitru::AnimationClip& candidate : preview.asset.clips) {
                if (!candidate.tracks.empty()) { clip = &candidate; break; }
            }
            if (clip) {
                float clipTime = previewTimeSeconds * clip->playbackSpeed;
                if (clip->looping && clip->durationSeconds > 0.0f)
                    clipTime = std::fmod(clipTime, clip->durationSeconds);
                else clipTime = std::min(clipTime, clip->durationSeconds);
                for (const vitru::JointAnimationTrack& track : clip->tracks) {
                    if (track.keyframes.empty()) continue;
                    float value = track.keyframes.front().rotationDegrees;
                    if (clipTime >= track.keyframes.back().timeSeconds) {
                        value = track.keyframes.back().rotationDegrees;
                    }
                    else {
                        for (std::size_t key = 1; key < track.keyframes.size(); ++key) {
                            if (clipTime > track.keyframes[key].timeSeconds) continue;
                            const vitru::JointKeyframe& a = track.keyframes[key - 1];
                            const vitru::JointKeyframe& b = track.keyframes[key];
                            const float span = b.timeSeconds - a.timeSeconds;
                            const float alpha = span > 0.0f
                                ? (clipTime - a.timeSeconds) / span : 0.0f;
                            value = a.rotationDegrees +
                                (b.rotationDegrees - a.rotationDegrees) * alpha;
                            break;
                        }
                    }
                    for (std::size_t nodeIndex = 0;
                        nodeIndex < preview.asset.nodes.size(); ++nodeIndex) {
                        if (preview.asset.nodes[nodeIndex].source.id == track.jointNodeId) {
                            previewInstance.jointValuesDegrees[nodeIndex] = value;
                            break;
                        }
                    }
                }
                previewInstance.activeClip = clip->name;
                previewInstance.animationTimeSeconds = clipTime;
                instance = &previewInstance;
            }
        }
        displayKinematicParticle(preview.asset, instance, true);
        return;
    }

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    for (const vitru::AssemblyNode& node : assembly.nodes) {
        glPushMatrix();
        glTranslatef(
            node.localTransform.position.x,
            node.localTransform.position.y,
            node.localTransform.position.z
        );
        if (node.type == vitru::AssemblyNodeType::MESH) {
            const vitru::StaticParticleAsset* asset =
                repository.findStaticParticle(node.staticAssetId);
            if (asset) drawMvpStaticMesh(*asset);
        }
        else if (node.type == vitru::AssemblyNodeType::JOINT) {
            glColor4f(0.15f, 0.45f, 1.0f, 1.0f);
            glutWireSphere(0.16, 12, 8);
        }
        else {
            glColor4f(0.15f, 1.0f, 0.35f, 1.0f);
            glutSolidSphere(0.12, 10, 8);
        }
        glPopMatrix();
    }
    glPopAttrib();
}

void EuclidRenderer::displaySandbox(const vitru::SandboxRuntime& sandbox) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    glColor4f(0.16f, 0.20f, 0.19f, 1.0f);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-20.0f, -0.5f, -20.0f);
    glVertex3f(-20.0f, -0.5f, 10.0f);
    glVertex3f(20.0f, -0.5f, 10.0f);
    glVertex3f(20.0f, -0.5f, -20.0f);
    glEnd();

    const float obstaclePositions[][3] = {
        { -3.5f, 0.0f, -4.0f },
        { 3.5f, 0.25f, -6.0f },
        { 0.0f, 0.0f, -14.0f }
    };
    for (const auto& obstacle : obstaclePositions) {
        glPushMatrix();
        glTranslatef(obstacle[0], obstacle[1], obstacle[2]);
        glColor4f(0.35f, 0.38f, 0.42f, 1.0f);
        glutSolidCube(1.0);
        glPopMatrix();
    }

    for (const vitru::SandboxTarget& target : sandbox.targets()) {
        glPushMatrix();
        glTranslatef(target.position.x, target.position.y, target.position.z);
        if (target.hit) glColor4f(1.0f, 0.35f, 0.08f, 0.45f);
        else glColor4f(1.0f, 0.18f, 0.12f, 1.0f);
        glutWireSphere(target.radius, 16, 10);
        glPopMatrix();
    }

    for (const vitru::SandboxProjectile& projectile : sandbox.projectiles()) {
        glPushMatrix();
        glTranslatef(projectile.position.x, projectile.position.y, projectile.position.z);
        glColor4f(1.0f, 0.85f, 0.18f, 1.0f);
        glutSolidSphere(0.10, 10, 8);
        glPopMatrix();
    }
    glPopAttrib();

    if (sandbox.activeAsset() && sandbox.instance().spawned) {
        displayKinematicParticle(
            *sandbox.activeAsset(),
            &sandbox.instance(),
            false
        );
    }
}
