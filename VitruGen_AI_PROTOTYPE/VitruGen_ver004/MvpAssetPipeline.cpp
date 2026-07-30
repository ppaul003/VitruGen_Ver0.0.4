#include "MvpAssetPipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace vitru {
namespace {

	constexpr float kPi = 3.14159265358979323846f;
	constexpr std::uint32_t kProjectVersion = 1;
	constexpr char kProjectMagic[] = "VITRUGEN_MVP_PROJECT";
	constexpr std::uint64_t kMaxSerializedItems = 100000000ull;

	bool finiteFloat(float value) {
		return std::isfinite(value) != 0;
	}

	bool finiteVec(const Vec3& value) {
		return finiteFloat(value.x) &&
			finiteFloat(value.y) &&
			finiteFloat(value.z);
	}

	Vec3 add(const Vec3& a, const Vec3& b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vec3 subtract(const Vec3& a, const Vec3& b) {
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	Vec3 multiply(const Vec3& value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	float dot(const Vec3& a, const Vec3& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	Vec3 cross(const Vec3& a, const Vec3& b) {
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		};
	}

	float lengthSquared(const Vec3& value) {
		return dot(value, value);
	}

	Vec3 normalized(const Vec3& value) {
		const float squareLength = lengthSquared(value);
		if (!finiteFloat(squareLength) || squareLength <= 1.0e-20f) {
			return { 0.0f, 1.0f, 0.0f };
		}
		return multiply(value, 1.0f / std::sqrt(squareLength));
	}

	void appendError(std::vector<std::string>* errors, const std::string& error) {
		if (errors) errors->push_back(error);
	}

	struct Matrix4 {
		float m[16]{};
	};

	Matrix4 identityMatrix() {
		Matrix4 result;
		result.m[0] = 1.0f;
		result.m[5] = 1.0f;
		result.m[10] = 1.0f;
		result.m[15] = 1.0f;
		return result;
	}

	Matrix4 multiplyMatrix(const Matrix4& a, const Matrix4& b) {
		Matrix4 result;
		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				float value = 0.0f;
				for (int k = 0; k < 4; ++k) {
					value += a.m[row * 4 + k] * b.m[k * 4 + column];
				}
				result.m[row * 4 + column] = value;
			}
		}
		return result;
	}

	Matrix4 translationMatrix(const Vec3& translation) {
		Matrix4 result = identityMatrix();
		result.m[3] = translation.x;
		result.m[7] = translation.y;
		result.m[11] = translation.z;
		return result;
	}

	Matrix4 scaleMatrix(const Vec3& scale) {
		Matrix4 result = identityMatrix();
		result.m[0] = scale.x;
		result.m[5] = scale.y;
		result.m[10] = scale.z;
		return result;
	}

	Matrix4 rotationXMatrix(float radians) {
		Matrix4 result = identityMatrix();
		const float sine = std::sin(radians);
		const float cosine = std::cos(radians);
		result.m[5] = cosine;
		result.m[6] = -sine;
		result.m[9] = sine;
		result.m[10] = cosine;
		return result;
	}

	Matrix4 rotationYMatrix(float radians) {
		Matrix4 result = identityMatrix();
		const float sine = std::sin(radians);
		const float cosine = std::cos(radians);
		result.m[0] = cosine;
		result.m[2] = sine;
		result.m[8] = -sine;
		result.m[10] = cosine;
		return result;
	}

	Matrix4 rotationZMatrix(float radians) {
		Matrix4 result = identityMatrix();
		const float sine = std::sin(radians);
		const float cosine = std::cos(radians);
		result.m[0] = cosine;
		result.m[1] = -sine;
		result.m[4] = sine;
		result.m[5] = cosine;
		return result;
	}

	Matrix4 transformMatrix(const Transform& transform) {
		const float toRadians = kPi / 180.0f;
		const Matrix4 rotation = multiplyMatrix(
			rotationZMatrix(transform.rotationDegrees.z * toRadians),
			multiplyMatrix(
				rotationYMatrix(transform.rotationDegrees.y * toRadians),
				rotationXMatrix(transform.rotationDegrees.x * toRadians)
			)
		);
		return multiplyMatrix(
			translationMatrix(transform.position),
			multiplyMatrix(rotation, scaleMatrix(transform.scale))
		);
	}

	Vec3 transformPoint(const Matrix4& matrix, const Vec3& point) {
		return {
			matrix.m[0] * point.x + matrix.m[1] * point.y +
				matrix.m[2] * point.z + matrix.m[3],
			matrix.m[4] * point.x + matrix.m[5] * point.y +
				matrix.m[6] * point.z + matrix.m[7],
			matrix.m[8] * point.x + matrix.m[9] * point.y +
				matrix.m[10] * point.z + matrix.m[11]
		};
	}

	void includeTransformedBounds(
		Bounds3& output,
		const Bounds3& input,
		const Matrix4& transform) {
		if (!input.valid) return;
		for (int x = 0; x < 2; ++x) {
			for (int y = 0; y < 2; ++y) {
				for (int z = 0; z < 2; ++z) {
					const Vec3 corner{
						x ? input.maximum.x : input.minimum.x,
						y ? input.maximum.y : input.minimum.y,
						z ? input.maximum.z : input.minimum.z
					};
					output.include(transformPoint(transform, corner));
				}
			}
		}
	}

	struct QuantizedVertex {
		long long x = 0;
		long long y = 0;
		long long z = 0;

		bool operator<(const QuantizedVertex& other) const {
			if (x != other.x) return x < other.x;
			if (y != other.y) return y < other.y;
			return z < other.z;
		}

		bool operator==(const QuantizedVertex& other) const {
			return x == other.x && y == other.y && z == other.z;
		}
	};

	QuantizedVertex quantize(const Vec3& position, float epsilon) {
		const float safeEpsilon = std::max(epsilon, 1.0e-8f);
		return {
			static_cast<long long>(std::llround(position.x / safeEpsilon)),
			static_cast<long long>(std::llround(position.y / safeEpsilon)),
			static_cast<long long>(std::llround(position.z / safeEpsilon))
		};
	}

	struct TriangleKey {
		std::array<QuantizedVertex, 3> vertices;

		bool operator<(const TriangleKey& other) const {
			for (std::size_t i = 0; i < vertices.size(); ++i) {
				if (vertices[i] < other.vertices[i]) return true;
				if (other.vertices[i] < vertices[i]) return false;
			}
			return false;
		}
	};

	TriangleKey triangleKey(const Vec3& a, const Vec3& b, const Vec3& c, float epsilon) {
		TriangleKey result{ { quantize(a, epsilon), quantize(b, epsilon), quantize(c, epsilon) } };
		std::sort(result.vertices.begin(), result.vertices.end());
		return result;
	}

	bool readPpmToken(std::istream& input, std::string& token) {
		token.clear();
		char ch = 0;
		while (input.get(ch)) {
			if (ch == '#') {
				input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				continue;
			}
			if (!std::isspace(static_cast<unsigned char>(ch))) {
				token.push_back(ch);
				break;
			}
		}
		while (input.get(ch)) {
			if (std::isspace(static_cast<unsigned char>(ch))) return !token.empty();
			token.push_back(ch);
		}
		return !token.empty();
	}

	void addIssue(
		std::vector<ValidationIssue>& issues,
		const std::string& code,
		const std::string& message) {
		issues.push_back({ code, message });
	}

	class BinaryWriter {
	public:
		explicit BinaryWriter(std::ostream& output) : m_output(output) {}

		template <typename T>
		void pod(const T& value) {
			m_output.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		void bytes(const void* data, std::size_t size) {
			if (size == 0) return;
			m_output.write(reinterpret_cast<const char*>(data),
				static_cast<std::streamsize>(size));
		}

		void text(const std::string& value) {
			const std::uint64_t size = static_cast<std::uint64_t>(value.size());
			pod(size);
			bytes(value.data(), value.size());
		}

		bool good() const { return m_output.good(); }

	private:
		std::ostream& m_output;
	};

	class BinaryReader {
	public:
		explicit BinaryReader(std::istream& input) : m_input(input) {}

		template <typename T>
		bool pod(T& value) {
			m_input.read(reinterpret_cast<char*>(&value), sizeof(T));
			return m_input.good();
		}

		bool bytes(void* data, std::size_t size) {
			if (size == 0) return true;
			m_input.read(reinterpret_cast<char*>(data),
				static_cast<std::streamsize>(size));
			return m_input.good();
		}

		bool text(std::string& value) {
			std::uint64_t size = 0;
			if (!pod(size) || size > kMaxSerializedItems) return false;
			value.resize(static_cast<std::size_t>(size));
			return bytes(value.empty() ? nullptr : &value[0], value.size());
		}

	private:
		std::istream& m_input;
	};

	template <typename Enum>
	void writeEnum(BinaryWriter& writer, Enum value) {
		const std::uint32_t encoded = static_cast<std::uint32_t>(value);
		writer.pod(encoded);
	}

	template <typename Enum>
	bool readEnum(BinaryReader& reader, Enum& value, std::uint32_t maximumInclusive) {
		std::uint32_t encoded = 0;
		if (!reader.pod(encoded) || encoded > maximumInclusive) return false;
		value = static_cast<Enum>(encoded);
		return true;
	}

	void writeVec2(BinaryWriter& writer, const Vec2& value) {
		writer.pod(value.x); writer.pod(value.y);
	}

	bool readVec2(BinaryReader& reader, Vec2& value) {
		return reader.pod(value.x) && reader.pod(value.y);
	}

	void writeVec3(BinaryWriter& writer, const Vec3& value) {
		writer.pod(value.x); writer.pod(value.y); writer.pod(value.z);
	}

	bool readVec3(BinaryReader& reader, Vec3& value) {
		return reader.pod(value.x) && reader.pod(value.y) && reader.pod(value.z);
	}

	void writeColor(BinaryWriter& writer, const ColorRGBA8& value) {
		writer.pod(value.r); writer.pod(value.g); writer.pod(value.b); writer.pod(value.a);
	}

	bool readColor(BinaryReader& reader, ColorRGBA8& value) {
		return reader.pod(value.r) && reader.pod(value.g) &&
			reader.pod(value.b) && reader.pod(value.a);
	}

	void writeTransform(BinaryWriter& writer, const Transform& value) {
		writeVec3(writer, value.position);
		writeVec3(writer, value.rotationDegrees);
		writeVec3(writer, value.scale);
	}

	bool readTransform(BinaryReader& reader, Transform& value) {
		return readVec3(reader, value.position) &&
			readVec3(reader, value.rotationDegrees) &&
			readVec3(reader, value.scale);
	}

	void writeBounds(BinaryWriter& writer, const Bounds3& value) {
		writeVec3(writer, value.minimum);
		writeVec3(writer, value.maximum);
		const std::uint8_t valid = value.valid ? 1 : 0;
		writer.pod(valid);
	}

	bool readBounds(BinaryReader& reader, Bounds3& value) {
		std::uint8_t valid = 0;
		if (!readVec3(reader, value.minimum) || !readVec3(reader, value.maximum) ||
			!reader.pod(valid)) return false;
		value.valid = valid != 0;
		return true;
	}

	template <typename T, typename WriteElement>
	void writeVector(BinaryWriter& writer, const std::vector<T>& values, WriteElement writeElement) {
		const std::uint64_t count = static_cast<std::uint64_t>(values.size());
		writer.pod(count);
		for (const T& value : values) writeElement(writer, value);
	}

	template <typename T, typename ReadElement>
	bool readVector(BinaryReader& reader, std::vector<T>& values, ReadElement readElement) {
		std::uint64_t count = 0;
		if (!reader.pod(count) || count > kMaxSerializedItems) return false;
		values.clear();
		values.resize(static_cast<std::size_t>(count));
		for (T& value : values) {
			if (!readElement(reader, value)) return false;
		}
		return true;
	}

	void writeMesh(BinaryWriter& writer, const MeshGeometry& mesh) {
		writeVector(writer, mesh.positions,
			[](BinaryWriter& output, const Vec3& value) { writeVec3(output, value); });
		writeVector(writer, mesh.normals,
			[](BinaryWriter& output, const Vec3& value) { writeVec3(output, value); });
		writeVector(writer, mesh.uvs,
			[](BinaryWriter& output, const Vec2& value) { writeVec2(output, value); });
		writeVector(writer, mesh.indices,
			[](BinaryWriter& output, const std::uint32_t& value) { output.pod(value); });
	}

	bool readMesh(BinaryReader& reader, MeshGeometry& mesh) {
		return readVector(reader, mesh.positions,
			[](BinaryReader& input, Vec3& value) { return readVec3(input, value); }) &&
			readVector(reader, mesh.normals,
				[](BinaryReader& input, Vec3& value) { return readVec3(input, value); }) &&
			readVector(reader, mesh.uvs,
				[](BinaryReader& input, Vec2& value) { return readVec2(input, value); }) &&
			readVector(reader, mesh.indices,
				[](BinaryReader& input, std::uint32_t& value) { return input.pod(value); });
	}

	void writeTexture(BinaryWriter& writer, const SurfaceTexture& texture) {
		writer.pod(texture.width);
		writer.pod(texture.height);
		const std::uint8_t overlay = texture.uvOverlayVisible ? 1 : 0;
		writer.pod(overlay);
		writeVector(writer, texture.pixels,
			[](BinaryWriter& output, const ColorRGBA8& value) { writeColor(output, value); });
	}

	bool readTexture(BinaryReader& reader, SurfaceTexture& texture) {
		std::uint8_t overlay = 0;
		if (!reader.pod(texture.width) || !reader.pod(texture.height) ||
			!reader.pod(overlay)) return false;
		texture.uvOverlayVisible = overlay != 0;
		return readVector(reader, texture.pixels,
			[](BinaryReader& input, ColorRGBA8& value) { return readColor(input, value); });
	}

	void writeStaticAsset(BinaryWriter& writer, const StaticParticleAsset& asset) {
		writer.pod(asset.id);
		writer.text(asset.name);
		writeMesh(writer, asset.mesh);
		writeTexture(writer, asset.texture);
		writer.text(asset.material.name);
		writeColor(writer, asset.material.baseColor);
		writer.pod(asset.material.metallic);
		writer.pod(asset.material.roughness);
		writeBounds(writer, asset.bounds);
		writeEnum(writer, asset.collision.shape);
		writeVec3(writer, asset.collision.center);
		writeVec3(writer, asset.collision.halfExtents);
		writer.pod(asset.collision.radius);
		writeVector(writer, asset.volumetricSource,
			[](BinaryWriter& output, const float& value) { output.pod(value); });
	}

	bool readStaticAsset(BinaryReader& reader, StaticParticleAsset& asset) {
		if (!reader.pod(asset.id) || !reader.text(asset.name) ||
			!readMesh(reader, asset.mesh) || !readTexture(reader, asset.texture) ||
			!reader.text(asset.material.name) || !readColor(reader, asset.material.baseColor) ||
			!reader.pod(asset.material.metallic) || !reader.pod(asset.material.roughness) ||
			!readBounds(reader, asset.bounds) ||
			!readEnum(reader, asset.collision.shape,
				static_cast<std::uint32_t>(CollisionProxy::Shape::SPHERE)) ||
			!readVec3(reader, asset.collision.center) ||
			!readVec3(reader, asset.collision.halfExtents) ||
			!reader.pod(asset.collision.radius)) return false;
		return readVector(reader, asset.volumetricSource,
			[](BinaryReader& input, float& value) { return input.pod(value); });
	}

	void writeAssemblyNode(BinaryWriter& writer, const AssemblyNode& node) {
		writer.pod(node.id);
		writer.pod(node.parentId);
		writeEnum(writer, node.type);
		writer.text(node.name);
		writeTransform(writer, node.localTransform);
		writer.pod(node.staticAssetId);
		writeEnum(writer, node.jointType);
		writeVec3(writer, node.jointAxis);
		writer.pod(node.jointMinimumDegrees);
		writer.pod(node.jointMaximumDegrees);
		writeEnum(writer, node.interactionRole);
	}

	bool readAssemblyNode(BinaryReader& reader, AssemblyNode& node) {
		return reader.pod(node.id) && reader.pod(node.parentId) &&
			readEnum(reader, node.type,
				static_cast<std::uint32_t>(AssemblyNodeType::INTERACTION)) &&
			reader.text(node.name) && readTransform(reader, node.localTransform) &&
			reader.pod(node.staticAssetId) &&
			readEnum(reader, node.jointType,
				static_cast<std::uint32_t>(JointType::REVOLUTE)) &&
			readVec3(reader, node.jointAxis) &&
			reader.pod(node.jointMinimumDegrees) &&
			reader.pod(node.jointMaximumDegrees) &&
			readEnum(reader, node.interactionRole,
				static_cast<std::uint32_t>(InteractionRole::SURFACE_ANCHOR));
	}

	void writeKeyframe(BinaryWriter& writer, const JointKeyframe& keyframe) {
		writer.pod(keyframe.timeSeconds);
		writer.pod(keyframe.rotationDegrees);
	}

	bool readKeyframe(BinaryReader& reader, JointKeyframe& keyframe) {
		return reader.pod(keyframe.timeSeconds) && reader.pod(keyframe.rotationDegrees);
	}

	void writeTrack(BinaryWriter& writer, const JointAnimationTrack& track) {
		writer.pod(track.jointNodeId);
		writeVector(writer, track.keyframes,
			[](BinaryWriter& output, const JointKeyframe& value) { writeKeyframe(output, value); });
	}

	bool readTrack(BinaryReader& reader, JointAnimationTrack& track) {
		return reader.pod(track.jointNodeId) &&
			readVector(reader, track.keyframes,
				[](BinaryReader& input, JointKeyframe& value) { return readKeyframe(input, value); });
	}

	void writeClip(BinaryWriter& writer, const AnimationClip& clip) {
		writer.text(clip.name);
		writer.pod(clip.durationSeconds);
		writer.pod(clip.playbackSpeed);
		const std::uint8_t looping = clip.looping ? 1 : 0;
		writer.pod(looping);
		writeVector(writer, clip.tracks,
			[](BinaryWriter& output, const JointAnimationTrack& value) { writeTrack(output, value); });
	}

	bool readClip(BinaryReader& reader, AnimationClip& clip) {
		std::uint8_t looping = 0;
		if (!reader.text(clip.name) || !reader.pod(clip.durationSeconds) ||
			!reader.pod(clip.playbackSpeed) || !reader.pod(looping)) return false;
		clip.looping = looping != 0;
		return readVector(reader, clip.tracks,
			[](BinaryReader& input, JointAnimationTrack& value) { return readTrack(input, value); });
	}

	void writeAssembly(BinaryWriter& writer, const LinkedAssembly& assembly) {
		writer.pod(assembly.id);
		writer.text(assembly.name);
		writer.pod(assembly.nextNodeId);
		writeVector(writer, assembly.nodes,
			[](BinaryWriter& output, const AssemblyNode& value) { writeAssemblyNode(output, value); });
		writeVector(writer, assembly.clips,
			[](BinaryWriter& output, const AnimationClip& value) { writeClip(output, value); });
	}

	bool readAssembly(BinaryReader& reader, LinkedAssembly& assembly) {
		return reader.pod(assembly.id) && reader.text(assembly.name) &&
			reader.pod(assembly.nextNodeId) &&
			readVector(reader, assembly.nodes,
				[](BinaryReader& input, AssemblyNode& value) { return readAssemblyNode(input, value); }) &&
			readVector(reader, assembly.clips,
				[](BinaryReader& input, AnimationClip& value) { return readClip(input, value); });
	}

	void writeKinematic(BinaryWriter& writer, const KinematicParticleAsset& asset) {
		writer.pod(asset.id);
		writer.pod(asset.sourceAssemblyId);
		writer.text(asset.name);
		writer.pod(asset.rootNodeId);
		writeVector(writer, asset.nodes,
			[](BinaryWriter& output, const CompiledNode& value) {
				writeAssemblyNode(output, value.source);
				for (float matrixValue : value.bindMatrix) output.pod(matrixValue);
			});
		writeVector(writer, asset.embeddedStaticAssets,
			[](BinaryWriter& output, const StaticParticleAsset& value) { writeStaticAsset(output, value); });
		writeVector(writer, asset.clips,
			[](BinaryWriter& output, const AnimationClip& value) { writeClip(output, value); });
		writeBounds(writer, asset.bounds);
		writeEnum(writer, asset.rootCollision.shape);
		writeVec3(writer, asset.rootCollision.center);
		writeVec3(writer, asset.rootCollision.halfExtents);
		writer.pod(asset.rootCollision.radius);
		const std::uint8_t ready = asset.runtimeReady ? 1 : 0;
		writer.pod(ready);
	}

	bool readKinematic(BinaryReader& reader, KinematicParticleAsset& asset) {
		if (!reader.pod(asset.id) || !reader.pod(asset.sourceAssemblyId) ||
			!reader.text(asset.name) || !reader.pod(asset.rootNodeId) ||
			!readVector(reader, asset.nodes,
				[](BinaryReader& input, CompiledNode& value) {
					if (!readAssemblyNode(input, value.source)) return false;
					for (float& matrixValue : value.bindMatrix) {
						if (!input.pod(matrixValue)) return false;
					}
					return true;
				}) ||
			!readVector(reader, asset.embeddedStaticAssets,
				[](BinaryReader& input, StaticParticleAsset& value) { return readStaticAsset(input, value); }) ||
			!readVector(reader, asset.clips,
				[](BinaryReader& input, AnimationClip& value) { return readClip(input, value); }) ||
			!readBounds(reader, asset.bounds) ||
			!readEnum(reader, asset.rootCollision.shape,
				static_cast<std::uint32_t>(CollisionProxy::Shape::SPHERE)) ||
			!readVec3(reader, asset.rootCollision.center) ||
			!readVec3(reader, asset.rootCollision.halfExtents) ||
			!reader.pod(asset.rootCollision.radius)) return false;
		std::uint8_t ready = 0;
		if (!reader.pod(ready)) return false;
		asset.runtimeReady = ready != 0;
		return true;
	}

	float evaluateTrack(const JointAnimationTrack& track, float timeSeconds) {
		if (track.keyframes.empty()) return 0.0f;
		if (timeSeconds <= track.keyframes.front().timeSeconds) {
			return track.keyframes.front().rotationDegrees;
		}
		for (std::size_t i = 1; i < track.keyframes.size(); ++i) {
			const JointKeyframe& right = track.keyframes[i];
			if (timeSeconds > right.timeSeconds) continue;
			const JointKeyframe& left = track.keyframes[i - 1];
			const float duration = right.timeSeconds - left.timeSeconds;
			if (duration <= 0.0f) return right.rotationDegrees;
			const float alpha = (timeSeconds - left.timeSeconds) / duration;
			return left.rotationDegrees +
				(right.rotationDegrees - left.rotationDegrees) * alpha;
		}
		return track.keyframes.back().rotationDegrees;
	}

} // namespace

bool ColorRGBA8::operator==(const ColorRGBA8& other) const {
	return r == other.r && g == other.g && b == other.b && a == other.a;
}

void Bounds3::reset() {
	minimum = {};
	maximum = {};
	valid = false;
}

void Bounds3::include(const Vec3& point) {
	if (!finiteVec(point)) return;
	if (!valid) {
		minimum = point;
		maximum = point;
		valid = true;
		return;
	}
	minimum.x = std::min(minimum.x, point.x);
	minimum.y = std::min(minimum.y, point.y);
	minimum.z = std::min(minimum.z, point.z);
	maximum.x = std::max(maximum.x, point.x);
	maximum.y = std::max(maximum.y, point.y);
	maximum.z = std::max(maximum.z, point.z);
}

void Bounds3::include(const Bounds3& bounds) {
	if (!bounds.valid) return;
	include(bounds.minimum);
	include(bounds.maximum);
}

Vec3 Bounds3::center() const {
	if (!valid) return {};
	return multiply(add(minimum, maximum), 0.5f);
}

Vec3 Bounds3::extent() const {
	if (!valid) return {};
	return subtract(maximum, minimum);
}

std::size_t MeshGeometry::triangleCount() const {
	return indices.empty() ? positions.size() / 3u : indices.size() / 3u;
}

Bounds3 MeshGeometry::calculateBounds() const {
	Bounds3 bounds;
	for (const Vec3& position : positions) bounds.include(position);
	return bounds;
}

bool MeshGeometry::validate(std::vector<std::string>* errors) const {
	bool valid = true;
	if (positions.empty()) {
		appendError(errors, "Mesh contains no positions.");
		valid = false;
	}
	for (const Vec3& position : positions) {
		if (!finiteVec(position)) {
			appendError(errors, "Mesh contains a NaN or infinite position.");
			valid = false;
			break;
		}
	}
	if (indices.empty()) {
		if (positions.size() % 3u != 0u) {
			appendError(errors, "Non-indexed mesh vertex count is not divisible by three.");
			valid = false;
		}
	}
	else {
		if (indices.size() % 3u != 0u) {
			appendError(errors, "Mesh index count is not divisible by three.");
			valid = false;
		}
		for (std::uint32_t index : indices) {
			if (index >= positions.size()) {
				appendError(errors, "Mesh index is outside the position array.");
				valid = false;
				break;
			}
		}
	}
	if (!normals.empty() && normals.size() != positions.size()) {
		appendError(errors, "Normal count does not match position count.");
		valid = false;
	}
	for (const Vec3& normal : normals) {
		if (!finiteVec(normal)) {
			appendError(errors, "Mesh contains a NaN or infinite normal.");
			valid = false;
			break;
		}
	}
	if (!uvs.empty() && uvs.size() != positions.size()) {
		appendError(errors, "UV count does not match position count.");
		valid = false;
	}
	for (const Vec2& uv : uvs) {
		if (!finiteFloat(uv.x) || !finiteFloat(uv.y)) {
			appendError(errors, "Mesh contains a NaN or infinite UV coordinate.");
			valid = false;
			break;
		}
	}
	return valid;
}

MeshProcessingReport processMesh(
	MeshGeometry& mesh,
	const MeshProcessingOptions& options) {
	MeshProcessingReport report;
	report.inputTriangles = mesh.triangleCount();

	std::vector<std::uint32_t> sourceIndices = mesh.indices;
	if (sourceIndices.empty()) {
		sourceIndices.resize(mesh.positions.size());
		for (std::size_t i = 0; i < sourceIndices.size(); ++i) {
			sourceIndices[i] = static_cast<std::uint32_t>(i);
		}
	}

	MeshGeometry output;
	std::map<QuantizedVertex, std::uint32_t> welded;
	std::set<TriangleKey> triangles;
	std::vector<Vec3> normalSums;
	std::size_t acceptedVertexInstances = 0;

	auto emitVertex = [&](const Vec3& position) -> std::uint32_t {
		if (options.weldVertices) {
			const QuantizedVertex key = quantize(position, options.weldEpsilon);
			const auto found = welded.find(key);
			if (found != welded.end()) return found->second;
			const std::uint32_t index = static_cast<std::uint32_t>(output.positions.size());
			welded[key] = index;
			output.positions.push_back(position);
			normalSums.push_back({});
			return index;
		}
		const std::uint32_t index = static_cast<std::uint32_t>(output.positions.size());
		output.positions.push_back(position);
		normalSums.push_back({});
		return index;
	};

	for (std::size_t triangle = 0; triangle + 2u < sourceIndices.size(); triangle += 3u) {
		const std::uint32_t ia = sourceIndices[triangle];
		const std::uint32_t ib = sourceIndices[triangle + 1u];
		const std::uint32_t ic = sourceIndices[triangle + 2u];
		if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size()) {
			++report.removedNonFinite;
			continue;
		}
		const Vec3& a = mesh.positions[ia];
		const Vec3& b = mesh.positions[ib];
		const Vec3& c = mesh.positions[ic];
		if (!finiteVec(a) || !finiteVec(b) || !finiteVec(c)) {
			++report.removedNonFinite;
			continue;
		}
		const Vec3 faceCross = cross(subtract(b, a), subtract(c, a));
		if (options.removeDegenerateTriangles &&
			lengthSquared(faceCross) <= options.weldEpsilon * options.weldEpsilon) {
			++report.removedDegenerate;
			continue;
		}
		if (options.removeDuplicateTriangles) {
			const TriangleKey key = triangleKey(a, b, c, options.weldEpsilon);
			if (!triangles.insert(key).second) {
				++report.removedDuplicate;
				continue;
			}
		}

		const std::uint32_t oa = emitVertex(a);
		const std::uint32_t ob = emitVertex(b);
		const std::uint32_t oc = emitVertex(c);
		acceptedVertexInstances += 3u;
		output.indices.push_back(oa);
		output.indices.push_back(ob);
		output.indices.push_back(oc);
		const Vec3 faceNormal = normalized(faceCross);
		normalSums[oa] = add(normalSums[oa], faceNormal);
		normalSums[ob] = add(normalSums[ob], faceNormal);
		normalSums[oc] = add(normalSums[oc], faceNormal);
	}

	output.normals.resize(output.positions.size());
	if (options.smoothNormals) {
		for (std::size_t i = 0; i < output.normals.size(); ++i) {
			output.normals[i] = normalized(normalSums[i]);
		}
	}
	else {
		for (std::size_t triangle = 0; triangle + 2u < output.indices.size(); triangle += 3u) {
			const std::uint32_t a = output.indices[triangle];
			const std::uint32_t b = output.indices[triangle + 1u];
			const std::uint32_t c = output.indices[triangle + 2u];
			const Vec3 face = normalized(cross(
				subtract(output.positions[b], output.positions[a]),
				subtract(output.positions[c], output.positions[a])));
			output.normals[a] = face;
			output.normals[b] = face;
			output.normals[c] = face;
		}
	}

	const Bounds3 bounds = output.calculateBounds();
	const Vec3 extent = bounds.extent();
	const float safeX = std::max(extent.x, 1.0e-6f);
	const float safeZ = std::max(extent.z, 1.0e-6f);
	output.uvs.resize(output.positions.size());
	for (std::size_t i = 0; i < output.positions.size(); ++i) {
		output.uvs[i] = {
			(output.positions[i].x - bounds.minimum.x) / safeX,
			(output.positions[i].z - bounds.minimum.z) / safeZ
		};
	}

	report.outputTriangles = output.triangleCount();
	report.weldedVertices = acceptedVertexInstances >= output.positions.size()
		? acceptedVertexInstances - output.positions.size() : 0;
	report.valid = output.validate(&report.errors) && report.outputTriangles > 0;
	if (!report.valid && report.errors.empty()) {
		report.errors.push_back("Mesh processing produced no valid triangles.");
	}
	mesh = std::move(output);
	return report;
}

bool SurfaceTexture::create(
	std::uint32_t newWidth,
	std::uint32_t newHeight,
	const ColorRGBA8& clearColor) {
	if (newWidth == 0 || newHeight == 0) return false;
	const std::uint64_t count = static_cast<std::uint64_t>(newWidth) * newHeight;
	if (count > kMaxSerializedItems) return false;
	width = newWidth;
	height = newHeight;
	pixels.assign(static_cast<std::size_t>(count), clearColor);
	return true;
}

bool SurfaceTexture::valid() const {
	return width > 0 && height > 0 &&
		pixels.size() == static_cast<std::size_t>(width) * height;
}

void SurfaceTexture::clear(const ColorRGBA8& color) {
	std::fill(pixels.begin(), pixels.end(), color);
}

void SurfaceTexture::paintCircle(
	int centerX,
	int centerY,
	int radius,
	const ColorRGBA8& color) {
	if (!valid()) return;
	radius = std::max(radius, 1);
	const int squareRadius = radius * radius;
	const int minX = std::max(0, centerX - radius);
	const int maxX = std::min(static_cast<int>(width) - 1, centerX + radius);
	const int minY = std::max(0, centerY - radius);
	const int maxY = std::min(static_cast<int>(height) - 1, centerY + radius);
	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			const int dx = x - centerX;
			const int dy = y - centerY;
			if (dx * dx + dy * dy > squareRadius) continue;
			pixels[static_cast<std::size_t>(y) * width + x] = color;
		}
	}
}

