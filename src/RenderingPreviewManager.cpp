#include <d3d12.h>
#include "RenderingPreviewManager.h"
#include "RenderingManager.h"
#include "StateTracking.h"
#include "resource.h"
#include "Util.h"

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

RenderingPreviewManager::RenderingPreviewManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, RenderingShaderManager& shManager) : uiData(data), resourceManager(rManager), shaderManager(shManager)
{
}

RenderingPreviewManager::~RenderingPreviewManager()
{
}

void RenderingPreviewManager::UpdatePreview(command_list* cmd_list, uint64_t callLocation, uint64_t invocation)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    // Remove call location from queue
    commandListData.commandQueue &= ~(invocation << (callLocation * MATCH_DELIMITER));

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0) {
        return;
    }

    RuntimeDataContainer& runtimeData = deviceData.current_runtime->get_private_data<RuntimeDataContainer>();

    ToggleGroup& group = uiData.GetToggleGroups().at(uiData.GetToggleGroupIdShaderEditing());

    // Set views during draw call since we can be sure the correct ones are bound at that point
    if (!callLocation && deviceData.huntPreview.target == 0)
    {
        ResourceViewData active_target;

        if (invocation & MATCH_PREVIEW_PS)
        {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 0, invocation & MATCH_PREVIEW_PS);
        }
        else if (invocation & MATCH_PREVIEW_VS)
        {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 1, invocation & MATCH_PREVIEW_VS);
        }
        else if (invocation & MATCH_PREVIEW_CS)
        {
            active_target = RenderingManager::GetCurrentResourceView(cmd_list, deviceData, &group, commandListData, 2, invocation & MATCH_PREVIEW_CS);
        }

        if (active_target.resource != 0)
        {
            resource_desc desc = device->get_resource_desc(active_target.resource);
            //cmd_list->get_private_data<state_tracking>().start_resource_barrier_tracking(res, resource_usage::render_target);

            deviceData.huntPreview.target = active_target.resource;
            deviceData.huntPreview.target_desc = desc;
            deviceData.huntPreview.format = desc.texture.format;
            deviceData.huntPreview.view_format = active_target.format;
            deviceData.huntPreview.width = desc.texture.width;
            deviceData.huntPreview.height = desc.texture.height;
        }
        else
        {
            return;
        }
    }

    if (deviceData.huntPreview.target == 0 || !(!callLocation && !deviceData.huntPreview.target_invocation_location || callLocation & deviceData.huntPreview.target_invocation_location))
    {
        return;
    }

    if (group.getId() == uiData.GetToggleGroupIdShaderEditing() && !deviceData.huntPreview.matched)
    {
        resource rs = deviceData.huntPreview.target;
        //resource_usage rs_usage = cmd_list->get_private_data<state_tracking>().stop_resource_barrier_tracking(rs);
        //if (rs_usage == resource_usage::undefined)
        //{
        //    return;
        //}

        if (!resourceManager.IsCompatibleWithPreviewFormat(device, rs, deviceData.huntPreview.view_format))
        {
            deviceData.huntPreview.recreate_preview = true;
        }
        else
        {
            bool supportsAlphaClear = device->get_api() == device_api::d3d9 || device->get_api() == device_api::d3d10 || device->get_api() == device_api::d3d11 || device->get_api() == device_api::d3d12;

            resource previewResPing = resource{ 0 };
            resource previewResPong = resource{ 0 };
            resource_view preview_pong_rtv = resource_view{ 0 };
            resource_view preview_ping_srv = resource_view{ 0 };

            resourceManager.SetPingPreviewHandles(device, &previewResPing, nullptr, &preview_ping_srv);
            resourceManager.SetPongPreviewHandles(device, &previewResPong, &preview_pong_rtv, nullptr);

            if (previewResPong != 0 && (!group.getClearPreviewAlpha() || !supportsAlphaClear))
            {
                //resource resources[2] = { rs, previewResPong };
                //resource_usage from[2] = { rs_usage, resource_usage::shader_resource };
                //resource_usage to[2] = { resource_usage::copy_source, resource_usage::copy_dest };
                
                //cmd_list->barrier(2, resources, from, to);
                cmd_list->copy_resource(rs, previewResPong);
                //cmd_list->barrier(2, resources, to, from);
            }
            else if (previewResPing != 0 && previewResPong != 0 && preview_ping_srv != 0 && preview_pong_rtv != 0 && supportsAlphaClear)
            {
                //resource resources[2] = { rs, previewResPing };
                //resource_usage from[2] = { rs_usage, resource_usage::shader_resource };
                //resource_usage to[2] = { resource_usage::copy_source, resource_usage::copy_dest };
                
                //cmd_list->barrier(2, resources, from, to);
                cmd_list->copy_resource(rs, previewResPing);
                //cmd_list->barrier(2, resources, to, from);

                //cmd_list->barrier(previewResPong, resource_usage::shader_resource, resource_usage::render_target);
                shaderManager.CopyResource(cmd_list, preview_ping_srv, preview_pong_rtv, deviceData.huntPreview.width, deviceData.huntPreview.height);
                //cmd_list->barrier(previewResPong, resource_usage::render_target, resource_usage::shader_resource);
            }

            if (group.getFlipBuffer() && runtimeData.specialEffects[REST_FLIP].technique != 0)
            {
                deviceData.current_runtime->render_technique(runtimeData.specialEffects[REST_FLIP].technique, cmd_list, preview_pong_rtv, preview_pong_rtv);
            }

            if (group.getToneMap() && runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique != 0)
            {
                deviceData.current_runtime->render_technique(runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique, cmd_list, preview_pong_rtv, preview_pong_rtv);
            }
        }

        deviceData.huntPreview.matched = true;
    }
}