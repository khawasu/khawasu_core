#include "logical_device_manager.h"
#include "net_utils.h"

using namespace LogicalProto;
using namespace OverlayProto;

PoolMemoryAllocator<LOG_PACKET_POOL_ALLOC_PART_SIZE, LOG_PACKET_POOL_ALLOC_COUNT>*
        OverlayPacketBuilder::log_ovl_packet_alloc = nullptr;


// overlay builder
OverlayPacketBuilder::OverlayPacketBuilder(MeshProto::far_addr_t dst_phy, uint size, OverlayProtoType ovl_type,
                                           void** user_write_addr_p)
: mesh(*g_fresh_mesh, dst_phy, size + OverlayPacket::get_packet_size(ovl_type)),
  packet((OverlayPacket*) log_ovl_packet_alloc->alloc(size + OverlayPacket::get_packet_size(ovl_type)))
{
    net_store(packet->type, ovl_type);

    switch (ovl_type) {
        case OverlayProtoType::RELIABLE:   { *user_write_addr_p = packet->reliable.data;   break; }
        case OverlayProtoType::UNRELIABLE: { *user_write_addr_p = packet->unreliable.data; break; }
        default: { printf("OverlayPacketBuilder: unknown type\n"); break;}
    }
}

void OverlayPacketBuilder::send() {
    mesh.write((ubyte*) packet, mesh.stream_size);
}

OverlayPacketBuilder::~OverlayPacketBuilder() {
    log_ovl_packet_alloc->free(packet);
}


// logical device manager
LogicalDevice* LogicalDeviceManager::lookup_device(ushort port) {
    auto iter = devices.find(port);
    if (iter == devices.end())
        return nullptr;
    return iter->second;
}

LogicalPacketPtr LogicalDeviceManager::alloc_logical_packet_ptr(LogicalAddress dst_addr, ushort src_port, uint size,
                                                                OverlayProtoType ovl_type, LogicalPacketType log_type) {
    auto packet = alloc_raw_logical_ptr(dst_addr.phy, size + LogicalPacket::get_packet_size(log_type), ovl_type);

    net_store(packet.ptr()->type, log_type);
    net_store(packet.ptr()->src_addr, src_port);
    net_store(packet.ptr()->dst_addr, dst_addr.log);
    return packet;
}

void LogicalDeviceManager::dispatch_packet(LogicalPacket* packet, ushort size, MeshProto::far_addr_t src_phy) {
    if (LOG_PACKET_SIZE(dst_addr) > size)
        return;

    auto dst_addr = net_load(packet->dst_addr);
    if (dst_addr == BROADCAST_PORT) {
        for (auto [_, device] : devices)
            handle_packet(device, packet, size, src_phy);
    }
    else {
        auto device = lookup_device(dst_addr);
        if (device != nullptr)
            handle_packet(device, packet, size, src_phy);
    }
}

void LogicalDeviceManager::add_device(LogicalDevice* device) {
    devices[device->self_port] = device;
    device->post_init();
}

void LogicalDeviceManager::remove_device(LogicalDevice* device) {
    devices.erase(device->self_port);
}