void SurfaceTexture::eraseCircle(int centerX, int centerY, int radius) {
	paintCircle(centerX, centerY, radius, { 0, 0, 0, 0 });
}

bool SurfaceTexture::fillRegion(int x, int y, const ColorRGBA8& color) {
	if (!valid() || x < 0 || y < 0 ||
		x >= static_cast<int>(width) || y >= static_cast<int>(height)) return false;
	const std::size_t start = static_cast<std::size_t>(y) * width + x;
	const ColorRGBA8 replaced = pixels[start];
	if (replaced == color) return true;
	std::deque<std::pair<int, int>> pending;
	pending.push_back({ x, y });
	while (!pending.empty()) {
		const int px = pending.front().first;
		const int py = pending.front().second;
		pending.pop_front();
		if (px < 0 || py < 0 || px >= static_cast<int>(width) ||
			py >= static_cast<int>(height)) continue;
		const std::size_t index = static_cast<std::size_t>(py) * width + px;
		if (pixels[index] != replaced) continue;
		pixels[index] = color;
		pending.push_back({ px - 1, py });
		pending.push_back({ px + 1, py });
		pending.push_back({ px, py - 1 });
		pending.push_back({ px, py + 1 });
	}
	return true;
}

bool SurfaceTexture::savePPM(const std::string& path, std::string* error) const {
	if (!valid()) {
		if (error) *error = "Texture is not valid.";
		return false;
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) {
		if (error) *error = "Could not open texture path for writing: " + path;
		return false;
	}
	output << "P6\n" << width << " " << height << "\n255\n";
	for (const ColorRGBA8& pixelColor : pixels) {
		const char rgb[3] = {
			static_cast<char>(pixelColor.r),
			static_cast<char>(pixelColor.g),
			static_cast<char>(pixelColor.b)
		};
		output.write(rgb, 3);
	}
	if (!output.good()) {
		if (error) *error = "Texture write failed: " + path;
		return false;
	}
	return true;
}

