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

	void enterWorkspaceMenu(TheArbiter& arbiter) {
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_E);
		require(arbiter.isEnvironmentConfigLayer(),
			"Workspace-domain entry must reach Layer 1.");
	}

	void verifyCatalogAvailability() {
		require(TheArbiter::getWorkspaceAvailability(
			TheArbiter::WorkspaceId::TEXTURE_MAP_2D) ==
			TheArbiter::WorkspaceAvailability::AVAILABLE,
			"Texture workspace must be available.");
		require(TheArbiter::getWorkspaceAvailability(
			TheArbiter::WorkspaceId::LINKED_PARTICLES_MCAD) ==
			TheArbiter::WorkspaceAvailability::AVAILABLE,
			"Linked-particle workspace must be available.");
		require(TheArbiter::getWorkspaceAvailability(
			TheArbiter::WorkspaceId::SANDBOX_SIM) ==
			TheArbiter::WorkspaceAvailability::AVAILABLE,
			"Sandbox workspace must be available.");
	}

	void verifyLegacyRouteAndMvpCycle() {
		TheArbiter arbiter;
		enterWorkspaceMenu(arbiter);

		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isSingleParticleSelected(),
			"First D must preserve the legacy Single Particle route.");
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isParticlesSelected(),
			"Second D must preserve the legacy particle-simulation route.");
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isTextureMapSelected(),
			"MVP cycle must expose TEXTURE_MAP_2D.");
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isLinkedParticlesSelected(),
			"MVP cycle must expose LINKED_PARTICLES_MCAD.");
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isSandboxSelected(),
			"MVP cycle must expose SANDBOX_SIM.");
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isGraphSelected(),
			"MVP workspace cycle must wrap to GRAPH_3D.");
	}

	void verifyTextureWorkspaceCommands() {
		TheArbiter arbiter;
		enterWorkspaceMenu(arbiter);
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isTextureMapSelected(), "Texture workspace must be selected.");
		send(arbiter, KeyboardInput::KEY_E);
		require(arbiter.isParticleConfigLayer(), "Texture workspace must use Layer 2 configuration.");
		const TheArbiter::ArbiterResult open = send(arbiter, KeyboardInput::KEY_E);
		require(arbiter.isSimulationRunLayer(), "Texture workspace must enter Layer 3.");
		require(open.command == TheArbiter::CMD_OPEN_TEXTURE_MAP_2D,
			"Texture workspace entry must emit its engine command.");

		send(arbiter, KeyboardInput::KEY_S);
		const int colorBefore = arbiter.getTextureColorIndex();
		send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.getTextureColorIndex() != colorBefore,
			"Texture color panel must cycle its palette.");

		// W from the color item reaches brush, then W wraps to Apply/Return.
		send(arbiter, KeyboardInput::KEY_W);
		send(arbiter, KeyboardInput::KEY_W);
		const TheArbiter::ArbiterResult apply = send(arbiter, KeyboardInput::KEY_E);
		require(apply.command == TheArbiter::CMD_TEXTURE_APPLY,
			"Apply/Return must emit the texture round-trip command.");
		require(arbiter.isSingleParticleSelected(),
			"Applying a texture must return to SINGLE_PARTICLE_MCAD.");
	}

	void verifyLinkedAndSandboxCommands() {
		TheArbiter arbiter;
		enterWorkspaceMenu(arbiter);
		for (int i = 0; i < 4; ++i) send(arbiter, KeyboardInput::KEY_D);
		require(arbiter.isLinkedParticlesSelected(), "Linked workspace must be selected.");
		send(arbiter, KeyboardInput::KEY_E);
		const TheArbiter::ArbiterResult openLinked = send(arbiter, KeyboardInput::KEY_E);
		require(openLinked.command == TheArbiter::CMD_OPEN_LINKED_PARTICLES_MCAD,
			"Linked workspace entry must emit its engine command.");
		const TheArbiter::ArbiterResult create = send(arbiter, KeyboardInput::KEY_E);
		require(create.command == TheArbiter::CMD_LINK_CREATE_ASSEMBLY,
			"First linked action must create an editable assembly.");

		for (int i = 0; i < 8; ++i) send(arbiter, KeyboardInput::KEY_S);
		const TheArbiter::ArbiterResult openSandbox = send(arbiter, KeyboardInput::KEY_E);
		require(openSandbox.command == TheArbiter::CMD_OPEN_SANDBOX_SIM,
			"Linked pipeline must transition to the sandbox.");
		require(arbiter.isSandboxSelected(), "Sandbox must become the active workspace identity.");

		require(send(arbiter, KeyboardInput::KEY_W).command ==
			TheArbiter::CMD_SANDBOX_DRIVE_FORWARD,
			"Sandbox W key must drive forward.");
		require(send(arbiter, KeyboardInput::KEY_E).command ==
			TheArbiter::CMD_SANDBOX_FIRE,
			"Sandbox E key must fire.");
		require(send(arbiter, KeyboardInput::KEY_R).command ==
			TheArbiter::CMD_SANDBOX_RESET,
			"Sandbox R key must reset.");
		require(send(arbiter, KeyboardInput::KEY_Z).command ==
			TheArbiter::CMD_SANDBOX_JOINT_DECREASE,
			"Sandbox Z key must directly decrease a joint value.");
		require(send(arbiter, KeyboardInput::KEY_X).command ==
			TheArbiter::CMD_SANDBOX_JOINT_INCREASE,
			"Sandbox X key must directly increase a joint value.");
		require(send(arbiter, KeyboardInput::KEY_V).command ==
			TheArbiter::CMD_SANDBOX_TOGGLE_FOLLOW_CAMERA,
			"Sandbox V key must toggle follow-camera mode.");
	}

	void verifyLinkedEditingCommands() {
		TheArbiter arbiter;
		enterWorkspaceMenu(arbiter);
		for (int i = 0; i < 4; ++i) send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_E);
		send(arbiter, KeyboardInput::KEY_E);
		for (int i = 0; i < 9; ++i) send(arbiter, KeyboardInput::KEY_S);
		require(send(arbiter, KeyboardInput::KEY_D).command ==
			TheArbiter::CMD_LINK_ADJUST,
			"Linked node selection must emit an adjustment command.");
		for (int i = 0; i < 11; ++i) send(arbiter, KeyboardInput::KEY_S);
		require(send(arbiter, KeyboardInput::KEY_E).command ==
			TheArbiter::CMD_LINK_PREVIEW_ANIMATION,
			"Linked editor must expose animation preview playback.");
		send(arbiter, KeyboardInput::KEY_S);
		require(send(arbiter, KeyboardInput::KEY_E).command ==
			TheArbiter::CMD_LINK_REMOVE_NODE,
			"Linked editor must expose selected-branch removal.");
	}
}

int main() {
	verifyCatalogAvailability();
	verifyLegacyRouteAndMvpCycle();
	verifyTextureWorkspaceCommands();
	verifyLinkedAndSandboxCommands();
	verifyLinkedEditingCommands();
	std::cout << "PASS: VitruGen MVP navigation regression suite" << std::endl;
	return EXIT_SUCCESS;
}
