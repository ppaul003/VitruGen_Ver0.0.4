#include "Mouse2D.h"
#include "renderer2D_Euclid.h"
#include <GL/freeglut.h>
#include <cmath>

using namespace std;

MouseInput2D::MouseInput2D() {}
MouseInput2D::~MouseInput2D() {}

bool MouseInput2D::onButton(
	int button,
	int state,
	int x,
	int y,
	EuclidRenderer2D* renderer) {

	if (!renderer) {
		m_ox = x;
		m_oy = y;
		return false;
	}
	// Mouse wheel zoom.
	// GLUT commonly reports wheel up/down as button 3 and 4.
	if (button == 3 || button == 4) {
		if (state == GLUT_DOWN) {
			if (button == 3) {
				renderer->zoomAtScreenPoint(m_zoomStep, x, y);
			}
			else {
				renderer->zoomAtScreenPoint(1.0f / m_zoomStep, x, y);
			}
		}
		
		m_ox = x;
		m_oy = y;
		return true;
	}

	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			m_buttonState = button;
			m_mode = M_PAN;
			m_dragging = true;

			m_ox = x;
			m_oy = y;
			return true;
		}

		if (state == GLUT_UP) {
			m_buttonState = -1;
			m_mode = M_NONE;
			m_dragging = false;

			m_ox = x;
			m_oy = y;
			return true;
		}
	}

	m_ox = x;
	m_oy = y;
	return true;
}

bool MouseInput2D::onMotion(
	int x,
	int y,
	EuclidRenderer2D* renderer) {

	if (!renderer) {
		m_ox = x;
		m_oy = y;
		return false;
	}

	const int dx = x - m_ox;
	const int dy = y - m_oy;

	bool consumed = false;


	if (m_dragging &&
		m_mode == M_PAN &&
		m_buttonState == GLUT_LEFT_BUTTON) {
		
		renderer->panPixels(
			static_cast<float>(dx),
			static_cast<float>(dy)
		);

		consumed = true;
	}

	m_ox = x;
	m_oy = y;

	return consumed;
}

bool MouseInput2D::onPassiveMotion(int x, int y) {
	m_ox = x;
	m_oy = y;

	return true;
}