bool SurfaceTexture::importPPM(const std::string& path, std::string* error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		if (error) *error = "Could not open texture image: " + path;
		return false;
	}
	std::string magic;
	std::string widthToken;
	std::string heightToken;
	std::string maxToken;
	if (!readPpmToken(input, magic) || !readPpmToken(input, widthToken) ||
		!readPpmToken(input, heightToken) || !readPpmToken(input, maxToken) ||
		magic != "P6") {
		if (error) *error = "Only binary PPM (P6) images are supported.";
		return false;
	}
	std::uint32_t parsedWidth = 0;
	std::uint32_t parsedHeight = 0;
	int maximum = 0;
	try {
		parsedWidth = static_cast<std::uint32_t>(std::stoul(widthToken));
		parsedHeight = static_cast<std::uint32_t>(std::stoul(heightToken));
		maximum = std::stoi(maxToken);
	}
	catch (...) {
		if (error) *error = "PPM header contains an invalid number.";
		return false;
	}
	if (maximum != 255 || !create(parsedWidth, parsedHeight)) {
		if (error) *error = "PPM dimensions or color range are unsupported.";
		return false;
	}
	for (ColorRGBA8& pixelColor : pixels) {
		char rgb[3]{};
		input.read(rgb, 3);
		if (!input) {
			if (error) *error = "PPM image ended before all pixels were read.";
			return false;
		}
		pixelColor = {
			static_cast<std::uint8_t>(static_cast<unsigned char>(rgb[0])),
			static_cast<std::uint8_t>(static_cast<unsigned char>(rgb[1])),
			static_cast<std::uint8_t>(static_cast<unsigned char>(rgb[2])),
			255
		};
	}
	return true;
}

