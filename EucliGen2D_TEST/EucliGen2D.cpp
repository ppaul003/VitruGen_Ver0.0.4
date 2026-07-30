#include "EuclidEngine2D.h"

int main(int argc, char** argv) {
	EuclidEngine2D engine;

	if (!engine.init(argc, argv)) {
		return 1;
	}

	engine.run();

	return 0;
}