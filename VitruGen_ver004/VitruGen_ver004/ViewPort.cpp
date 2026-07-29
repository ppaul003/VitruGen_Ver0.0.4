#include "ViewPort.h"

#include <algorithm>
#include <cstdio>

using namespace std;

ViewPort::ViewPort() {}
ViewPort::~ViewPort() {}

int ViewPort::clampPositive(int v) {
	return max(1, v);
}
float ViewPort::getAspect() const {
	return (m_window_h > 0)
		? static_cast<float>(m_window_w) / static_cast<float>(m_window_h)
		: 1.0f;
}
float ViewPort::panelOffsetX() const {
	const float hiddenX = -(m_menuPanelW + m_margin + 24.0f);
	return hiddenX * (1.0f - m_panelSlide);
}
float ViewPort::subLayerPanelOffsetY() const {
	const float hiddenY = m_subPanelH + m_margin + 24.0f;
	return hiddenY * (1.0f - m_subLayerPanelSlide);
}

void ViewPort::resize(int w, int h) {
	m_window_w = clampPositive(w);
	m_window_h = clampPositive(h);

	glViewport(0, 0, m_window_w, m_window_h);
}
void ViewPort::beginOverlay2D() {
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
void ViewPort::endOverlay2D() {
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}
void ViewPort::applyPerspective(float fovDegrees) {
	m_fov = fovDegrees;

	glViewport(0, 0, m_window_w, m_window_h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(m_fov, getAspect(), 0.1, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}
void ViewPort::drawText2D(float x, float y, const char* text, void* font) {
	if (!text) return;

	glRasterPos2f(x, y);

	for (const char* p = text; *p; p++) {
		glutBitmapCharacter(font, *p);
	}
}

void ViewPort::updatePanelAnimation(bool visible) {
	const float target = visible ? 1.0f : 0.0f;

	// Lightweight exponential smoothing. Because onIdle posts redisplay, this
	// creates a continuous slide without adding a separate animation system.
	m_panelSlide += (target - m_panelSlide) * 0.15f;

	if (m_panelSlide < 0.001f) m_panelSlide = 0.0f;
	if (m_panelSlide > 0.999f) m_panelSlide = 1.0f;
}

void ViewPort::drawPanelBackground() {
	const float panelX0 = panelX(m_margin);
	const float panelY0 = m_margin;
	const float panelX1 = panelX(m_menuPanelW);
	const float panelY1 = static_cast<float>(m_window_h) - m_margin;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(0.02f, 0.04f, 0.06f, 0.76f * m_panelSlide);
	glBegin(GL_QUADS);
	glVertex2f(panelX0, panelY0);
	glVertex2f(panelX1, panelY0);
	glVertex2f(panelX1, panelY1);
	glVertex2f(panelX0, panelY1);
	glEnd();

	glLineWidth(1.5f);
	glColor4f(1.0f, 1.0f, 1.0f, 0.95f * m_panelSlide);
	glBegin(GL_LINE_LOOP);
	glVertex2f(panelX0, panelY0);
	glVertex2f(panelX1, panelY0);
	glVertex2f(panelX1, panelY1);
	glVertex2f(panelX0, panelY1);
	glEnd();

	glColor3f(1.0f, 1.0f, 1.0f);
	drawText2D(95.0f, 100.0f, "ANAHEIM SYSTEMS DYNAMICS", GLUT_BITMAP_HELVETICA_18);
	drawText2D(95.0f, 128.0f, "VitruGen SIMCAD Ver 0.0.4", GLUT_BITMAP_HELVETICA_18);

	glLineWidth(1.0f);
}
void ViewPort::drawSelectableLine(float x, float y, bool active, const char* text) {
	const float px = panelX(x);

	if (active) {
		glColor4f(0.45f, 1.0f, 0.65f, m_panelSlide);
		drawText2D(px - 28.0f, y, ">", GLUT_BITMAP_HELVETICA_18);
	}
	else {
		glColor4f(0.72f, 0.78f, 0.82f, m_panelSlide);
	}

	drawText2D(px, y, text, GLUT_BITMAP_HELVETICA_18);
}
void ViewPort::drawHelpFooter(const char* line1, const char* line2) {
	const float yBase = static_cast<float>(m_window_h) - 135.0f;

	glColor4f(0.75f, 0.75f, 0.75f, m_panelSlide);
	if (line1) drawText2D(95.0f, yBase, line1, GLUT_BITMAP_HELVETICA_18);
	if (line2) drawText2D(95.0f, yBase + 34.0f, line2, GLUT_BITMAP_HELVETICA_18);
}
void ViewPort::drawWorkspaceFrame(float alpha, const char* label) {
	const float margin = 24.0f;
	const float x0 = margin;
	const float y0 = margin;
	const float x1 = static_cast<float>(m_window_w) - margin;
	const float y1 = static_cast<float>(m_window_h) - margin;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth(1.0f);
	glColor4f(0.85f, 0.95f, 1.0f, alpha);
	glBegin(GL_LINE_LOOP);
	glVertex2f(x0, y0);
	glVertex2f(x1, y0);
	glVertex2f(x1, y1);
	glVertex2f(x0, y1);
	glEnd();

	if (label) {
		glColor4f(0.85f, 0.95f, 1.0f, alpha);
		drawText2D(x0 + 18.0f, y0 + 30.0f, label, GLUT_BITMAP_HELVETICA_18);
	}

	glLineWidth(1.0f);
}

void ViewPort::updateSubLayerPanelAnimation(bool visible) {
	const float target = visible ? 1.0f : 0.0f;

	m_subLayerPanelSlide +=
		(target - m_subLayerPanelSlide) * 0.18f;

	if (m_subLayerPanelSlide < 0.001f) m_subLayerPanelSlide = 0.0f;
	if (m_subLayerPanelSlide > 0.999f) m_subLayerPanelSlide = 1.0f;
}

void ViewPort::drawOverlay(
	const TheArbiter& arbiter,
	const MarchingCubesPanelData* mcData,
	const ObjExportPanelData* exportData,
	bool paused,
	bool meshAvailable) {

	updatePanelAnimation(!arbiter.isSimulationRunLayer());
	updateSubLayerPanelAnimation(
		arbiter.isSubLayerPanelOpen() &&
		arbiter.isSubLayerPanelEligible()
	);

	beginOverlay2D();

	if (arbiter.isMenuLayer()) {
		drawLayer0Menu(arbiter);
	}
	else if (arbiter.isEnvironmentConfigLayer()) {
		drawLayer1EnvironmentConfig(arbiter);
	}
	else if (arbiter.isParticleConfigLayer()) {
		drawLayer2ParticleConfig(arbiter, meshAvailable);
	}
	else if (arbiter.isSimulationRunLayer()) {
		drawLayer3SimulationRun(arbiter, paused);
		drawSubLayerPanel(arbiter, mcData);
	}
	else {
		drawWorkspaceFrame(0.20f, nullptr);
	}

	if (exportData && exportData->mode !=
		ObjExportPanelMode::HIDDEN) {

		drawObjExportPanel(*exportData);
	}

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	endOverlay2D();
}

void ViewPort::drawSubLayerPanelLine(float x, float y, bool active, const char* text, float alpha) {

	if (active) {
		glColor4f(0.45f, 1.0f, 0.65f, alpha);
		drawText2D(x - 22.0f, y, ">", GLUT_BITMAP_HELVETICA_18);
	}
	else {
		glColor4f(0.72f, 0.78f, 0.82f, alpha);
	}

	drawText2D(x, y, text, GLUT_BITMAP_HELVETICA_18);
}
void ViewPort::drawSubLayerPanel(const TheArbiter& arbiter, const MarchingCubesPanelData* mcData) {
	if (m_subLayerPanelSlide <= 0.0f) return;

	const float alpha = m_subLayerPanelSlide;
	const float panelW = m_menuPanelW;
	const float x0 = m_margin;
	const float y0Visible = 170.0f;
	const float panelH = static_cast<float>(m_window_h) - y0Visible - m_margin;
	const float hiddenOffsetY = panelH + m_margin + 24.0f;
	const float y0 = y0Visible + hiddenOffsetY * (1.0f - m_subLayerPanelSlide);
	const float x1 = x0 + panelW;
	const float y1 = y0 + panelH;

	const float sectionX = x0 + 54.0f;
	const float labelX = x0 + 84.0f;
	const float dividerX0 = x0 + 42.0f;
	const float dividerX1 = x1 - 42.0f;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(0.02f, 0.04f, 0.06f, 0.80f * alpha);
	glBegin(GL_QUADS);
	glVertex2f(x0, y0);
	glVertex2f(x1, y0);
	glVertex2f(x1, y1);
	glVertex2f(x0, y1);
	glEnd();

	glLineWidth(1.5f);
	glColor4f(1.0f, 1.0f, 1.0f, 0.92f * alpha);
	glBegin(GL_LINE_LOOP);
	glVertex2f(x0, y0);
	glVertex2f(x1, y0);
	glVertex2f(x1, y1);
	glVertex2f(x0, y1);
	glEnd();

	auto drawDivider = [&](float y) {
		glLineWidth(1.0f);
		glColor4f(0.85f, 0.95f, 1.0f, 0.72f * alpha);
		glBegin(GL_LINES);
		glVertex2f(dividerX0, y);
		glVertex2f(dividerX1, y);
		glEnd();
	};

	auto drawSectionTitle = [&](float y, const char* text) {
		glColor4f(0.85f, 0.95f, 1.0f, alpha);
		drawText2D(sectionX, y, text, GLUT_BITMAP_HELVETICA_18);
	};

	// ---------------------------------------------------------------------
	// Header and assembly-track indicator.
	// ---------------------------------------------------------------------
	glColor4f(0.85f, 0.95f, 1.0f, alpha);
	drawText2D(
		sectionX,
		y0 + 40.0f,
		"SINGLE_PARTICLE MODE",
		GLUT_BITMAP_HELVETICA_18
	);

	if (arbiter.isMarchingCubesSubLayer()) {
		drawText2D(
			sectionX,
			y0 + 68.0f,
			"SUB-LAYER_3 PANEL",
			GLUT_BITMAP_HELVETICA_18
		);

		glColor4f(0.72f, 0.78f, 0.82f, alpha);
		drawText2D(
			sectionX,
			y0 + 108.0f,
			"SUB_LAYER_2 PREVIEW  -------->  SUB_LAYER_3 MC",
			GLUT_BITMAP_HELVETICA_18
		);
	}
	else {
		drawText2D(
			sectionX,
			y0 + 68.0f,
			"SUB-LAYER_2 PANEL",
			GLUT_BITMAP_HELVETICA_18
		);

		glColor4f(0.72f, 0.78f, 0.82f, alpha);
		drawText2D(
			sectionX,
			y0 + 100.0f,
			arbiter.getVolumeAssemblyNodeName(),
			GLUT_BITMAP_HELVETICA_18
		);

		const float starX[4] = {
			x0 + 112.0f,
			x0 + 252.0f,
			x0 + 392.0f,
			x0 + 532.0f
		};

		glColor4f(0.72f, 0.78f, 0.82f, 0.72f * alpha);
		glBegin(GL_LINES);
		glVertex2f(starX[0] + 8.0f, y0 + 137.0f);
		glVertex2f(starX[3] + 8.0f, y0 + 137.0f);
		glEnd();

		for (int i = 0; i < 4; i++) {
			const bool active =
				static_cast<int>(arbiter.getVolumeAssemblyNode()) == i;

			if (active)
				glColor4f(0.45f, 1.0f, 0.65f, alpha);
			else
				glColor4f(0.55f, 0.62f, 0.68f, alpha);

			drawText2D(
				starX[i],
				y0 + 142.0f,
				"*",
				GLUT_BITMAP_HELVETICA_18
			);

			char nodeLabel[32];
			snprintf(nodeLabel, sizeof(nodeLabel), "Node_%d", i);
			drawText2D(
				starX[i] - 18.0f,
				y0 + 168.0f,
				nodeLabel,
				GLUT_BITMAP_HELVETICA_12
			);
		}
	}

	const bool marchingCubesPanel =
		arbiter.isMarchingCubesSubLayer();

	// ---------------------------------------------------------------------
	// Marching Cubes classification report.
	// ---------------------------------------------------------------------
	if (marchingCubesPanel) {

		drawSectionTitle(
			y0 + 140.0f,
			"MC CLASSIFICATION REPORT"
		);

		if (mcData && mcData->available) {

			const double occupancyPercent =
				mcData->totalVoxels > 0
				? 100.0 *
				static_cast<double>(mcData->activeVoxels) /
				static_cast<double>(mcData->totalVoxels)
				: 0.0;

			char gridLine[192];
			snprintf(
				gridLine,
				sizeof(gridLine),
				"Grid: %u x %u x %u    |    ISO: %.4f",
				mcData->gridX,
				mcData->gridY,
				mcData->gridZ,
				mcData->isoValue
			);

			char voxelLine[192];
			snprintf(
				voxelLine,
				sizeof(voxelLine),
				"Active cells: %u / %u    (%.2f%%)",
				mcData->activeVoxels,
				mcData->totalVoxels,
				occupancyPercent
			);

			char outputLine[192];
			snprintf(
				outputLine,
				sizeof(outputLine),
				"Output: %u vertices    |    %u triangles",
				mcData->totalVertices,
				mcData->totalTriangles
			);

			glColor4f(0.72f, 0.78f, 0.82f, alpha);

			drawText2D(
				sectionX,
				y0 + 166.0f,
				gridLine,
				GLUT_BITMAP_HELVETICA_12
			);

			drawText2D(
				sectionX,
				y0 + 188.0f,
				voxelLine,
				GLUT_BITMAP_HELVETICA_12
			);

			drawText2D(
				sectionX,
				y0 + 210.0f,
				outputLine,
				GLUT_BITMAP_HELVETICA_12
			);

			const bool hasSurface =
				mcData->activeVoxels > 0 &&
				mcData->totalVertices > 0;

			const char* statusText = nullptr;

			if (mcData->meshReady) {
				statusText = "STATUS: TRIANGLE MESH READY";

				glColor4f(0.45f, 1.0f, 0.65f, alpha);
			}
			else if (hasSurface) {
				statusText = "STATUS: CLASSIFIED / EXTRACTION INCOMPLETE";

				glColor4f(0.95f, 0.82f, 0.30f, alpha);
			}
			else {
				statusText = "STATUS: NO ISO-SURFACE DETECTED";

				glColor4f(1.0f, 0.50f, 0.25f, alpha);
			}

			drawText2D(
				sectionX,
				y0 + 232.0f,
				statusText,
				GLUT_BITMAP_HELVETICA_12
			);
		}
		else {
			glColor4f(1.0f, 0.50f, 0.25f, alpha);

			drawText2D(
				sectionX,
				y0 + 178.0f,
				"Marching Cubes classification data unavailable.",
				GLUT_BITMAP_HELVETICA_12
			);
		}

		drawDivider(y0 + 254.0f);
	}
	else {
		// Preserve the original divider position for Sub-Layer 2.
		drawDivider(y0 + 190.0f);
	}

	const int activeItem =
		arbiter.getActiveSubLayerPanelItem();

	// ---------------------------------------------------------------------
	// SUB_LAYER_3 panel.
	// ---------------------------------------------------------------------
	if (marchingCubesPanel) {

		drawSectionTitle(
			y0 + 292.0f,
			"Output:"
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 334.0f,
			activeItem == TheArbiter::MC_LIST_EXPORT_OBJ,
			"[1] Export .OBJ",
			alpha
		);

		drawDivider(y0 + 370.0f);

		drawSectionTitle(
			y0 + 410.0f,
			"Previous Sub-layer:"
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 452.0f,
			activeItem == TheArbiter::MC_LIST_TO_SUB_LAYER_2,
			"[2] To Sub-Layer_2 Preview",
			alpha
		);

		drawDivider(y0 + 488.0f);

		drawSectionTitle(
			y0 + 528.0f,
			"Next Sub-layer:"
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 570.0f,
			activeItem == TheArbiter::MC_LIST_TO_SUB_LAYER_0,
			"[3] To Sub-Layer_0 Reference",
			alpha
		);
	}
	// ---------------------------------------------------------------------
	// Node 0: Preview.
	// ---------------------------------------------------------------------
	else if (arbiter.getVolumeAssemblyNode() == TheArbiter::VOLUME_NODE_PREVIEW) {

		drawSectionTitle(
			y0 + 230.0f,
			"Volume Injection:"
		);

		char injectionLine[160];

		snprintf(
			injectionLine,
			sizeof(injectionLine),
			"[1] Injection Voxels { %s }",
			arbiter.getVolumeInjectionVoxelName()
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 272.0f,
			activeItem ==
			TheArbiter::PREVIEW_LIST_INJECTION_MODE,
			injectionLine,
			alpha
		);

		drawDivider(y0 + 308.0f);

		drawSectionTitle(
			y0 + 348.0f,
			"Next Node:"
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 390.0f,
			activeItem ==
			TheArbiter::PREVIEW_LIST_EDIT_OBJECT,
			"[2] Edit Object",
			alpha
		);

		drawDivider(y0 + 426.0f);

		drawSectionTitle(
			y0 + 466.0f,
			"Next Sub-Layer:"
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 508.0f,
			activeItem ==
			TheArbiter::PREVIEW_LIST_RUN_MC,
			"[3] Run MC_Mode",
			alpha
		);
	}
	// ---------------------------------------------------------------------
	// Node 1: object editing.
	//
	// Default path:
	//     Injection Voxels { NONE }
	//     existing Volume_0 editor
	//
	// Injection path:
	//     Injection Voxels != NONE
	//     dynamic VOXEL_0 / VOXEL_1 editor
	// ---------------------------------------------------------------------
	else if (arbiter.getVolumeAssemblyNode() == TheArbiter::VOLUME_NODE_EDIT_OBJECT) {

		if (arbiter.hasInjectionVoxelSelected()) {

			const bool editingVoxel1 =
				arbiter.isEditingInjectionVoxel1();

			drawSectionTitle(
				y0 + 224.0f,
				"Select Volume To Edit:"
			);

			char targetLine[128];
			snprintf(
				targetLine,
				sizeof(targetLine),
				"[1] Edit { %s }",
				arbiter.getVolumeEditTargetName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 266.0f,
				activeItem ==
				TheArbiter::INJECTION_EDIT_LIST_TARGET,
				targetLine,
				alpha
			);

			char objectLine[160];
			snprintf(
				objectLine,
				sizeof(objectLine),
				"[2] Select Object { %s }",
				arbiter.getVolumeEditTargetObjectName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 306.0f,
				activeItem ==
				TheArbiter::INJECTION_EDIT_LIST_OBJECT,
				objectLine,
				alpha
			);

			drawDivider(y0 + 342.0f);

			if (!editingVoxel1) {

				drawSectionTitle(
					y0 + 382.0f,
					"Rotation Angle Increment:"
				);

				char angleLine[96];
				snprintf(
					angleLine,
					sizeof(angleLine),
					"[3] Deg { %d }",
					arbiter.getRotationAngleIncrementDeg()
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 424.0f,
					activeItem ==
					TheArbiter::INJECTION_EDIT_LIST_ROTATION_INCREMENT,
					angleLine,
					alpha
				);

				drawDivider(y0 + 460.0f);

				drawSectionTitle(
					y0 + 500.0f,
					"Next/Prev Node:"
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 542.0f,
					activeItem ==
					TheArbiter::INJECTION_EDIT_LIST_OFFSET_OBJECT,
					"[4] Offset Object",
					alpha
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 584.0f,
					activeItem ==
					TheArbiter::INJECTION_EDIT_LIST_PREVIEW_OBJECT,
					"[5] Preview Object",
					alpha
				);

				char transformLine[220];
				snprintf(
					transformLine,
					sizeof(transformLine),
					"Volume_0 | Scale %s %.2f/%.2f/%.2f/%.2f | Rot %s %.0f/%.0f/%.0f",
					arbiter.getObjectEditModeName(),
					arbiter.getVolumeScaleWhole(),
					arbiter.getVolumeScaleX(),
					arbiter.getVolumeScaleY(),
					arbiter.getVolumeScaleZ(),
					arbiter.getObjectRotationModeName(),
					arbiter.getRotationPitchDeg(),
					arbiter.getRotationYawDeg(),
					arbiter.getRotationRollDeg()
				);

				glColor4f(0.72f, 0.78f, 0.82f, alpha);

				drawText2D(
					sectionX,
					y0 + 678.0f,
					transformLine,
					GLUT_BITMAP_HELVETICA_12
				);
			}
			else {

				drawSectionTitle(
					y0 + 382.0f,
					"Reset Injection Brush Base:"
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 424.0f,
					activeItem ==
					TheArbiter::INJECTION_EDIT_LIST_COMMIT_BASE,
					"[3] Commit Brush Base",
					alpha
				);

				drawDivider(y0 + 460.0f);

				drawSectionTitle(
					y0 + 500.0f,
					"Mirrored Injection:"
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 542.0f,
					activeItem ==
					TheArbiter::INJECTION_EDIT_LIST_MIRROR,
					"[4] Mirror { None } (placeholder)",
					alpha
				);

				glColor4f(0.72f, 0.78f, 0.82f, alpha);

				drawText2D(
					sectionX,
					y0 + 604.0f,
					"Commit Brush Base bakes VOLUME_1 rotation into its local basis.",
					GLUT_BITMAP_HELVETICA_12
				);

				drawText2D(
					sectionX,
					y0 + 626.0f,
					"No fuse/cut is applied here.",
					GLUT_BITMAP_HELVETICA_12
				);
			}
		}
		else {

			drawSectionTitle(
				y0 + 224.0f,
				"=== Edit Volume_0 ==="
			);

			drawSectionTitle(
				y0 + 258.0f,
				"Object Primitives:"
			);

			char objectLine[160];
			snprintf(
				objectLine,
				sizeof(objectLine),
				"[1] Select Object { %s }",
				arbiter.getVolumePrimitiveName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 298.0f,
				activeItem == TheArbiter::EDIT_LIST_OBJECT,
				objectLine,
				alpha
			);

			drawDivider(y0 + 328.0f);

			drawSectionTitle(
				y0 + 364.0f,
				"Rotation Angle Increment:"
			);

			char angleLine[96];
			snprintf(
				angleLine,
				sizeof(angleLine),
				"[2] Deg { %d }",
				arbiter.getRotationAngleIncrementDeg()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 404.0f,
				activeItem ==
				TheArbiter::EDIT_LIST_ROTATION_INCREMENT,
				angleLine,
				alpha
			);

			drawDivider(y0 + 434.0f);

			drawSectionTitle(
				y0 + 470.0f,
				"Next/Prev Node:"
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 510.0f,
				activeItem ==
				TheArbiter::EDIT_LIST_OFFSET_OBJECT,
				"[3] Offset Object",
				alpha
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 550.0f,
				activeItem ==
				TheArbiter::EDIT_LIST_PREVIEW_OBJECT,
				"[4] Preview Object",
				alpha
			);

			char transformLine[220];
			snprintf(
				transformLine,
				sizeof(transformLine),
				"Scale %s %.2f/%.2f/%.2f/%.2f | Rot %s %.0f/%.0f/%.0f",
				arbiter.getObjectEditModeName(),
				arbiter.getVolumeScaleWhole(),
				arbiter.getVolumeScaleX(),
				arbiter.getVolumeScaleY(),
				arbiter.getVolumeScaleZ(),
				arbiter.getObjectRotationModeName(),
				arbiter.getRotationPitchDeg(),
				arbiter.getRotationYawDeg(),
				arbiter.getRotationRollDeg()
			);

			glColor4f(0.72f, 0.78f, 0.82f, alpha);

			drawText2D(
				sectionX,
				y0 + 636.0f,
				transformLine,
				GLUT_BITMAP_HELVETICA_12
			);
		}
	}
	// ---------------------------------------------------------------------
	// Node 2: offset / injection rail panel.
	// ---------------------------------------------------------------------
	else if (arbiter.getVolumeAssemblyNode() == TheArbiter::VOLUME_NODE_OFFSET_OBJECT) {

		if (arbiter.hasInjectionVoxelSelected()) {

			drawSectionTitle(
				y0 + 230.0f,
				"Edit Offset Volume:"
			);

			char targetLine[128];

			snprintf(
				targetLine,
				sizeof(targetLine),
				"[1] Select { %s }",
				arbiter.getVolumeEditTargetName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 272.0f,
				activeItem ==
				TheArbiter::INJECTION_OFFSET_LIST_TARGET,
				targetLine,
				alpha
			);

			drawDivider(y0 + 310.0f);

			drawSectionTitle(
				y0 + 350.0f,
				"Offset Grid Vector:"
			);

			char vectorLine[128];

			snprintf(
				vectorLine,
				sizeof(vectorLine),
				"[2] Offset { %s }",
				arbiter.getOffsetVectorName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 392.0f,
				activeItem ==
				TheArbiter::INJECTION_OFFSET_LIST_VECTOR,
				vectorLine,
				alpha
			);

			char incrementLine[128];

			snprintf(
				incrementLine,
				sizeof(incrementLine),
				"[3] Increments { %s }",
				arbiter.getOffsetIncrementName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 434.0f,
				activeItem ==
				TheArbiter::INJECTION_OFFSET_LIST_DISTANCE,
				incrementLine,
				alpha
			);

			drawDivider(y0 + 472.0f);

			if (arbiter.isEditingInjectionVoxel0()) {

				drawSectionTitle(
					y0 + 512.0f,
					"Injection Rail:"
				);

				char railLine[160];

				snprintf(
					railLine,
					sizeof(railLine),
					"[4] Injection Vector { %.2f }",
					arbiter.getInjectionRailT()
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 554.0f,
					activeItem ==
					TheArbiter::INJECTION_OFFSET_LIST_RAIL,
					railLine,
					alpha
				);

				drawDivider(y0 + 592.0f);

				drawSectionTitle(
					y0 + 632.0f,
					"Next/Prev Node:"
				);

				char applyLine[192];

				if (arbiter.getVolumeInjectionMode() == TheArbiter::VOLUME_CUT) {

					snprintf(
						applyLine,
						sizeof(applyLine),
						"[5] Cut Voxel From Anchor { READY }"
					);
				}
				else if (!arbiter.isVolumeBoundarySensorReady()) {

					snprintf(
						applyLine,
						sizeof(applyLine),
						"[5] Fuse Voxel To Anchor { CHECKING }"
					);
				}
				else if (arbiter.isVolumeBoundarySafe()) {

					snprintf(
						applyLine,
						sizeof(applyLine),
						"[5] Fuse Voxel To Anchor { READY }"
					);
				}
				else {

					snprintf(
						applyLine,
						sizeof(applyLine),
						"[5] Fuse Voxel To Anchor { BLOCKED: %u PATCHES }",
						arbiter.getVolumeBoundaryUnsafeCount()
					);
				}

				if (arbiter.canApplyVolumeToBase()) {

					drawSubLayerPanelLine(
						labelX,
						y0 + 674.0f,
						activeItem ==
							TheArbiter::INJECTION_OFFSET_LIST_APPLY_TO_BASE,
						applyLine,
						alpha
					);
				}
				else {

					glColor4f(1.0f, 0.38f, 0.08f, alpha);

					drawText2D(
						labelX,
						y0 + 674.0f,
						applyLine,
						GLUT_BITMAP_HELVETICA_18
					);
				}

				drawSubLayerPanelLine(
					labelX,
					y0 + 716.0f,
					activeItem ==
						TheArbiter::INJECTION_OFFSET_LIST_EDIT_OBJECT,
					"[6] Edit Object",
					alpha
				);
			}
			else {

				drawSectionTitle(
					y0 + 512.0f,
					"Voxel Injection Mode:"
				);

				char modeLine[128];

				snprintf(
					modeLine,
					sizeof(modeLine),
					"[4] Inject { %s }",
					arbiter.getVolumeInjectionModeName()
				);

				drawSubLayerPanelLine(
					labelX,
					y0 + 554.0f,
					activeItem ==
					TheArbiter::INJECTION_OFFSET_LIST_MODE,
					modeLine,
					alpha
				);

				glColor4f(0.72f, 0.78f, 0.82f, alpha);

				drawText2D(
					sectionX,
					y0 + 620.0f,
					"VOLUME_1 offset edits the injection brush local placement.",
					GLUT_BITMAP_HELVETICA_12
				);

				drawText2D(
					sectionX,
					y0 + 642.0f,
					"Switch to VOLUME_0 to drive rail depth and final apply.",
					GLUT_BITMAP_HELVETICA_12
				);
			}
		}
		else { // Injection Voxel None

			drawSectionTitle(
				y0 + 230.0f,
				"Offset Grid Vector:"
			);

			char vectorLine[128];

			snprintf(
				vectorLine,
				sizeof(vectorLine),
				"[1] Offset { %s }",
				arbiter.getOffsetVectorName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 272.0f,
				activeItem ==
				TheArbiter::OFFSET_LIST_VECTOR,
				vectorLine,
				alpha
			);

			char incrementLine[128];

			snprintf(
				incrementLine,
				sizeof(incrementLine),
				"[2] Increments { %s }",
				arbiter.getOffsetIncrementName()
			);

			drawSubLayerPanelLine(
				labelX,
				y0 + 314.0f,
				activeItem ==
				TheArbiter::OFFSET_LIST_DISTANCE,
				incrementLine,
				alpha
			);

			drawDivider(y0 + 350.0f);

			drawSectionTitle(
				y0 + 390.0f,
				"Next/Prev Node:"
			);

			char applyLine[160];

			if (!arbiter.isVolumeBoundarySensorReady()) {

				snprintf(
					applyLine,
					sizeof(applyLine),
					"[3] Apply To Base { CHECKING }"
				);
			}
			else if (arbiter.isVolumeBoundarySafe()) {

				snprintf(
					applyLine,
					sizeof(applyLine),
					"[3] Apply To Base { READY }"
				);
			}
			else {

				snprintf(
					applyLine,
					sizeof(applyLine),
					"[3] Apply To Base { BLOCKED: %u PATCHES }",
					arbiter.getVolumeBoundaryUnsafeCount()
				);
			}

			if (arbiter.canApplyVolumeToBase()) {

				drawSubLayerPanelLine(
					labelX,
					y0 + 432.0f,
					activeItem == TheArbiter::OFFSET_LIST_APPLY_TO_BASE,
					applyLine,
					alpha
				);
			}
			else {

				glColor4f(1.0f, 0.38f, 0.08f, alpha);

				drawText2D(
					labelX,
					y0 + 432.0f,
					applyLine,
					GLUT_BITMAP_HELVETICA_18
				);
			}

			drawSubLayerPanelLine(
				labelX,
				y0 + 480.0f,
				activeItem ==
				TheArbiter::OFFSET_LIST_EDIT_OBJECT,
				"[4] Edit Object",
				alpha
			);
		}
	}
	// ---------------------------------------------------------------------
	// Node 3: traversal/loop placeholder.
	// ---------------------------------------------------------------------
	else if (arbiter.getVolumeAssemblyNode() == TheArbiter::VOLUME_NODE_APPLY_TO_BASE) {
	char operationLine[160];

		snprintf(
			operationLine,
			sizeof(operationLine),
			"Apply Operation Mode { %s }",
			arbiter.getVolumeInjectionModeName()
		);

		drawSectionTitle(
			y0 + 230.0f,
			operationLine
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 282.0f,
			activeItem ==
			TheArbiter::APPLY_LIST_COMMIT,
			"[1] Commit / Loop Back To Preview",
			alpha
		);

		drawDivider(y0 + 326.0f);

		drawSectionTitle(
			y0 + 366.0f,
			"Next/Prev Node:"
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 408.0f,
			activeItem ==
			TheArbiter::APPLY_LIST_CANCEL_TO_PREVIEW,
			"[2] Preview Object",
			alpha
		);

		drawSubLayerPanelLine(
			labelX,
			y0 + 450.0f,
			activeItem ==
			TheArbiter::APPLY_LIST_OFFSET_OBJECT,
			"[3] Offset Object",
			alpha
		);
	}

	// ---------------------------------------------------------------------
	// Footer.
	// ---------------------------------------------------------------------
	drawDivider(y1 - 92.0f);
	glColor4f(0.75f, 0.75f, 0.75f, alpha);
	drawText2D(
		sectionX,
		y1 - 58.0f,
		"W/S: Select    A/D: Change value    E: Activate    TAB: Hide    Q: Back",
		GLUT_BITMAP_HELVETICA_12
	);

	glLineWidth(1.0f);
}
void ViewPort::drawLayer0Menu(const TheArbiter& arbiter) {
	drawPanelBackground();

	glColor4f(1.0f, 1.0f, 1.0f, m_panelSlide);
	drawText2D(panelX(95.0f), 180.0f, "LAYER 0 -> MENU", GLUT_BITMAP_HELVETICA_18);

	char line[256];
	snprintf(
		line,
		sizeof(line),
		"[1]: ENVIRONMENT SELECTION { %s }",
		arbiter.getSelectedDomainDisplayName()
	);

	drawSelectableLine(95.0f, 245.0f, true, line);
	const char* statusText = "UNKNOWN ENVIORNMENT";
	float statusR = 1.0f;
	float statusG = 0.45f;
	float statusB = 0.45f;

	if (arbiter.isIdleSelected()) {
		statusText =
			"IDLE selected: E is locked.";
	}
	else {
		switch (arbiter.getSelectedDomain()) {
		case TheArbiter::WorkspaceDomain::GRID_2D:
			statusText =
				"GRID_2D selected: press E to configure.";

			statusR = 0.45f;
			statusG = 1.0f;
			statusB = 0.65f;
			break;

		case TheArbiter::WorkspaceDomain::GRID_3D:
			statusText =
				"GRID_3D selected: press E to configure.";

			statusR = 0.45f;
			statusG = 1.0f;
			statusB = 0.65f;
			break;

		case TheArbiter::WorkspaceDomain::SIMCAD_4D:
			statusText =
				"SIMCAD_4D selected: press E to configure.";

			statusR = 0.45f;
			statusG = 1.0f;
			statusB = 0.65f;
			break;

		default:
		case TheArbiter::WorkspaceDomain::NONE:
		case TheArbiter::WorkspaceDomain::COUNT:
			break;
		}
	}

	glColor4f(statusR, statusG, statusB, m_panelSlide);
	drawText2D(
		panelX(95.0f),
		310.0f,
		statusText,
		GLUT_BITMAP_HELVETICA_18
	);

	drawHelpFooter("A / D: Change selection     E: Enter", "ESC: Exit");
	drawWorkspaceFrame(0.30f);
}
void ViewPort::drawLayer1EnvironmentConfig(const TheArbiter& arbiter) {

	drawPanelBackground();
	glColor4f(1.0f, 1.0f, 1.0f, m_panelSlide);
	drawText2D(
		panelX(95.0f),
		180.0f,
		"LAYER 1 -> WORKSPACE SELECTION",
		GLUT_BITMAP_HELVETICA_18);

	char line[256];
	snprintf(
		line,
		sizeof(line),
		"[1]: %s SELECTION { %s }",
		arbiter.getSelectedDomainDisplayName(),
		arbiter.getSelectedWorkspaceDisplayName()
	);

	drawSelectableLine(95.0f, 245.0f, true, line);

	const TheArbiter::WorkspaceId selectedWorkspace =
		arbiter.getSelectedWorkspace();

	const bool workspaceAvailable =
		TheArbiter::getWorkspaceAvailability(selectedWorkspace) ==
		TheArbiter::WorkspaceAvailability::AVAILABLE;

	char statusLine[256];
	if (workspaceAvailable) {
		snprintf(
			statusLine,
			sizeof(statusLine),
			"%s selected: press E to configure.",
			arbiter.getSelectedWorkspaceDisplayName()
		);

		// Green: workspace is available.
		glColor4f(
			0.45f,
			1.0f,
			0.65f,
			m_panelSlide
		);
	}
	else {
		snprintf(
			statusLine,
			sizeof(statusLine),
			"%s is reserved for the next pass.",
			arbiter.getSelectedWorkspaceDisplayName()
		);

		// Amber: workspace is visible but unavailable.
		glColor4f(
			1.0f,
			0.82f,
			0.45f,
			m_panelSlide
		);
	}

	drawText2D(
		panelX(95.0f),
		310.0f,
		statusLine,
		GLUT_BITMAP_HELVETICA_18
	);

	drawHelpFooter(
		"A / D: Change selection     E: Enter",
		"Q: Back one layer     ESC: Exit"
	);

	drawWorkspaceFrame(0.20f, nullptr);
}
void ViewPort::drawLayer2ParticleConfig(const TheArbiter& arbiter, bool meshAvailable) {
	drawPanelBackground();

	glColor4f(1.0f, 1.0f, 1.0f, m_panelSlide);
	drawText2D(panelX(95.0f), 180.0f, "LAYER 2 -> 3D_GRID MODE CONFIGURATION", GLUT_BITMAP_HELVETICA_18);

	char modeLine[128];
	snprintf(
		modeLine,
		sizeof(modeLine),
		"MODE: %s",
		arbiter.getSelectedWorkspaceDisplayName()
	);
	drawText2D(panelX(95.0f), 210.0f, modeLine, GLUT_BITMAP_HELVETICA_18);

	char line1[256];
	snprintf(
		line1,
		sizeof(line1),
		"[1]: PARTICLE COLOR SELECTION { %s }",
		arbiter.getParticleColorName()
	);

	drawSelectableLine(
		95.0f,
		285.0f,
		arbiter.getActiveParticleConfigList() == TheArbiter::PARTICLE_LIST_COLOR,
		line1
	);

	if (arbiter.isSingleParticleSelected()) {
		char radiusLine[256];
		snprintf(
			radiusLine,
			sizeof(radiusLine),
			"[2]: ADJUST PARTICLE RADIUS { %.4f }",
			arbiter.getParticleRadius()
		);

		drawSelectableLine(
			95.0f,
			340.0f,
			arbiter.getActiveParticleConfigList() == TheArbiter::PARTICLE_LIST_RADIUS,
			radiusLine
		);

		char renderModeLine[256];
		snprintf(
			renderModeLine,
			sizeof(renderModeLine),
			"[3]: PARTICLE RENDER MODE { %s }",
			arbiter.getParticleRenderModeName()
		);

		drawSelectableLine(
			95.0f,
			395.0f,
			arbiter.getActiveParticleConfigList() == TheArbiter::PARTICLE_LIST_RENDER_MODE,
			renderModeLine
		);

		drawSelectableLine(
			95.0f,
			450.0f,
			arbiter.getActiveParticleConfigList() == TheArbiter::PARTICLE_LIST_RUN,
			"[4]: RUN SIMULATION LAYER"
		);

		if (arbiter.isParticleRenderMesh()) {
			if (meshAvailable) {
				glColor4f(0.45f, 1.0f, 0.65f, m_panelSlide);
				drawText2D(
					panelX(95.0f),
					520.0f,
					"MESH AVAILABLE",
					GLUT_BITMAP_HELVETICA_18
				);
			}
			else {
				glColor4f(1.0f, 0.72f, 0.25f, m_panelSlide);
				drawText2D(
					panelX(95.0f),
					520.0f,
					"MESH UNAVAILABLE",
					GLUT_BITMAP_HELVETICA_18
				);
			}
		}

		drawHelpFooter(
			"W / S: Select list     A / D: Change value",
			"E: Run when LIST 4 selected     Q: Back one layer"
		);

		drawWorkspaceFrame(0.20f, nullptr);
		return;
	}

	char line2[256];
	snprintf(
		line2,
		sizeof(line2),
		"[2]: PARTICLE RESET MODE { %s }",
		arbiter.getParticleResetModeName()
	);

	drawSelectableLine(
		95.0f,
		340.0f,
		arbiter.getActiveParticleConfigList() == TheArbiter::PARTICLE_LIST_RESET,
		line2
	);

	drawSelectableLine(
		95.0f,
		395.0f,
		arbiter.getActiveParticleConfigList() == TheArbiter::PARTICLE_LIST_RUN,
		"[3]: PRESS E RUN PARTICLES"
	);

	drawHelpFooter("W / S: Select list     A / D: Change value", "E: Run when LIST 3 selected     Q: Back one layer");
	drawWorkspaceFrame(0.20f, nullptr);
}
void ViewPort::drawLayer3SimulationRun(const TheArbiter& arbiter, bool paused) {
	drawWorkspaceFrame(0.22f, nullptr);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(0.02f, 0.04f, 0.06f, 0.55f);
	glBegin(GL_QUADS);
	glVertex2f(24.0f, 24.0f);
	glVertex2f(1120.0f, 24.0f);
	glVertex2f(1120.0f, 158.0f);
	glVertex2f(24.0f, 158.0f);
	glEnd();

	glColor3f(0.85f, 0.95f, 1.0f);
	char modeLine[256];
	snprintf(
		modeLine,
		sizeof(modeLine),
		"LAYER 3 -> SIMULATION RUN (%s)",
		arbiter.getSelectedWorkspaceDisplayName()
	);

	drawText2D(40.0f, 52.0f, modeLine, GLUT_BITMAP_HELVETICA_18);

	if (arbiter.isSingleParticleSelected()) {
		char subLayerLine[256];
		snprintf(
			subLayerLine,
			sizeof(subLayerLine),
			"SINGLE_PARTICLE: %s",
			arbiter.getSingleParticleSubLayerName()
		);
		drawText2D(40.0f, 82.0f, subLayerLine, GLUT_BITMAP_HELVETICA_18);

		char primitiveLine[256];

		if (arbiter.isVolumeRenderSubLayer()) {

			if (arbiter.getVolumeAssemblyNode() ==
				TheArbiter::VOLUME_NODE_OFFSET_OBJECT) {

				const char* boundaryStatus =
					!arbiter.isVolumeBoundarySensorReady()
					? "CHECKING"
					: (
						arbiter.isVolumeBoundarySafe()
						? "SAFE"
						: "CONTACT"
						);

				snprintf(
					primitiveLine,
					sizeof(primitiveLine),
					"OBJECT: %s | OFFSET %.3f/%.3f/%.3f | "
					"VECTOR %s | INC %s | BOUNDARY %s:%u",
					arbiter.getVolumePrimitiveName(),
					arbiter.getOffsetX(),
					arbiter.getOffsetY(),
					arbiter.getOffsetZ(),
					arbiter.getOffsetVectorName(),
					arbiter.getOffsetIncrementName(),
					boundaryStatus,
					arbiter.getVolumeBoundaryUnsafeCount()
				);
			}
			else {

				snprintf(
					primitiveLine,
					sizeof(primitiveLine),
					"OBJECT: %s | MODE: %s | SCALE %s %.2f/%.2f/%.2f/%.2f | ROT %s %.0f/%.0f/%.0f | Deg %d",
					arbiter.getVolumePrimitiveName(),
					arbiter.getObjectTransformModeName(),
					arbiter.getObjectEditModeName(),
					arbiter.getVolumeScaleWhole(),
					arbiter.getVolumeScaleX(),
					arbiter.getVolumeScaleY(),
					arbiter.getVolumeScaleZ(),
					arbiter.getObjectRotationModeName(),
					arbiter.getRotationPitchDeg(),
					arbiter.getRotationYawDeg(),
					arbiter.getRotationRollDeg(),
					arbiter.getRotationAngleIncrementDeg()
				);
			}
		}

		drawText2D(40.0f, 108.0f, primitiveLine, GLUT_BITMAP_HELVETICA_18);

		const char* helpLine = nullptr;

		if (arbiter.isWorkplaneParticleSelectSubLayer()) {
			helpLine = "W/S: Move workplane    LMB: Select particle    Q: Back    RMB: Menu";
		}
		else if (arbiter.isVolumeRenderSubLayer()) {
			if (arbiter.isSubLayerPanelOpen()) {
				helpLine = "TAB: Hide panel    W/S: Select panel item    A/D: Change selected item    E: Marching Cubes    Q: Back";
			}
			else {
				if (arbiter.getVolumeAssemblyNode() ==
					TheArbiter::VOLUME_NODE_OFFSET_OBJECT) {

					helpLine =
						"TAB: Object panel    W/S: Move along selected vector    "
						"RMB: Edit menu    E: Next node    Q: Back";
				}
				else {
					helpLine =
						"TAB: Object panel    W/S: Scale selected mode    "
						"A/D: Rotate selected mode    RMB: Edit menu    "
						"E: Next node    Q: Back";
				}
			}
		}
		else if (arbiter.isMarchingCubesSubLayer()) {
			helpLine = "TAB: Toggle sub-layer panel    E: Return to reference    Q: Back    RMB: Menu";
		}
		else {
			helpLine = "E: Advance sub-layer    Q: Back    RMB: Menu";
		}

		drawText2D(40.0f, 134.0f, helpLine, GLUT_BITMAP_HELVETICA_18);

		return;
	}
	char line1[256];
	snprintf(
		line1,
		sizeof(line1),
		"COLOR: %s     RESET: %s     STATUS: %s",
		arbiter.getParticleColorName(),
		arbiter.getParticleResetModeName(),
		paused ? "PAUSED" : "RUNNING"
	);
	drawText2D(40.0f, 82.0f, line1, GLUT_BITMAP_HELVETICA_18);

	char line2[256];
	snprintf(
		line2,
		sizeof(line2),
		"Q: Back one layer    SPACE: %s    ENTER: Step    RMB: Rotate    Wheel: Zoom",
		paused ? "Resume" : "Pause"
	);
	drawText2D(40.0f, 108.0f, line2, GLUT_BITMAP_HELVETICA_18);
}

void ViewPort::drawObjExportPanel(const ObjExportPanelData& data) {
	if (data.mode == ObjExportPanelMode::HIDDEN) return;

	const float screenW = static_cast<float>(m_window_w);
	const float screenH = static_cast<float>(m_window_h);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glUseProgram(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// ---------------------------------------------------------
	// Full-screen modal veil.
	// ---------------------------------------------------------
	glColor4f(0.0f, 0.0f, 0.0f, 0.68f);

	glBegin(GL_QUADS);
	glVertex2f(0.0f, 0.0f);
	glVertex2f(screenW, 0.0f);
	glVertex2f(screenW, screenH);
	glVertex2f(0.0f, screenH);
	glEnd();

	const bool confirmation =
		data.mode == ObjExportPanelMode::CONFIRM;

	const float panelW =
		confirmation
		? 580.0f
		: min(1120.0f, screenW - 100.0f);

	const float panelH =
		confirmation
		? 280.0f
		: min(660.0f, screenH - 100.0f);

	const float x0 = 0.5f * (screenW - panelW);
	const float y0 = 0.5f * (screenH - panelH);

	const float x1 = x0 + panelW;
	const float y1 = y0 + panelH;

	// ---------------------------------------------------------
	// Main modal body.
	// ---------------------------------------------------------
	glColor4f(0.015f, 0.028f, 0.045f, 0.97f);

	glBegin(GL_QUADS);
	glVertex2f(x0, y0);
	glVertex2f(x1, y0);
	glVertex2f(x1, y1);
	glVertex2f(x0, y1);
	glEnd();

	glLineWidth(1.5f);
	glColor4f(0.82f, 0.95f, 1.0f, 0.96f);

	glBegin(GL_LINE_LOOP);
	glVertex2f(x0, y0);
	glVertex2f(x1, y0);
	glVertex2f(x1, y1);
	glVertex2f(x0, y1);
	glEnd();

	const float textX = x0 + 38.0f;

	// =========================================================
	// Confirmation dialog
	// =========================================================
	if (confirmation) {

		glColor4f(0.85f, 0.95f, 1.0f, 1.0f);
		drawText2D(
			textX,
			y0 + 52.0f,
			"VITRUGEN OBJ EXPORT",
			GLUT_BITMAP_HELVETICA_18
		);

		glColor4f(0.72f, 0.78f, 0.82f, 1.0f);
		drawText2D(
			textX,
			y0 + 94.0f,
			"Confirm Export .OBJ?",
			GLUT_BITMAP_HELVETICA_18
		);

		auto drawChoice =
			[&](float y, bool active, const char* text) {

			if (active) {

				glColor4f(0.45f, 1.0f, 0.65f, 1.0f);
				drawText2D(
					textX,
					y,
					">",
					GLUT_BITMAP_HELVETICA_18
				);
			}
			else {

				glColor4f(0.72f, 0.78f, 0.82f, 1.0f);
			}

			drawText2D(
				textX + 28.0f,
				y,
				text,
				GLUT_BITMAP_HELVETICA_18
			);
		};

		drawChoice(
			y0 + 142.0f,
			data.yesSelected,
			"[1] Yes"
		);

		drawChoice(
			y0 + 182.0f,
			!data.yesSelected,
			"[2] No"
		);

		glColor4f(0.62f, 0.68f, 0.72f, 1.0f);
		drawText2D(
			textX,
			y1 - 34.0f,
			"W/S or A/D: Select    E: Activate    Q: Cancel",
			GLUT_BITMAP_HELVETICA_12
		);

		glLineWidth(1.0f);
		return;
	}

	// =========================================================
	// Export progress display
	// =========================================================
	glColor4f(0.85f, 0.95f, 1.0f, 1.0f);

	drawText2D(
		textX,
		y0 + 44.0f,
		"VITRUGEN OBJ EXPORT PIPELINE",
		GLUT_BITMAP_HELVETICA_18
	);

	const char* phaseText =
		data.mode == ObjExportPanelMode::COMPLETE
		? "STATUS: COMPLETE"
		: data.mode == ObjExportPanelMode::FAILED
		? "STATUS: EXPORT FAILED"
		: "STATUS: PROCESSING";

	if (data.mode == ObjExportPanelMode::COMPLETE) {
		glColor4f(0.45f, 1.0f, 0.65f, 1.0f);
	}
	else if (data.mode == ObjExportPanelMode::FAILED) {
		glColor4f(1.0f, 0.35f, 0.22f, 1.0f);
	}
	else {
		glColor4f(0.95f, 0.82f, 0.30f, 1.0f);
	}

	drawText2D(
		textX,
		y0 + 74.0f,
		phaseText,
		GLUT_BITMAP_HELVETICA_12
	);

	// ---------------------------------------------------------
	// Console-style output window.
	// ---------------------------------------------------------
	const float consoleX0 = x0 + 34.0f;
	const float consoleY0 = y0 + 98.0f;
	const float consoleX1 = x1 - 34.0f;
	const float consoleY1 = y1 - 142.0f;

	glColor4f(0.005f, 0.010f, 0.016f, 0.92f);

	glBegin(GL_QUADS);
	glVertex2f(consoleX0, consoleY0);
	glVertex2f(consoleX1, consoleY0);
	glVertex2f(consoleX1, consoleY1);
	glVertex2f(consoleX0, consoleY1);
	glEnd();

	glColor4f(0.48f, 0.58f, 0.64f, 0.92f);

	glBegin(GL_LINE_LOOP);
	glVertex2f(consoleX0, consoleY0);
	glVertex2f(consoleX1, consoleY0);
	glVertex2f(consoleX1, consoleY1);
	glVertex2f(consoleX0, consoleY1);
	glEnd();

	const int maxVisibleLines = 16;

	const int lineCount =
		static_cast<int>(data.logLines.size());

	const int firstLine =
		max(0, lineCount - maxVisibleLines);

	float logY = consoleY0 + 24.0f;

	for (int i = firstLine; i < lineCount; i++) {

		glColor4f(0.72f, 0.82f, 0.86f, 1.0f);

		drawText2D(
			consoleX0 + 16.0f,
			logY,
			data.logLines[i].c_str(),
			GLUT_BITMAP_HELVETICA_12
		);

		logY += 20.0f;
	}

	// ---------------------------------------------------------
	// Progress bar.
	//
	// Twenty slash characters:
	//     one slash = five percent.
	// ---------------------------------------------------------
	const int progress =
		max(0, min(100, data.progressPercent));

	const int filledSlashes = progress / 5;

	const float progressY = y1 - 92.0f;

	glColor4f(0.82f, 0.90f, 0.94f, 1.0f);

	drawText2D(
		textX,
		progressY,
		"Loading:",
		GLUT_BITMAP_HELVETICA_18
	);

	float slashX = textX + 92.0f;

	const float slashAdvance =
		static_cast<float>(glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, '/'));

	for (int i = 0; i < 20; i++) {

		if (i < filledSlashes) {
			glColor4f(0.30f, 1.0f, 0.55f, 1.0f);
		}
		else {
			glColor4f(0.80f, 0.86f, 0.90f, 0.82f);
		}

		drawText2D(
			slashX,
			progressY,
			"/",
			GLUT_BITMAP_HELVETICA_18
		);

		slashX += slashAdvance;
	}

	char percentText[32];

	snprintf(
		percentText,
		sizeof(percentText),
		" %d%%",
		progress
	);

	glColor4f(0.85f, 0.95f, 1.0f, 1.0f);

	drawText2D(
		slashX + 8.0f,
		progressY,
		percentText,
		GLUT_BITMAP_HELVETICA_18
	);

	// ---------------------------------------------------------
	// Spinner/status line.
	// ---------------------------------------------------------
	const char spinnerFrames[4] = {
		'/',
		'-',
		'\\',
		'-'
	};

	char operationLine[256];

	if (data.mode == ObjExportPanelMode::COMPLETE) {
		snprintf(
			operationLine,
			sizeof(operationLine),
			"Exporting to .OBJ ... COMPLETE!"
		);
	}
	else if (data.mode == ObjExportPanelMode::FAILED) {
		snprintf(
			operationLine,
			sizeof(operationLine),
			"Exporting to .OBJ ... FAILED"
		);
	}
	else {
		snprintf(
			operationLine,
			sizeof(operationLine),
			"Exporting to .OBJ ...%c",
			spinnerFrames[data.spinnerFrame % 4]
		);
	}

	if (data.mode == ObjExportPanelMode::FAILED) {
		glColor4f(1.0f, 0.35f, 0.22f, 1.0f);
	}
	else {
		glColor4f(0.45f, 1.0f, 0.65f, 1.0f);
	}

	drawText2D(
		textX,
		y1 - 50.0f,
		operationLine,
		GLUT_BITMAP_HELVETICA_18
	);

	if (!data.statusText.empty()) {
		glColor4f(0.68f, 0.74f, 0.78f, 1.0f);

		drawText2D(
			x1 - 360.0f,
			y1 - 50.0f,
			data.statusText.c_str(),
			GLUT_BITMAP_HELVETICA_12
		);
	}

	glLineWidth(1.0f);
}