bool SurfaceTexture::importBMP(const std::string& path, std::string* error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		if (error) *error = "Could not open texture image: " + path;
		return false;
	}
	std::array<unsigned char, 54> header{};
	input.read(reinterpret_cast<char*>(header.data()), header.size());
	if (!input || header[0] != 'B' || header[1] != 'M') {
		if (error) *error = "BMP header is missing or truncated.";
		return false;
	}
	auto u16 = [&](std::size_t offset) -> std::uint16_t {
		return static_cast<std::uint16_t>(header[offset]) |
			(static_cast<std::uint16_t>(header[offset + 1]) << 8u);
	};
	auto u32 = [&](std::size_t offset) -> std::uint32_t {
		return static_cast<std::uint32_t>(header[offset]) |
			(static_cast<std::uint32_t>(header[offset + 1]) << 8u) |
			(static_cast<std::uint32_t>(header[offset + 2]) << 16u) |
			(static_cast<std::uint32_t>(header[offset + 3]) << 24u);
	};
	const std::uint32_t dataOffset = u32(10);
	const std::uint32_t dibSize = u32(14);
	const std::int32_t signedWidth = static_cast<std::int32_t>(u32(18));
	const std::int32_t signedHeight = static_cast<std::int32_t>(u32(22));
	const std::uint16_t planes = u16(26);
	const std::uint16_t bitsPerPixel = u16(28);
	const std::uint32_t compression = u32(30);
	if (dibSize < 40 || signedWidth <= 0 || signedHeight == 0 ||
		planes != 1 || (bitsPerPixel != 24 && bitsPerPixel != 32) || compression != 0) {
		if (error) *error = "Only uncompressed 24-bit or 32-bit BMP images are supported.";
		return false;
	}
	if (signedHeight == std::numeric_limits<std::int32_t>::min()) {
		if (error) *error = "BMP height is invalid.";
		return false;
	}
	const std::uint32_t parsedWidth = static_cast<std::uint32_t>(signedWidth);
	const std::uint32_t parsedHeight = static_cast<std::uint32_t>(
		signedHeight < 0 ? -signedHeight : signedHeight);
	if (!create(parsedWidth, parsedHeight)) {
		if (error) *error = "BMP dimensions are unsupported.";
		return false;
	}
	const std::uint64_t rowStride64 =
		((static_cast<std::uint64_t>(parsedWidth) * bitsPerPixel + 31ull) / 32ull) * 4ull;
	if (rowStride64 > kMaxSerializedItems ||
		rowStride64 * parsedHeight > kMaxSerializedItems * 4ull) {
		if (error) *error = "BMP pixel data is too large.";
		return false;
	}
	const std::size_t rowStride = static_cast<std::size_t>(rowStride64);
	std::vector<unsigned char> row(rowStride);
	input.seekg(static_cast<std::streamoff>(dataOffset), std::ios::beg);
	const std::size_t bytesPerPixel = bitsPerPixel / 8u;
	for (std::uint32_t fileY = 0; fileY < parsedHeight; ++fileY) {
		input.read(reinterpret_cast<char*>(row.data()), row.size());
		if (!input) {
			if (error) *error = "BMP image ended before all pixels were read.";
			return false;
		}
		const std::uint32_t targetY = signedHeight < 0
			? fileY : parsedHeight - 1u - fileY;
		for (std::uint32_t x = 0; x < parsedWidth; ++x) {
			const std::size_t source = static_cast<std::size_t>(x) * bytesPerPixel;
			pixels[static_cast<std::size_t>(targetY) * parsedWidth + x] = {
				row[source + 2u], row[source + 1u], row[source],
				bitsPerPixel == 32 ? row[source + 3u] : static_cast<std::uint8_t>(255)
			};
		}
	}
	return true;
}

