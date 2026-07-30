#ifndef VIEWPORT_2D_H
#define VIEWPORT_2D_H

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "TheArbiter2D.h"

class ViewPort2D {
public:
	struct Rect {
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
	};

	ViewPort2D();
	~ViewPort2D();

	void resize(int w, int h);
	
	// Kept for consistency with the 3D EucliGen viewport. For the 2D grid,
	// prefer applyOrtho2D() / applyGridViewport().
	void applyPerspective(float fovDegrees = 60.0f);

	// Full-window viewport/projection helpers.
	void applyFullViewport();
	void applyOrtho2D();

	// Grid display area. In menu layer, this places the 2D grid on the
	// right side of the screen, leaving the left menu panel visible.
	void applyGridViewport(bool menuLayer);
	Rect getGridViewport(bool menuLayer) const;

	// Convert GLUT mouse coordinates into local grid viewport coordinates.
	// Returns false if the mouse point is outside the active grid viewport.
	bool screenToGridLocal(bool menuLayer, int sx, int sy, int& gx, int& gy) const;
	bool isInsideGridViewport(bool menuLayer, int sx, int sy) const;

	void beginOverlay2D();
	void endOverlay2D();

	void drawText2D(
		float x,
		float y,
		const char* text,
		void* font = GLUT_BITMAP_HELVETICA_18
	);
	
	void drawOverlay(const TheArbiter2D& arbiter, bool paused = false);

	int getWidth() const { return m_window_w; }
	int getHeight() const { return m_window_h; }
	float getAspect() const;

	// Useful for tuning the menu layout.
	void setMenuPanelWidth(float w) { m_menuPanelW = w; }
	void setMargin(float m) { m_margin = m; }
	void setGridGap(float g) { m_gridGap = g; }

private:
	static int clampPositive(int v);

	void drawPanelBackground();
	void drawGridViewportFrame(bool menuLayer, const char* label);
	void drawLayer0Menu(const TheArbiter2D& arbiter);
	void drawLayer1EnvironmentConfig(const TheArbiter2D& arbiter);
	void drawLayer2ParticleConfig(const TheArbiter2D& arbiter);
	void drawLayer3SimulationRun(const TheArbiter2D& arbiter, bool paused);

	void drawHelpFooter(const char* line1, const char* line2 = nullptr);
	void drawSelectableLine(float x, float y, bool active, const char* text);

private:
	int m_window_w = 1920;
	int m_window_h = 1080;
	float m_fov = 60.0f;

	float m_menuPanelW = 590.0f;
	float m_margin = 42.0f;
	float m_gridGap = 32.0f;
};

#endif
