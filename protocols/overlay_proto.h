#pragma once

#pragma pack(push, 1)
namespace OverlayProto
{
    enum class OverlayProtoType : ubyte
    {
        UNKNOWN = 0,
        RELIABLE = 1,
        UNRELIABLE = 2
    };
    // todo add support for reliable packets
    // todo add reliable streams for large (uint32-sized) amount data

    struct ReliablePacket
    {
        ushort sequence_num;
        ubyte data[0];
    };

    struct UnreliablePacket
    {
        // no additional fields
        ubyte data[0];
    };

#define OVL_PACKET_SIZE(field_name) (uintptr_t) (&((OverlayProto::OverlayPacket*) nullptr)->field_name + 1)
    struct OverlayPacket
    {
        OverlayProtoType type;
        union {
            ReliablePacket reliable;
            UnreliablePacket unreliable;
        };

        static ushort get_packet_size(OverlayProtoType type_) {
            switch (type_) {
                case OverlayProtoType::UNKNOWN: return 0;
                case OverlayProtoType::RELIABLE: return OVL_PACKET_SIZE(reliable);
                case OverlayProtoType::UNRELIABLE: return OVL_PACKET_SIZE(unreliable);
            }
            return 0;
        }

        OverlayPacket() = delete;
    };
}
#pragma pack(pop)
