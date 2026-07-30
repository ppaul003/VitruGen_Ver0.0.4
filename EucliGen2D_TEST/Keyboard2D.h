#ifndef KEYBOARD_INPUT_2D_H
#define KEYBOARD_INPUT_2D_H

class KeyboardInput2D {
public:
	enum KeySignal {
		KEY_NONE = 0,

		KEY_ESCAPE,
		KEY_ENTER,
		KEY_SPACE,

		KEY_A,
		KEY_D,
		KEY_E,
		KEY_Q,
		KEY_W,
		KEY_S,

		KEY_BACKSPACE,
		KEY_DIGIT,

		KEY_UNKNOWN
	};

	struct KeyEvent {
		KeySignal signal;
		unsigned char rawKey;
		int x;
		int y;
	};

	KeyboardInput2D();
	~KeyboardInput2D();

	KeyEvent onKey(unsigned char key, int x, int y);

private:
	KeySignal decode(unsigned char key) const;
};

#endif