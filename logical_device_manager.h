#pragma once

#include <unordered_map>
#include "pool_memory_allocator.h"
#include "logical_device.h"
#include "protocols/overlay_proto.h"
#include "mesh_stream_builder.h"
#include "to_fix.h"


class OverlayPacketBuilder
{
public:
    static PoolMemoryAllocator<LOG_PACKET_POOL_ALLOC_PART_SIZE, LOG_PACKET_POOL_ALLOC_COUNT>* log_ovl_packet_alloc;

    MeshStreamBuilder mesh;
    OverlayProto::OverlayPacket* packet;

    OverlayPacketBuilder(MeshProto::far_addr_t dst_phy, uint size, OverlayProto::OverlayProtoType ovl_type,
                         void** user_write_addr_p);

    void send();

    ~OverlayPacketBuilder();
};


class LogicalPacketPtr
{
    friend LogicalDeviceManager;
public:
    LogicalPacketPtr(LogicalProto::LogicalPacket* ptr_, OverlayPacketBuilder* ovl_, uint size_)
    : _ptr(ptr_), ovl(ovl_), size(size_) {}
    LogicalPacketPtr() = default;

    inline LogicalProto::LogicalPacket* ptr() {
        return _ptr;
    }

protected:
    LogicalProto::LogicalPacket* _ptr;
    OverlayPacketBuilder* ovl;
    uint size;
};


class LogicalDeviceManager
{
public:
    std::unordered_map<ushort, LogicalDevice*> devices;

    void add_device(LogicalDevice* device);

    void remove_device(LogicalDevice* device);

    LogicalDevice* lookup_device(ushort port);

    void dispatch_packet(LogicalProto::LogicalPacket* packet, ushort size, MeshProto::far_addr_t src_phy);

    void handle_packet(LogicalDevice* device, LogicalProto::LogicalPacket* packet, ushort size,
                       MeshProto::far_addr_t src_phy);

    LogicalPacketPtr alloc_logical_packet_ptr(LogicalAddress dst_addr, ushort src_port, uint size,
                                              OverlayProto::OverlayProtoType ovl_type,
                                              LogicalProto::LogicalPacketType log_type);

    LogicalPacketPtr alloc_raw_logical_ptr(MeshProto::far_addr_t dst_phy, uint log_size,
                                           OverlayProto::OverlayProtoType ovl_type);

    void finish_ptr(LogicalPacketPtr ptr);
};
