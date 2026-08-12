#include "AssetPathResolver.h"
#include "JsonValue.h"
#include "MeshUVGenerator.h"
#include "ObjMtlImporter.h"
#include "PngImage.h"
#include "ProjectAssetRepository.h"
#include "StaticParticleAssetIO.h"
#include "TextureMapWorkspace.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
	if (condition) return;
	std::cerr << "FAIL: " << message << std::endl;
	std::exit(EXIT_FAILURE);
}

vitru::StaticParticleAsset cubeAsset(const std::string& name) {
	vitru::StaticParticleAsset asset; asset.name = name;
	asset.mesh.positions = {
		{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
		{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
	};
	asset.mesh.indices = {
		0,2,1,0,3,2,4,5,6,4,6,7,0,4,7,0,7,3,
		1,2,6,1,6,5,0,1,5,0,5,4,3,7,6,3,6,2
	};
	asset.mesh.normals.resize(asset.mesh.positions.size(), { 0,1,0 });
	asset.materials.emplace_back(); asset.refreshDerivedData(); return asset;
}

void writeText(const fs::path& path, const std::string& text) {
	std::ofstream output(path, std::ios::binary | std::ios::trunc); require(static_cast<bool>(output), "test file open"); output << text; require(output.good(), "test file write");
}

void verifyModelAndRepository() {
	require(vitru::sanitizeAssetName("CON") == "Asset_CON", "reserved Windows asset-name sanitization");
	require(vitru::sanitizeAssetName("Anaheim Test / 01") == "Anaheim_Test_01", "portable asset-name sanitization");
	vitru::StaticParticleAsset asset = cubeAsset("Valid"); std::vector<std::string> errors;
	require(asset.validate(&errors), "simple asset must validate");
	vitru::StaticParticleAsset invalid = asset; invalid.submeshes[0].indexCount = 999u; errors.clear(); require(!invalid.validate(&errors), "invalid submesh must be rejected");
	vitru::TextureResource texture; texture.id = "duplicate"; invalid = asset; invalid.textures = { texture, texture }; errors.clear(); require(!invalid.validate(&errors), "duplicate texture ID must be rejected");
	invalid = asset; invalid.mesh.clear(); errors.clear(); require(!invalid.validate(&errors), "missing mesh must be rejected");
	vitru::ProjectAssetRepository repository; const vitru::AssetId id = repository.addStaticParticle(asset); require(repository.findStaticParticle(id) != nullptr, "repository find"); require(repository.setActiveStaticParticle(id), "repository activate");
	vitru::StaticParticleAsset replacement = cubeAsset("Replacement"); require(repository.replaceStaticParticle(id, replacement), "repository replace"); require(repository.activeStaticParticle()->name == "Replacement", "replacement preserved active ID");
}

void verifyJson() {
	const std::string sample = "{\"a\":[true,false,null,-1.25e2],\"unicode\":\"\\u0041\\uD83D\\uDE80\"}";
	const vitru::JsonParseResult parsed = vitru::parseJson(sample); require(parsed.success, "standards-aware JSON parser");
	const std::string written = vitru::writeJson(parsed.value, 2); require(vitru::parseJson(written).success, "JSON writer output must parse");
	require(!vitru::parseJson("{\"broken\":]").success, "invalid JSON must be rejected");
}

void verifyUvAndPng(const fs::path& root) {
	vitru::StaticParticleAsset first = cubeAsset("UV"); const vitru::MeshUVGenerationReport report = vitru::generateBoxAtlasUVs(first.mesh); require(report.success, "box atlas generation"); require(first.mesh.uvs.size() == first.mesh.positions.size(), "one UV per render vertex"); require(report.seamDuplicates > 0u, "box atlas must duplicate seams");
	for (const vitru::Vec2& uv : first.mesh.uvs) require(std::isfinite(uv.x) && std::isfinite(uv.y) && uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f, "UV range");
	vitru::StaticParticleAsset second = cubeAsset("UV"); require(vitru::generateBoxAtlasUVs(second.mesh).success, "deterministic UV generation second run"); require(second.mesh.uvs.size() == first.mesh.uvs.size(), "deterministic UV vertex count");
	for (std::size_t i = 0; i < first.mesh.uvs.size(); ++i) require(first.mesh.uvs[i].x == second.mesh.uvs[i].x && first.mesh.uvs[i].y == second.mesh.uvs[i].y, "deterministic UV values");
	const fs::path guide = root / "guide.png"; std::string error; require(vitru::writeUvGuidePng(first.mesh, guide, 256u, &error), "UV guide PNG write"); require(fs::file_size(guide) > 100u, "UV guide must be non-empty");
	vitru::ImageRGBA8 loaded; require(vitru::loadPngImage(guide, loaded, &error, false), "PNG read"); require(loaded.width == 256u && loaded.height == 256u && loaded.valid(), "PNG dimensions and RGBA data");
}

void verifyObjMtl(const fs::path& root) {
	const fs::path assetRoot = root / "Imported"; fs::create_directories(assetRoot / "geometry"); fs::create_directories(assetRoot / "materials"); fs::create_directories(assetRoot / "textures" / "a"); fs::create_directories(assetRoot / "textures" / "b");
	std::string imageError; const vitru::ImageRGBA8 image = vitru::makeSolidImage(4, 4, 80, 120, 160); require(vitru::writePngImage(assetRoot / "textures" / "Texture With Spaces.png", image, &imageError), "texture fixture"); require(vitru::writePngImage(assetRoot / "textures" / "a" / "ambiguous.png", image, &imageError), "ambiguous fixture a"); require(vitru::writePngImage(assetRoot / "textures" / "b" / "ambiguous.png", image, &imageError), "ambiguous fixture b");
	writeText(assetRoot / "materials" / "Fixture Material.mtl",
		"newmtl first\nKd 1 1 1\nmap_Kd C:\\\\obsolete\\\\Texture With Spaces.png\n"
		"newmtl second\nKd 0.5 0.5 0.5\nmap_bump missing bump.png\n");
	writeText(assetRoot / "geometry" / "Fixture Mesh.obj",
		"mtllib Fixture Material.mtl\n"
		"o Fixture Object\ng first group\n"
		"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
		"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
		"vn 0 0 1\nusemtl first\nf 1/1/1 2/2/1 3/3/1\n"
		"usemtl second\nf -4/1/1 -2/3/1 -1/4/1 -3/2/1\n");
	vitru::StaticParticleAsset asset; vitru::ObjImportReport report;
	require(vitru::importObjStaticParticle(assetRoot / "geometry" / "Fixture Mesh.obj", assetRoot, asset, report), "OBJ/MTL import");
	require(asset.mesh.uvs.size() == asset.mesh.positions.size(), "imported UV preservation"); require(asset.submeshes.size() == 2u, "material draw ranges"); require(asset.mesh.triangleCount() == 3u, "polygon fan triangulation"); require(asset.materials.size() >= 3u, "MTL materials"); require(!asset.materials[1].baseColorTextureId.empty(), "legacy absolute diffuse resolution"); require(!asset.materials[2].heightTextureId.empty(), "bump classified as height");
	require(asset.mesh.positions.size() > 4u, "position/UV/normal/material tuple identity must split render vertices");
	const vitru::AssetPathResolution ambiguous = vitru::resolveAssetPath("C:\\\\old\\\\ambiguous.png", {}, {}, assetRoot); require(ambiguous.found && ambiguous.ambiguous, "ambiguous filename warning");
	bool missingWarning = false; for (const std::string& warning : report.warnings) if (warning.find("missing") != std::string::npos || warning.find("Missing") != std::string::npos) missingWarning = true; require(missingWarning, "missing texture warning");
}

void verifyBundleRoundTrip(const fs::path& root) {
	vitru::StaticParticleAsset asset = cubeAsset("Anaheim SP Test 01");
	vitru::TextureResource emissive;
	emissive.id = "tm_emissive";
	emissive.relativePath = "textures/Anaheim_SP_Test_01_emissive.png";
	emissive.usage = vitru::TextureUsage::Emissive;
	emissive.colorSpace = vitru::TextureColorSpace::SRGB;
	emissive.width = 8u; emissive.height = 8u; emissive.channels = 4u;
	emissive.pixels = vitru::makeSolidImage(8u, 8u, 20u, 40u, 80u, 255u).pixels;
	emissive.loaded = true; emissive.valid = true;
	asset.textures.push_back(emissive);
	asset.materials[0].emissiveTextureId = emissive.id;
	asset.materials[0].emissiveFactor[0] = 1.0f;
	asset.materials[0].emissiveFactor[1] = 1.0f;
	asset.materials[0].emissiveFactor[2] = 1.0f;
	asset.materials[0].emissiveIntensity = 2.25f;
	vitru::SurfaceTarget target;
	target.name = "eye_outer"; target.faceIndex = 4u;
	target.normalizedPolygon = { {0.2f,0.2f}, {0.8f,0.2f}, {0.5f,0.8f} };
	asset.surfaceTargets.push_back(target);
	const fs::path output = root / "OUTPUT"; const fs::path p0 = root / "SINGLE_PARTICLE_DATA" / "p0.obj";
	vitru::StaticAssetOperationReport save; require(vitru::saveStaticParticleBundle(asset, output, asset.name, p0, save), "named asset save"); require(fs::exists(save.manifestPath), "VSPA manifest created"); require(fs::exists(save.assetRoot / "geometry" / "Anaheim_SP_Test_01.obj"), "named OBJ created"); require(fs::exists(save.assetRoot / "materials" / "Anaheim_SP_Test_01.mtl"), "named MTL created"); require(fs::exists(save.assetRoot / "textures" / "Anaheim_SP_Test_01_basecolor.png"), "default base color created"); require(fs::exists(save.assetRoot / "textures" / "Anaheim_SP_Test_01_uv_guide.png"), "UV guide created"); require(fs::exists(p0), "p0 workspace register refreshed");
	vitru::ProjectAssetRepository repository; vitru::StaticParticleAsset loaded; vitru::StaticAssetOperationReport load; require(vitru::loadStaticParticleBundle(save.manifestPath, loaded, load, &repository, p0), "named bundle load"); require(loaded.mesh.triangleCount() == asset.mesh.triangleCount(), "bundle triangle round-trip"); require(loaded.mesh.uvs.size() == loaded.mesh.positions.size(), "bundle UV round-trip"); require(repository.activeStaticParticle() != nullptr, "loaded asset active in repository");
	require(!loaded.materials.empty() && loaded.materials[0].emissiveTextureId == "tm_emissive", "emissive texture reference round-trip");
	require(std::fabs(loaded.materials[0].emissiveIntensity - 2.25f) <= 1.0e-6f, "emissive intensity round-trip");
	require(loaded.surfaceTargets.size() == 1u && loaded.surfaceTargets[0].name == "eye_outer", "surface target name round-trip");
	require(loaded.surfaceTargets[0].faceIndex == 4u && loaded.surfaceTargets[0].normalizedPolygon.size() == 3u, "surface target polygon round-trip");
	const std::string manifestBefore = save.manifestPath.string(); vitru::StaticAssetOperationReport replace; require(vitru::saveStaticParticleBundle(loaded, output, asset.name, p0, replace), "atomic destination replacement"); require(replace.manifestPath.string() == manifestBefore, "replacement destination stable");
	vitru::StaticParticleAsset invalid = loaded; invalid.mesh.clear(); vitru::StaticAssetOperationReport rollback; require(!vitru::saveStaticParticleBundle(invalid, output, asset.name, p0, rollback), "invalid save rejected"); require(fs::exists(replace.manifestPath), "failed save leaves previous asset intact");
	const fs::path invalidManifest = root / "invalid.vspa.json"; writeText(invalidManifest, "{\"schema\":\"wrong.schema\",\"schema_version\":1}"); vitru::StaticParticleAsset rejected; vitru::VspaLoadReport rejectedReport; require(!vitru::loadVspaManifest(invalidManifest, rejected, rejectedReport), "invalid manifest schema rejected");
	std::ifstream obj(save.assetRoot / "geometry" / "Anaheim_SP_Test_01.obj"); std::string objText((std::istreambuf_iterator<char>(obj)), {}); require(objText.find("vt ") != std::string::npos && objText.find("mtllib ") != std::string::npos && objText.find("usemtl ") != std::string::npos && objText.find("/1/1") != std::string::npos, "OBJ vt/material/index contract");
}

void verifyTextureMapAuthoringRuntime(const fs::path& root) {
	const fs::path output = root / "TM_OUTPUT";
	const fs::path materials = root / "TM_BASE_MATERIALS";
	fs::create_directories(output);
	fs::create_directories(materials);

	vitru::StaticParticleAsset asset = cubeAsset("TextureMapRuntime");
	require(vitru::generateBoxAtlasUVs(asset.mesh).success, "texture-map fixture UV atlas");
	vitru::TextureResource base;
	base.id = "tm_base"; base.relativePath = "textures/tm_base.png";
	base.usage = vitru::TextureUsage::BaseColor;
	base.colorSpace = vitru::TextureColorSpace::SRGB;
	base.width = 96u; base.height = 64u; base.channels = 4u;
	base.pixels = vitru::makeSolidImage(96u, 64u, 0u, 0u, 0u, 255u).pixels;
	base.loaded = true; base.valid = true;
	asset.textures.push_back(base);
	asset.materials[0].baseColorTextureId = base.id;

	vitru::ProjectAssetRepository repository;
	const vitru::AssetId id = repository.addStaticParticle(asset);
	vitru::TextureMapWorkspace workspace;
	require(workspace.initialize(&repository, output, materials), "texture-map workspace initialize");
	require(workspace.activateLoadedTarget(id), "texture-map target activate");
	require(workspace.target().readiness == vitru::TextureTargetReadiness::Ready, "texture-map target ready");
	require(workspace.beginAuthoringRuntime(), "texture-map Layer 3 runtime begin");
	require(workspace.adjustRuntimeValue(1, 1), "committed preview source select");
	std::string diagnostic;

	require(workspace.activateRuntimeRow(2) == vitru::TextureMapWorkspaceAction::StateChanged, "coloring setup enter");
	require(workspace.activateRuntimeRow(2) == vitru::TextureMapWorkspaceAction::StateChanged, "coloring grid enter");
	require(workspace.beginAuthoringStroke(0, 0), "coloring stroke begin");
	require(workspace.continueAuthoringStroke(31, 31), "coloring interpolated stroke");
	workspace.endAuthoringStroke();
	require(workspace.session().dirty, "coloring stroke dirty");
	vitru::StaticParticleAsset saveAsSnapshot;
	require(workspace.buildSaveAsSnapshot("TextureMapCopy", saveAsSnapshot, &diagnostic), "dirty coloring save-as snapshot");
	require(saveAsSnapshot.findTexture("tm_base")->pixels != base.pixels, "save-as includes dirty edit with committed preview selected");
	require(workspace.buildPreviewAsset().findTexture("tm_base")->pixels == base.pixels, "committed preview remains presentation-only");
	require(!workspace.canExitLayer3(&diagnostic) && !diagnostic.empty(), "dirty structural exit blocked");
	const std::uint64_t revision = workspace.session().revision;
	require(workspace.toggleRuntimeView() && workspace.toggleRuntimeView(), "edit preview round-trip");
	require(workspace.session().revision == revision, "edit preview preserves working state");
	require(workspace.activateRuntimeRow(5) == vitru::TextureMapWorkspaceAction::StateChanged, "coloring review enter");
	require(workspace.activateRuntimeRow(0) == vitru::TextureMapWorkspaceAction::StateChanged, "coloring commit");
	require(!workspace.session().dirty, "coloring commit clean");
	require(repository.findStaticParticle(id)->findTexture("tm_base")->pixels != base.pixels, "coloring commit changed canonical texture");

	require(workspace.activateRuntimeRow(4) == vitru::TextureMapWorkspaceAction::StateChanged, "return cycle setup");
	require(workspace.adjustRuntimeValue(0, 1), "select contour mode");
	require(workspace.activateRuntimeRow(2) == vitru::TextureMapWorkspaceAction::StateChanged, "contour setup enter");
	require(workspace.activateRuntimeRow(2) == vitru::TextureMapWorkspaceAction::StateChanged, "contour grid enter");
	require(workspace.addContourPoint(4, 4), "contour point one");
	require(workspace.addContourPoint(24, 4), "contour point two");
	require(workspace.addContourPoint(14, 24), "contour point three");
	require(workspace.closeContour(&diagnostic), "contour close");
	require(workspace.activateRuntimeRow(3) == vitru::TextureMapWorkspaceAction::StateChanged, "contour review enter");
	require(workspace.activateRuntimeRow(0) == vitru::TextureMapWorkspaceAction::RequestSurfaceTargetName, "new contour requests name");
	require(workspace.completeSurfaceTargetName("eye_outer", &diagnostic), "named contour commit");
	require(repository.findStaticParticle(id)->surfaceTargets.size() == 1u, "named contour canonical commit");

	require(workspace.activateRuntimeRow(4) == vitru::TextureMapWorkspaceAction::StateChanged, "contour return cycle");
	require(workspace.adjustRuntimeValue(0, 1), "select panel-lines mode");
	require(workspace.activateRuntimeRow(2) == vitru::TextureMapWorkspaceAction::StateChanged, "panel setup enter");
	require(workspace.activateRuntimeRow(2) == vitru::TextureMapWorkspaceAction::StateChanged, "panel grid enter");
	require(workspace.adjustRuntimeValue(1, 1), "panel thickness adjust");
	require(workspace.beginAuthoringStroke(1, 1), "panel stroke begin");
	require(workspace.continueAuthoringStroke(30, 20), "panel interpolated stroke");
	workspace.endAuthoringStroke();
	require(workspace.session().dirty, "panel stroke dirty");
}

int runMasterChief(const fs::path& manifest, const fs::path& outputRoot, const fs::path& p0) {
	vitru::StaticParticleAsset asset; vitru::StaticAssetOperationReport load;
	if (!vitru::loadStaticParticleBundle(manifest, asset, load, nullptr, p0)) { for (const std::string& error : load.errors) std::cerr << error << std::endl; return EXIT_FAILURE; }
	require(!asset.mesh.uvs.empty(), "Master Chief authored UVs"); require(asset.materials.size() >= 2u, "Master Chief materials");
	bool baseLoaded = false, bumpHeight = false; for (const vitru::TextureResource& texture : asset.textures) { if (texture.usage == vitru::TextureUsage::BaseColor && texture.loaded) baseLoaded = true; if (texture.usage == vitru::TextureUsage::Height) bumpHeight = true; }
	require(baseLoaded, "Master Chief base-color PNG load"); require(bumpHeight, "Master Chief bump preserved as height"); require(asset.anchor.pivotMode == vitru::ParticlePivotMode::GroundCenter && asset.anchor.fitMode == vitru::ParticleFitMode::CollisionSafe, "Master Chief anchor policy");
	vitru::StaticAssetOperationReport save; require(vitru::saveStaticParticleBundle(asset, outputRoot, "MasterChief_RoundTrip_A0", p0, save), "Master Chief Save As");
	vitru::StaticParticleAsset reopened; vitru::StaticAssetOperationReport reload; require(vitru::loadStaticParticleBundle(save.manifestPath, reopened, reload, nullptr, p0), "Master Chief OUTPUT reload"); require(reopened.mesh.triangleCount() == asset.mesh.triangleCount(), "Master Chief stable triangles"); require(std::fabs(reopened.bounds.maxAxisExtent - asset.bounds.maxAxisExtent) <= 1.0e-5f, "Master Chief stable bounds");
	std::cout << "MASTER_CHIEF PASS triangles=" << asset.mesh.triangleCount() << " renderVertices=" << asset.mesh.positions.size() << " materials=" << asset.materials.size() << " textures=" << asset.textures.size() << " baseColorLoaded=YES p0=YES roundTrip=YES" << std::endl;
	return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv) {
	if (argc == 5 && std::string(argv[1]) == "--master-chief") return runMasterChief(argv[2], argv[3], argv[4]);
	const fs::path root = fs::temp_directory_path() / "vitrugen_static_asset_tests_a0"; std::error_code error; fs::remove_all(root, error); fs::create_directories(root, error); require(!error, "temporary test directory");
	verifyModelAndRepository(); verifyJson(); verifyUvAndPng(root); verifyObjMtl(root); verifyBundleRoundTrip(root); verifyTextureMapAuthoringRuntime(root);
	fs::remove_all(root, error);
	std::cout << "STATIC_PARTICLE_ASSET_TESTS PASS validation=YES repository=YES json=YES objMtl=YES legacyPaths=YES uv=YES png=YES atomicSave=YES bundleRoundTrip=YES textureMapRuntime=YES p0=YES" << std::endl;
	return EXIT_SUCCESS;
}
