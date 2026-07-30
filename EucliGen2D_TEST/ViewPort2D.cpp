#include "ViewPort2D.h"

#include <algorithm>
#include <cstdio>

using namespace std;

ViewPort2D::ViewPort2D() {}
ViewPort2D::~ViewPort2D() {}

int ViewPort2D::clampPositive(int v) {
	return max(1, v);
}

void ViewPort2D::resize(int w, int h) {
	m_window_w = clampPositive(w);
	m_window_h = clampPositive(h);

	applyFullViewport();
}

float ViewPort2D::getAspect() const {
	return (m_window_h > 0)
		? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
		: 1.0f;
}

void ViewPort2D::applyPerspective(float fovDegrees) {
	m_fov = fovDegrees;

	glViewport(0, 0, m_window_w, m_window_h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(m_fov, getAspect(), 0.1, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void ViewPort2D::applyFullViewport() {
	glViewport(0, 0, m_window_w, m_window_h);
}

void ViewPort2D::applyOrtho2D() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(
		0.0,
		static_cast<double>(m_window_w),
		static_cast<double>(m_window_h),
		0.0
	);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

ViewPort2D::Rect ViewPort2D::getGridViewport(bool menuLayer) const {
	Rect r;

	if (!menuLayer) {
		r.x = 0;
		r.y = 0;
		r.w = m_window_w;
		r.h = m_window_h;
		return r;
	}

	const int leftPanelRight = static_cast<int>(m_menuPanelW + m_gridGap);
	const int margin = static_cast<int>(m_margin);

	r.x = max(leftPanelRight, margin);
	r.y = margin;
	r.w = max(1, m_window_w - r.x - margin);
	r.h = max(1, m_window_h - margin * 2);

	return r;
}

void ViewPort2D::applyGridViewport(bool menuLayer) {
	const Rect r = getGridViewport(menuLayer);
	glViewport(r.x, r.y, r.w, r.h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, static_cast<double>(r.w), static_cast<double>(r.h), 0.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

bool ViewPort2D::isInsideGridViewport(
	bool menuLayer, 
	int sx, 
	int sy) const {

	int gx = 0;
	int gy = 0;
	return screenToGridLocal(menuLayer, sx, sy, gx, gy);
}

bool ViewPort2D::screenToGridLocal(
	bool menuLayer,
	int sx, int sy,
	int& gx, int& gy) const {

	const Rect r = getGridViewport(menuLayer);

	// GLUT mouse y is top-left based. OpenGL viewport y is bottom-left based,
	// but our local renderer/projection uses top-left coordinates.
	gx = sx - r.x;
	gy = sy - (m_window_h - (r.y + r.h));

	if (gx < 0 || gy < 0 || gx >= r.w || gy >= r.h)
		return false;

	return true;
}

void ViewPort2D::beginOverlay2D() {
	glViewport(0, 0, m_window_w, m_window_h);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(
		0.0,
		static_cast<double>(m_window_w),
		static_cast<double>(m_window_h),
		0.0
	);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}

void ViewPort2D::endOverlay2D() {
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}

void ViewPort2D::drawText2D(float x, float y, const char* text, void* font) {
	if (!text) return;

	glRasterPos2f(x, y);

	for (const char* p = text; *p; ++p) {
		glutBitmapCharacter(font, *p);
	}
}

void ViewPort2D::drawPanelBackground() {
	const float panelX0 = m_margin;
	const float panelY0 = m_margin;
	const float panelX1 = m_menuPanelW;
	const float panelY1 = static_cast<float>(m_window_h) - m_margin;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(0.02f, 0.04f, 0.06f, 0.76f);
	glBegin(GL_QUADS);
		glVertex2f(panelX0, panelY0);
		glVertex2f(panelX1, panelY0);
		glVertex2f(panelX1, panelY1);
		glVertex2f(panelX0, panelY1);
	glEnd();

	glLineWidth(1.5f);
	glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
	glBegin(GL_LINE_LOOP);
		glVertex2f(panelX0, panelY0);
		glVertex2f(panelX1, panelY0);
		glVertex2f(panelX1, panelY1);
		glVertex2f(panelX0, panelY1);
	glEnd();

	glColor3f(1.0f, 1.0f, 1.0f);
	drawText2D(95.0f, 100.0f, "ANAHEIM SYSTEMS DYNAMICS", GLUT_BITMAP_HELVETICA_18);
	drawText2D(95.0f, 128.0f, "EucliGen2D / CUDA GRID", GLUT_BITMAP_HELVETICA_18);

	glLineWidth(1.0f);
}

void ViewPort2D::drawSelectableLine(float x, float y, bool active, const char* text) {
	if (active) {
		glColor3f(0.45f, 1.0f, 0.65f);
		drawText2D(x - 28.0f, y, ">", GLUT_BITMAP_HELVETICA_18);
	}
	else {
		glColor3f(0.72f, 0.78f, 0.82f);
	}

	drawText2D(x, y, text, GLUT_BITMAP_HELVETICA_18);
}

void ViewPort2D::drawHelpFooter(const char* line1, const char* line2) {
	const float yBase = static_cast<float>(m_window_h) - 135.0f;

	glColor3f(0.75f, 0.75f, 0.75f);
	if (line1) drawText2D(95.0f, yBase, line1, GLUT_BITMAP_HELVETICA_18);
	if (line2) drawText2D(95.0f, yBase + 34.0f, line2, GLUT_BITMAP_HELVETICA_18);
}

void ViewPort2D::drawGridViewportFrame(bool menuLayer, const char* label) {
	const Rect r = getGridViewport(menuLayer);

	// Convert OpenGL viewport bottom-left rect into overlay top-left coords.
	const float x0 = static_cast<float>(r.x);
	const float y0 = static_cast<float>(m_window_h - (r.y + r.h));
	const float x1 = static_cast<float>(r.x + r.w);
	const float y1 = static_cast<float>(m_window_h - r.y);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Subtle frame behind the right-side CUDA grid.
	glColor4f(0.02f, 0.04f, 0.06f, menuLayer ? 0.18f : 0.04f);
	glBegin(GL_QUADS);
		glVertex2f(x0, y0);
		glVertex2f(x1, y0);
		glVertex2f(x1, y1);
		glVertex2f(x0, y1);
	glEnd();

	glLineWidth(menuLayer ? 2.0f : 1.0f);
	glColor4f(0.85f, 0.95f, 1.0f, menuLayer ? 0.85f : 0.25f);
	glBegin(GL_LINE_LOOP);
		glVertex2f(x0, y0);
		glVertex2f(x1, y0);
		glVertex2f(x1, y1);
		glVertex2f(x0, y1);
	glEnd();

	if (label) {
		glColor4f(0.85f, 0.95f, 1.0f, 0.90f);
		drawText2D(x0 + 18.0f, y0 + 30.0f, label, GLUT_BITMAP_HELVETICA_18);
	}

	glDisable(GL_BLEND);
	glLineWidth(1.0f);
}

void ViewPort2D::drawLayer0Menu(const TheArbiter2D& arbiter) {
	drawPanelBackground();

	glColor3f(1.0f, 1.0f, 1.0f);
	drawText2D(95.0f, 180.0f, "LAYER 0 -> MENU", GLUT_BITMAP_HELVETICA_18);

	char line[256];
	snprintf(
		line,
		sizeof(line),
		"[1]: ENVIRONMENT SELECTION { %s }",
		arbiter.getEnvironmentName()
	);

	drawSelectableLine(95.0f, 245.0f, true, line);

	if (arbiter.isIdleSelected()) {
		glColor3f(1.0f, 0.45f, 0.45f);
		drawText2D(95.0f, 310.0f, "IDLE selected: E is locked.", GLUT_BITMAP_HELVETICA_18);
	}
	else {
		glColor3f(0.45f, 1.0f, 0.65f);
		drawText2D(95.0f, 310.0f, "2D_GRID selected: press E to configure.", GLUT_BITMAP_HELVETICA_18);
	}

	drawHelpFooter("A / D: Change selection     E: Enter", "ESC: Exit");
	drawGridViewportFrame(true, "2D_GRID PREVIEW");
}

void ViewPort2D::drawLayer1EnvironmentConfig(const TheArbiter2D& arbiter) {
	drawPanelBackground();

	glColor3f(1.0f, 1.0f, 1.0f);
	drawText2D(95.0f, 180.0f, "LAYER 1 -> ENVIRONMENT CONFIGURATION", GLUT_BITMAP_HELVETICA_18);

	char line1[256];
	char line2[256];
	char line3[256];

	snprintf(
		line1,
		sizeof(line1),
		"[1]: 2D_GRID SIZE SELECTION { %u CELLS }",
		arbiter.getGridCellCountSelection()
	);

	snprintf(
		line2,
		sizeof(line2),
		"[2]: 2D SIM WORLD BOX SCALING { %.0f }",
		arbiter.getWorldBoxScaleSelection()
	);

	snprintf(
		line3,
		sizeof(line3),
		"[3]: 2D_GRID MODE SELECTION { %s }",
		arbiter.getGridSelectionName()
	);

	drawSelectableLine(
		95.0f, 
		245.0f, 
		arbiter.getActiveEnvironmentConfigList() == TheArbiter2D::ENV_CONFIG_LIST_GRID_SIZE, 
		line1
	);

	drawSelectableLine(
		95.0f,
		300.0f,
		arbiter.getActiveEnvironmentConfigList() == TheArbiter2D::ENV_CONFIG_SIM_WORLD_SCALE,
		line2
	);

	drawSelectableLine(
		95.0f,
		355.0f,
		arbiter.getActiveEnvironmentConfigList() == TheArbiter2D::ENV_CONFIG_LIST_GRID_MODE,
		line3
	);

	drawSelectableLine(
		95.0f,
		410.0f,
		arbiter.getActiveEnvironmentConfigList() == TheArbiter2D::ENV_CONFIG_LIST_CONFIRM,
		"[4]: PRESS E TO CONFIRM SELECTION"
	);

	char gridInfo[256];
	snprintf(
		gridInfo,
		sizeof(gridInfo),
		"GRID: %u x %u CELLS | MAX PARTICLES: %u",
		arbiter.getGridCellsPerAxisSelection(),
		arbiter.getGridCellsPerAxisSelection(),
		arbiter.getMaxGeneratedParticles()
	);

	glColor3f(0.85f, 0.95f, 1.0f);
	drawText2D(95.0f, 470.0f, gridInfo, GLUT_BITMAP_HELVETICA_18);

	if (arbiter.isGraphSelected()) {
		glColor3f(1.0f, 0.82f, 0.45f);
		drawText2D(95.0f, 520.0f, "GRAPH is reserved for the next pass.", GLUT_BITMAP_HELVETICA_18);
	}
	else {
		glColor3f(0.45f, 1.0f, 0.65f);
		drawText2D(95.0f, 520.0f, "PARTICLES selected: press E to configure.", GLUT_BITMAP_HELVETICA_18);
	}

	drawHelpFooter(
		"W / S: Select list     A / D: Change selected value",
		"E: Confirm on LIST 4     Q: Back one layer     ESC: Exit"
	);

	drawGridViewportFrame(false, nullptr);
}

void ViewPort2D::drawLayer2ParticleConfig(const TheArbiter2D& arbiter) {
	drawPanelBackground();

	glColor3f(1.0f, 1.0f, 1.0f);
	drawText2D(95.0f, 180.0f, "LAYER 2 -> 2D_GRID MODE CONFIGURATION", GLUT_BITMAP_HELVETICA_18);
	drawText2D(95.0f, 210.0f, "MODE: PARTICLES", GLUT_BITMAP_HELVETICA_18);

	char line1[256];
	char line2[256];

	if (arbiter.isParticleCountEntryActive()) {
		const char* entryText =
			(arbiter.getParticleEntryText()[0] != '\0')
			? arbiter.getParticleEntryText()
			: "0";

		snprintf(
			line1,
			sizeof(line1),
			"[1]: GENERATE PARTICLE { %s } [ :=%s ]",
			arbiter.getParticleColorName(),
			entryText
		);
	}
	else {
		snprintf(
			line1,
			sizeof(line1),
			"[1]: GENERATE PARTICLE { %s } [ %u ]",
			arbiter.getParticleColorName(),
			arbiter.getSelectedParticleCount()
		);

	}
	

	snprintf(
		line2,
		sizeof(line2),
		"[2]: PARTICLE RESET MODE { %s }",
		arbiter.getParticleResetModeName()
	);

	drawSelectableLine(
		95.0f,
		285.0f,
		arbiter.getActiveParticleConfigList() == TheArbiter2D::PARTICLE_LIST_GENERATE,
		line1
	);

	drawSelectableLine(
		95.0f,
		340.0f,
		arbiter.getActiveParticleConfigList() == TheArbiter2D::PARTICLE_LIST_RESET,
		line2
	);

	drawSelectableLine(
		95.0f,
		395.0f,
		arbiter.getActiveParticleConfigList() == TheArbiter2D::PARTICLE_LIST_RUN,
		"[3]: PRESS E TO RUN PARTICLES"
	);
	
	char totalLine[256];
	snprintf(
		totalLine,
		sizeof(totalLine),
		"RED %u | BLUE %u | GREEN %u | YELLOW %u | TOTAL %u / %u",
		arbiter.getRedParticleCount(),
		arbiter.getBlueParticleCount(),
		arbiter.getGreenParticleCount(),
		arbiter.getYellowParticleCount(),
		arbiter.getTotalGeneratedParticles(),
		arbiter.getMaxGeneratedParticles()
	);
	glColor3f(0.85f, 0.95f, 1.0f);
	drawText2D(95.0f, 455.0f, totalLine, GLUT_BITMAP_HELVETICA_18);

	if (arbiter.getParticleConfigMessage() && arbiter.getParticleConfigMessage()[0] != '\0') {
		glColor3f(1.0f, 0.82f, 0.45f);
		drawText2D(95.0f, 495.0f, arbiter.getParticleConfigMessage(), GLUT_BITMAP_HELVETICA_18);
	}

	if (arbiter.isParticleCountEntryActive()) {
		drawHelpFooter("DATA ENTRY: 0-9 type     BACKSPACE delete    A / D +/-1", "ENTER: Confirm");
	}
	else {
		drawHelpFooter(
			"W / S: Select list     A / D: Change color/reset",
			"ENTER: Edit count on LIST [1]    E: RUN on LIST [3]     Q: Back"
		);
	}
	
	drawGridViewportFrame(false, nullptr);
}

void ViewPort2D::drawLayer3SimulationRun(const TheArbiter2D& arbiter, bool paused) {
	beginOverlay2D();

	drawGridViewportFrame(false, nullptr);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(0.02f, 0.04f, 0.06f, 0.55f);
	glBegin(GL_QUADS);
		glVertex2f(24.0f, 24.0f);
		glVertex2f(920.0f, 24.0f);
		glVertex2f(920.0f, 128.0f);
		glVertex2f(24.0f, 128.0f);
	glEnd();

	glColor3f(0.85f, 0.95f, 1.0f);
	drawText2D(40.0f, 52.0f, "LAYER 3 -> SIMULATION RUN (PARTICLES)", GLUT_BITMAP_HELVETICA_18);

	char countLine1[256];
	char countLine2[256];

	snprintf(
		countLine1,
		sizeof(countLine1),
		"RED %u | BLUE %u | GREEN %u | YELLOW %u",
		arbiter.getRedParticleCount(),
		arbiter.getBlueParticleCount(),
		arbiter.getGreenParticleCount(),
		arbiter.getYellowParticleCount()
	);

	snprintf(
		countLine2,
		sizeof(countLine2),
		"ACTIVE TOTAL %u / %u",
		arbiter.getTotalGeneratedParticles(),
		arbiter.getMaxGeneratedParticles()
	);

	glColor3f(0.85f, 0.95f, 1.0f);
	drawText2D(40.0f, 78.0f, countLine1, GLUT_BITMAP_HELVETICA_18);
	drawText2D(40.0f, 100.0f, countLine2, GLUT_BITMAP_HELVETICA_18);

	char status[256];
	snprintf(
		status,
		sizeof(status),
		"Q: Back one layer    SPACE: %s    ENTER: Step    LMB: Pan    Wheel: Zoom",
		paused ? "Resume" : "Pause"
	);

	drawText2D(40.0f, 112.0f, status, GLUT_BITMAP_HELVETICA_18);

	glDisable(GL_BLEND);

	endOverlay2D();
}

void ViewPort2D::drawOverlay(const TheArbiter2D& arbiter, bool paused) {
	if (arbiter.isSimulationRunLayer()) {
		drawLayer3SimulationRun(arbiter, paused);
		return;
	}

	beginOverlay2D();

	if (arbiter.isMenuLayer()) {
		drawLayer0Menu(arbiter);
	}
	else if (arbiter.isEnvironmentConfigLayer()) {
		drawLayer1EnvironmentConfig(arbiter);
	}
	else if (arbiter.isParticleConfigLayer()) {
		drawLayer2ParticleConfig(arbiter);
	}
	else {
		drawGridViewportFrame(false, nullptr);
	}

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);


	endOverlay2D();
}
