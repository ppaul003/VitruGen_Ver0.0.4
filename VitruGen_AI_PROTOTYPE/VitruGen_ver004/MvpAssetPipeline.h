#ifndef VITRUGEN_MVP_ASSET_PIPELINE_H
#define VITRUGEN_MVP_ASSET_PIPELINE_H

#include <cstdint>
#include <string>
#include <vector>

namespace vitru {

using AssetId = std::uint64_t;
using NodeId = std::uint32_t;

static constexpr AssetId INVALID_ASSET_ID = 0;
static constexpr NodeId INVALID_NODE_ID = 0;

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;
};

struct Vec3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct ColorRGBA8 {
	std::uint8_t r = 255;
	std::uint8_t g = 255;
	std::uint8_t b = 255;
	std::uint8_t a = 255;

	bool operator==(const ColorRGBA8& other) const;
	bool operator!=(const ColorRGBA8& other) const { return !(*this == other); }
};

struct Transform {
	Vec3 position{};
	Vec3 rotationDegrees{};
	Vec3 scale{ 1.0f, 1.0f, 1.0f };
};

struct Bounds3 {
	Vec3 minimum{};
	Vec3 maximum{};
	bool valid = false;

	void reset();
	void include(const Vec3& point);
	void include(const Bounds3& bounds);
	Vec3 center() const;
	Vec3 extent() const;
};

struct CollisionProxy {
	enum class Shape {
		BOX = 0,
		SPHERE
	};

	Shape shape = Shape::BOX;
	Vec3 center{};
	Vec3 halfExtents{ 0.5f, 0.5f, 0.5f };
	float radius = 0.5f;
};

struct MeshGeometry {
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> uvs;
	std::vector<std::uint32_t> indices;

	bool empty() const { return positions.empty(); }
	std::size_t triangleCount() const;
	Bounds3 calculateBounds() const;
	bool validate(std::vector<std::string>* errors = nullptr) const;
};

struct MeshProcessingOptions {
	bool removeDegenerateTriangles = true;
	bool removeDuplicateTriangles = true;
	bool weldVertices = true;
	bool smoothNormals = true;
	float weldEpsilon = 0.00001f;
};

struct MeshProcessingReport {
	std::size_t inputTriangles = 0;
	std::size_t outputTriangles = 0;
	std::size_t removedNonFinite = 0;
	std::size_t removedDegenerate = 0;
	std::size_t removedDuplicate = 0;
	std::size_t weldedVertices = 0;
	bool valid = false;
	std::vector<std::string> errors;
};

MeshProcessingReport processMesh(
	MeshGeometry& mesh,
	const MeshProcessingOptions& options = MeshProcessingOptions{}
);

struct SurfaceTexture {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<ColorRGBA8> pixels;
	bool uvOverlayVisible = true;

	bool create(
		std::uint32_t newWidth,
		std::uint32_t newHeight,
		const ColorRGBA8& clearColor = ColorRGBA8{}
	);
	bool valid() const;
	void clear(const ColorRGBA8& color);
	void paintCircle(int centerX, int centerY, int radius, const ColorRGBA8& color);
	void eraseCircle(int centerX, int centerY, int radius);
	bool fillRegion(int x, int y, const ColorRGBA8& color);
	bool savePPM(const std::string& path, std::string* error = nullptr) const;
	bool importPPM(const std::string& path, std::string* error = nullptr);
	bool importBMP(const std::string& path, std::string* error = nullptr);
	bool importImage(const std::string& path, std::string* error = nullptr);
	const ColorRGBA8* pixel(std::uint32_t x, std::uint32_t y) const;
};

struct MaterialInfo {
	std::string name = "Default Material";
	ColorRGBA8 baseColor{ 180, 190, 205, 255 };
	float metallic = 0.1f;
	float roughness = 0.65f;
};

struct StaticParticleAsset {
	AssetId id = INVALID_ASSET_ID;
	std::string name;
	MeshGeometry mesh;
	SurfaceTexture texture;
	MaterialInfo material;
	Bounds3 bounds;
	CollisionProxy collision;
	std::vector<float> volumetricSource;

	bool validate(std::vector<std::string>* errors = nullptr) const;
	void refreshDerivedData();
};

enum class AssemblyNodeType {
	MESH = 0,
	JOINT,
	INTERACTION
};

enum class JointType {
	FIXED = 0,
	REVOLUTE
};

enum class InteractionRole {
	CONTACT = 0,
	WHEEL,
	LEFT_PROPULSION_TRACK,
	RIGHT_PROPULSION_TRACK,
	THRUSTER,
	WEAPON_MUZZLE,
	SENSOR,
	SURFACE_ANCHOR
};

struct AssemblyNode {
	NodeId id = INVALID_NODE_ID;
	NodeId parentId = INVALID_NODE_ID;
	AssemblyNodeType type = AssemblyNodeType::MESH;
	std::string name;
	Transform localTransform;

	// Red mesh node data.
	AssetId staticAssetId = INVALID_ASSET_ID;

	// Blue joint node data.
	JointType jointType = JointType::FIXED;
	Vec3 jointAxis{ 0.0f, 1.0f, 0.0f };
	float jointMinimumDegrees = -90.0f;
	float jointMaximumDegrees = 90.0f;

	// Green interaction node data.
	InteractionRole interactionRole = InteractionRole::CONTACT;
};

