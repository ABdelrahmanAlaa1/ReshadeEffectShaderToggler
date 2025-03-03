#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <unordered_set>

#include "GameHookT.h"
#include "ResourceShim.h"

using namespace sigmatch_literals;

static const sigmatch::signature ffxiv_texture_create = "48 89 5C 24 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? B8 00 21 00 00"_sig;
static const sigmatch::signature ffxiv_textures_create = "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 48"_sig;
static const sigmatch::signature ffxiv_textures_recreate =
  "40 55 57 41 55 48 8D 6C 24 ?? 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 ?? 4C 8B 2D ?? ?? ?? ??"_sig;

namespace Shim {
namespace Resources {
// Texture offsets in order of initialization
static const std::vector<uintptr_t> RT_OFFSET_LIST = {
    0x20, // Normals
    0x98, // Normals Decal
    0x370 // UI
    // ...
    // There are more, but these are the ones we care about
};

enum RT_OFFSET : uint32_t { RT_NORMALS = 0x20, RT_NORMALS_DECAL = 0x98, RT_UI = 0x370 };

class ResourceShimFFXIV final : public virtual ResourceShim {
  public:
    virtual bool Init() override final;
    virtual bool UnInit() override final;

    virtual bool OnCreateResource(reshade::api::device* device,
                                  reshade::api::resource_desc& desc,
                                  reshade::api::subresource_data* initial_data,
                                  reshade::api::resource_usage initial_state) override final;
    virtual void OnDestroyResource(reshade::api::device* device, reshade::api::resource res) override final;
    virtual void OnInitResource(reshade::api::device* device,
                                const reshade::api::resource_desc& desc,
                                const reshade::api::subresource_data* initData,
                                reshade::api::resource_usage usage,
                                reshade::api::resource handle) override final;
    virtual bool OnCreateResourceView(reshade::api::device* device,
                                      reshade::api::resource resource,
                                      reshade::api::resource_usage usage_type,
                                      reshade::api::resource_view_desc& desc) override final;

  private:
    static uintptr_t* ffxiv_texture_data;

    static uintptr_t ffxiv_recreation_struct;

    static uintptr_t ffxiv_creation_struct;
    static int32_t ffxiv_current_creation_index;
    static std::unordered_set<uintptr_t> ffxiv_created_resources;

    static sig_ffxiv_texture_create* org_ffxiv_texture_create;
    static sig_ffxiv_textures_create* org_ffxiv_textures_create;
    static sig_ffxiv_textures_recreate* org_ffxiv_textures_recreate;

    static void __fastcall detour_ffxiv_texture_create(uintptr_t*, uintptr_t*);
    static uintptr_t __fastcall detour_ffxiv_textures_create(uintptr_t);
    static uintptr_t __fastcall detour_ffxiv_textures_recreate(uintptr_t);
};
}
}
