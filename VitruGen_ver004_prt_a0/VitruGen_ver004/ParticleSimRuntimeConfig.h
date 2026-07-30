#ifndef PARTICLE_SIM_RUNTIME_CONFIG_H
#define PARTICLE_SIM_RUNTIME_CONFIG_H

#include "TheArbiter.h"

struct ParticleSimRuntimeConfig {
	unsigned int capacity = 0;
	unsigned int activeCount = 0;
	unsigned int redCount = 0;
	unsigned int greenCount = 0;
	unsigned int blueCount = 0;

	TheArbiter::ParticleColorMode colorMode =
		TheArbiter::ParticleColorMode::Default;

	TheArbiter::ParticleSimResetMode resetMode =
		TheArbiter::ParticleSimResetMode::Default;

	static bool resolve(
		const TheArbiter::ParticleSimDraftConfig& draft,
		unsigned int allocatedCapacity,
		ParticleSimRuntimeConfig& resolved) {

		const unsigned long long requestedCount =
			draft.colorMode == TheArbiter::ParticleColorMode::Default
			? static_cast<unsigned long long>(
				draft.defaultParticleCount
			)
			: static_cast<unsigned long long>(draft.redCount) +
				static_cast<unsigned long long>(draft.greenCount) +
				static_cast<unsigned long long>(draft.blueCount);

		if (requestedCount > allocatedCapacity)
			return false;

		resolved.capacity = allocatedCapacity;
		resolved.activeCount =
			static_cast<unsigned int>(requestedCount);

		resolved.colorMode = draft.colorMode;
		resolved.resetMode = draft.resetMode;

		if (draft.colorMode == TheArbiter::ParticleColorMode::RGB) {
			resolved.redCount = draft.redCount;
			resolved.greenCount = draft.greenCount;
			resolved.blueCount = draft.blueCount;
		}
		else {
			resolved.redCount = 0;
			resolved.greenCount = 0;
			resolved.blueCount = 0;
		}

		return true;
	}
};

#endif