bool SurfaceTexture::importImage(const std::string& path, std::string* error) {
	std::ifstream input(path, std::ios::binary);
	char signature[2]{};
	input.read(signature, 2);
	if (!input) {
		if (error) *error = "Could not open texture image: " + path;
		return false;
	}
	if (signature[0] == 'B' && signature[1] == 'M') return importBMP(path, error);
	return importPPM(path, error);
}

const ColorRGBA8* SurfaceTexture::pixel(std::uint32_t x, std::uint32_t y) const {
	if (!valid() || x >= width || y >= height) return nullptr;
	return &pixels[static_cast<std::size_t>(y) * width + x];
}

bool StaticParticleAsset::validate(std::vector<std::string>* errors) const {
	bool validAsset = true;
	if (id == INVALID_ASSET_ID) {
		appendError(errors, "Static particle has no asset identity.");
		validAsset = false;
	}
	if (name.empty()) {
		appendError(errors, "Static particle has no name.");
		validAsset = false;
	}
	if (!mesh.validate(errors)) validAsset = false;
	if (!texture.valid()) {
		appendError(errors, "Static particle has no valid surface texture.");
		validAsset = false;
	}
	if (material.name.empty()) {
		appendError(errors, "Static particle has no material identity.");
		validAsset = false;
	}
	if (!bounds.valid) {
		appendError(errors, "Static particle bounds are invalid.");
		validAsset = false;
	}
	if (!finiteVec(collision.center) || !finiteVec(collision.halfExtents) ||
		!finiteFloat(collision.radius)) {
		appendError(errors, "Static particle collision proxy contains invalid values.");
		validAsset = false;
	}
	return validAsset;
}

