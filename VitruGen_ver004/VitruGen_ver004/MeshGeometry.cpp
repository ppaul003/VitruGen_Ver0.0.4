#include "MeshGeometry.h"

namespace vitru {

void MeshGeometry::clear() {
	positions.clear();
	normals.clear();
	uvs.clear();
	indices.clear();
	bounds = MeshBounds{};
}

} // namespace vitru
