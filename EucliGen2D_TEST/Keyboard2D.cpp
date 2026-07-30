#include "Keyboard2D.h"

#include <cctype>

using namespace std;

KeyboardInput2D::KeyboardInput2D() {}
KeyboardInput2D::~KeyboardInput2D() {}

KeyboardInput2D::KeyEvent KeyboardInput2D::onKey(unsigned char key, int x, int y) {
	KeyEvent event;
	event.rawKey = key;
	event.x = x;
	event.y = y;
	event.signal = decode(key);

	return event;
}

KeyboardInput2D::KeySignal KeyboardInput2D::decode(unsigned char key) const {
	if (key == 27) {
		return KEY_ESCAPE;
	}

	if (key == 13) {
		return KEY_ENTER;
	}

	if (key == ' ') {
		return KEY_SPACE;
	}

	if (key == 8 || key == 127) {
		return KEY_BACKSPACE;
	}

	if (key >= '0' && key <= '9') {
		return KEY_DIGIT;
	}

	unsigned char lowered =
		static_cast<unsigned char>(
			tolower(static_cast<unsigned char>(key)));

	switch (lowered) {
	case 'a':
		return KEY_A;

	case 'd':
		return KEY_D;

	case 'e':
		return KEY_E;

	case 'q':
		return KEY_Q;

	case 'w':
		return KEY_W;

	case 's':
		return KEY_S;

	default:
		return KEY_UNKNOWN;
	}
}