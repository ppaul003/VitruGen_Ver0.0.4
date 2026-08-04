#include "TextureMapWorkspace.h"

#include <cstdio>
#include <system_error>

namespace vitru {

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

        m_target =
            TextureMapTargetContext{};

        m_session =
            TextureMapEditSession{};

        m_focus =
            TextureMapFocus::TextureCanvas;

        m_subLayer =
            TextureMapSubLayer::BaseMaterial;

        m_initialized = true;

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

} // namespace vitru