void StaticParticleAsset::refreshDerivedData() {
	bounds = mesh.calculateBounds();
	collision.shape = CollisionProxy::Shape::BOX;
	collision.center = bounds.center();
	collision.halfExtents = multiply(bounds.extent(), 0.5f);
	collision.radius = 0.5f * std::sqrt(lengthSquared(bounds.extent()));
}

NodeId LinkedAssembly::addNode(const AssemblyNode& node) {
	AssemblyNode stored = node;
	if (stored.id == INVALID_NODE_ID || findNode(stored.id)) {
		stored.id = nextNodeId++;
	}
	else {
		nextNodeId = std::max(nextNodeId, static_cast<NodeId>(stored.id + 1u));
	}
	nodes.push_back(stored);
	return stored.id;
}

bool LinkedAssembly::removeNode(NodeId nodeId) {
	if (!findNode(nodeId)) return false;
	std::unordered_set<NodeId> removed;
	removed.insert(nodeId);
	bool changed = true;
	while (changed) {
		changed = false;
		for (const AssemblyNode& node : nodes) {
			if (removed.count(node.parentId) && !removed.count(node.id)) {
				removed.insert(node.id);
				changed = true;
			}
		}
	}
	nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
		[&](const AssemblyNode& node) { return removed.count(node.id) != 0; }), nodes.end());
	for (AnimationClip& clip : clips) {
		clip.tracks.erase(std::remove_if(clip.tracks.begin(), clip.tracks.end(),
			[&](const JointAnimationTrack& track) {
				return removed.count(track.jointNodeId) != 0;
			}), clip.tracks.end());
	}
	return true;
}

AssemblyNode* LinkedAssembly::findNode(NodeId nodeId) {
	for (AssemblyNode& node : nodes) if (node.id == nodeId) return &node;
	return nullptr;
}

const AssemblyNode* LinkedAssembly::findNode(NodeId nodeId) const {
	for (const AssemblyNode& node : nodes) if (node.id == nodeId) return &node;
	return nullptr;
}

const CompiledNode* KinematicParticleAsset::findNode(NodeId nodeId) const {
	for (const CompiledNode& node : nodes) if (node.source.id == nodeId) return &node;
	return nullptr;
}

void SandboxRuntime::configureDefaultEnvironment() {
	m_targets.clear();
	m_targets.push_back({ 1, { 0.0f, 0.75f, -8.0f }, 0.75f, false });
	m_targets.push_back({ 2, { 3.0f, 0.75f, -12.0f }, 0.75f, false });
	m_targets.push_back({ 3, { -3.0f, 0.75f, -12.0f }, 0.75f, false });
	m_projectiles.clear();
	m_hitCount = 0;
}

bool SandboxRuntime::spawn(const KinematicParticleAsset& asset, const Vec3& position) {
	if (!asset.runtimeReady || !asset.bounds.valid) return false;
	m_activeAsset = &asset;
	m_instance = {};
	m_instance.assetId = asset.id;
	m_instance.rootTransform.position = position;
	m_instance.jointValuesDegrees.assign(asset.nodes.size(), 0.0f);
	m_instance.spawned = true;
	m_projectiles.clear();
	return true;
}

void SandboxRuntime::reset() {
	const KinematicParticleAsset* asset = m_activeAsset;
	configureDefaultEnvironment();
	if (asset) spawn(*asset, {});
}

void SandboxRuntime::drive(
	float forwardAmount,
	float turnAmount,
	float deltaSeconds) {
	if (!m_instance.spawned || !finiteFloat(deltaSeconds) || deltaSeconds <= 0.0f) return;
	m_instance.rootTransform.rotationDegrees.y += turnAmount * 90.0f * deltaSeconds;
	const float radians = m_instance.rootTransform.rotationDegrees.y * kPi / 180.0f;
	m_instance.rootTransform.position.x += std::sin(radians) * forwardAmount * 4.0f * deltaSeconds;
	m_instance.rootTransform.position.z -= std::cos(radians) * forwardAmount * 4.0f * deltaSeconds;
}

bool SandboxRuntime::setJoint(NodeId jointNodeId, float degrees) {
	if (!m_instance.spawned || !m_activeAsset) return false;
	for (std::size_t i = 0; i < m_activeAsset->nodes.size(); ++i) {
		const AssemblyNode& node = m_activeAsset->nodes[i].source;
		if (node.id != jointNodeId || node.type != AssemblyNodeType::JOINT) continue;
		m_instance.jointValuesDegrees[i] = std::max(
			node.jointMinimumDegrees,
			std::min(node.jointMaximumDegrees, degrees));
		return true;
	}
	return false;
}

bool SandboxRuntime::playAnimation(const std::string& clipName) {
	if (!m_instance.spawned || !m_activeAsset) return false;
	for (const AnimationClip& clip : m_activeAsset->clips) {
		if (clip.name != clipName) continue;
		m_instance.activeClip = clipName;
		m_instance.animationTimeSeconds = 0.0f;
		return true;
	}
	return false;
}

bool SandboxRuntime::fireProjectile(float speed) {
	if (!m_instance.spawned || !m_activeAsset || speed <= 0.0f) return false;
	const CompiledNode* muzzle = nullptr;
	for (const CompiledNode& node : m_activeAsset->nodes) {
		if (node.source.type == AssemblyNodeType::INTERACTION &&
			node.source.interactionRole == InteractionRole::WEAPON_MUZZLE) {
			muzzle = &node;
			break;
		}
	}
	if (!muzzle) return false;
	const float yaw = m_instance.rootTransform.rotationDegrees.y * kPi / 180.0f;
	const Vec3 forward{ std::sin(yaw), 0.0f, -std::cos(yaw) };
	Vec3 origin{
		muzzle->bindMatrix[3],
		muzzle->bindMatrix[7],
		muzzle->bindMatrix[11]
	};
	origin = add(origin, m_instance.rootTransform.position);
	m_projectiles.push_back({ origin, multiply(forward, speed), 0.0f, true });
	return true;
}

void SandboxRuntime::update(float deltaSeconds) {
	if (!m_instance.spawned || !m_activeAsset || deltaSeconds <= 0.0f) return;
	if (!m_instance.activeClip.empty()) {
		for (const AnimationClip& clip : m_activeAsset->clips) {
			if (clip.name != m_instance.activeClip) continue;
			m_instance.animationTimeSeconds += deltaSeconds * clip.playbackSpeed;
			if (clip.looping && clip.durationSeconds > 0.0f) {
				m_instance.animationTimeSeconds =
					std::fmod(m_instance.animationTimeSeconds, clip.durationSeconds);
			}
			else if (m_instance.animationTimeSeconds > clip.durationSeconds) {
				m_instance.animationTimeSeconds = clip.durationSeconds;
			}
			for (const JointAnimationTrack& track : clip.tracks) {
				setJoint(track.jointNodeId,
					evaluateTrack(track, m_instance.animationTimeSeconds));
			}
			break;
		}
	}

	for (SandboxProjectile& projectile : m_projectiles) {
		if (!projectile.active) continue;
		projectile.position = add(projectile.position,
			multiply(projectile.velocity, deltaSeconds));
		projectile.ageSeconds += deltaSeconds;
		if (projectile.ageSeconds > 5.0f) projectile.active = false;
		for (SandboxTarget& target : m_targets) {
			if (target.hit || !projectile.active) continue;
			if (lengthSquared(subtract(projectile.position, target.position)) <=
				target.radius * target.radius) {
				target.hit = true;
				projectile.active = false;
				++m_hitCount;
			}
		}
	}
	const auto inactive = [](const SandboxProjectile& projectile) {
		return !projectile.active && projectile.ageSeconds > 0.25f;
	};
	m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(), inactive),
		m_projectiles.end());
}

