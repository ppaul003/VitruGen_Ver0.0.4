#ifndef VITRUGEN_TEXTURE_MAP_WORKSPACE_H
#define VITRUGEN_TEXTURE_MAP_WORKSPACE_H

#include "PngImage.h"
#include "ProjectAssetRepository.h"
#include "StaticParticleAssetIO.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vitru {

    enum class TextureMapFocus {
        TextureCanvas = 0,
        MeshInspection
    };

    enum class TextureMapSubLayer {
        BaseMaterial = 0,
        MaterialTint,
        PanelLines,
        ApplySave
    };

    enum class TextureTargetReadiness {
        NoTarget = 0,
        Ready,
        NeedsUv,
        NeedsTexture,
        Invalid
    };

    struct BaseMaterialCatalogEntry {
        std::string id;
        std::string displayName;

        std::filesystem::path rootPath;
        std::filesystem::path baseColorPath;

        std::uint32_t width = 0;
        std::uint32_t height = 0;

        bool valid = false;
        std::string status;
    };

    struct TextureDirtyRegion {
        bool valid = false;

        std::uint32_t minimumX = 0;
        std::uint32_t minimumY = 0;
        std::uint32_t maximumX = 0;
        std::uint32_t maximumY = 0;

        void clear() {
            valid = false;
            minimumX = minimumY = 0;
            maximumX = maximumY = 0;
        }
    };

    struct TextureMapTargetContext {
        AssetId assetId = INVALID_ASSET_ID;

        std::size_t submeshIndex = 0;
        std::size_t materialIndex = 0;

        TextureUsage channel = TextureUsage::BaseColor;
        std::string textureId;

        float previewParticleRadius = 0.125f;
        std::uint32_t pixelGridDivisions = 64;

        TextureTargetReadiness readiness =
            TextureTargetReadiness::NoTarget;

        bool loaded = false;
    };

    struct TextureMapEditSession {
        bool active = false;
        bool dirty = false;

        AssetId assetId = INVALID_ASSET_ID;

        std::size_t submeshIndex = 0;
        std::size_t materialIndex = 0;

        std::string textureId;

        // Snapshot used by Discard Changes.
        ImageRGBA8 originalImage;

        // Source material selected in Sub-Layer 0.
        ImageRGBA8 baseMaterialImage;

        // Transparent authoring layer used by panel lines.
        ImageRGBA8 panelLineOverlay;

        // Base material + tint + panel-line overlay.
        ImageRGBA8 compositeImage;

        std::array<float, 4> tint{
            1.0f,
            1.0f,
            1.0f,
            1.0f
        };

        TextureDirtyRegion dirtyRegion;
        std::uint64_t revision = 0;

        void clear();
    };

    class TextureMapWorkspace {
    public:
        bool initialize(
            ProjectAssetRepository* repository,
            const std::filesystem::path& outputStaticParticlesRoot,
            const std::filesystem::path& baseMaterialsRoot
        );

        void reset();

        bool refreshOutputCatalog();
        bool refreshBaseMaterialCatalog();

        bool selectOutputAsset(int direction);
        bool selectBaseMaterial(int direction);

        const StaticAssetCatalogEntry* selectedOutputAsset() const;
        const BaseMaterialCatalogEntry* selectedBaseMaterial() const;

        bool activateLoadedTarget(AssetId assetId);
        bool refreshTargetContext();

        bool beginEditSession();
        bool cancelEditSession();
        bool applyEditSessionToAsset();

        ProjectAssetRepository* repository() {
            return m_repository;
        }

        const TextureMapTargetContext& target() const {
            return m_target;
        }

        TextureMapTargetContext& target() {
            return m_target;
        }

        const TextureMapEditSession& session() const {
            return m_session;
        }

        TextureMapEditSession& session() {
            return m_session;
        }

        TextureMapFocus focus() const {
            return m_focus;
        }

        void toggleFocus();

        TextureMapSubLayer subLayer() const {
            return m_subLayer;
        }

        void setSubLayer(TextureMapSubLayer value) {
            m_subLayer = value;
        }

        bool initialized() const {
            return m_initialized;
        }

    private:
        ProjectAssetRepository* m_repository = nullptr;

        std::filesystem::path m_outputStaticParticlesRoot;
        std::filesystem::path m_baseMaterialsRoot;

        std::vector<StaticAssetCatalogEntry> m_outputCatalog;
        std::size_t m_selectedOutputIndex = 0;

        std::vector<BaseMaterialCatalogEntry> m_baseMaterialCatalog;
        std::size_t m_selectedBaseMaterialIndex = 0;

        TextureMapTargetContext m_target;
        TextureMapEditSession m_session;

        TextureMapFocus m_focus =
            TextureMapFocus::TextureCanvas;

        TextureMapSubLayer m_subLayer =
            TextureMapSubLayer::BaseMaterial;

        bool m_initialized = false;
    };

} // namespace vitru

#endif
