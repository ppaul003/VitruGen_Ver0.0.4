#include "TheTesseract2D.h"

#include <cmath>

Tesseract2D::Tesseract2D() {
    for (int i = 0; i < 16; ++i) {
        m_modelView[i] = 0.0f;
    }

    m_modelView[0] = 1.0f;
    m_modelView[5] = 1.0f;
    m_modelView[10] = 1.0f;
    m_modelView[15] = 1.0f;

    m_tesseractSt = MENU_PREVIEW;
    m_transitionState = TRANS_NONE;

    m_previewRotation = 0.0f;
    m_sliceAnimation = 0.5f;
    m_gridPulse = 0.0f;
    m_gridAlpha = 1.0f;
    m_transitionT = 0.0f;
    m_cameraFocusRequested = false;
}

Tesseract2D::~Tesseract2D() {}

float Tesseract2D::smoothStep01(float t) {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;

    return t * t * (3.0f - 2.0f * t);
}

float Tesseract2D::lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

void Tesseract2D::beginEnter2DTransition(float timeS) {
    m_transitionState = TRANS_TO_2D_ORIENT;
    m_transitionStartTime = timeS;
    m_transitionDuration = 0.75f;
    m_transitionT = 0.0f;

    m_startPreviewRotation = std::fmod(m_previewRotation, 360.0f);
    if (m_startPreviewRotation < 0.0f) {
        m_startPreviewRotation += 360.0f;
    }

    m_startSliceAnimation = m_sliceAnimation;

    m_cameraFocusRequested = false;
    m_tesseractSt = CUDA_2D_SIMULATION;
}

void Tesseract2D::beginReturnToMenu(float) {
    m_tesseractSt = MENU_PREVIEW;
    m_transitionState = TRANS_NONE;
    m_transitionT = 0.0f;

    m_cameraFocusRequested = false;

    // Leave the menu preview alive instead of hard-freezing it.
    m_gridAlpha = 1.0f;
}

bool Tesseract2D::consumeCameraFocusRequest() {
    if (!m_cameraFocusRequested) {
        return false;
    }

    m_cameraFocusRequested = false;
    return true;
}

void Tesseract2D::updatePreviewAnimation(float timeS) {
    if (m_transitionState == TRANS_TO_2D_ORIENT) {
        const float rawT =
            (timeS - m_transitionStartTime) / m_transitionDuration;

        const float t = smoothStep01(rawT);
        m_transitionT = t;

        // Flatten/settle menu preview into a stable 2D grid.
        m_previewRotation = lerp(m_startPreviewRotation, 0.0f, t);
        m_sliceAnimation = lerp(m_startSliceAnimation, 0.5f, t);

        // Slight brightening as it becomes the active 2D workspace.
        m_gridAlpha = lerp(0.65f, 1.0f, t);
        m_gridPulse = 0.5f + 0.5f * std::sinf(timeS * 8.0f) * (1.0f - t);

        if (rawT >= 1.0f) {
            m_previewRotation = 0.0f;
            m_sliceAnimation = 0.5f;
            m_gridPulse = 0.0f;
            m_gridAlpha = 1.0f;

            m_transitionState = TRANS_TO_2D_CAMERA;
            m_transitionStartTime = timeS;
            m_transitionDuration = 0.50f;
            m_transitionT = 0.0f;

            // Engine should respond by focusing/resetting CameraProcessor2D.
            m_cameraFocusRequested = true;
        }

        return;
    }

    if (m_transitionState == TRANS_TO_2D_CAMERA) {
        const float rawT =
            (timeS - m_transitionStartTime) / m_transitionDuration;

        const float t = smoothStep01(rawT);
        m_transitionT = t;

        m_previewRotation = 0.0f;
        m_sliceAnimation = 0.5f;
        m_gridPulse = 0.0f;
        m_gridAlpha = 1.0f;

        if (rawT >= 1.0f) {
            m_transitionState = TRANS_NONE;
            m_transitionT = 1.0f;
        }

        return;
    }

    if (m_tesseractSt == MENU_PREVIEW) {
        // Lightweight idle diagnostic animation for the right-side menu grid.
        m_previewRotation = std::fmod(timeS * 15.0f, 360.0f);
        m_sliceAnimation = 0.5f + 0.5f * std::sinf(timeS * 0.75f);
        m_gridPulse = 0.5f + 0.5f * std::sinf(timeS * 2.0f);
        m_gridAlpha = 0.72f + 0.18f * m_gridPulse;
        m_transitionT = 0.0f;
        return;
    }

    // Active 2D workspace: stable grid.
    m_previewRotation = 0.0f;
    m_sliceAnimation = 0.5f;
    m_gridPulse = 0.0f;
    m_gridAlpha = 1.0f;
}

const char* Tesseract2D::getModeName() const {
    switch (m_tesseractSt) {
    case MENU_PREVIEW:
        return "MENU_PREVIEW";

    case CUDA_2D_SIMULATION:
        return "CUDA_2D_SIMULATION";

    case CUDA_2D_GRAPH:
        return "CUDA_2D_GRAPH";

    case TEXT_EDITOR:
        return "TEXT_EDITOR";

    default:
        return "UNKNOWN_TESSERACT_2D_STATE";
    }
}

const char* Tesseract2D::getTransitionName() const {
    switch (m_transitionState) {
    case TRANS_NONE:
        return "TRANS_NONE";

    case TRANS_TO_2D_ORIENT:
        return "TRANS_TO_2D_ORIENT";

    case TRANS_TO_2D_CAMERA:
        return "TRANS_TO_2D_CAMERA";

    default:
        return "UNKNOWN_2D_TRANSITION";
    }
}
