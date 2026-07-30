#include <cstdlib>
#include <iostream>

#include "ParticleSimRuntimeConfig.h"

namespace {

	void require(bool condition, const char* message) {
		if (condition) return;

		std::cerr << "FAIL: " << message << '\n';
		std::exit(EXIT_FAILURE);
	}

	ParticleSimRuntimeConfig resolve(
		const TheArbiter::ParticleSimDraftConfig& draft,
		unsigned int capacity = TheArbiter::kParticleSimCapacity) {

		ParticleSimRuntimeConfig result;
		require(
			ParticleSimRuntimeConfig::resolve(
				draft,
				capacity,
				result
			),
			"Expected runtime configuration to resolve"
		);

		return result;
	}

}

int main() {
	TheArbiter::ParticleSimDraftConfig draft;

	const ParticleSimRuntimeConfig defaultConfig =
		resolve(draft);

	require(
		defaultConfig.activeCount == 4200,
		"DEFAULT mode should resolve the default count"
	);

	draft.colorMode = TheArbiter::ParticleColorMode::RGB;
	draft.redCount = 420;
	draft.greenCount = 240;
	draft.blueCount = 120;

	const ParticleSimRuntimeConfig mixedRGB =
		resolve(draft);

	require(
		mixedRGB.activeCount == 780,
		"Mixed RGB mode should resolve the channel total"
	);

	require(
		mixedRGB.redCount == 420 &&
		mixedRGB.greenCount == 240 &&
		mixedRGB.blueCount == 120,
		"Mixed RGB channel counts should remain intact"
	);

	draft.redCount = 780;
	draft.greenCount = 0;
	draft.blueCount = 0;

	const ParticleSimRuntimeConfig allRed =
		resolve(draft);

	require(
		allRed.activeCount == 780 &&
		allRed.redCount == 780 &&
		allRed.greenCount == 0 &&
		allRed.blueCount == 0,
		"All-red RGB mode should remain a contiguous red-only range"
	);

	draft.redCount = 0;
	const ParticleSimRuntimeConfig zero =
		resolve(draft);

	require(
		zero.activeCount == 0,
		"Zero-particle mode should resolve safely"
	);

	draft.redCount = TheArbiter::kParticleSimCapacity;
	const ParticleSimRuntimeConfig capacity =
		resolve(draft);

	require(
		capacity.activeCount == TheArbiter::kParticleSimCapacity,
		"Full capacity should resolve without reallocation"
	);

	draft.redCount = TheArbiter::kParticleSimCapacity;
	draft.greenCount = 1;

	ParticleSimRuntimeConfig rejected;
	require(
		!ParticleSimRuntimeConfig::resolve(
			draft,
			TheArbiter::kParticleSimCapacity,
			rejected
		),
		"Counts above capacity must be rejected"
	);

	std::cout
		<< "PASS: PARTICLE_SIM runtime configuration resolution tests\n";

	return EXIT_SUCCESS;
}
