#include "TheArbiter.h"

#include <cstdlib>
#include <iostream>

namespace {
	void require(bool condition, const char* message) {
		if (condition) return;

		std::cerr << "FAIL: " << message << std::endl;
		std::exit(EXIT_FAILURE);
	}

	KeyboardInput::KeyEvent key(KeyboardInput::KeySignal signal) {
		KeyboardInput::KeyEvent event;
		event.signal = signal;
		return event;
	}

	TheArbiter::ArbiterResult send(
		TheArbiter& arbiter,
		KeyboardInput::KeySignal signal) {
		return arbiter.processKeyboard(key(signal));
	}

	void verifyCatalog() {
		for (int value = 0;
			value < static_cast<int>(TheArbiter::WorkspaceId::COUNT);
			++value) {
			const TheArbiter::WorkspaceId workspace =
				static_cast<TheArbiter::WorkspaceId>(value);

			const TheArbiter::WorkspaceDescriptor& descriptor =
				TheArbiter::describeWorkspace(workspace);

			require(
				descriptor.id == workspace,
				"Every WorkspaceId must resolve to its own descriptor."
			);
			require(
				TheArbiter::workspaceBelongsToDomain(
					workspace,
					descriptor.domain
				),
				"Workspace descriptor domain mapping must be stable."
			);
		}
	}

	void verifySingleParticleRoute() {
		TheArbiter arbiter;

		require(arbiter.isMenuLayer(), "Arbiter must start at Layer 0.");
		require(arbiter.isIdleSelected(), "Layer 0 must start on IDLE.");
		require(
			arbiter.getSelectedDomain() == TheArbiter::WorkspaceDomain::GRID_3D,
			"The preserved default workspace domain must be GRID_3D."
		);
		require(
			arbiter.getSelectedWorkspace() == TheArbiter::WorkspaceId::GRAPH_3D,
			"The preserved default workspace must be GRAPH_3D."
		);

		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.is3DGridSelected(), "D must select workspace entry.");

		send(arbiter, KeyboardInput::KEY_E);
		require(
			arbiter.isEnvironmentConfigLayer(),
			"E must enter the legacy Layer 1 route."
		);

		send(arbiter, KeyboardInput::KEY_D);
		require(
			arbiter.getSelectedDomain() == TheArbiter::WorkspaceDomain::GRID_3D,
			"SINGLE_PARTICLE_MCAD must belong to GRID_3D."
		);
		require(
			arbiter.getSelectedWorkspace() ==
				TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD,
			"The legacy single-particle selection must map to SINGLE_PARTICLE_MCAD."
		);

		send(arbiter, KeyboardInput::KEY_E);
		require(
			arbiter.isParticleConfigLayer(),
			"SINGLE_PARTICLE_MCAD must enter Layer 2 configuration."
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_S);

		const TheArbiter::ArbiterResult enterResult =
			send(arbiter, KeyboardInput::KEY_E);

		require(
			arbiter.isSimulationRunLayer(),
			"SINGLE_PARTICLE_MCAD RUN must enter Layer 3."
		);
		require(
			enterResult.command == TheArbiter::CMD_PLACE_SINGLE_PARTICLE,
			"SINGLE_PARTICLE_MCAD RUN must preserve its engine command."
		);

		send(arbiter, KeyboardInput::KEY_Q);
		require(
			arbiter.isParticleConfigLayer(),
			"Q at Single Particle sub-layer 0 must return to Layer 2."
		);
	}

	void verifyParticleSimulationRoute() {
		TheArbiter arbiter;

		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_E);
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_D);

		require(
			arbiter.getSelectedDomain() == TheArbiter::WorkspaceDomain::SIMCAD_4D,
			"The original particle simulation must map to SIMCAD_4D."
		);
		require(
			arbiter.getSelectedWorkspace() ==
				TheArbiter::WorkspaceId::PARTICLE_SIMULATION,
			"The legacy particle route must map to PARTICLE_SIMULATION."
		);
		require(
			arbiter.getWorkspaceSelection(TheArbiter::WorkspaceDomain::GRID_3D) ==
				TheArbiter::WorkspaceId::SINGLE_PARTICLE_MCAD,
			"GRID_3D must remember its last workspace selection."
		);

		send(arbiter, KeyboardInput::KEY_E);
		require(
			arbiter.isParticleConfigLayer(),
			"PARTICLE_SIMULATION must enter Layer 2 configuration."
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_S);

		const TheArbiter::ArbiterResult enterResult =
			send(arbiter, KeyboardInput::KEY_E);

		require(
			arbiter.isSimulationRunLayer(),
			"PARTICLE_SIMULATION RUN must enter Layer 3."
		);
		require(
			enterResult.command == TheArbiter::CMD_START_CUDA_SIMULATION,
			"PARTICLE_SIMULATION RUN must preserve its engine command."
		);

		send(arbiter, KeyboardInput::KEY_Q);
		require(
			arbiter.isParticleConfigLayer(),
			"Q must return PARTICLE_SIMULATION to Layer 2."
		);
	}
}

int main() {
	verifyCatalog();
	verifySingleParticleRoute();
	verifyParticleSimulationRoute();

	std::cout << "PASS: TheArbiter navigation regression suite" << std::endl;
	return EXIT_SUCCESS;
}
