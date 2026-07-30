#include "MvpAssetPipeline.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
	void require(bool condition, const char* message) {
		if (condition) return;
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(EXIT_FAILURE);
	}

	vitru::MeshGeometry cubeMesh() {
		using vitru::Vec3;
		vitru::MeshGeometry mesh;
		mesh.positions = {
			{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f},
			{-0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
			{-0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
			{-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
			{-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f},
			{-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f},
			{ 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f},
			{ 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
			{-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
			{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f},
			{-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
			{-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}
		};
		return mesh;
	}

	vitru::StaticParticleAsset makeStatic(const char* name, const vitru::ColorRGBA8& color) {
		vitru::StaticParticleAsset asset;
		asset.name = name;
		asset.mesh = cubeMesh();
		const vitru::MeshProcessingReport report = vitru::processMesh(asset.mesh);
		require(report.valid, "Cube mesh must process successfully.");
		asset.material.name = std::string(name) + " Material";
		asset.material.baseColor = color;
		asset.texture.create(32, 32, color);
		return asset;
	}

	void verifyMeshProcessing() {
		vitru::MeshGeometry mesh = cubeMesh();
		mesh.positions.insert(mesh.positions.end(), mesh.positions.begin(), mesh.positions.begin() + 3);
		mesh.positions.push_back({ 0.0f, 0.0f, 0.0f });
		mesh.positions.push_back({ 0.0f, 0.0f, 0.0f });
		mesh.positions.push_back({ 0.0f, 0.0f, 0.0f });
		mesh.positions.push_back({ std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f });
		mesh.positions.push_back({ 1.0f, 0.0f, 0.0f });
		mesh.positions.push_back({ 0.0f, 1.0f, 0.0f });

		const vitru::MeshProcessingReport report = vitru::processMesh(mesh);
		require(report.valid, "Processed mesh must be valid.");
		require(report.outputTriangles == 12, "Cube must retain its twelve unique triangles.");
		require(report.removedDuplicate == 1, "Duplicate triangle must be detected.");
		require(report.removedDegenerate == 1, "Degenerate triangle must be removed.");
		require(report.removedNonFinite == 1, "NaN triangle must be removed.");
		require(mesh.positions.size() == 8, "Vertex welding must create an indexed eight-vertex cube.");
		require(mesh.indices.size() == 36, "Indexed cube must contain 36 indices.");
		require(mesh.normals.size() == mesh.positions.size(), "Every processed vertex needs a normal.");
		require(mesh.uvs.size() == mesh.positions.size(), "Every processed vertex needs a UV.");
	}

	void verifyTextureAuthoring() {
		vitru::SurfaceTexture texture;
		require(texture.create(16, 16, { 10, 20, 30, 255 }), "Texture creation must succeed.");
		texture.paintCircle(8, 8, 3, { 255, 0, 0, 255 });
		require(texture.pixel(8, 8) && texture.pixel(8, 8)->r == 255,
			"Paint brush must modify the selected texel.");
		texture.eraseCircle(8, 8, 1);
		require(texture.pixel(8, 8) && texture.pixel(8, 8)->a == 0,
			"Erase brush must clear alpha.");
		require(texture.fillRegion(0, 0, { 0, 255, 0, 255 }),
			"Region fill must succeed.");
		require(texture.pixel(0, 0) && texture.pixel(0, 0)->g == 255,
			"Region fill must replace the connected color.");

		std::string error;
		const char* imagePath = "mvp_texture_roundtrip.ppm";
		require(texture.savePPM(imagePath, &error), "Texture PPM save must succeed.");
		vitru::SurfaceTexture imported;
		require(imported.importPPM(imagePath, &error), "Texture PPM import must succeed.");
		require(imported.width == texture.width && imported.height == texture.height,
			"Imported texture dimensions must match.");
		std::remove(imagePath);

		const char* bmpPath = "mvp_texture_import.bmp";
		std::vector<unsigned char> bmp(70u, 0u);
		auto write16 = [&](std::size_t offset, std::uint16_t value) {
			bmp[offset] = static_cast<unsigned char>(value & 0xffu);
			bmp[offset + 1u] = static_cast<unsigned char>((value >> 8u) & 0xffu);
		};
		auto write32 = [&](std::size_t offset, std::uint32_t value) {
			for (unsigned int i = 0; i < 4; ++i)
				bmp[offset + i] = static_cast<unsigned char>((value >> (i * 8u)) & 0xffu);
		};
		bmp[0] = 'B'; bmp[1] = 'M';
		write32(2, static_cast<std::uint32_t>(bmp.size()));
		write32(10, 54); write32(14, 40); write32(18, 2); write32(22, 2);
		write16(26, 1); write16(28, 24); write32(34, 16);
		// BMP stores BGR and positive-height rows bottom-up.
		bmp[54] = 255; bmp[55] = 0; bmp[56] = 0;
		bmp[57] = 255; bmp[58] = 255; bmp[59] = 255;
		bmp[62] = 0; bmp[63] = 0; bmp[64] = 255;
		bmp[65] = 0; bmp[66] = 255; bmp[67] = 0;
		{
			std::ofstream output(bmpPath, std::ios::binary);
			output.write(reinterpret_cast<const char*>(bmp.data()), bmp.size());
		}
		vitru::SurfaceTexture bmpImported;
		require(bmpImported.importImage(bmpPath, &error),
			"Texture BMP import must succeed.");
		require(bmpImported.width == 2 && bmpImported.height == 2,
			"Imported BMP dimensions must match its header.");
		require(bmpImported.pixel(0, 0) && bmpImported.pixel(0, 0)->r == 255,
			"BMP import must honor bottom-up row order and BGR channels.");
		std::remove(bmpPath);
	}

	void verifyEndToEndPipeline() {
		vitru::ProjectAssetRepository repository;
		const vitru::AssetId hullId = repository.addStaticParticle(
			makeStatic("Hull", { 75, 115, 70, 255 }));
		const vitru::AssetId turretId = repository.addStaticParticle(
			makeStatic("Turret", { 95, 130, 85, 255 }));
		const vitru::AssetId barrelId = repository.addStaticParticle(
			makeStatic("Barrel", { 70, 70, 75, 255 }));

		vitru::LinkedAssembly assembly;
		assembly.name = "MVP Test Vehicle";

		vitru::AssemblyNode hull;
		hull.type = vitru::AssemblyNodeType::MESH;
		hull.name = "Hull";
		hull.staticAssetId = hullId;
		const vitru::NodeId hullNode = assembly.addNode(hull);

		vitru::AssemblyNode fixed;
		fixed.type = vitru::AssemblyNodeType::JOINT;
		fixed.jointType = vitru::JointType::FIXED;
		fixed.name = "Turret Mount";
		fixed.parentId = hullNode;
		fixed.localTransform.position = { 0.0f, 0.75f, 0.0f };
		const vitru::NodeId fixedNode = assembly.addNode(fixed);

		vitru::AssemblyNode turret;
		turret.type = vitru::AssemblyNodeType::MESH;
		turret.name = "Turret";
		turret.parentId = fixedNode;
		turret.staticAssetId = turretId;
		const vitru::NodeId turretNode = assembly.addNode(turret);

		vitru::AssemblyNode revolute;
		revolute.type = vitru::AssemblyNodeType::JOINT;
		revolute.jointType = vitru::JointType::REVOLUTE;
		revolute.name = "Gun Elevation";
		revolute.parentId = turretNode;
		revolute.jointAxis = { 1.0f, 0.0f, 0.0f };
		revolute.jointMinimumDegrees = -10.0f;
		revolute.jointMaximumDegrees = 35.0f;
		revolute.localTransform.position = { 0.0f, 0.0f, -0.75f };
		const vitru::NodeId revoluteNode = assembly.addNode(revolute);

		vitru::AssemblyNode barrel;
		barrel.type = vitru::AssemblyNodeType::MESH;
		barrel.name = "Barrel";
		barrel.parentId = revoluteNode;
		barrel.staticAssetId = barrelId;
		barrel.localTransform.position = { 0.0f, 0.0f, -1.5f };
		barrel.localTransform.scale = { 0.25f, 0.25f, 3.0f };
		const vitru::NodeId barrelNode = assembly.addNode(barrel);

		vitru::AssemblyNode muzzle;
		muzzle.type = vitru::AssemblyNodeType::INTERACTION;
		muzzle.name = "Muzzle";
		muzzle.parentId = barrelNode;
		muzzle.interactionRole = vitru::InteractionRole::WEAPON_MUZZLE;
		muzzle.localTransform.position = { 0.0f, 0.0f, -0.75f };
		assembly.addNode(muzzle);

		vitru::AssemblyNode propulsion;
		propulsion.type = vitru::AssemblyNodeType::INTERACTION;
		propulsion.name = "Left Track";
		propulsion.parentId = hullNode;
		propulsion.interactionRole = vitru::InteractionRole::LEFT_PROPULSION_TRACK;
		propulsion.localTransform.position = { -1.0f, -0.5f, 0.0f };
		assembly.addNode(propulsion);

		vitru::AssemblyNode contact;
		contact.type = vitru::AssemblyNodeType::INTERACTION;
		contact.name = "Ground Contact";
		contact.parentId = hullNode;
		contact.interactionRole = vitru::InteractionRole::CONTACT;
		contact.localTransform.position = { 0.0f, -0.5f, 0.0f };
		assembly.addNode(contact);

		vitru::AnimationClip recoil;
		recoil.name = "RECOIL";
		recoil.durationSeconds = 0.4f;
		recoil.looping = false;
		vitru::JointAnimationTrack track;
		track.jointNodeId = revoluteNode;
		track.keyframes = { {0.0f, 0.0f}, {0.15f, 8.0f}, {0.4f, 0.0f} };
		recoil.tracks.push_back(track);
		assembly.clips.push_back(recoil);

		const vitru::AssetId assemblyId = repository.addAssembly(assembly);
		const vitru::BakeResult bake = repository.bakeAndStore(assemblyId, "MVP Test Vehicle Runtime");
		if (!bake.success) {
			for (const vitru::ValidationIssue& issue : bake.issues) {
				std::cerr << issue.code << ": " << issue.message << std::endl;
			}
		}
		require(bake.success, "Valid assembly must bake.");
		require(bake.asset.runtimeReady, "Baked asset must be runtime-ready.");
		require(bake.asset.embeddedStaticAssets.size() == 3,
			"Bake must embed each referenced static asset once.");
		require(bake.asset.bounds.valid, "Bake must calculate assembly bounds.");

		vitru::SandboxRuntime sandbox;
		sandbox.configureDefaultEnvironment();
		require(sandbox.spawn(bake.asset, {}), "Baked asset must spawn in the sandbox.");
		require(sandbox.playAnimation("RECOIL"), "Named animation must play at runtime.");
		sandbox.update(0.15f);
		require(sandbox.setJoint(revoluteNode, 999.0f), "Joint control must find the revolute joint.");
		sandbox.drive(1.0f, 0.25f, 0.5f);
		require(sandbox.instance().rootTransform.position.z != 0.0f,
			"Drive control must move the runtime asset.");
		sandbox.reset();
		require(sandbox.fireProjectile(), "Weapon emitter must fire a projectile.");
		for (int i = 0; i < 120; ++i) sandbox.update(1.0f / 60.0f);
		require(sandbox.hitCount() >= 1, "Projectile must hit a sandbox target.");

		std::string error;
		const char* projectPath = "mvp_project_roundtrip.vitru";
		require(repository.saveProject(projectPath, &error), "Project save must succeed.");
		vitru::ProjectAssetRepository reopened;
		require(reopened.loadProject(projectPath, &error), "Saved project must reopen.");
		require(reopened.staticParticles().size() == 3,
			"Reopened project must preserve reusable static particles.");
		require(reopened.assemblies().size() == 1,
			"Reopened project must preserve editable assemblies.");
		require(reopened.kinematicParticles().size() == 1,
			"Reopened project must preserve baked runtime particles.");
		const char* corruptPath = "mvp_project_truncated.vitru";
		{
			std::ifstream source(projectPath, std::ios::binary);
			std::ofstream corrupt(corruptPath, std::ios::binary);
			char prefix[16]{};
			source.read(prefix, sizeof(prefix));
			corrupt.write(prefix, source.gcount());
		}
		require(!reopened.loadProject(corruptPath, &error),
			"A truncated project must be rejected without crashing.");
		require(reopened.staticParticles().size() == 3,
			"A failed project load must leave the active repository unchanged.");
		std::remove(projectPath);
		std::remove(corruptPath);

		vitru::LinkedAssembly invalid = assembly;
		invalid.id = vitru::INVALID_ASSET_ID;
		invalid.nodes[0].parentId = invalid.nodes.back().id;
		const vitru::AssetId invalidId = repository.addAssembly(invalid);
		const vitru::BakeResult rejected = repository.validateAndBake(invalidId, "Invalid");
		require(!rejected.success && !rejected.issues.empty(),
			"Bake validation must reject a cyclic assembly with no valid root.");
	}
}

int main() {
	verifyMeshProcessing();
	verifyTextureAuthoring();
	verifyEndToEndPipeline();
	std::cout << "PASS: VitruGen MVP asset-pipeline regression suite" << std::endl;
	return EXIT_SUCCESS;
}
