#include "Camera2D.h"
#include "renderer2D_Euclid.h"

using namespace glm;

CameraProcessor2D::CameraProcessor2D() {}
CameraProcessor2D::~CameraProcessor2D() {}

void CameraProcessor2D::resize(int w, int h) {
	m_window_w = max(1, w);
	m_window_h = max(1, h);
}

void CameraProcessor2D::reset(
	const vec2& centerWorld, 
	float pixelsPerWorldUnit) {

	m_centerWorld = centerWorld;
	m_centerWorldLag = centerWorld;

	m_pixelsPerWorldUnit = clampZoom(pixelsPerWorldUnit);
	m_pixelsPerWorldUnitLag = m_pixelsPerWorldUnit;
}

void CameraProcessor2D::updateLag() {
	m_centerWorldLag += (m_centerWorld - m_centerWorldLag) * kInertia;
	m_pixelsPerWorldUnitLag +=
		(m_pixelsPerWorldUnit - m_pixelsPerWorldUnitLag) * kInertia;
}

void CameraProcessor2D::panPixels(float dxPixels, float dyPixels) {
	// Grab-and-drag behavior:
	// mouse right means the camera center moves left in world space.
	m_centerWorld -= screenDeltaToWorldDelta(dxPixels, dyPixels);
}

void CameraProcessor2D::panWorld(const vec2& deltaWorld) {
	m_centerWorld += deltaWorld;
}

void CameraProcessor2D::zoom(float zoomFactor) {
	if (zoomFactor <= 0.0f)
		return;

	m_pixelsPerWorldUnit =
		clampZoom(m_pixelsPerWorldUnit * zoomFactor);
}

void CameraProcessor2D::zoomAtScreenPoint(float zoomFactor, int mouseX, int mouseY) {
	if (zoomFactor <= 0.0f)
		return;

	const vec2 before = screenToWorld(mouseX, mouseY);

	zoom(zoomFactor);

	const vec2 after = screenToWorld(mouseX, mouseY);

	// shift camera so the point under the mouse stays under the mouse
	m_centerWorld += (before - after);
}

vec2 CameraProcessor2D::screenToWorld(int sx, int sy) const {
	const vec2 screen = vec2((float)sx, (float)sy);
	const vec2 center = vec2(
		0.5f * (float)m_window_w,
		0.5f * (float)m_window_h
	);

	return vec2(
		m_centerWorld.x + (screen.x - center.x) / m_pixelsPerWorldUnit,
		m_centerWorld.y - (screen.y - center.y) / m_pixelsPerWorldUnit
	);
}

vec2 CameraProcessor2D::screenToWorldLagged(int sx, int sy) const {
	const vec2 screen = vec2((float)sx, (float)sy);
	const vec2 center = vec2(
		0.5f * (float)m_window_w,
		0.5f * (float)m_window_h
	);

	return vec2(
		m_centerWorldLag.x + (screen.x - center.x) / m_pixelsPerWorldUnitLag,
		m_centerWorldLag.y - (screen.y - center.y) / m_pixelsPerWorldUnitLag
	);
}

vec2 CameraProcessor2D::screenDeltaToWorldDelta(
	float dxPixels, 
	float dyPixels) const {

	return vec2(
		dxPixels / m_pixelsPerWorldUnit,
		-dyPixels / m_pixelsPerWorldUnit
	);
}

vec2 CameraProcessor2D::screenDeltaToWorldDeltaLagged(
	float dxPixels, 
	float dyPixels) const {

	return vec2(
		dxPixels / m_pixelsPerWorldUnitLag,
		-dyPixels / m_pixelsPerWorldUnitLag
	);
}

void CameraProcessor2D::applyToRenderer(
	EuclidRenderer2D* renderer) const {

	if (!renderer)
		return;

	renderer->setCameraCenter(m_centerWorld);
	renderer->setPixelsPerWorldUnit(m_pixelsPerWorldUnit);
}

void CameraProcessor2D::applyLaggedToRenderer(
	EuclidRenderer2D* renderer) const {

	if (!renderer)
		return;

	renderer->setCameraCenter(m_centerWorldLag);
	renderer->setPixelsPerWorldUnit(m_pixelsPerWorldUnitLag);
}

void CameraProcessor2D::setZoomLimits(
	float minPixelsPerWorldUnit, 
	float maxPixelsPerWorldUnit) {

	m_minPixelsPerWorldUnit = std::max(0.001f, minPixelsPerWorldUnit);
	m_maxPixelsPerWorldUnit =
		std::max(m_minPixelsPerWorldUnit, maxPixelsPerWorldUnit);

	m_pixelsPerWorldUnit = clampZoom(m_pixelsPerWorldUnit);
	m_pixelsPerWorldUnitLag = clampZoom(m_pixelsPerWorldUnitLag);
}

float CameraProcessor2D::clampZoom(float value) const {
	return std::max(
		m_minPixelsPerWorldUnit,
		std::min(value, m_maxPixelsPerWorldUnit)
	);
}