AssetId ProjectAssetRepository::allocateAssetId() {
	if (m_nextAssetId == INVALID_ASSET_ID) ++m_nextAssetId;
	return m_nextAssetId++;
}

void ProjectAssetRepository::ensureNextAssetIdBeyond(AssetId id) {
	if (id >= m_nextAssetId) m_nextAssetId = id + 1u;
}

AssetId ProjectAssetRepository::addStaticParticle(StaticParticleAsset asset) {
	if (asset.id == INVALID_ASSET_ID || findStaticParticle(asset.id) ||
		findAssembly(asset.id) || findKinematicParticle(asset.id)) {
		asset.id = allocateAssetId();
	}
	else ensureNextAssetIdBeyond(asset.id);
	asset.refreshDerivedData();
	if (!asset.texture.valid()) {
		asset.texture.create(256, 256, asset.material.baseColor);
	}
	m_staticParticles.push_back(std::move(asset));
	return m_staticParticles.back().id;
}

AssetId ProjectAssetRepository::addAssembly(LinkedAssembly assembly) {
	if (assembly.id == INVALID_ASSET_ID || findStaticParticle(assembly.id) ||
		findAssembly(assembly.id) || findKinematicParticle(assembly.id)) {
		assembly.id = allocateAssetId();
	}
	else ensureNextAssetIdBeyond(assembly.id);
	m_assemblies.push_back(std::move(assembly));
	return m_assemblies.back().id;
}

AssetId ProjectAssetRepository::addKinematicParticle(KinematicParticleAsset asset) {
	if (asset.id == INVALID_ASSET_ID || findStaticParticle(asset.id) ||
		findAssembly(asset.id) || findKinematicParticle(asset.id)) {
		asset.id = allocateAssetId();
	}
	else ensureNextAssetIdBeyond(asset.id);
	m_kinematicParticles.push_back(std::move(asset));
	return m_kinematicParticles.back().id;
}

StaticParticleAsset* ProjectAssetRepository::findStaticParticle(AssetId id) {
	for (StaticParticleAsset& asset : m_staticParticles) if (asset.id == id) return &asset;
	return nullptr;
}

const StaticParticleAsset* ProjectAssetRepository::findStaticParticle(AssetId id) const {
	for (const StaticParticleAsset& asset : m_staticParticles) if (asset.id == id) return &asset;
	return nullptr;
}

LinkedAssembly* ProjectAssetRepository::findAssembly(AssetId id) {
	for (LinkedAssembly& asset : m_assemblies) if (asset.id == id) return &asset;
	return nullptr;
}

const LinkedAssembly* ProjectAssetRepository::findAssembly(AssetId id) const {
	for (const LinkedAssembly& asset : m_assemblies) if (asset.id == id) return &asset;
	return nullptr;
}

KinematicParticleAsset* ProjectAssetRepository::findKinematicParticle(AssetId id) {
	for (KinematicParticleAsset& asset : m_kinematicParticles) if (asset.id == id) return &asset;
	return nullptr;
}

const KinematicParticleAsset* ProjectAssetRepository::findKinematicParticle(AssetId id) const {
	for (const KinematicParticleAsset& asset : m_kinematicParticles) if (asset.id == id) return &asset;
	return nullptr;
}

BakeResult ProjectAssetRepository::validateAndBake(
	AssetId assemblyId,
	const std::string& bakedName) const {
	BakeResult result;
	const LinkedAssembly* assembly = findAssembly(assemblyId);
	if (!assembly) {
		addIssue(result.issues, "ASSEMBLY_NOT_FOUND", "The editable assembly does not exist.");
		return result;
	}
	if (assembly->nodes.empty()) {
		addIssue(result.issues, "EMPTY_ASSEMBLY", "The assembly contains no nodes.");
		return result;
	}

	std::unordered_map<NodeId, const AssemblyNode*> nodes;
	std::vector<NodeId> roots;
	for (const AssemblyNode& node : assembly->nodes) {
		if (node.id == INVALID_NODE_ID || nodes.count(node.id)) {
			addIssue(result.issues, "INVALID_NODE_ID", "Assembly node identities must be unique and non-zero.");
			continue;
		}
		nodes[node.id] = &node;
		if (node.parentId == INVALID_NODE_ID) roots.push_back(node.id);
	}
	if (roots.size() != 1u) {
		std::ostringstream message;
		message << "A baked assembly requires exactly one root; found " << roots.size() << ".";
		addIssue(result.issues, "ROOT_COUNT", message.str());
	}
	for (const AssemblyNode& node : assembly->nodes) {
		if (node.parentId != INVALID_NODE_ID && !nodes.count(node.parentId)) {
			addIssue(result.issues, "MISSING_PARENT", "Node '" + node.name + "' references a missing parent.");
		}
		if (!finiteVec(node.localTransform.position) ||
			!finiteVec(node.localTransform.rotationDegrees) ||
			!finiteVec(node.localTransform.scale)) {
			addIssue(result.issues, "INVALID_TRANSFORM", "Node '" + node.name + "' has a non-finite transform.");
		}
		if (node.type == AssemblyNodeType::MESH) {
			const StaticParticleAsset* staticAsset = findStaticParticle(node.staticAssetId);
			if (!staticAsset) {
				addIssue(result.issues, "MISSING_MESH", "Mesh node '" + node.name + "' references a missing static particle.");
			}
			else {
				std::vector<std::string> errors;
				if (!staticAsset->validate(&errors)) {
					for (const std::string& error : errors) {
						addIssue(result.issues, "INVALID_STATIC_ASSET", staticAsset->name + ": " + error);
					}
				}
			}
		}
		else if (node.type == AssemblyNodeType::JOINT) {
			if (node.parentId == INVALID_NODE_ID) {
				addIssue(result.issues, "ROOT_JOINT", "A joint node cannot be the assembly root.");
			}
			if (!finiteVec(node.jointAxis) || lengthSquared(node.jointAxis) <= 1.0e-12f) {
				addIssue(result.issues, "INVALID_JOINT_AXIS", "Joint '" + node.name + "' requires a non-zero axis.");
			}
			if (node.jointMinimumDegrees > node.jointMaximumDegrees) {
				addIssue(result.issues, "INVALID_JOINT_LIMITS", "Joint '" + node.name + "' has reversed limits.");
			}
			bool hasChild = false;
			for (const AssemblyNode& candidate : assembly->nodes) {
				if (candidate.parentId == node.id) { hasChild = true; break; }
			}
			if (!hasChild) addIssue(result.issues, "JOINT_WITHOUT_CHILD", "Joint '" + node.name + "' has no child node.");
		}
	}

	std::unordered_map<NodeId, int> visit;
	std::function<void(NodeId)> detectCycle = [&](NodeId nodeId) {
		if (visit[nodeId] == 1) {
			addIssue(result.issues, "PARENT_CYCLE", "The assembly parent hierarchy contains a cycle.");
			return;
		}
		if (visit[nodeId] == 2) return;
		visit[nodeId] = 1;
		const auto found = nodes.find(nodeId);
		if (found != nodes.end() && found->second->parentId != INVALID_NODE_ID &&
			nodes.count(found->second->parentId)) {
			detectCycle(found->second->parentId);
		}
		visit[nodeId] = 2;
	};
	for (const auto& pair : nodes) detectCycle(pair.first);

	for (const AnimationClip& clip : assembly->clips) {
		if (clip.name.empty() || !finiteFloat(clip.durationSeconds) || clip.durationSeconds <= 0.0f ||
			!finiteFloat(clip.playbackSpeed) || clip.playbackSpeed <= 0.0f) {
			addIssue(result.issues, "INVALID_CLIP", "Every animation clip requires a name, duration, and positive playback speed.");
		}
		for (const JointAnimationTrack& track : clip.tracks) {
			const auto found = nodes.find(track.jointNodeId);
			if (found == nodes.end() || found->second->type != AssemblyNodeType::JOINT) {
				addIssue(result.issues, "INVALID_TRACK_JOINT", "Animation track references a missing or non-joint node.");
			}
			float previousTime = -std::numeric_limits<float>::infinity();
			for (const JointKeyframe& keyframe : track.keyframes) {
				if (!finiteFloat(keyframe.timeSeconds) || !finiteFloat(keyframe.rotationDegrees) ||
					keyframe.timeSeconds < previousTime || keyframe.timeSeconds > clip.durationSeconds) {
					addIssue(result.issues, "INVALID_KEYFRAME", "Animation keyframes must be finite, ordered, and inside the clip duration.");
					break;
				}
				previousTime = keyframe.timeSeconds;
			}
		}
	}

	if (!result.issues.empty()) return result;

	result.asset.name = bakedName.empty() ? assembly->name + " Baked" : bakedName;
	result.asset.sourceAssemblyId = assembly->id;
	result.asset.rootNodeId = roots.front();
	result.asset.clips = assembly->clips;
	std::unordered_map<NodeId, Matrix4> worldMatrices;
	std::function<Matrix4(NodeId)> resolveWorld = [&](NodeId nodeId) -> Matrix4 {
		const auto existing = worldMatrices.find(nodeId);
		if (existing != worldMatrices.end()) return existing->second;
		const AssemblyNode* node = nodes[nodeId];
		const Matrix4 local = transformMatrix(node->localTransform);
		const Matrix4 world = node->parentId == INVALID_NODE_ID
			? local : multiplyMatrix(resolveWorld(node->parentId), local);
		worldMatrices[nodeId] = world;
		return world;
	};
	std::unordered_set<AssetId> embedded;
	for (const AssemblyNode& node : assembly->nodes) {
		CompiledNode compiled;
		compiled.source = node;
		const Matrix4 world = resolveWorld(node.id);
		std::copy(std::begin(world.m), std::end(world.m), std::begin(compiled.bindMatrix));
		result.asset.nodes.push_back(compiled);
		if (node.type == AssemblyNodeType::MESH) {
			const StaticParticleAsset* staticAsset = findStaticParticle(node.staticAssetId);
			includeTransformedBounds(result.asset.bounds, staticAsset->bounds, world);
			if (embedded.insert(staticAsset->id).second) {
				result.asset.embeddedStaticAssets.push_back(*staticAsset);
			}
		}
	}
	if (!result.asset.bounds.valid) {
		addIssue(result.issues, "NO_RENDERABLE_BOUNDS", "The assembly contains no renderable mesh bounds.");
		return result;
	}
	result.asset.rootCollision.shape = CollisionProxy::Shape::BOX;
	result.asset.rootCollision.center = result.asset.bounds.center();
	result.asset.rootCollision.halfExtents = multiply(result.asset.bounds.extent(), 0.5f);
	result.asset.rootCollision.radius =
		0.5f * std::sqrt(lengthSquared(result.asset.bounds.extent()));
	result.asset.runtimeReady = true;
	result.success = true;
	return result;
}