struct JointKeyframe {
	float timeSeconds = 0.0f;
	float rotationDegrees = 0.0f;
};

struct JointAnimationTrack {
	NodeId jointNodeId = INVALID_NODE_ID;
	std::vector<JointKeyframe> keyframes;
};

struct AnimationClip {
	std::string name;
	float durationSeconds = 1.0f;
	float playbackSpeed = 1.0f;
	bool looping = true;
	std::vector<JointAnimationTrack> tracks;
};

struct LinkedAssembly {
	AssetId id = INVALID_ASSET_ID;
	std::string name;
	NodeId nextNodeId = 1;
	std::vector<AssemblyNode> nodes;
	std::vector<AnimationClip> clips;

	NodeId addNode(const AssemblyNode& node);
	bool removeNode(NodeId nodeId);
	AssemblyNode* findNode(NodeId nodeId);
	const AssemblyNode* findNode(NodeId nodeId) const;
};

struct CompiledNode {
	AssemblyNode source;
	float bindMatrix[16]{};
};

struct KinematicParticleAsset {
	AssetId id = INVALID_ASSET_ID;
	AssetId sourceAssemblyId = INVALID_ASSET_ID;
	std::string name;
	NodeId rootNodeId = INVALID_NODE_ID;
	std::vector<CompiledNode> nodes;
	std::vector<StaticParticleAsset> embeddedStaticAssets;
	std::vector<AnimationClip> clips;
	Bounds3 bounds;
	CollisionProxy rootCollision;
	bool runtimeReady = false;

	const CompiledNode* findNode(NodeId nodeId) const;
};

struct ValidationIssue {
	std::string code;
	std::string message;
};

struct BakeResult {
	bool success = false;
	KinematicParticleAsset asset;
	std::vector<ValidationIssue> issues;
};

struct SandboxTarget {
	std::uint32_t id = 0;
	Vec3 position{};
	float radius = 0.5f;
	bool hit = false;
};

struct SandboxProjectile {
	Vec3 position{};
	Vec3 velocity{};
	float ageSeconds = 0.0f;
	bool active = true;
};

struct SandboxInstance {
	AssetId assetId = INVALID_ASSET_ID;
	Transform rootTransform;
	std::string activeClip;
	float animationTimeSeconds = 0.0f;
	std::vector<float> jointValuesDegrees;
	bool spawned = false;
};

class SandboxRuntime {
public:
	void configureDefaultEnvironment();
	bool spawn(const KinematicParticleAsset& asset, const Vec3& position);
	void reset();
	void drive(float forwardAmount, float turnAmount, float deltaSeconds);
	bool setJoint(NodeId jointNodeId, float degrees);
	bool playAnimation(const std::string& clipName);
	bool fireProjectile(float speed = 12.0f);
	void update(float deltaSeconds);

	const SandboxInstance& instance() const { return m_instance; }
	const std::vector<SandboxTarget>& targets() const { return m_targets; }
	const std::vector<SandboxProjectile>& projectiles() const { return m_projectiles; }
	std::uint32_t hitCount() const { return m_hitCount; }
	const KinematicParticleAsset* activeAsset() const { return m_activeAsset; }

private:
	const KinematicParticleAsset* m_activeAsset = nullptr;
	SandboxInstance m_instance;
	std::vector<SandboxTarget> m_targets;
	std::vector<SandboxProjectile> m_projectiles;
	std::uint32_t m_hitCount = 0;
};

class ProjectAssetRepository {
public:
	AssetId addStaticParticle(StaticParticleAsset asset);
	AssetId addAssembly(LinkedAssembly assembly);
	AssetId addKinematicParticle(KinematicParticleAsset asset);

	StaticParticleAsset* findStaticParticle(AssetId id);
	const StaticParticleAsset* findStaticParticle(AssetId id) const;
	LinkedAssembly* findAssembly(AssetId id);
	const LinkedAssembly* findAssembly(AssetId id) const;
	KinematicParticleAsset* findKinematicParticle(AssetId id);
	const KinematicParticleAsset* findKinematicParticle(AssetId id) const;

	const std::vector<StaticParticleAsset>& staticParticles() const { return m_staticParticles; }
	const std::vector<LinkedAssembly>& assemblies() const { return m_assemblies; }
	const std::vector<KinematicParticleAsset>& kinematicParticles() const { return m_kinematicParticles; }

	BakeResult validateAndBake(AssetId assemblyId, const std::string& bakedName) const;
	BakeResult bakeAndStore(AssetId assemblyId, const std::string& bakedName);

	bool saveProject(const std::string& path, std::string* error = nullptr) const;
	bool loadProject(const std::string& path, std::string* error = nullptr);
	void clear();

	AssetId nextAssetId() const { return m_nextAssetId; }

private:
	AssetId allocateAssetId();
	void ensureNextAssetIdBeyond(AssetId id);

	AssetId m_nextAssetId = 1;
	std::vector<StaticParticleAsset> m_staticParticles;
	std::vector<LinkedAssembly> m_assemblies;
	std::vector<KinematicParticleAsset> m_kinematicParticles;
};

const char* assemblyNodeTypeName(AssemblyNodeType type);
const char* jointTypeName(JointType type);
const char* interactionRoleName(InteractionRole role);

} // namespace vitru

#endif
