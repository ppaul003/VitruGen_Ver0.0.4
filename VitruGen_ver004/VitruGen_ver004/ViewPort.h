#ifndef VIEWPORT_H
#define VIEWPORT_H

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <string>
#include <vector>

#include "TheArbiter.h"

class ViewPort {
public:

	enum class ObjExportPanelMode {
		HIDDEN = 0,
		SELECT,
		CONFIRM,
		WORKING,
		COMPLETE,
		FAILED
	};

	struct ObjExportPanelData {
		ObjExportPanelMode mode =
			ObjExportPanelMode::HIDDEN;

		bool yesSelected = true;

		int progressPercent = 0;
		int spinnerFrame = 0;

		std::string statusText;
		std::vector<std::string> logLines;
		std::string titleText;
		std::string confirmText;
		std::vector<std::string> selectionLines;
		int selectedIndex = 0;
	};

	// ---------------------------------------------------------
	// TEXTURE_MAP_2D Layer 1 presentation data.
	//
	// ViewPort receives already-prepared display information.
	// It must not scan directories, parse manifests, load assets,
	// or own the output catalog.
	//
	// EuclidEngine will populate this structure in a later pass.
	// ---------------------------------------------------------
	struct TextureMapLayer1PanelData {
		bool catalogReady = false;
		bool hasOutputAssets = false;
		bool selectedAssetValid = false;

		// True only after Row [3] successfully loaded the
		// currently selected OUTPUT asset as the active
		// TEXTURE_MAP_2D target.
		bool targetLoaded = false;

		std::string targetName;
		std::string statusMessage;
	};

	struct TextureMapLayer2PanelData {

		bool targetLoaded = false;
		bool targetReady = false;

		std::string targetName;

		float previewParticleRadius = 0.0098f;
		unsigned int pixelGridDivisions = 64;
	};

	struct TextureMapLayer3PanelData {
		bool panelVisible = true;
		std::string subLayerLabel;
		std::string targetName;
		std::string authoringMode;
		std::string status;
		std::vector<std::string> informationLines;
		std::vector<std::string> rows;
		std::string footerLine1;
		std::string footerLine2;
	};

	struct MarchingCubesPanelData {
		bool available = false;
		bool meshReady = false;

		unsigned int gridX = 0;
		unsigned int gridY = 0;
		unsigned int gridZ = 0;

		unsigned int totalVoxels = 0;
		unsigned int activeVoxels = 0;

		unsigned int totalVertices = 0;
		unsigned int totalTriangles = 0;

		float isoValue = 0.0f;
	};

	ViewPort();
	~ViewPort();

	void resize(int w, int h);
	void applyPerspective(float fovDegrees = 60.0f);
	void beginOverlay2D();
	void endOverlay2D();

	void drawText2D(
		float x,
		float y,
		const char* text,
		void* font = GLUT_BITMAP_HELVETICA_18
	);

	void drawOverlay(
		const TheArbiter& arbiter,
		const MarchingCubesPanelData* mcData,
		const ObjExportPanelData* exportData,
		bool paused = false,
		bool meshAvailable = false,
		const TextureMapLayer1PanelData* textureMapData = nullptr,
		const TextureMapLayer2PanelData* textureMapLayer2Data = nullptr,
		const TextureMapLayer3PanelData* textureMapLayer3Data = nullptr
	);

	int getWidth() const { return m_window_w; }
	int getHeight() const { return m_window_h; }
	float getAspect() const;

private:
	static int clampPositive(int v);

	void updatePanelAnimation(bool visible);

	void drawPanelBackground();
	void drawSelectableLine(float x, float y, bool active, const char* text);
	void drawHelpFooter(const char* line1, const char* line2 = nullptr);
	void drawWorkspaceFrame(float alpha, const char* label = nullptr);

	void updateSubLayerPanelAnimation(bool visible);
	void drawSubLayerPanel(const TheArbiter& arbiter, const MarchingCubesPanelData* mcData);
	void drawSubLayerPanelLine(float x, float y, bool active, const char* text, float alpha);

	void drawLayer0Menu(const TheArbiter& arbiter);

	void drawLayer1EnvironmentConfig(
		const TheArbiter& arbiter,
		const TextureMapLayer1PanelData* textureMapData
	);

	void drawTextureMapLayer1Config(
		const TheArbiter& arbiter,
		const TextureMapLayer1PanelData* textureMapData
	);

	void drawTextureMapLayer2Config(
		const TheArbiter& arbiter,
		const TextureMapLayer2PanelData* data
	);

	void drawTextureMapLayer3Runtime(
		const TheArbiter& arbiter,
		const TextureMapLayer3PanelData* data);

	void drawSingleParticleLayer1Config(const TheArbiter& arbiter);
	void drawParticleSimLayer1Config(const TheArbiter& arbiter);
	
	void drawLayer2ParticleConfig(const TheArbiter& arbiter, bool meshAvailable);
	void drawParticleSimLayer2Config(const TheArbiter& arbiter);
	void drawLayer3SimulationRun(const TheArbiter& arbiter, bool paused);

	void drawObjExportPanel(const ObjExportPanelData& data);
	// Global text-entry modal used by workflows that request
	// keyboard text outside the normal configuration panels.
	//
	// Current use:
	//     SINGLE_PARTICLE_MCAD
	//     SUB_LAYER_3
	//     SAVE STATIC PARTICLE AS
	void drawTextEntryPanel(const TheArbiter& arbiter);

	float panelOffsetX() const;
	float panelX(float x) const { return x + panelOffsetX(); }
	float subLayerPanelOffsetY() const;

private:
	int m_window_w = 1920;
	int m_window_h = 1080;
	float m_fov = 60.0f;

	float m_menuPanelW = 650.0f;
	float m_subPanelW = 325.0f;
	float m_subPanelH = 235.0f;
	float m_margin = 42.0f;

	// 1.0 = panel fully visible, 0.0 = panel slid out to the left.
	float m_panelSlide = 1.0f;

	// 1.0 = sub-layer panel visible, 0.0 = slid below the viewport.
	float m_subLayerPanelSlide = 0.0f;
};

#endif
