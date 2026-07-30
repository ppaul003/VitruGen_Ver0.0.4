#include <cstdlib>
#include <iostream>

#include "TheArbiter.h"

namespace {
	int g_failures = 0;

	TheArbiter::ArbiterResult send(
		TheArbiter& arbiter,
		KeyboardInput::KeySignal signal) {

		KeyboardInput::KeyEvent event;
		event.signal = signal;
		return arbiter.processKeyboard(event);
	}

	void expect(bool condition, const char* message) {
		if (condition) return;

		std::cerr << "FAIL: " << message << '\n';
		g_failures++;
	}

	void enterParticleSimLayer1(TheArbiter& arbiter) {
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_D);
		send(arbiter, KeyboardInput::KEY_E);

		expect(
			arbiter.getSelectedDomain() ==
				TheArbiter::WorkspaceDomain::SIMCAD_4D,
			"SIMCAD_4D should be selected"
		);

		expect(
			arbiter.isParticleSimLayer1PanelContext(),
			"PARTICLE_SIM Layer 1 panel should be active"
		);
	}

	void selectLayer1Row(
		TheArbiter& arbiter,
		TheArbiter::ParticleSimLayer1Item target) {

		for (int guard = 0; guard < 5; guard++) {
			if (arbiter.getParticleSimLayer1Selection() == target) {
				return;
			}

			send(arbiter, KeyboardInput::KEY_S);
		}
	}

	void enterParticleSimLayer2(TheArbiter& arbiter) {
		selectLayer1Row(
			arbiter,
			TheArbiter::ParticleSimLayer1Item::Configure
		);

		send(arbiter, KeyboardInput::KEY_E);

		expect(
			arbiter.getApplicationLayer() ==
				TheArbiter::ApplicationLayer::WORKSPACE_CONFIGURATION,
			"Layer 1 row 5 should open Layer 2"
		);
	}

	void testLayer1NavigationAndDraftModes() {
		TheArbiter arbiter;
		enterParticleSimLayer1(arbiter);

		expect(
			arbiter.getParticleSimLayer1Selection() ==
				TheArbiter::ParticleSimLayer1Item::Workspace,
			"Layer 1 should begin on the workspace row"
		);

		send(arbiter, KeyboardInput::KEY_E);
		expect(
			arbiter.getApplicationLayer() ==
				TheArbiter::ApplicationLayer::DOMAIN_SELECTION,
			"E on Layer 1 rows 1-4 must not advance"
		);

		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getSelectedWorkspace() ==
				TheArbiter::WorkspaceId::SANDBOX_SIM,
			"Layer 1 workspace row should expose SANDBOX_SIM"
		);

		send(arbiter, KeyboardInput::KEY_A);
		expect(
			arbiter.isParticleSimulationSelected(),
			"Layer 1 workspace row should return to PARTICLE_SIM"
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().gridLayout ==
				TheArbiter::ParticleGridLayout::None,
			"Grid layout should cycle DYNAMIC -> NONE"
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().colorMode ==
				TheArbiter::ParticleColorMode::RGB,
			"Color mode should cycle DEFAULT -> RGB"
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().radiusMode ==
				TheArbiter::ParticleRadiusMode::Random,
			"Radius mode should cycle UNIFORM -> RANDOM"
		);

		send(arbiter, KeyboardInput::KEY_S);
		expect(
			arbiter.getParticleSimLayer1Selection() ==
				TheArbiter::ParticleSimLayer1Item::Configure,
			"W/S should visit all five Layer 1 rows"
		);
	}

	void testDefaultCountAndEntryRequest() {
		TheArbiter arbiter;
		enterParticleSimLayer1(arbiter);
		enterParticleSimLayer2(arbiter);

		expect(
			arbiter.getParticleSimLayer2RowCount() == 4,
			"DEFAULT + UNIFORM should expose four rows"
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().resetMode ==
				TheArbiter::ParticleSimResetMode::Random,
			"Reset row should toggle DEFAULT -> RANDOM"
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().uniformRadius ==
				0.0127f,
			"Uniform radius should advance through the preset table"
		);

		send(arbiter, KeyboardInput::KEY_W);
		send(arbiter, KeyboardInput::KEY_W);

		for (int index = 0; index < 100; index++) {
			send(arbiter, KeyboardInput::KEY_A);
		}

		expect(
			arbiter.getParticleSimDraftConfig().defaultParticleCount == 0,
			"Default particle count should clamp at zero"
		);

		for (int index = 0; index < 200; index++) {
			send(arbiter, KeyboardInput::KEY_D);
		}

		expect(
			arbiter.getParticleSimDraftConfig().defaultParticleCount ==
				TheArbiter::kParticleSimCapacity,
			"Default particle count should clamp at capacity"
		);

		const TheArbiter::ArbiterResult result =
			send(arbiter, KeyboardInput::KEY_ENTER);

		expect(
			result.command ==
				TheArbiter::CMD_REQUEST_DEFAULT_PARTICLE_COUNT_ENTRY,
			"Enter on the default-count row should request future text entry"
		);

		expect(
			arbiter.getParticleCountEntryRequest().requested,
			"The default-count entry request should be visible to ViewPort"
		);
	}

	void testRGBChannelAndTotals() {
		TheArbiter arbiter;
		enterParticleSimLayer1(arbiter);

		selectLayer1Row(
			arbiter,
			TheArbiter::ParticleSimLayer1Item::ColorMode
		);
		send(arbiter, KeyboardInput::KEY_D);
		enterParticleSimLayer2(arbiter);

		expect(
			arbiter.getParticleSimLayer2RowCount() == 4,
			"RGB + UNIFORM should expose four rows"
		);

		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().selectedColorChannel ==
				TheArbiter::ParticleColorChannel::Green,
			"RGB row A/D should cycle RED -> GREEN"
		);

		send(arbiter, KeyboardInput::KEY_D);
		expect(
			arbiter.getParticleSimDraftConfig().selectedColorChannel ==
				TheArbiter::ParticleColorChannel::Blue,
			"RGB row A/D should cycle GREEN -> BLUE"
		);

		const TheArbiter::ArbiterResult result =
			send(arbiter, KeyboardInput::KEY_ENTER);

		expect(
			result.command ==
				TheArbiter::CMD_REQUEST_RGB_PARTICLE_COUNT_ENTRY,
			"Enter on the RGB-count row should request future text entry"
		);

		expect(
			arbiter.getParticleCountEntryRequest().channel ==
				TheArbiter::ParticleColorChannel::Blue,
			"RGB entry request should preserve the selected channel"
		);

		expect(
			arbiter.getParticleSimRGBTotal() == 0,
			"RGB totals should remain presentation-only defaults"
		);
	}

	void testRGBRandomConditionalLayout() {
		TheArbiter arbiter;
		enterParticleSimLayer1(arbiter);

		selectLayer1Row(
			arbiter,
			TheArbiter::ParticleSimLayer1Item::ColorMode
		);
		send(arbiter, KeyboardInput::KEY_D);

		selectLayer1Row(
			arbiter,
			TheArbiter::ParticleSimLayer1Item::RadiusMode
		);
		send(arbiter, KeyboardInput::KEY_D);

		enterParticleSimLayer2(arbiter);

		expect(
			arbiter.getParticleSimDraftConfig().colorMode ==
				TheArbiter::ParticleColorMode::RGB,
			"RGB draft mode should survive entry into Layer 2"
		);

		expect(
			arbiter.getParticleSimDraftConfig().radiusMode ==
				TheArbiter::ParticleRadiusMode::Random,
			"Random-radius draft mode should survive entry into Layer 2"
		);

		expect(
			arbiter.getParticleSimLayer2RowCount() == 5,
			"RGB + RANDOM should expose five rows"
		);
	}

	void testRandomRadiusOrderingAndRowClamp() {
		TheArbiter arbiter;
		enterParticleSimLayer1(arbiter);

		selectLayer1Row(
			arbiter,
			TheArbiter::ParticleSimLayer1Item::RadiusMode
		);
		send(arbiter, KeyboardInput::KEY_D);
		enterParticleSimLayer2(arbiter);

		expect(
			arbiter.getParticleSimLayer2RowCount() == 5,
			"DEFAULT + RANDOM should expose five rows"
		);

		send(arbiter, KeyboardInput::KEY_S);
		send(arbiter, KeyboardInput::KEY_S);

		for (int index = 0; index < 30; index++) {
			send(arbiter, KeyboardInput::KEY_D);
		}

		expect(
			arbiter.getParticleSimDraftConfig().minimumRadius <=
				arbiter.getParticleSimDraftConfig().maximumRadius,
			"Minimum radius must not exceed maximum radius"
		);

		send(arbiter, KeyboardInput::KEY_S);

		for (int index = 0; index < 30; index++) {
			send(arbiter, KeyboardInput::KEY_A);
		}

		expect(
			arbiter.getParticleSimDraftConfig().maximumRadius >=
				arbiter.getParticleSimDraftConfig().minimumRadius,
			"Maximum radius must not fall below minimum radius"
		);

		send(arbiter, KeyboardInput::KEY_S);
		expect(
			arbiter.getParticleSimLayer2Selection() == 4,
			"Random-radius run row should be row five"
		);

		send(arbiter, KeyboardInput::KEY_Q);
		selectLayer1Row(
			arbiter,
			TheArbiter::ParticleSimLayer1Item::RadiusMode
		);
		send(arbiter, KeyboardInput::KEY_D);

		expect(
			arbiter.getParticleSimLayer2RowCount() == 4,
			"UNIFORM mode should restore the four-row layout"
		);

		expect(
			arbiter.getParticleSimLayer2Selection() == 3,
			"Conditional layout change should clamp the selected row"
		);
	}

	void testRunAndSingleParticleRegression() {
		TheArbiter particleSim;
		enterParticleSimLayer1(particleSim);
		enterParticleSimLayer2(particleSim);

		send(particleSim, KeyboardInput::KEY_W);
		expect(
			particleSim.isParticleSimLayer2RunSelected(),
			"W should wrap from row one to the run row"
		);

		const TheArbiter::ArbiterResult runResult =
			send(particleSim, KeyboardInput::KEY_E);

		expect(
			runResult.command ==
				TheArbiter::CMD_START_PARTICLE_SIMULATION,
			"Run row should preserve CMD_START_PARTICLE_SIMULATION"
		);

		expect(
			particleSim.getApplicationLayer() ==
				TheArbiter::ApplicationLayer::ACTIVE_WORKSPACE,
			"Run row should enter the active workspace"
		);

		TheArbiter singleParticle;
		send(singleParticle, KeyboardInput::KEY_D);
		send(singleParticle, KeyboardInput::KEY_D);
		send(singleParticle, KeyboardInput::KEY_E);
		send(singleParticle, KeyboardInput::KEY_D);
		send(singleParticle, KeyboardInput::KEY_E);

		expect(
			singleParticle.isSingleParticleSelected(),
			"SINGLE_PARTICLE selection should remain available"
		);

		expect(
			singleParticle.getApplicationLayer() ==
				TheArbiter::ApplicationLayer::WORKSPACE_CONFIGURATION,
			"SINGLE_PARTICLE should retain its existing configuration layer"
		);

		send(singleParticle, KeyboardInput::KEY_S);
		expect(
			singleParticle.getActiveParticleConfigList() ==
				TheArbiter::PARTICLE_LIST_RADIUS,
			"SINGLE_PARTICLE W/S navigation should remain unchanged"
		);
	}
}

int main() {
	testLayer1NavigationAndDraftModes();
	testDefaultCountAndEntryRequest();
	testRGBChannelAndTotals();
	testRGBRandomConditionalLayout();
	testRandomRadiusOrderingAndRowClamp();
	testRunAndSingleParticleRegression();

	if (g_failures != 0) {
		std::cerr << g_failures
			<< " PARTICLE_SIM panel test(s) failed.\n";
		return EXIT_FAILURE;
	}

	std::cout
		<< "PASS: PARTICLE_SIM panel navigation and draft-state tests\n";

	return EXIT_SUCCESS;
}
