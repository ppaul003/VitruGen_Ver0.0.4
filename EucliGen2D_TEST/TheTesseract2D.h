#ifndef _THE_TESSERACT_2D_H_
#define _THE_TESSERACT_2D_H_

#include <GL/glew.h>

class Tesseract2D {
public:
	enum TesseractSTATE {
		MENU_PREVIEW = 0,
		CUDA_2D_SIMULATION,
		CUDA_2D_GRAPH,
		TEXT_EDITOR
	};

	enum TransitionState {
		TRANS_NONE = 0,
		TRANS_TO_2D_ORIENT,
		TRANS_TO_2D_CAMERA
	};

	Tesseract2D();
	~Tesseract2D();

	void setMode(TesseractSTATE mode) { m_tesseractSt = mode; };
	TesseractSTATE getMode() const { return m_tesseractSt; };

	void captureModelView() { glGetFloatv(GL_MODELVIEW_MATRIX, m_modelView); }
	const float* getModelView() const { return m_modelView; };

    // Call once per frame with GLUT elapsed time in seconds.
    void updatePreviewAnimation(float timeS);

    // Start transition from menu preview into the 2D grid/simulation layer.
    void beginEnter2DTransition(float timeS);

    // Use this when returning from LAYER_2D_VIEW back to LAYER_MENU.
    void beginReturnToMenu(float timeS = 0.0f);

    // One-shot request for the engine to reset/focus CameraProcessor2D.
    bool consumeCameraFocusRequest();

    // State queries.
    bool isTransitioningTo2D() const {
        return m_transitionState == TRANS_TO_2D_ORIENT ||
            m_transitionState == TRANS_TO_2D_CAMERA;
    }

    bool isOrientingTo2D() const {
        return m_transitionState == TRANS_TO_2D_ORIENT;
    }

    bool isFocusingTo2D() const {
        return m_transitionState == TRANS_TO_2D_CAMERA;
    }

    bool isMenuPreview() const { return m_tesseractSt == MENU_PREVIEW; }
    bool is2DSimulation() const { return m_tesseractSt == CUDA_2D_SIMULATION; }
    bool is2DGraph() const { return m_tesseractSt == CUDA_2D_GRAPH; }

    const char* getModeName() const;
    const char* getTransitionName() const;

    // Visual parameters for menu/right-side preview.
    float getPreviewRotation() const { return m_previewRotation; }
    float getGridPulse() const { return m_gridPulse; }
    float getGridAlpha() const { return m_gridAlpha; }
    float getTransitionT() const { return m_transitionT; }

    // Compatibility with the original 3D-style naming.
    float getSliceAnimation() const { return m_sliceAnimation; }

private:
    static float smoothStep01(float t);
    static float lerp(float a, float b, float t);

private:
	TesseractSTATE m_tesseractSt = MENU_PREVIEW;


	float m_modelView[16]{};

	TransitionState m_transitionState = TRANS_NONE;

    float m_transitionStartTime = 0.0f;
    float m_transitionDuration = 0.75f;
    float m_transitionT = 0.0f;

    float m_startPreviewRotation = 0.0f;
    float m_startSliceAnimation = 0.0f;

    // For 2D, previewRotation is just a subtle decorative value.
    // It can drive a minor grid shimmer/pulse if you want.
    float m_previewRotation = 0.0f;

    // Kept for naming compatibility with the 3D Tesseract.
    // In 2D it can be treated as a menu preview pulse/scan value.
    float m_sliceAnimation = 0.5f;

    float m_gridPulse = 0.0f;
    float m_gridAlpha = 1.0f;

    bool m_cameraFocusRequested = false;
};

#endif
