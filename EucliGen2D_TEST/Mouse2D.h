#ifndef MOUSE_CONTROL_2D_H
#define MOUSE_CONTROL_2D_H

class EuclidRenderer2D;

class MouseInput2D {
public:
	enum MouseMode {
		M_NONE = 0,
		M_PAN = 1
	};

	MouseInput2D();
	~MouseInput2D();

	bool onButton(
		int button,
		int state,
		int x,
		int y,
		EuclidRenderer2D* renderer
	);

	bool onMotion(
		int x,
		int y,
		EuclidRenderer2D* renderer
	);

	bool onPassiveMotion(int x, int y);
	int getLastX() const { return m_ox; }
	int getLastY() const { return m_oy; }
	int getButtonState() const { return m_buttonState; }
	MouseMode getMode() const { return m_mode; }
	bool isDragging() const { return m_dragging; }

private:
	int m_ox = 0;
	int m_oy = 0;
	int m_buttonState = -1;

	MouseMode m_mode = M_NONE;

	bool m_dragging = false;
	float m_zoomStep = 1.10f;
};

#endif

