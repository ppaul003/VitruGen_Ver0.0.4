#include "TextureMapWorkspace.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_set>
#include <system_error>
#include <utility>

namespace vitru {

    namespace {

        constexpr std::array<float, 17>
            kPreviewParticleRadiusPresets{
                0.0039f,
                0.0046f,
                0.0054f,
                0.0061f,
                0.0068f,
                0.0076f,
                0.0083f,
                0.0091f,
                0.0098f,
                0.0105f,
                0.0113f,
                0.0120f,
                0.0127f,
                0.0135f,
                0.0142f,
                0.0149f,
                0.0156f
            };

        constexpr std::array<std::uint32_t, 3>
            kPixelGridDivisionPresets{
                32,
                64,
                128
            };

        std::string lowerAscii(std::string value) {

            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char c) {
                    return static_cast<char>(
                        std::tolower(c)
                        );
                }
            );

            return value;
        }

        bool isVspaManifestPath(
            const std::filesystem::path& path) {

            const std::string filename =
                lowerAscii(
                    path.filename().string()
                );

            const std::string suffix =
                ".vspa.json";

            if (filename.size() < suffix.size()) {
                return false;
            }

            return filename.compare(
                filename.size() - suffix.size(),
                suffix.size(),
                suffix
            ) == 0;
        }

        std::string normalizedPathKey(
            const std::filesystem::path& path) {

            return lowerAscii(
                path
                .lexically_normal()
                .generic_string()
            );
        }

        int wrappedIndex(int value, int count) {
            if (count <= 0) return 0;
            return (value % count + count) % count;
        }

        bool validAuthoringName(const std::string& value) {
            if (value.empty()) return false;
            for (char c : value) {
                const unsigned char uc = static_cast<unsigned char>(c);
                if (!std::isalnum(uc) && c != '_' && c != '-') return false;
            }
            return true;
        }

        float orientation(
            const TextureMapGridCell& a,
            const TextureMapGridCell& b,
            const TextureMapGridCell& c) {

            return static_cast<float>(b.y - a.y) *
                static_cast<float>(c.x - b.x) -
                static_cast<float>(b.x - a.x) *
                static_cast<float>(c.y - b.y);
        }

        bool strictSegmentsIntersect(
            const TextureMapGridCell& a,
            const TextureMapGridCell& b,
            const TextureMapGridCell& c,
            const TextureMapGridCell& d) {

            const float abC = orientation(a, b, c);
            const float abD = orientation(a, b, d);
            const float cdA = orientation(c, d, a);
            const float cdB = orientation(c, d, b);
            return ((abC > 0.0f && abD < 0.0f) ||
                (abC < 0.0f && abD > 0.0f)) &&
                ((cdA > 0.0f && cdB < 0.0f) ||
                    (cdA < 0.0f && cdB > 0.0f));
        }

        bool pointInPolygon(
            float x,
            float y,
            const std::vector<Vec2>& polygon) {

            bool inside = false;
            if (polygon.size() < 3u) return false;
            for (std::size_t i = 0, j = polygon.size() - 1u;
                i < polygon.size();
                j = i++) {

                const Vec2& a = polygon[i];
                const Vec2& b = polygon[j];
                const bool crosses = ((a.y > y) != (b.y > y)) &&
                    (x < (b.x - a.x) * (y - a.y) /
                        ((b.y - a.y) == 0.0f ? 1.0e-12f : (b.y - a.y)) + a.x);
                if (crosses) inside = !inside;
            }
            return inside;
        }

    } // namespace

    void TextureMapEditSession::clear() {
        *this = TextureMapEditSession{};
    }


    bool TextureMapWorkspace::initialize(
        ProjectAssetRepository* repository,
        const std::filesystem::path& outputStaticParticlesRoot,
        const std::filesystem::path& baseMaterialsRoot) {

        // Do not leave a partially initialized workspace behind.
        m_initialized = false;
        m_repository = nullptr;

        m_outputStaticParticlesRoot.clear();
        m_baseMaterialsRoot.clear();

        if (!repository) {

            std::printf(
                "[TextureMapWorkspace] Initialization failed: "
                "ProjectAssetRepository is null.\n"
            );

            return false;
        }

        std::error_code error;

        const bool outputRootReady =
            std::filesystem::is_directory(
                outputStaticParticlesRoot,
                error
            );

        if (!outputRootReady || error) {

            std::printf(
                "[TextureMapWorkspace] Initialization failed: "
                "OUTPUT static-particle directory is unavailable: %s\n",
                outputStaticParticlesRoot.string().c_str()
            );

            return false;
        }

        error.clear();

        const bool materialRootReady =
            std::filesystem::is_directory(
                baseMaterialsRoot,
                error
            );

        if (!materialRootReady || error) {

            std::printf(
                "[TextureMapWorkspace] Initialization failed: "
                "base-material directory is unavailable: %s\n",
                baseMaterialsRoot.string().c_str()
            );

            return false;
        }

        m_repository = repository;

        m_outputStaticParticlesRoot =
            outputStaticParticlesRoot;

        m_baseMaterialsRoot =
            baseMaterialsRoot;

        // Initialize deterministic workspace defaults.
        m_outputCatalog.clear();
        m_baseMaterialCatalog.clear();

        m_selectedOutputIndex = 0;
        m_selectedBaseMaterialIndex = 0;

        m_outputCatalogReady = false;

        m_target =
            TextureMapTargetContext{};

        m_session =
            TextureMapEditSession{};

        m_focus =
            TextureMapFocus::TextureCanvas;

        m_subLayer =
            TextureMapSubLayer::CycleSetup;

        m_initialized = true;

        // Build the initial OUTPUT/STATIC_PARTICLES catalog.
        //
        // An empty catalog is valid.
        //
        // Failure to scan is non-fatal. refreshOutputCatalog()
        // remains the reusable internal rescan operation and will
        // later be invoked automatically when Layer 1 is re-entered.
        if (!refreshOutputCatalog()) {

            std::printf(
                "[TextureMapWorkspace] WARNING: "
                "initial OUTPUT catalog scan failed.\n"
            );
        }

        std::printf(
            "[TextureMapWorkspace] Runtime initialized.\n"
        );

        std::printf(
            "  OUTPUT catalog root: %s\n",
            m_outputStaticParticlesRoot.string().c_str()
        );

        std::printf(
            "  Base-material root: %s\n",
            m_baseMaterialsRoot.string().c_str()
        );

        return true;
    }

    bool TextureMapWorkspace::refreshOutputCatalog() {

        if (!m_initialized) {

            std::printf(
                "[TextureMapWorkspace] OUTPUT catalog refresh skipped: "
                "workspace is not initialized.\n"
            );

            return false;
        }

        std::error_code error;

        if (!std::filesystem::is_directory(
            m_outputStaticParticlesRoot,
            error) || error) {

            std::printf(
                "[TextureMapWorkspace] OUTPUT catalog refresh failed: %s\n",
                m_outputStaticParticlesRoot.string().c_str()
            );

            return false;
        }

        // ---------------------------------------------------------
        // Remember the currently selected manifest so a manual
        // refresh does not unnecessarily jump the user's selection.
        // ---------------------------------------------------------
        std::filesystem::path previousSelection;

        if (const StaticAssetCatalogEntry* selected =
            selectedOutputAsset()) {

            previousSelection =
                selected->manifestPath;
        }

        const std::string previousSelectionKey =
            normalizedPathKey(
                previousSelection
            );

        // Build into a temporary catalog.
        //
        // If scanning fails, the currently active catalog remains
        // untouched.
        std::vector<StaticAssetCatalogEntry> nextCatalog;

        std::filesystem::recursive_directory_iterator iterator(
            m_outputStaticParticlesRoot,
            std::filesystem::directory_options::skip_permission_denied,
            error
        );

        const std::filesystem::recursive_directory_iterator end;

        if (error) {

            std::printf(
                "[TextureMapWorkspace] Could not begin OUTPUT scan.\n"
            );

            return false;
        }

        while (iterator != end) {

            const std::filesystem::directory_entry entry =
                *iterator;

            std::error_code entryError;

            const bool regularFile =
                entry.is_regular_file(
                    entryError
                );

            if (!entryError &&
                regularFile &&
                isVspaManifestPath(entry.path())) {

                StaticParticleAsset manifestAsset;
                VspaLoadReport manifestReport;

                const bool valid =
                    loadVspaManifest(
                        entry.path(),
                        manifestAsset,
                        manifestReport
                    );

                StaticAssetCatalogEntry catalogEntry;

                catalogEntry.manifestPath =
                    entry.path().lexically_normal();

                catalogEntry.source =
                    "OUTPUT";

                catalogEntry.valid =
                    valid;

                if (!manifestAsset.name.empty()) {

                    catalogEntry.displayName =
                        manifestAsset.name;
                }
                else {

                    // foo.vspa.json
                    //   -> foo.vspa
                    //   -> foo
                    catalogEntry.displayName =
                        entry.path()
                        .stem()
                        .stem()
                        .string();
                }

                if (valid) {

                    catalogEntry.validationMessage =
                        "VALID";
                }
                else if (!manifestReport.errors.empty()) {

                    catalogEntry.validationMessage =
                        manifestReport.errors.front();
                }
                else {

                    catalogEntry.validationMessage =
                        "INVALID";
                }

                nextCatalog.push_back(
                    std::move(catalogEntry)
                );
            }

            iterator.increment(error);

            if (error) {

                std::printf(
                    "[TextureMapWorkspace] OUTPUT catalog scan failed "
                    "during directory traversal.\n"
                );

                return false;
            }
        }

        // ---------------------------------------------------------
        // Deterministic ordering:
        //
        //     display name, case-insensitive
        //     manifest path as tie-breaker
        // ---------------------------------------------------------
        std::sort(
            nextCatalog.begin(),
            nextCatalog.end(),
            [](const StaticAssetCatalogEntry& a,
                const StaticAssetCatalogEntry& b) {

                    const std::string nameA =
                        lowerAscii(a.displayName);

                    const std::string nameB =
                        lowerAscii(b.displayName);

                    if (nameA != nameB) {
                        return nameA < nameB;
                    }

                    return normalizedPathKey(
                        a.manifestPath
                    ) <
                        normalizedPathKey(
                            b.manifestPath
                        );
            }
        );

        std::size_t nextSelection = 0;
        bool restoredPreviousSelection = false;

        // ---------------------------------------------------------
        // Prefer preserving the manifest that was selected before
        // the refresh.
        // ---------------------------------------------------------
        if (!previousSelectionKey.empty()) {

            for (std::size_t index = 0;
                index < nextCatalog.size(); index++) {

                if (normalizedPathKey(
                    nextCatalog[index].manifestPath) ==
                    previousSelectionKey) {

                    nextSelection = index;

                    restoredPreviousSelection =
                        true;

                    break;
                }
            }
        }

        // ---------------------------------------------------------
        // If the old asset vanished, prefer the first VALID bundle.
        // If every entry is invalid, row zero remains selected so
        // the user can still inspect the invalid catalog entry.
        // ---------------------------------------------------------
        if (!restoredPreviousSelection) {

            for (std::size_t index = 0;
                index < nextCatalog.size();
                ++index) {

                if (nextCatalog[index].valid) {

                    nextSelection = index;
                    break;
                }
            }
        }

        m_outputCatalog =
            std::move(nextCatalog);

        m_selectedOutputIndex =
            m_outputCatalog.empty()
            ? 0
            : nextSelection;

        // A successful scan means the catalog is ready even when
        // OUTPUT/STATIC_PARTICLES contains zero assets.
        m_outputCatalogReady = true;

        std::printf(
            "[TextureMapWorkspace] OUTPUT catalog refreshed: "
            "%zu asset(s).\n",
            m_outputCatalog.size()
        );

        if (const StaticAssetCatalogEntry* selected =
            selectedOutputAsset()) {

            std::printf(
                "  Selected: %s [%s]\n",
                selected->displayName.c_str(),
                selected->valid
                ? "READY"
                : "INVALID"
            );
        }

        return true;
    }

    bool TextureMapWorkspace::selectOutputAsset(int direction) {

        if (direction == 0 ||
            m_outputCatalog.empty()) {

            return false;
        }

        const int count =
            static_cast<int>(
                m_outputCatalog.size()
                );

        const int current =
            static_cast<int>(
                m_selectedOutputIndex
                );

        const int step =
            direction < 0
            ? -1
            : +1;

        const int next =
            (current + step + count) %
            count;

        const bool changed =
            next != current;

        m_selectedOutputIndex =
            static_cast<std::size_t>(next);

        // ---------------------------------------------------------
        // Selecting a different OUTPUT asset invalidates the current
        // texture-map target.
        //
        // The previously loaded asset may remain in the shared
        // ProjectAssetRepository, but it is no longer the active
        // TEXTURE_MAP_2D target.
        // ---------------------------------------------------------
        if (changed) {

            m_target = TextureMapTargetContext{};
            m_session = TextureMapEditSession{};
        }

        return changed;
    }

    const StaticAssetCatalogEntry*
        TextureMapWorkspace::selectedOutputAsset() const {

        if (m_outputCatalog.empty() ||
            m_selectedOutputIndex >=
            m_outputCatalog.size()) {

            return nullptr;
        }

        return &m_outputCatalog[
            m_selectedOutputIndex
        ];
    }

    bool TextureMapWorkspace::activateLoadedTarget(
        AssetId assetId) {

        if (!m_repository) {

            m_target =
                TextureMapTargetContext{};

            m_target.readiness =
                TextureTargetReadiness::Invalid;

            return false;
        }

        const StaticParticleAsset* asset =
            m_repository->findStaticParticle(
                assetId
            );

        if (!asset) {

            m_target =
                TextureMapTargetContext{};

            m_target.readiness =
                TextureTargetReadiness::Invalid;

            return false;
        }

        // ---------------------------------------------------------
        // Establish a fresh Layer 2 target context.
        // ---------------------------------------------------------
        m_target =
            TextureMapTargetContext{};

        m_session =
            TextureMapEditSession{};

        m_target.assetId =
            assetId;

        m_target.loaded =
            true;

        // ---------------------------------------------------------
        // Basic mesh validation.
        // ---------------------------------------------------------
        if (asset->mesh.empty()) {

            m_target.readiness =
                TextureTargetReadiness::Invalid;

            return false;
        }

        // ---------------------------------------------------------
        // TEXCOORD_0 readiness.
        // ---------------------------------------------------------
        if (asset->mesh.uvs.size() !=
            asset->mesh.positions.size()) {

            m_target.readiness =
                TextureTargetReadiness::NeedsUv;

            return true;
        }

        // ---------------------------------------------------------
        // Determine the initial material slot.
        //
        // Prefer the first submesh's assigned material when valid.
        // Otherwise use material slot zero.
        // ---------------------------------------------------------
        if (asset->materials.empty()) {

            m_target.readiness =
                TextureTargetReadiness::NeedsTexture;

            return true;
        }

        std::size_t materialIndex = 0;

        if (!asset->submeshes.empty()) {

            const std::size_t candidate =
                static_cast<std::size_t>(
                    asset->submeshes.front().materialIndex
                    );

            if (candidate < asset->materials.size()) {

                materialIndex =
                    candidate;
            }
        }

        m_target.submeshIndex =
            0;

        m_target.materialIndex =
            materialIndex;

        m_target.channel =
            TextureUsage::BaseColor;

        const MaterialSlot& material =
            asset->materials[materialIndex];

        m_target.textureId =
            material.baseColorTextureId;

        // ---------------------------------------------------------
        // BASE_COLOR readiness.
        // ---------------------------------------------------------
        if (m_target.textureId.empty()) {

            m_target.readiness =
                TextureTargetReadiness::NeedsTexture;

            return true;
        }

        const TextureResource* texture =
            asset->findTexture(
                m_target.textureId
            );

        if (!texture ||
            !texture->valid) {

            m_target.readiness =
                TextureTargetReadiness::NeedsTexture;

            return true;
        }

        // ---------------------------------------------------------
        // Target is ready for TEXTURE_MAP_2D Layer 2.
        // ---------------------------------------------------------
        m_target.readiness =
            TextureTargetReadiness::Ready;

        return true;
    }

    bool TextureMapWorkspace::adjustPreviewParticleRadius(int direction) {

        if (!m_target.loaded ||
            direction == 0) {

            return false;
        }

        const float currentRadius =
            m_target.previewParticleRadius;

        std::size_t currentIndex = 0;

        float nearestDistance =
            std::fabs(
                currentRadius -
                kPreviewParticleRadiusPresets[0]
            );

        for (std::size_t i = 1;
            i < kPreviewParticleRadiusPresets.size();
            i++) {

            const float distance =
                std::fabs(
                    currentRadius -
                    kPreviewParticleRadiusPresets[i]
                );

            if (distance < nearestDistance) {

                nearestDistance = distance;
                currentIndex = i;
            }
        }

        std::size_t nextIndex = currentIndex;

        if (direction < 0) {

            if (currentIndex == 0) {
                return false;
            }

            nextIndex = currentIndex - 1;
        }
        else {

            if (currentIndex + 1 >=
                kPreviewParticleRadiusPresets.size()) {

                return false;
            }

            nextIndex = currentIndex + 1;
        }

        const float nextRadius =
            kPreviewParticleRadiusPresets[nextIndex];

        if (nextRadius == currentRadius) {

            return false;
        }

        m_target.previewParticleRadius = nextRadius;

        return true;
    }

    const std::array<TextureMapLineColorPreset, 4>&
        TextureMapWorkspace::lineColorPresets() {

        static const std::array<TextureMapLineColorPreset, 4> presets{
            TextureMapLineColorPreset{ "BLACK", { 12u, 12u, 14u, 255u } },
            TextureMapLineColorPreset{ "BROWN", { 96u, 55u, 32u, 255u } },
            TextureMapLineColorPreset{ "SILVER", { 184u, 192u, 204u, 255u } },
            TextureMapLineColorPreset{ "DARK_RED", { 92u, 10u, 18u, 255u } }
        };
        return presets;
    }

    const char* TextureMapWorkspace::boxAtlasFaceAxisName(
        std::uint32_t faceIndex) {

        // This table is the public editor contract for the existing
        // MeshUVGenerator::faceCell mapping.
        static const char* names[6]{
            "+X", "-X", "+Y", "-Y", "+Z", "-Z"
        };
        return faceIndex < 6u ? names[faceIndex] : "INVALID";
    }

    bool TextureMapWorkspace::channelActive(TextureMapChannel channel) {
        return channel == TextureMapChannel::BaseColor ||
            channel == TextureMapChannel::EmissiveColor;
    }

    bool TextureMapWorkspace::refreshTargetContext() {
        return m_target.assetId != INVALID_ASSET_ID &&
            activateLoadedTarget(m_target.assetId);
    }

    bool TextureMapWorkspace::beginAuthoringRuntime() {
        if (!m_initialized || !m_repository || !m_target.loaded ||
            m_target.readiness != TextureTargetReadiness::Ready ||
            !m_repository->findStaticParticle(m_target.assetId)) {
            m_runtimeStatusMessage = "TEXTURE MAP TARGET IS NOT READY.";
            return false;
        }

        m_subLayer = TextureMapSubLayer::CycleSetup;
        m_authoringMode = TextureMapAuthoringMode::Coloring;
        m_previewSource = TextureMapPreviewSource::Working;
        m_selectedChannel = TextureMapChannel::BaseColor;
        m_contourAction = TextureMapContourAction::New;
        m_selectedSurfaceTarget = -1;
        m_selectedContourTarget = -1;
        m_nestedFocus = false;
        m_baseMaterialSourceSelected = false;
        const StaticParticleAsset* canonical =
            m_repository->findStaticParticle(m_target.assetId);
        m_emissiveIntensity = canonical &&
            m_target.materialIndex < canonical->materials.size()
            ? canonical->materials[m_target.materialIndex].emissiveIntensity
            : 1.0f;
        m_runtimeStatusMessage.clear();
        m_session.clear();
        return true;
    }

    int TextureMapWorkspace::runtimeRowCount() const {
        switch (m_subLayer) {
        case TextureMapSubLayer::CycleSetup: return 3;
        case TextureMapSubLayer::BranchSetup: return 4;
        case TextureMapSubLayer::PixelEditor:
            if (m_authoringMode == TextureMapAuthoringMode::Coloring) return 7;
            return 5;
        case TextureMapSubLayer::CommitSave: return 5;
        default: return 0;
        }
    }

    bool TextureMapWorkspace::adjustRuntimeValue(int row, int direction) {
        if (direction == 0) return false;
        const int step = direction < 0 ? -1 : 1;
        StaticParticleAsset* asset = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;

        if (m_subLayer == TextureMapSubLayer::CycleSetup) {
            if (row == 0) {
                m_authoringMode = static_cast<TextureMapAuthoringMode>(
                    wrappedIndex(static_cast<int>(m_authoringMode) + step, 3));
                return true;
            }
            if (row == 1) {
                m_previewSource = m_previewSource == TextureMapPreviewSource::Working
                    ? TextureMapPreviewSource::Committed
                    : TextureMapPreviewSource::Working;
                return true;
            }
            return false;
        }

        if (m_subLayer == TextureMapSubLayer::BranchSetup) {
            if (m_authoringMode == TextureMapAuthoringMode::Coloring) {
                if (row == 0 && asset) {
                    const int count = static_cast<int>(asset->surfaceTargets.size()) + 1;
                    m_selectedSurfaceTarget =
                        wrappedIndex(m_selectedSurfaceTarget + 1 + step, count) - 1;
                    return true;
                }
                if (row == 1) {
                    if (m_nestedFocus) {
                        if (m_selectedChannel == TextureMapChannel::BaseColor)
                            return selectBaseMaterial(step);
                        if (m_selectedChannel == TextureMapChannel::EmissiveColor) {
                            const float next = std::max(0.0f, std::min(4.0f,
                                m_emissiveIntensity + 0.25f * static_cast<float>(step)));
                            if (next == m_emissiveIntensity) return false;
                            m_emissiveIntensity = next;
                            return true;
                        }
                        return false;
                    }
                    m_selectedChannel = static_cast<TextureMapChannel>(
                        wrappedIndex(static_cast<int>(m_selectedChannel) + step,
                            static_cast<int>(TextureMapChannel::Count)));
                    return true;
                }
            }
            else if (m_authoringMode == TextureMapAuthoringMode::Contour) {
                if (row == 0) {
                    m_contourAction = m_contourAction == TextureMapContourAction::New
                        ? TextureMapContourAction::EditExisting
                        : TextureMapContourAction::New;
					if (m_contourAction == TextureMapContourAction::EditExisting &&
						asset && !asset->surfaceTargets.empty())
						m_selectedContourTarget = 0;
                    return true;
                }
                if (row == 1 && asset &&
                    m_contourAction == TextureMapContourAction::EditExisting &&
                    !asset->surfaceTargets.empty()) {
                    m_selectedContourTarget = wrappedIndex(
                        m_selectedContourTarget + step,
                        static_cast<int>(asset->surfaceTargets.size()));
                    return true;
                }
            }
            else if (row == 0 && asset) {
                const int count = static_cast<int>(asset->surfaceTargets.size()) + 1;
                m_selectedSurfaceTarget =
                    wrappedIndex(m_selectedSurfaceTarget + 1 + step, count) - 1;
                return true;
            }
            return false;
        }

        if (m_subLayer != TextureMapSubLayer::PixelEditor) return false;

        if (row == 0) {
            if (m_authoringMode == TextureMapAuthoringMode::Contour &&
                !m_session.contourCells.empty()) {
                m_runtimeStatusMessage = "SELECT FACE IS LOCKED WHILE CONTOUR HAS POINTS.";
                return false;
            }
            m_session.selectedFace = static_cast<std::uint32_t>(
                wrappedIndex(static_cast<int>(m_session.selectedFace) + step, 6));
            return true;
        }

        if (m_authoringMode == TextureMapAuthoringMode::Coloring &&
            row >= 1 && row <= 4) {
            const std::size_t component = static_cast<std::size_t>(row - 1);
            const int value = static_cast<int>(m_session.paintColor[component]) + step;
            const std::uint8_t next = static_cast<std::uint8_t>(
                std::max(0, std::min(255, value)));
            if (next == m_session.paintColor[component]) return false;
            m_session.paintColor[component] = next;
            return true;
        }

        if (m_authoringMode == TextureMapAuthoringMode::PanelLines) {
            if (row == 1) {
                const int next = std::max(1, std::min(8,
                    m_session.lineThickness + step));
                if (next == m_session.lineThickness) return false;
                m_session.lineThickness = next;
                return true;
            }
            if (row == 2) {
                m_session.lineColorPreset = static_cast<std::size_t>(wrappedIndex(
                    static_cast<int>(m_session.lineColorPreset) + step,
                    static_cast<int>(lineColorPresets().size())));
                return true;
            }
        }
        return false;
    }

    TextureMapWorkspaceAction TextureMapWorkspace::activateRuntimeRow(
        int row,
        std::string* diagnostic) {

        auto reject = [&](const std::string& message) {
            m_runtimeStatusMessage = message;
            if (diagnostic) *diagnostic = message;
            return TextureMapWorkspaceAction::Rejected;
        };

        m_runtimeStatusMessage.clear();

        if (m_subLayer == TextureMapSubLayer::CycleSetup) {
            if (row != 2) return TextureMapWorkspaceAction::None;
            m_subLayer = TextureMapSubLayer::BranchSetup;
            m_nestedFocus = false;
            return TextureMapWorkspaceAction::StateChanged;
        }

        if (m_subLayer == TextureMapSubLayer::BranchSetup) {
            if (row == 1 && m_authoringMode == TextureMapAuthoringMode::Coloring) {
                if (!channelActive(m_selectedChannel))
                    return reject("SELECTED TEXTURE CHANNEL IS INACTIVE.");
                m_nestedFocus = !m_nestedFocus;
                return TextureMapWorkspaceAction::StateChanged;
            }

            if (row == 2) {
                if (m_authoringMode == TextureMapAuthoringMode::Coloring &&
                    !channelActive(m_selectedChannel))
                    return reject("SELECTED TEXTURE CHANNEL IS INACTIVE.");
                const StaticParticleAsset* asset = m_repository
                    ? m_repository->findStaticParticle(m_target.assetId)
                    : nullptr;
                if (m_authoringMode == TextureMapAuthoringMode::Contour &&
                    m_contourAction == TextureMapContourAction::EditExisting &&
                    (!asset || asset->surfaceTargets.empty()))
                    return reject("NO EXISTING SURFACE TARGETS ARE AVAILABLE.");
                if (!prepareWorkingPass(diagnostic))
                    return reject(diagnostic && !diagnostic->empty()
                        ? *diagnostic : "WORKING PASS COULD NOT BE PREPARED.");
                m_subLayer = TextureMapSubLayer::PixelEditor;
                m_nestedFocus = false;
                return TextureMapWorkspaceAction::StateChanged;
            }

            if (row == 3) {
                abandonWorkingPass();
                m_subLayer = TextureMapSubLayer::CycleSetup;
                m_nestedFocus = false;
                return TextureMapWorkspaceAction::StateChanged;
            }
            return TextureMapWorkspaceAction::None;
        }

        if (m_subLayer == TextureMapSubLayer::PixelEditor) {
            if (m_authoringMode == TextureMapAuthoringMode::Coloring) {
                if (row == 5) {
                    m_subLayer = TextureMapSubLayer::CommitSave;
                    return TextureMapWorkspaceAction::StateChanged;
                }
                if (row == 6) {
                    abandonWorkingPass();
                    m_subLayer = TextureMapSubLayer::BranchSetup;
                    return TextureMapWorkspaceAction::StateChanged;
                }
            }
            else if (m_authoringMode == TextureMapAuthoringMode::Contour) {
                if (row == 1) {
                    return closeContour(diagnostic)
                        ? TextureMapWorkspaceAction::StateChanged
                        : TextureMapWorkspaceAction::Rejected;
                }
                if (row == 2) {
                    return undoContourPoint()
                        ? TextureMapWorkspaceAction::StateChanged
                        : TextureMapWorkspaceAction::Rejected;
                }
                if (row == 3) {
                    if (!m_session.contourClosed || contourHasSelfIntersection())
                        return reject("CLOSE A VALID CONTOUR BEFORE REVIEW / COMMIT.");
                    m_subLayer = TextureMapSubLayer::CommitSave;
                    return TextureMapWorkspaceAction::StateChanged;
                }
                if (row == 4) {
                    abandonWorkingPass();
                    m_subLayer = TextureMapSubLayer::BranchSetup;
                    return TextureMapWorkspaceAction::StateChanged;
                }
            }
            else {
                if (row == 3) {
                    m_subLayer = TextureMapSubLayer::CommitSave;
                    return TextureMapWorkspaceAction::StateChanged;
                }
                if (row == 4) {
                    abandonWorkingPass();
                    m_subLayer = TextureMapSubLayer::BranchSetup;
                    return TextureMapWorkspaceAction::StateChanged;
                }
            }
            return TextureMapWorkspaceAction::None;
        }

        if (m_subLayer == TextureMapSubLayer::CommitSave) {
            if (row == 0) {
                if (!m_session.dirty) return reject("WORKING EDIT IS ALREADY CLEAN.");
                if (m_authoringMode == TextureMapAuthoringMode::Contour &&
                    m_contourAction == TextureMapContourAction::New)
                    return TextureMapWorkspaceAction::RequestSurfaceTargetName;
                return commitWorkingEdit(std::string{}, diagnostic)
                    ? TextureMapWorkspaceAction::StateChanged
                    : TextureMapWorkspaceAction::Rejected;
            }
            if (row == 1) {
                if (m_session.dirty)
                    return reject("COMMIT WORKING EDIT TO TARGET FIRST.");
                return TextureMapWorkspaceAction::RequestSaveCurrent;
            }
            if (row == 2) return TextureMapWorkspaceAction::RequestSaveAs;
            if (row == 3) {
                m_subLayer = TextureMapSubLayer::PixelEditor;
                return TextureMapWorkspaceAction::StateChanged;
            }
            if (row == 4) {
                abandonWorkingPass();
                m_subLayer = TextureMapSubLayer::CycleSetup;
                return TextureMapWorkspaceAction::StateChanged;
            }
        }
        return TextureMapWorkspaceAction::None;
    }

    ImageRGBA8 TextureMapWorkspace::textureImage(
        const TextureResource* texture) const {

        ImageRGBA8 image;
        if (!texture) return image;
        if (texture->width > 0u && texture->height > 0u &&
            texture->channels == 4u &&
            texture->pixels.size() == static_cast<std::size_t>(texture->width) *
                texture->height * 4u) {
            image.width = texture->width;
            image.height = texture->height;
            image.pixels = texture->pixels;
            return image;
        }
        if (!texture->sourcePath.empty()) {
            std::string ignored;
            loadPngImage(texture->sourcePath, image, &ignored, true);
        }
        return image;
    }

    bool TextureMapWorkspace::prepareWorkingPass(std::string* diagnostic) {
        StaticParticleAsset* asset = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;
        if (!asset || m_target.materialIndex >= asset->materials.size()) {
            if (diagnostic) *diagnostic = "CANONICAL TARGET MATERIAL IS UNAVAILABLE.";
            return false;
        }

        TextureMapEditSession session;
        session.active = true;
        session.assetId = asset->id;
        session.materialIndex = m_target.materialIndex;
        session.submeshIndex = m_target.submeshIndex;
        session.authoringMode = m_authoringMode;
        session.channel = m_selectedChannel;
        session.contourAction = m_contourAction;
        session.selectedSurfaceTarget = m_selectedSurfaceTarget;
        session.selectedContourTarget = m_selectedContourTarget;
        session.viewMode = TextureMapViewMode::Edit;
        session.emissiveIntensity = m_emissiveIntensity;

        const MaterialSlot& material = asset->materials[m_target.materialIndex];
        const TextureResource* base = asset->findTexture(material.baseColorTextureId);
        ImageRGBA8 baseImage = textureImage(base);
        if (!baseImage.valid()) {
            if (diagnostic) *diagnostic = "BASE COLOR TEXTURE DATA IS UNAVAILABLE.";
            return false;
        }

        session.originalImage = baseImage;
        session.baseMaterialImage = baseImage;
        session.compositeImage = baseImage;
        session.workingImage = baseImage;
        session.textureId = material.baseColorTextureId;

        if (m_authoringMode == TextureMapAuthoringMode::Coloring) {
            if (m_selectedChannel == TextureMapChannel::BaseColor) {
                const BaseMaterialCatalogEntry* source = selectedBaseMaterial();
                if (m_baseMaterialSourceSelected && source && source->valid) {
                    ImageRGBA8 selected;
                    std::string error;
                    if (loadPngImage(source->baseColorPath, selected, &error, true) &&
                        selected.width == baseImage.width && selected.height == baseImage.height) {
                        session.workingImage = selected;
                        session.baseMaterialImage = selected;
                        session.dirty = selected.pixels != baseImage.pixels;
                    }
                }
            }
            else if (m_selectedChannel == TextureMapChannel::EmissiveColor) {
                const TextureResource* emissive = asset->findTexture(material.emissiveTextureId);
                ImageRGBA8 emissiveImage = textureImage(emissive);
                if (!emissiveImage.valid())
                    emissiveImage = makeSolidImage(baseImage.width, baseImage.height, 0u, 0u, 0u, 0u);
                session.originalImage = emissiveImage;
                session.workingImage = emissiveImage;
                session.textureId = material.emissiveTextureId;
				session.dirty = std::fabs(
					session.emissiveIntensity - material.emissiveIntensity) > 0.0001f;
            }
        }
        else if (m_authoringMode == TextureMapAuthoringMode::Contour) {
            if (m_contourAction == TextureMapContourAction::EditExisting) {
                if (asset->surfaceTargets.empty()) {
                    if (diagnostic) *diagnostic = "NO EXISTING SURFACE TARGETS ARE AVAILABLE.";
                    return false;
                }
                const int targetIndex = wrappedIndex(m_selectedContourTarget,
                    static_cast<int>(asset->surfaceTargets.size()));
                session.selectedContourTarget = targetIndex;
                const SurfaceTarget& target = asset->surfaceTargets[
                    static_cast<std::size_t>(targetIndex)];
                session.selectedFace = target.faceIndex;
                const float grid = static_cast<float>(m_target.pixelGridDivisions);
                for (const Vec2& point : target.normalizedPolygon) {
                    session.contourCells.push_back({
                        static_cast<int>(std::lround(point.x * grid - 0.5f)),
                        static_cast<int>(std::lround(point.y * grid - 0.5f))
                    });
                }
                session.contourClosed = true;
            }
        }

        session.revision = 1u;
        m_session = std::move(session);
        return true;
    }

    bool TextureMapWorkspace::beginEditSession() {
        return prepareWorkingPass(nullptr);
    }

    bool TextureMapWorkspace::abandonWorkingPass() {
        const bool hadSession = m_session.active;
        m_session.clear();
        return hadSession;
    }

    bool TextureMapWorkspace::cancelEditSession() {
        return abandonWorkingPass();
    }

    bool TextureMapWorkspace::cellInsideSelectedSurfaceTarget(
        const TextureMapGridCell& cell) const {

        if (m_selectedSurfaceTarget < 0) return true;
        const StaticParticleAsset* asset = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;
        if (!asset || static_cast<std::size_t>(m_selectedSurfaceTarget) >=
            asset->surfaceTargets.size()) return false;
        const SurfaceTarget& target = asset->surfaceTargets[
            static_cast<std::size_t>(m_selectedSurfaceTarget)];
        if (target.faceIndex != m_session.selectedFace) return false;
        const float grid = static_cast<float>(m_target.pixelGridDivisions);
        return pointInPolygon(
            (static_cast<float>(cell.x) + 0.5f) / grid,
            (static_cast<float>(cell.y) + 0.5f) / grid,
            target.normalizedPolygon);
    }

    bool TextureMapWorkspace::applyCell(
        const TextureMapGridCell& cell,
        bool panelLine) {

        if (!m_session.active || !m_session.workingImage.valid() ||
            cell.x < 0 || cell.y < 0 ||
            cell.x >= static_cast<int>(m_target.pixelGridDivisions) ||
            cell.y >= static_cast<int>(m_target.pixelGridDivisions) ||
            !cellInsideSelectedSurfaceTarget(cell)) return false;

        const int divisions = static_cast<int>(m_target.pixelGridDivisions);
        const int column = static_cast<int>(m_session.selectedFace % 3u);
        const int row = static_cast<int>(m_session.selectedFace / 3u);
        const int halfThickness = panelLine ? (m_session.lineThickness - 1) / 2 : 0;
        const int extraThickness = panelLine ? m_session.lineThickness / 2 : 0;
        const std::array<std::uint8_t, 4> color = panelLine
            ? lineColorPresets()[m_session.lineColorPreset].rgba
            : m_session.paintColor;
        bool changed = false;

        for (int oy = -halfThickness; oy <= extraThickness; oy++) {
            for (int ox = -halfThickness; ox <= extraThickness; ox++) {
                const TextureMapGridCell expanded{ cell.x + ox, cell.y + oy };
                if (expanded.x < 0 || expanded.y < 0 ||
                    expanded.x >= divisions || expanded.y >= divisions ||
                    !cellInsideSelectedSurfaceTarget(expanded)) continue;

                const float localX0 = static_cast<float>(expanded.x) /
                    static_cast<float>(divisions);
                const float localX1 = static_cast<float>(expanded.x + 1) /
                    static_cast<float>(divisions);
                const float localY0 = static_cast<float>(expanded.y) /
                    static_cast<float>(divisions);
                const float localY1 = static_cast<float>(expanded.y + 1) /
                    static_cast<float>(divisions);
                const int pixelX0 = static_cast<int>(std::floor(
                    (static_cast<float>(column) + localX0) / 3.0f *
                    static_cast<float>(m_session.workingImage.width)));
                const int pixelX1 = static_cast<int>(std::ceil(
                    (static_cast<float>(column) + localX1) / 3.0f *
                    static_cast<float>(m_session.workingImage.width))) - 1;
                const int pixelY0 = static_cast<int>(std::floor(
                    (static_cast<float>(row) + localY0) / 2.0f *
                    static_cast<float>(m_session.workingImage.height)));
                const int pixelY1 = static_cast<int>(std::ceil(
                    (static_cast<float>(row) + localY1) / 2.0f *
                    static_cast<float>(m_session.workingImage.height))) - 1;

                for (int py = pixelY0; py <= pixelY1; py++) {
                    for (int px = pixelX0; px <= pixelX1; px++) {
                        if (px < 0 || py < 0 ||
                            px >= static_cast<int>(m_session.workingImage.width) ||
                            py >= static_cast<int>(m_session.workingImage.height)) continue;
                        const std::size_t offset =
                            (static_cast<std::size_t>(py) * m_session.workingImage.width +
                                static_cast<std::size_t>(px)) * 4u;
                        for (std::size_t component = 0; component < 4u; component++) {
                            if (m_session.workingImage.pixels[offset + component] != color[component]) {
                                m_session.workingImage.pixels[offset + component] = color[component];
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        if (changed) {
            m_session.dirty = true;
            m_session.revision++;
        }
        return changed;
    }

    bool TextureMapWorkspace::rasterizeStroke(
        const TextureMapGridCell& from,
        const TextureMapGridCell& to,
        bool panelLine) {

        int x0 = from.x;
        int y0 = from.y;
        const int x1 = to.x;
        const int y1 = to.y;
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        bool changed = false;

        for (;;) {
            changed = applyCell({ x0, y0 }, panelLine) || changed;
            if (x0 == x1 && y0 == y1) break;
            const int twiceError = error * 2;
            if (twiceError >= dy) { error += dy; x0 += sx; }
            if (twiceError <= dx) { error += dx; y0 += sy; }
        }
        return changed;
    }

    bool TextureMapWorkspace::setCursorCell(int x, int y) {
        const int divisions = static_cast<int>(m_target.pixelGridDivisions);
        TextureMapGridCell next;
        if (x >= 0 && y >= 0 && x < divisions && y < divisions)
            next = { x, y };
        if (next == m_session.cursorCell) return false;
        m_session.cursorCell = next;
        return true;
    }

    bool TextureMapWorkspace::beginAuthoringStroke(int x, int y) {
        if (m_subLayer != TextureMapSubLayer::PixelEditor ||
            m_session.viewMode != TextureMapViewMode::Edit) return false;
        if (m_authoringMode == TextureMapAuthoringMode::Contour)
            return addContourPoint(x, y);
        const TextureMapGridCell cell{ x, y };
        if (!cell.valid()) return false;
        m_session.strokeActive = true;
        m_session.previousStrokeCell = cell;
        return applyCell(cell,
            m_authoringMode == TextureMapAuthoringMode::PanelLines);
    }

    bool TextureMapWorkspace::continueAuthoringStroke(int x, int y) {
        if (!m_session.strokeActive ||
            m_authoringMode == TextureMapAuthoringMode::Contour) return false;
        const TextureMapGridCell cell{ x, y };
        if (!cell.valid() || cell == m_session.previousStrokeCell) return false;
        const bool changed = rasterizeStroke(
            m_session.previousStrokeCell,
            cell,
            m_authoringMode == TextureMapAuthoringMode::PanelLines);
        m_session.previousStrokeCell = cell;
        return changed;
    }

    bool TextureMapWorkspace::endAuthoringStroke() {
        const bool active = m_session.strokeActive;
        m_session.strokeActive = false;
        m_session.previousStrokeCell = TextureMapGridCell{};
        return active;
    }

    bool TextureMapWorkspace::addContourPoint(int x, int y) {
        if (!m_session.active || m_authoringMode != TextureMapAuthoringMode::Contour ||
            m_session.viewMode != TextureMapViewMode::Edit ||
            m_session.contourClosed) return false;
        const int divisions = static_cast<int>(m_target.pixelGridDivisions);
        const TextureMapGridCell cell{ x, y };
        if (x < 0 || y < 0 || x >= divisions || y >= divisions) return false;
        for (const TextureMapGridCell& existing : m_session.contourCells)
            if (existing == cell) return false;
        m_session.contourCells.push_back(cell);
        m_session.dirty = true;
        m_session.revision++;
        return true;
    }

    bool TextureMapWorkspace::contourHasSelfIntersection() const {
        const std::size_t count = m_session.contourCells.size();
        if (count < 4u) return false;
        for (std::size_t i = 0; i < count; i++) {
            const std::size_t iNext = (i + 1u) % count;
            for (std::size_t j = i + 1u; j < count; j++) {
                const std::size_t jNext = (j + 1u) % count;
                if (i == j || iNext == j || jNext == i) continue;
                if (strictSegmentsIntersect(
                    m_session.contourCells[i], m_session.contourCells[iNext],
                    m_session.contourCells[j], m_session.contourCells[jNext]))
                    return true;
            }
        }
        return false;
    }

    bool TextureMapWorkspace::closeContour(std::string* diagnostic) {
        if (m_session.contourCells.size() < 3u) {
            m_runtimeStatusMessage = "CONTOUR REQUIRES AT LEAST 3 UNIQUE POINTS.";
            if (diagnostic) *diagnostic = m_runtimeStatusMessage;
            return false;
        }
        if (contourHasSelfIntersection()) {
            m_runtimeStatusMessage = "CONTOUR SELF-INTERSECTION IS NOT ALLOWED.";
            if (diagnostic) *diagnostic = m_runtimeStatusMessage;
            return false;
        }
        m_session.contourClosed = true;
        m_session.dirty = true;
        m_session.revision++;
        return true;
    }

    bool TextureMapWorkspace::undoContourPoint() {
        if (m_session.contourCells.empty()) return false;
        m_session.contourClosed = false;
        m_session.contourCells.pop_back();
        m_session.dirty = true;
        m_session.revision++;
        return true;
    }

    bool TextureMapWorkspace::toggleRuntimeView() {
        if (m_subLayer != TextureMapSubLayer::PixelEditor) return false;
        endAuthoringStroke();
        m_session.viewMode = m_session.viewMode == TextureMapViewMode::Edit
            ? TextureMapViewMode::Preview
            : TextureMapViewMode::Edit;
        return true;
    }

	bool TextureMapWorkspace::adjustEditorZoom(int direction) {
		if (m_subLayer != TextureMapSubLayer::PixelEditor || direction == 0)
			return false;
		const float next = std::max(0.65f, std::min(1.65f,
			m_session.editorZoom + (direction < 0 ? -0.10f : 0.10f)));
		if (next == m_session.editorZoom) return false;
		m_session.editorZoom = next;
		return true;
	}

    bool TextureMapWorkspace::canExitLayer3(std::string* diagnostic) const {
        if (!m_session.dirty) return true;
        if (diagnostic)
            *diagnostic = "WORKING EDIT IS DIRTY. COMMIT OR RETURN TO AUTHORING CYCLE SETUP.";
        return false;
    }

    TextureResource* TextureMapWorkspace::ensureEditableTexture(
        StaticParticleAsset& asset,
        TextureMapChannel channel,
        const ImageRGBA8& image) const {

        if (m_target.materialIndex >= asset.materials.size() || !image.valid())
            return nullptr;
        MaterialSlot& material = asset.materials[m_target.materialIndex];
        std::string* materialTextureId = channel == TextureMapChannel::EmissiveColor
            ? &material.emissiveTextureId
            : &material.baseColorTextureId;
        TextureResource* texture = asset.findTexture(*materialTextureId);
        if (!texture) {
            std::string stem = asset.name;
            for (char& c : stem) {
                const unsigned char uc = static_cast<unsigned char>(c);
                if (!std::isalnum(uc) && c != '_' && c != '-') c = '_';
            }
            const bool emissive = channel == TextureMapChannel::EmissiveColor;
            TextureResource created;
            created.id = "tex_" + stem + (emissive ? "_emissive" : "_basecolor");
            created.relativePath = "textures/" + stem +
                (emissive ? "_emissive.png" : "_basecolor.png");
            created.type = TextureType::Texture2D;
            created.usage = emissive ? TextureUsage::Emissive : TextureUsage::BaseColor;
            created.colorSpace = TextureColorSpace::SRGB;
            asset.textures.push_back(std::move(created));
            *materialTextureId = asset.textures.back().id;
            texture = &asset.textures.back();
        }
        texture->width = image.width;
        texture->height = image.height;
        texture->channels = 4u;
        texture->pixels = image.pixels;
        texture->loaded = true;
        texture->valid = true;
        texture->renderingDeferred = false;
        texture->sourcePath.clear();
        return texture;
    }

    bool TextureMapWorkspace::commitWorkingEdit(
        const std::string& newSurfaceTargetName,
        std::string* diagnostic) {

        StaticParticleAsset* asset = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;
        if (!asset || !m_session.active || !m_session.dirty) {
            if (diagnostic) *diagnostic = "NO DIRTY WORKING EDIT IS AVAILABLE.";
            return false;
        }

        if (m_authoringMode == TextureMapAuthoringMode::Contour) {
            if (!m_session.contourClosed || contourHasSelfIntersection()) {
                if (diagnostic) *diagnostic = "WORKING CONTOUR IS NOT CLOSED AND VALID.";
                return false;
            }
            SurfaceTarget target;
            target.faceIndex = m_session.selectedFace;
            const float divisions = static_cast<float>(m_target.pixelGridDivisions);
            for (const TextureMapGridCell& cell : m_session.contourCells) {
                target.normalizedPolygon.push_back({
                    (static_cast<float>(cell.x) + 0.5f) / divisions,
                    (static_cast<float>(cell.y) + 0.5f) / divisions
                });
            }

            if (m_contourAction == TextureMapContourAction::New) {
                if (!validAuthoringName(newSurfaceTargetName)) {
                    if (diagnostic)
                        *diagnostic = "SURFACE TARGET NAME MAY USE LETTERS, NUMBERS, '_' OR '-'.";
                    return false;
                }
                const std::string key = lowerAscii(newSurfaceTargetName);
                for (const SurfaceTarget& existing : asset->surfaceTargets) {
                    if (lowerAscii(existing.name) == key) {
                        if (diagnostic) *diagnostic = "SURFACE TARGET NAME ALREADY EXISTS.";
                        return false;
                    }
                }
                target.name = newSurfaceTargetName;
                asset->surfaceTargets.push_back(std::move(target));
                m_selectedContourTarget =
                    static_cast<int>(asset->surfaceTargets.size()) - 1;
            }
            else {
                if (m_session.selectedContourTarget < 0 ||
                    static_cast<std::size_t>(m_session.selectedContourTarget) >=
                    asset->surfaceTargets.size()) {
                    if (diagnostic) *diagnostic = "EDIT CONTOUR TARGET IS UNAVAILABLE.";
                    return false;
                }
                target.name = asset->surfaceTargets[
                    static_cast<std::size_t>(m_session.selectedContourTarget)].name;
                asset->surfaceTargets[
                    static_cast<std::size_t>(m_session.selectedContourTarget)] =
                    std::move(target);
            }
        }
        else {
            const TextureMapChannel commitChannel =
                m_authoringMode == TextureMapAuthoringMode::PanelLines
                ? TextureMapChannel::BaseColor
                : m_selectedChannel;
            TextureResource* texture = ensureEditableTexture(
                *asset, commitChannel, m_session.workingImage);
            if (!texture) {
                if (diagnostic) *diagnostic = "WORKING TEXTURE COULD NOT BE COMMITTED.";
                return false;
            }
            m_session.textureId = texture->id;
            if (commitChannel == TextureMapChannel::EmissiveColor &&
                m_target.materialIndex < asset->materials.size()) {
                MaterialSlot& material = asset->materials[m_target.materialIndex];
                material.emissiveIntensity = m_session.emissiveIntensity;
                material.emissiveFactor[0] = 1.0f;
                material.emissiveFactor[1] = 1.0f;
                material.emissiveFactor[2] = 1.0f;
            }
        }

        asset->assetRevision++;
        m_session.dirty = false;
        m_session.originalImage = m_session.workingImage;
		m_emissiveIntensity = m_session.emissiveIntensity;
        m_session.revision++;
        m_runtimeStatusMessage = "WORKING EDIT COMMITTED TO IN-MEMORY TARGET.";
        return true;
    }

    bool TextureMapWorkspace::completeSurfaceTargetName(
        const std::string& name,
        std::string* diagnostic) {

        return commitWorkingEdit(name, diagnostic);
    }

    bool TextureMapWorkspace::validateNewSurfaceTargetName(
        const std::string& name,
        std::string* diagnostic) const {

        if (!validAuthoringName(name)) {
            if (diagnostic)
                *diagnostic = "SURFACE TARGET NAME MAY USE LETTERS, NUMBERS, '_' OR '-'.";
            return false;
        }
        const StaticParticleAsset* asset = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;
        if (!asset) {
            if (diagnostic) *diagnostic = "CANONICAL TARGET IS UNAVAILABLE.";
            return false;
        }
        const std::string key = lowerAscii(name);
        for (const SurfaceTarget& target : asset->surfaceTargets) {
            if (lowerAscii(target.name) == key) {
                if (diagnostic) *diagnostic = "SURFACE TARGET NAME ALREADY EXISTS.";
                return false;
            }
        }
        return true;
    }

    bool TextureMapWorkspace::applyEditSessionToAsset() {
        return commitWorkingEdit(std::string{}, nullptr);
    }

    StaticParticleAsset TextureMapWorkspace::buildPreviewAsset() const {
        const StaticParticleAsset* canonical = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;
        if (!canonical) return StaticParticleAsset{};
        StaticParticleAsset preview = *canonical;
        if (!m_session.active || !previewUsesWorkingState() ||
            m_authoringMode == TextureMapAuthoringMode::Contour ||
            !m_session.workingImage.valid() ||
            m_target.materialIndex >= preview.materials.size()) return preview;

        const TextureMapChannel channel =
            m_authoringMode == TextureMapAuthoringMode::PanelLines
            ? TextureMapChannel::BaseColor
            : m_selectedChannel;
        MaterialSlot& material = preview.materials[m_target.materialIndex];
        std::string* id = channel == TextureMapChannel::EmissiveColor
            ? &material.emissiveTextureId
            : &material.baseColorTextureId;
        TextureResource* texture = preview.findTexture(*id);
        if (!texture) {
            TextureResource created;
            created.id = channel == TextureMapChannel::EmissiveColor
                ? "__tm_working_emissive"
                : "__tm_working_basecolor";
            created.relativePath = channel == TextureMapChannel::EmissiveColor
                ? "textures/working_emissive.png"
                : "textures/working_basecolor.png";
            created.usage = channel == TextureMapChannel::EmissiveColor
                ? TextureUsage::Emissive
                : TextureUsage::BaseColor;
            created.colorSpace = TextureColorSpace::SRGB;
            preview.textures.push_back(std::move(created));
            *id = preview.textures.back().id;
            texture = &preview.textures.back();
        }
        texture->width = m_session.workingImage.width;
        texture->height = m_session.workingImage.height;
        texture->channels = 4u;
        texture->pixels = m_session.workingImage.pixels;
        texture->loaded = true;
        texture->valid = true;
        texture->sourcePath.clear();
        if (channel == TextureMapChannel::EmissiveColor) {
            material.emissiveIntensity = m_session.emissiveIntensity;
            material.emissiveFactor[0] = 1.0f;
            material.emissiveFactor[1] = 1.0f;
            material.emissiveFactor[2] = 1.0f;
        }
        return preview;
    }

    bool TextureMapWorkspace::buildSaveAsSnapshot(
        const std::string& assetName,
        StaticParticleAsset& output,
        std::string* diagnostic,
        const std::string& uncommittedSurfaceTargetName) const {

        if (!validAuthoringName(assetName)) {
            if (diagnostic)
                *diagnostic = "STATIC PARTICLE NAME MAY USE LETTERS, NUMBERS, '_' OR '-'.";
            return false;
        }
        const StaticParticleAsset* canonical = m_repository
            ? m_repository->findStaticParticle(m_target.assetId)
            : nullptr;
        if (!canonical || canonical->mesh.empty()) {
            if (diagnostic) *diagnostic = "CANONICAL TARGET IS UNAVAILABLE.";
            return false;
        }
        output = *canonical;
        if (m_session.dirty &&
            m_authoringMode != TextureMapAuthoringMode::Contour) {
            const TextureMapChannel channel =
                m_authoringMode == TextureMapAuthoringMode::PanelLines
                ? TextureMapChannel::BaseColor
                : m_selectedChannel;
            TextureResource* texture = ensureEditableTexture(
                output, channel, m_session.workingImage);
            if (!texture) {
                if (diagnostic) *diagnostic =
                    "WORKING TEXTURE COULD NOT BE INCLUDED IN SAVE AS.";
                return false;
            }
            if (channel == TextureMapChannel::EmissiveColor &&
                m_target.materialIndex < output.materials.size()) {
                MaterialSlot& material = output.materials[m_target.materialIndex];
                material.emissiveIntensity = m_session.emissiveIntensity;
                material.emissiveFactor[0] = 1.0f;
                material.emissiveFactor[1] = 1.0f;
                material.emissiveFactor[2] = 1.0f;
            }
        }
        if (m_session.dirty && m_authoringMode == TextureMapAuthoringMode::Contour) {
            if (!m_session.contourClosed || contourHasSelfIntersection()) {
                if (diagnostic) *diagnostic = "WORKING CONTOUR IS NOT CLOSED AND VALID.";
                return false;
            }
            SurfaceTarget target;
            target.faceIndex = m_session.selectedFace;
            const float divisions = static_cast<float>(m_target.pixelGridDivisions);
            for (const TextureMapGridCell& cell : m_session.contourCells) {
                target.normalizedPolygon.push_back({
                    (static_cast<float>(cell.x) + 0.5f) / divisions,
                    (static_cast<float>(cell.y) + 0.5f) / divisions
                });
            }
            if (m_contourAction == TextureMapContourAction::New) {
                if (!validateNewSurfaceTargetName(
                    uncommittedSurfaceTargetName, diagnostic)) return false;
                target.name = uncommittedSurfaceTargetName;
                output.surfaceTargets.push_back(std::move(target));
            }
            else if (m_session.selectedContourTarget >= 0 &&
                static_cast<std::size_t>(m_session.selectedContourTarget) <
                output.surfaceTargets.size()) {
                target.name = output.surfaceTargets[
                    static_cast<std::size_t>(m_session.selectedContourTarget)].name;
                output.surfaceTargets[
                    static_cast<std::size_t>(m_session.selectedContourTarget)] =
                    std::move(target);
            }
            else {
                if (diagnostic) *diagnostic = "EDIT CONTOUR TARGET IS UNAVAILABLE.";
                return false;
            }
        }
        output.id = INVALID_ASSET_ID;
        output.name = assetName;
        output.assetRevision++;
        return true;
    }

    bool TextureMapWorkspace::adoptSavedTarget(AssetId assetId) {
        if (!activateLoadedTarget(assetId)) return false;
        return beginAuthoringRuntime();
    }

    void TextureMapWorkspace::reset() {
        m_target = TextureMapTargetContext{};
        m_session.clear();
        m_focus = TextureMapFocus::TextureCanvas;
        m_subLayer = TextureMapSubLayer::CycleSetup;
        m_authoringMode = TextureMapAuthoringMode::Coloring;
        m_previewSource = TextureMapPreviewSource::Working;
        m_selectedChannel = TextureMapChannel::BaseColor;
        m_contourAction = TextureMapContourAction::New;
        m_selectedSurfaceTarget = -1;
        m_selectedContourTarget = -1;
        m_nestedFocus = false;
        m_baseMaterialSourceSelected = false;
        m_emissiveIntensity = 1.0f;
        m_runtimeStatusMessage.clear();
    }

    bool TextureMapWorkspace::selectBaseMaterial(int direction) {
        if (direction == 0 || m_baseMaterialCatalog.empty()) return false;
        const bool newlySelected = !m_baseMaterialSourceSelected;
        m_baseMaterialSourceSelected = true;
        const int count = static_cast<int>(m_baseMaterialCatalog.size());
        const int current = static_cast<int>(m_selectedBaseMaterialIndex);
        const int next = wrappedIndex(current + (direction < 0 ? -1 : 1), count);
        if (next == current) return newlySelected;
        m_selectedBaseMaterialIndex = static_cast<std::size_t>(next);
        return true;
    }

    const BaseMaterialCatalogEntry* TextureMapWorkspace::selectedBaseMaterial() const {
        if (m_selectedBaseMaterialIndex >= m_baseMaterialCatalog.size()) return nullptr;
        return &m_baseMaterialCatalog[m_selectedBaseMaterialIndex];
    }

    void TextureMapWorkspace::replaceBaseMaterialCatalog(
        std::vector<BaseMaterialCatalogEntry> catalog) {

        m_baseMaterialCatalog = std::move(catalog);
        if (m_selectedBaseMaterialIndex >= m_baseMaterialCatalog.size())
            m_selectedBaseMaterialIndex = 0;
    }

    void TextureMapWorkspace::toggleFocus() {
        m_focus = m_focus == TextureMapFocus::TextureCanvas
            ? TextureMapFocus::MeshInspection
            : TextureMapFocus::TextureCanvas;
    }

    bool TextureMapWorkspace::adjustPixelGridDivisions(
        int direction) {

        if (!m_target.loaded ||
            direction == 0) {

            return false;
        }

        std::size_t currentIndex = 0;

        for (std::size_t i = 0;
            i < kPixelGridDivisionPresets.size();
            i++) {

            if (kPixelGridDivisionPresets[i] ==
                m_target.pixelGridDivisions) {

                currentIndex = i;
                break;
            }
        }

        const int count =
            static_cast<int>(
                kPixelGridDivisionPresets.size()
            );

        const int step =
            direction < 0
            ? -1
            : +1;

        const std::size_t nextIndex =
            static_cast<std::size_t>(
                (
                    static_cast<int>(currentIndex) +
                    step +
                    count
                ) % count
            );

        const std::uint32_t nextDivisions =
            kPixelGridDivisionPresets[nextIndex];

        if (nextDivisions ==
            m_target.pixelGridDivisions) {

            return false;
        }

        m_target.pixelGridDivisions =
            nextDivisions;

        return true;
    }
} // namespace vitru
