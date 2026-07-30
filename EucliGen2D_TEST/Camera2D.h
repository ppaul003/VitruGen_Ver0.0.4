#ifndef CAMERA_PROCESS_2D_H
#define CAMERA_PROCESS_2D_H

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

class EuclidRenderer2D;

class CameraProcessor2D {
public:
	CameraProcessor2D();
	~CameraProcessor2D();

	// window / viewport size
	void resize(int w, int h);

	void reset(
		const glm::vec2& centerWorld = glm::vec2(0.0f, 0.0f),
		float pixelPerWorldUnit = 120.0f
	);
	
	// smooth lag update. once per frame
	void updateLag();

	// Mouse-drag style panning.
	// Positive dx means mouse moved right.
	// This behaves like "grabbing" the grid and dragging it.
	void panPixels(float dxPixels, float dyPixels);

	// direct world-space pan
	void panWorld(const glm::vec2& deltaWorld);

	void zoom(float zoomFactor);

	// zoom while preserving world point under mouse cursor
	void zoomAtScreenPoint(float zoomFactor, int mouseX, int mouseY);

	// coordinate conversion
	glm::vec2 screenToWorld(int sx, int sy) const;
	glm::vec2 screenToWorldLagged(int sx, int sy) const;

	glm::vec2 screenDeltaToWorldDelta(float dxPixels, float dyPixels) const;
	glm::vec2 screenDeltaToWorldDeltaLagged(float dxPixels, float dyPixels) const;

	// push camera state into renderer before display()
	void applyToRenderer(EuclidRenderer2D* renderer) const;
	void applyLaggedToRenderer(EuclidRenderer2D* renderer) const;

	// getters
	glm::vec2 getCenterWorld() const { return m_centerWorld; }
	glm::vec2 getLaggedCenterWorld() const { return m_centerWorldLag; }

	float getPixelsPerWorldUnit() const { return m_pixelsPerWorldUnit; }
	float getLaggedPixelsPerWorldUnit() const { return m_pixelsPerWorldUnitLag; }

	int getWindowWidth() const { return m_window_w; }
	int getWindowHeight() const { return m_window_h; }

	// setters
	void setCenterWorld(const glm::vec2& centerWorld) { m_centerWorld = centerWorld; };
	void setPixelsPerWorldUnit(float pixelsPerWorldUnit) { m_pixelsPerWorldUnit = clampZoom(pixelsPerWorldUnit); };

	void setZoomLimits(float minPixelsPerWorldUnit, float maxPixelsPerWorldUnit);

private:
	float clampZoom(float value) const;

private:
	static constexpr float kInertia = 0.15f;

	int m_window_w = 1920;
	int m_window_h = 1080;

	// target camera state
	glm::vec2 m_centerWorld = glm::vec2(0.0f, 0.0f);
	float m_pixelsPerWorldUnit = 120.0f;

	// lagged / smoothed camera state
	glm::vec2 m_centerWorldLag = glm::vec2(0.0f, 0.0f);
	float m_pixelsPerWorldUnitLag = 120.0f;

	float m_minPixelsPerWorldUnit = 10.0f;
	float m_maxPixelsPerWorldUnit = 5000.0f;
};

#endif