BakeResult ProjectAssetRepository::bakeAndStore(
	AssetId assemblyId,
	const std::string& bakedName) {
	BakeResult result = validateAndBake(assemblyId, bakedName);
	if (!result.success) return result;
	result.asset.id = addKinematicParticle(result.asset);
	return result;
}

bool ProjectAssetRepository::saveProject(
	const std::string& path,
	std::string* error) const {
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) {
		if (error) *error = "Could not open project for writing: " + path;
		return false;
	}
	BinaryWriter writer(output);
	const std::uint32_t magicSize = static_cast<std::uint32_t>(std::strlen(kProjectMagic));
	writer.pod(magicSize);
	writer.bytes(kProjectMagic, magicSize);
	writer.pod(kProjectVersion);
	writer.pod(m_nextAssetId);
	writeVector(writer, m_staticParticles,
		[](BinaryWriter& destination, const StaticParticleAsset& value) { writeStaticAsset(destination, value); });
	writeVector(writer, m_assemblies,
		[](BinaryWriter& destination, const LinkedAssembly& value) { writeAssembly(destination, value); });
	writeVector(writer, m_kinematicParticles,
		[](BinaryWriter& destination, const KinematicParticleAsset& value) { writeKinematic(destination, value); });
	if (!writer.good()) {
		if (error) *error = "Project write failed: " + path;
		return false;
	}
	return true;
}

bool ProjectAssetRepository::loadProject(
	const std::string& path,
	std::string* error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		if (error) *error = "Could not open project: " + path;
		return false;
	}
	BinaryReader reader(input);
	std::uint32_t magicSize = 0;
	if (!reader.pod(magicSize) || magicSize == 0 || magicSize > 128u) {
		if (error) *error = "Project header is invalid.";
		return false;
	}
	std::string magic(magicSize, '\0');
	if (!reader.bytes(&magic[0], magic.size()) || magic != kProjectMagic) {
		if (error) *error = "File is not a VitruGen MVP project.";
		return false;
	}
	std::uint32_t version = 0;
	ProjectAssetRepository loaded;
	if (!reader.pod(version) || version != kProjectVersion ||
		!reader.pod(loaded.m_nextAssetId) ||
		!readVector(reader, loaded.m_staticParticles,
			[](BinaryReader& source, StaticParticleAsset& value) { return readStaticAsset(source, value); }) ||
		!readVector(reader, loaded.m_assemblies,
			[](BinaryReader& source, LinkedAssembly& value) { return readAssembly(source, value); }) ||
		!readVector(reader, loaded.m_kinematicParticles,
			[](BinaryReader& source, KinematicParticleAsset& value) { return readKinematic(source, value); })) {
		if (error) *error = version != kProjectVersion
			? "Project version is unsupported." : "Project data is truncated or corrupt.";
		return false;
	}
	for (const StaticParticleAsset& asset : loaded.m_staticParticles) {
		std::vector<std::string> errors;
		if (!asset.validate(&errors)) {
			if (error) *error = "Loaded static asset is invalid: " +
				(errors.empty() ? asset.name : errors.front());
			return false;
		}
		loaded.ensureNextAssetIdBeyond(asset.id);
	}
	for (const LinkedAssembly& asset : loaded.m_assemblies) loaded.ensureNextAssetIdBeyond(asset.id);
	for (const KinematicParticleAsset& asset : loaded.m_kinematicParticles) loaded.ensureNextAssetIdBeyond(asset.id);
	*this = std::move(loaded);
	return true;
}

void ProjectAssetRepository::clear() {
	m_nextAssetId = 1;
	m_staticParticles.clear();
	m_assemblies.clear();
	m_kinematicParticles.clear();
}

const char* assemblyNodeTypeName(AssemblyNodeType type) {
	switch (type) {
	case AssemblyNodeType::MESH: return "RED MESH";
	case AssemblyNodeType::JOINT: return "BLUE JOINT";
	case AssemblyNodeType::INTERACTION: return "GREEN INTERACTION";
	default: return "UNKNOWN";
	}
}

const char* jointTypeName(JointType type) {
	switch (type) {
	case JointType::FIXED: return "FIXED";
	case JointType::REVOLUTE: return "REVOLUTE";
	default: return "UNKNOWN";
	}
}

const char* interactionRoleName(InteractionRole role) {
	switch (role) {
	case InteractionRole::CONTACT: return "CONTACT";
	case InteractionRole::WHEEL: return "WHEEL";
	case InteractionRole::LEFT_PROPULSION_TRACK: return "LEFT PROPULSION";
	case InteractionRole::RIGHT_PROPULSION_TRACK: return "RIGHT PROPULSION";
	case InteractionRole::THRUSTER: return "THRUSTER";
	case InteractionRole::WEAPON_MUZZLE: return "WEAPON MUZZLE";
	case InteractionRole::SENSOR: return "SENSOR";
	case InteractionRole::SURFACE_ANCHOR: return "SURFACE ANCHOR";
	default: return "UNKNOWN";
	}
}

} // namespace vitru