void LogicalDeviceManager::handle_packet(LogicalDevice* device, LogicalPacket* packet, ushort size,
                                         MeshProto::far_addr_t src_phy) {
    if (!device->on_general_packet_accept(packet, size, src_phy))
        return;

    auto src_port = net_load(packet->src_addr);

    switch (packet->type) {
        case LogicalPacketType::HELLO_WORLD: {
            if (src_port == device->self_port && src_phy == g_fresh_mesh->self_addr)
                break; // skipping if got self packet
            device->send_hello_world(LogicalPacketType::HELLO_WORLD_RESPONSE, src_phy, src_port);
            device->on_device_discover(packet, size, src_phy);
            break;
        }
        case LogicalPacketType::HELLO_WORLD_RESPONSE: {
            device->on_device_discover(packet, size, src_phy);
            break;
        }
        case LogicalPacketType::FIELD_DICTIONARY_REQUEST: {
            device->send_field_dictionary({src_phy, src_port});
            break;
        }
        case LogicalPacketType::FIELD_DICTIONARY_RESPONSE: {
            // validating packet size
            if (LOG_PACKET_SIZE(field_dictionary_response) > size)
                break;

            auto curr_ptr = packet->field_dictionary_response.fields;
            auto end_ptr = (ubyte*) packet + size;
            auto field_count = net_load(packet->field_dictionary_response.field_count);
            for (int i = 0; i < field_count; ++i) {
                if ((ubyte*) curr_ptr >= end_ptr)
                    break;
                curr_ptr = (FieldDictionaryResponsePacket::ApiFieldLayout*) curr_ptr->string + net_load(curr_ptr->length);
            }
            if ((ubyte*) curr_ptr > end_ptr)
                break;

            device->on_device_field_dictionary(packet->field_dictionary_response.fields, field_count, src_phy);
            break;
        }

        case LogicalPacketType::GROUPS_LIST_REQUEST: { break; }
        case LogicalPacketType::GROUPS_LIST_RESPONSE: { break; }
        case LogicalPacketType::GROUPS_ADD: { break; }
        case LogicalPacketType::GROUPS_EDIT: { break; }
        case LogicalPacketType::GROUPS_REMOVE: { break; }
        case LogicalPacketType::GROUPS_FIND_USERS_REQUEST: { break; }
        case LogicalPacketType::GROUPS_FIND_USERS_RESPONSE: { break; }

        case LogicalPacketType::ACTION_RESPONSE: {
            if (LOG_PACKET_SIZE(action_response) > size)
                break;

            device->on_action_get_response(net_load(packet->action_response.action_id),
                                           packet->action_response.payload,
                                           size - LOG_PACKET_SIZE(action_response),
                                           {src_phy, src_port},
                                           net_load(packet->action_response.request_id));
            break;
        }
        case LogicalPacketType::ACTION_FETCH: {
            if (LOG_PACKET_SIZE(action_fetch) > size)
                break;

            device->on_action_get(net_load(packet->action_fetch.action_id),
                                  packet->action_fetch.payload,
                                  size - LOG_PACKET_SIZE(action_fetch),
                                  {src_phy, src_port},
                                  net_load(packet->action_fetch.request_id));
            break;
        }
        case LogicalPacketType::ACTION_EXECUTE: {
            if (LOG_PACKET_SIZE(action_execute) > size)
                break;

            auto action_id = net_load(packet->action_execute.action_id);
            auto status = device->on_action_set(action_id, packet->action_execute.payload,
                              size - LOG_PACKET_SIZE(action_execute), {src_phy, src_port});


            if (net_load(packet->action_execute.flags) & ActionExecuteFlags::REQUIRE_STATUS_RESPONSE) {
                auto log = alloc_logical_packet_ptr({src_phy, src_port}, device->self_port, 0,
                                                    OverlayProtoType::UNRELIABLE,
                                                    LogicalPacketType::ACTION_EXECUTE_RESULT);
                net_store(log.ptr()->action_execute_result.status, status);
                net_store(log.ptr()->action_execute_result.request_id, net_load(packet->action_execute.request_id));
                finish_ptr(log);
            }
            break;
        }
        case LogicalPacketType::SUBSCRIPTION_START: {
            if (LOG_PACKET_SIZE(subscription_start) > size)
                break;
            device->subscriptions.add_subscriber(&packet->subscription_start,
                                                 size - LOG_PACKET_SIZE(subscription_start),
                                                 {src_phy, src_port});
            break;
        }
        case LogicalPacketType::SUBSCRIPTION_DONE: {
            // not implemented currently
            break;
        }
        case LogicalPacketType::SUBSCRIPTION_CALLBACK: {
            if (LOG_PACKET_SIZE(subscription_callback) > size)
                break;
            device->on_subscription_data(packet->subscription_callback.payload,
                                         size - LOG_PACKET_SIZE(subscription_callback),
                                         {src_phy, src_port},
                                         net_load(packet->subscription_callback.id));
            break;
        }
        case LogicalPacketType::SUBSCRIPTION_STOP: {
            if (LOG_PACKET_SIZE(subscription_stop) > size)
                break;
            device->subscriptions.stop_subscription(&packet->subscription_stop, {src_phy, src_port});
            break;
        }
        default: break;
    }
}

void LogicalDeviceManager::finish_ptr(LogicalPacketPtr ptr) {
    auto raw = ptr.ptr();

    if (ptr.ovl) {
        ptr.ovl->send();
        if (net_load(raw->dst_addr) == BROADCAST_PORT)
            dispatch_packet(raw, ptr.size, g_fresh_mesh->self_addr);
        delete ptr.ovl;
    } else {
        dispatch_packet(raw, ptr.size, g_fresh_mesh->self_addr);
        OverlayPacketBuilder::log_ovl_packet_alloc->free(raw);
    }
}

LogicalPacketPtr LogicalDeviceManager::alloc_raw_logical_ptr(MeshProto::far_addr_t dst_phy, uint log_size,
                                                             OverlayProto::OverlayProtoType ovl_type) {
    LogicalPacket* packet;

    if (g_fresh_mesh->self_addr == dst_phy) {
        packet = (LogicalPacket*) OverlayPacketBuilder::log_ovl_packet_alloc->alloc(log_size);
        return {packet, nullptr, log_size};
    } else {
        auto ovl_ptr = new OverlayPacketBuilder(dst_phy, log_size, ovl_type, (void**) &packet);
        return {packet, ovl_ptr, log_size};
    }
}