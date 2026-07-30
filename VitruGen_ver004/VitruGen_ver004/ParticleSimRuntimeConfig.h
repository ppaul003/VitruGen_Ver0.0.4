#ifndef PARTICLE_SIM_RUNTIME_CONFIG_H
#define PARTICLE_SIM_RUNTIME_CONFIG_H

#include <cmath>

#include "TheArbiter.h"

struct ParticleSimRuntimeConfig {
	static constexpr float kMaximumSupportedRadius = 0.0156f;

	unsigned int capacity = 0;
	unsigned int activeCount = 0;
	unsigned int redCount = 0;
	unsigned int greenCount = 0;
	unsigned int blueCount = 0;

	float uniformRadius = 0.0120f;
	float minimumRadius = 0.0098f;
	float maximumRadius = 0.0156f;
	float placementRadius = 0.0120f;

	TheArbiter::ParticleColorMode colorMode =
		TheArbiter::ParticleColorMode::Default;

	TheArbiter::ParticleRadiusMode radiusMode =
		TheArbiter::ParticleRadiusMode::Uniform;

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

		const bool uniformRadiusIsValid =
			std::isfinite(draft.uniformRadius) &&
			draft.uniformRadius > 0.0f &&
			draft.uniformRadius <= kMaximumSupportedRadius;

		const bool randomRadiusRangeIsValid =
			std::isfinite(draft.minimumRadius) &&
			std::isfinite(draft.maximumRadius) &&
			draft.minimumRadius > 0.0f &&
			draft.minimumRadius <= draft.maximumRadius &&
			draft.maximumRadius <= kMaximumSupportedRadius;

		if (draft.radiusMode == TheArbiter::ParticleRadiusMode::Uniform) {
			if (!uniformRadiusIsValid)
				return false;
		}
		else if (draft.radiusMode == TheArbiter::ParticleRadiusMode::Random) {
			if (!randomRadiusRangeIsValid)
				return false;
		}
		else {
			return false;
		}

		resolved.capacity = allocatedCapacity;
		resolved.activeCount =
			static_cast<unsigned int>(requestedCount);

		resolved.colorMode = draft.colorMode;
		resolved.radiusMode = draft.radiusMode;
		resolved.resetMode = draft.resetMode;
		resolved.uniformRadius = draft.uniformRadius;
		resolved.minimumRadius = draft.minimumRadius;
		resolved.maximumRadius = draft.maximumRadius;
		resolved.placementRadius =
			draft.radiusMode == TheArbiter::ParticleRadiusMode::Random
			? draft.maximumRadius
			: draft.uniformRadius;

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
