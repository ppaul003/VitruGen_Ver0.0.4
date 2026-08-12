#include "TextureMapWorkspace.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>
#include <utility>

namespace vitru {

    namespace {

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

    } // namespace


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
            TextureMapSubLayer::BaseMaterial;

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

    bool TextureMapWorkspace::adjustPreviewParticleRadius(int direction, float step) {

        if (!m_target.loaded ||
            direction == 0 ||
            step <= 0.0f) {

            return false;
        }

        const float current =
            m_target.previewParticleRadius;

        float next =
            current +
            (direction < 0
                ? -step
                : +step);

        // Prevent a zero/negative preview radius.
        //
        // We intentionally do not reuse the SINGLE_PARTICLE maximum
        // clamp here because TEXTURE_MAP_2D defaults to 0.125.
        next =
            std::max(
                step,
                next
            );

        if (next == current) {

            return false;
        }

        m_target.previewParticleRadius = next;

        return true;
    }
} // namespace vitru