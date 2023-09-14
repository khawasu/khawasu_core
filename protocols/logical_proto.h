#pragma once

#include "types.h"

// logical protocol is a basement of Khawasu: it defines what a logical device is, and how they interact with each other
//
// logical device - is a simple periphery device, e.g. button, temperature sensor, relay module, and so on
// multiple logical devices can be hosted on a single physical device (microcontroller)
// logical devices can also be virtual, meaning there's no periphery behind it, e.g. some controllers
// so, the whole smart home consists of a large number of logical devices
//
// for example, to make a power switch, you need 3 logical devices: button, relay, and controller, where controller
// will subscribe to button for events `button changed its state` and will send this state to the relay

// after the logical device created and initialized, it broadcasts the packet HELLO_WORLD,
// carrying out simple info about this device:
//  its type (button/relay/etc...), called a device class,
//  its string location (a lamp on working desk) and
//  special attributes that extend the device class or carry some special debugging info (more about it later)
// then, every logical device in the network responds with HELLO_RESPONSE packet, that describes the responder device's info
// this way, devices may know about each other if they need to (useful for controllers, admin panels)
// peripheral devices usually ignore the data
//
// device classes describe the way other network members can interact with this device, unifying the access to similar hardware
// for example, RELAY device class can change it's state by DATA_TRANSFER packet with one-byte payload:
//  0x00 means to set output to LOW, 0x01 - to HIGH, and 0xFF to switch state from current one
// also you can request relay state by DATA_REQUEST packet with no payload, and result will be returned as
// DATA_ANSWER packet with a single byte, describing LOW/HIGH state.
// you can subscribe to the relay module to know when it's state has changed,
// the format of this is described in RELAY device class as well
//
// there are 3 main packets to transfer any data: DATA_REQUEST, DATA_RESPONSE and DATA_TRANSFER
// DATA_REQUEST is used to request some data from device, and the response will be a DATA_RESPONSE packet
// DATA_TRANSFER is used to transfer connectionless data directly to device, e.g. to change some state
//
// subscription API:
// subscriptions are a dedicated API inside logical protocol, used to implement callbacks between devices
// the main three packets are: SUBSCRIPTION_START, SUBSCRIPTION_STOP, and SUBSCRIPTION_CALLBACK
// each subscription has a few parameters:
// - id: used to implement multiple simultaneous subscriptions between the same devices
// - duration: how many seconds the subscription will be active. you can renew it by sending the same START packet before or after the current subscription stopped
// - period:
// - period strictness: non-strict period allows the notifier device to increase period if makes no sense to send callbacks at specified frequency
// - format specifier: device-class-specific format, specifying the events or targets you want to subscribe

// todo add a strict/non-strict subscription periods, where strict will require exactly the specified period,
//  and non-strict means the notifier may increase the period if it makes no sense for it to send callbacks more often


#pragma pack(push, 1)
namespace LogicalProto
{
    const ushort BROADCAST_PORT = 65535;

    enum class LogicalPacketType : ubyte
    {
        UNKNOWN = 0,

        HELLO_WORLD,                // broadcasting this when start
        HELLO_WORLD_RESPONSE,       // unicast response to HELLO_WORLD packet
        FIELD_DICTIONARY_REQUEST,   // request a dictionary with api string indices
        FIELD_DICTIONARY_RESPONSE,  // response to previous

        GROUPS_LIST_REQUEST,        // request groups this device currently in
        GROUPS_LIST_RESPONSE,       // response to previous
        GROUPS_ADD,                 // add this device to new group
        GROUPS_EDIT,                // edit specific groups settings
        GROUPS_REMOVE,              // remove this device from some groups
        GROUPS_FIND_USERS_REQUEST,  // broadcast to find members of some groups
        GROUPS_FIND_USERS_RESPONSE, // unicast response to previous if current device is a member of requested group

        ACTION_EXECUTE,             // execute specified action
        ACTION_EXECUTE_RESULT,      // result status for ACTION_EXECUTE
        ACTION_FETCH,               // request action data
        ACTION_RESPONSE,            // response to the previous

        SUBSCRIPTION_START,         // initializes subscription to specific action
        SUBSCRIPTION_DONE,          // response to previous
        SUBSCRIPTION_CALLBACK,      // event callback to subscriber
        SUBSCRIPTION_STOP,          // stops existing subscription (from subscriber side)
    };

    enum class DeviceClassEnum : uint
    {
        UNKNOWN = 0,

        BUTTON = 1,
        RELAY = 2,
        TEMPERATURE_SENSOR = 3,
        TEMP_HUM_SENSOR = 4,
        CONTROLLER = 5, // means you should not directly interact with it. its standalone
        PC2LOGICAL_ADAPTER = 6,
        LUA_INTERPRETER = 7,
        LED_1_DIM = 8,
        LED_2_DIM = 9,
        HW_ACCESSOR = 10, // for virtual devices, interfacing with specific chips
        PY_INTERPRETER = 11,

        STRING_NAME = (0u - 1)
    };

    enum class ActionType : ubyte
    {
        UNKNOWN = 0,
        IMMEDIATE,
        TOGGLE,
        RANGE,      // For number range [0, 255]
        LABEL,
        TEMPERATURE,
        HUMIDITY,
        TIME_DELTA, // for uptime and so on
        TIME,
    };

    enum class ActionExecuteStatus : ubyte
    {
        UNKNOWN = 0,
        SUCCESS,
        FAIL,
        ARGUMENTS_ERROR,
        ACTION_NOT_FOUND,
        TIMEOUT,
        // ...
    };

    enum ActionExecuteFlags : ubyte
    {
        REQUIRE_STATUS_RESPONSE = 1 << 0,
    };

    // todo think of device attribute utilization
    struct HelloWorldPacket
    {
        DeviceClassEnum device_class;
        ubyte name_len;
        ubyte special_attrib_count;
        ubyte action_count;
        ubyte name[0]; // real size is `name_len`
        struct HelloWorldDeviceAttrib {
            ubyte key_len;
            ubyte value_len;
            ubyte key[0];    // real size is `key_len`
            ubyte value[0];  // real size is `value_len`
        } attribs[0];        // real size is `special_attrib_count`
        struct ActionData {
            ActionType type;
            ubyte name_length;
            ubyte name[0];
        } actions[0];
    };

    struct HelloWorldResponsePacket : public HelloWorldPacket
    {
        //
    };

    struct FieldDictionaryRequestPacket
    {
        // no fields
    };

    struct FieldDictionaryResponsePacket
    {
        ushort field_count;
        struct ApiFieldLayout {
            ubyte length;
            ubyte string[0];  // real size is `length`
        } fields[0];          // real size is `field_count`
    };

    struct GroupsListRequestPacket
    {
        // no fields
    };

    struct GroupsListResponsePacket
    {
        ubyte groups_count;
        // todo groups layout
    };

    struct GroupsAddPacket
    {
        //
    };

    struct GroupsEditPacket
    {
        //
    };

    struct GroupsRemovePacket
    {
        //
    };

    struct GroupsFindUsersRequestPacket
    {
        //
    };

    struct GroupsFindUsersResponsePacket
    {
        //
    };

    struct ActionExecutePacket
    {
        ushort action_id;
        ubyte request_id;
        ActionExecuteFlags flags;
        ubyte payload[0];
    };

    struct ActionExecuteResultPacket
    {
        ushort action_id;
        ubyte request_id;
        ActionExecuteStatus status;
    };

    struct ActionFetchPacket
    {
        ushort action_id;
        ubyte request_id;
        ubyte payload[0];
    };

    struct ActionResponsePacket
    {
        ActionExecuteStatus status;
        ushort action_id;
        ubyte request_id;
        ubyte payload[0];
    };

    struct SubscriptionStartPacket
    {
        uint id;               // subscription id
        ushort action_id;      // action id
        ushort duration;       // how long this subscription will be active. in seconds
        uint period;           // for regularly updated devices: how often updated info will be sent. in milliseconds
        ubyte info_payload[0]; // description of events for subscription
    };

    struct SubscriptionDonePacket
    {
        uint id;
        uint state; // zero - "OK", others - error
    };

    struct SubscriptionCallbackPacket
    {
        uint id;
        ubyte payload[0];
    };

    struct SubscriptionStopPacket
    {
        uint id;
    };

    struct SubscriptionTerminatedPacket
    {
        uint id;
        ubyte reason[0];
    };

#define LOG_PACKET_SIZE(field_name) (uintptr_t) (&((LogicalProto::LogicalPacket*) nullptr)->field_name + 1)
    struct LogicalPacket
    {
        LogicalPacketType type;
        ushort src_addr;
        ushort dst_addr;
        union {
            HelloWorldPacket hello_world;
            HelloWorldResponsePacket hello_world_response;
            FieldDictionaryRequestPacket field_dictionary_request;
            FieldDictionaryResponsePacket field_dictionary_response;

            GroupsListRequestPacket groups_list_request;
            GroupsListResponsePacket groups_list_response;
            GroupsAddPacket groups_add;
            GroupsEditPacket groups_edit;
            GroupsRemovePacket groups_remove;
            GroupsFindUsersRequestPacket groups_find_users_request;
            GroupsFindUsersResponsePacket groups_find_users_response;

            ActionExecutePacket action_execute;
            ActionExecuteResultPacket action_execute_result;
            ActionFetchPacket action_fetch;
            ActionResponsePacket action_response;

            SubscriptionStartPacket subscription_start;
            SubscriptionDonePacket subscription_done;
            SubscriptionCallbackPacket subscription_callback;
            SubscriptionStopPacket subscription_stop;

            ubyte payload[0];
        };

        static inline uint get_header_size() {
            return offsetof(LogicalPacket, hello_world);
        }

        static ushort get_packet_size(LogicalPacketType type_) {
            switch(type_) {
                case LogicalPacketType::UNKNOWN: return 0;
                case LogicalPacketType::HELLO_WORLD: return LOG_PACKET_SIZE(hello_world);
                case LogicalPacketType::HELLO_WORLD_RESPONSE: return LOG_PACKET_SIZE(hello_world_response);
                case LogicalPacketType::FIELD_DICTIONARY_REQUEST: return LOG_PACKET_SIZE(field_dictionary_request);
                case LogicalPacketType::FIELD_DICTIONARY_RESPONSE: return LOG_PACKET_SIZE(field_dictionary_response);
                case LogicalPacketType::GROUPS_LIST_REQUEST: return LOG_PACKET_SIZE(groups_list_request);
                case LogicalPacketType::GROUPS_LIST_RESPONSE: return LOG_PACKET_SIZE(groups_list_response);
                case LogicalPacketType::GROUPS_ADD: return LOG_PACKET_SIZE(groups_add);
                case LogicalPacketType::GROUPS_EDIT: return LOG_PACKET_SIZE(groups_edit);
                case LogicalPacketType::GROUPS_REMOVE: return LOG_PACKET_SIZE(groups_remove);
                case LogicalPacketType::GROUPS_FIND_USERS_REQUEST: return LOG_PACKET_SIZE(groups_find_users_request);
                case LogicalPacketType::GROUPS_FIND_USERS_RESPONSE: return LOG_PACKET_SIZE(groups_find_users_response);
                case LogicalPacketType::ACTION_EXECUTE: return LOG_PACKET_SIZE(action_execute);
                case LogicalPacketType::ACTION_EXECUTE_RESULT: return LOG_PACKET_SIZE(action_execute_result);
                case LogicalPacketType::ACTION_FETCH: return LOG_PACKET_SIZE(action_fetch);
                case LogicalPacketType::ACTION_RESPONSE: return LOG_PACKET_SIZE(action_response);
                case LogicalPacketType::SUBSCRIPTION_START: return LOG_PACKET_SIZE(subscription_start);
                case LogicalPacketType::SUBSCRIPTION_DONE: return LOG_PACKET_SIZE(subscription_done);
                case LogicalPacketType::SUBSCRIPTION_CALLBACK: return LOG_PACKET_SIZE(subscription_callback);
                case LogicalPacketType::SUBSCRIPTION_STOP: return LOG_PACKET_SIZE(subscription_stop);
            }
            return 0;
        }

        LogicalPacket() = delete;
    };
}
#pragma pack(pop)

#ifdef KHAWASU_ENABLE_REFLECTION
#include <unordered_map>
#include <string>
namespace KhawasuReflection {

    inline std::unordered_map<LogicalProto::DeviceClassEnum, std::string>& getAllDeviceClasses() {
        #define PAIR(MEMBER) { LogicalProto::DeviceClassEnum::MEMBER, #MEMBER }
        static std::unordered_map<LogicalProto::DeviceClassEnum, std::string> classes = {
                PAIR(UNKNOWN),
                PAIR(BUTTON),
                PAIR(RELAY),
                PAIR(TEMPERATURE_SENSOR),
                PAIR(TEMP_HUM_SENSOR),
                PAIR(CONTROLLER),
                PAIR(PC2LOGICAL_ADAPTER),
                PAIR(LUA_INTERPRETER),
                PAIR(LED_1_DIM),
                PAIR(LED_2_DIM),
                PAIR(HW_ACCESSOR),
                PAIR(PY_INTERPRETER),
                PAIR(STRING_NAME),
        };
        #undef PAIR
        return classes;
    }

    inline std::unordered_map<LogicalProto::ActionType, std::string>& getAllDeviceActions() {
        #define PAIR(MEMBER) { LogicalProto::ActionType::MEMBER, #MEMBER }
        static std::unordered_map<LogicalProto::ActionType, std::string> actions = {
                PAIR(UNKNOWN),
                PAIR(IMMEDIATE),
                PAIR(TOGGLE),
                PAIR(RANGE),
                PAIR(LABEL),
                PAIR(TEMPERATURE),
                PAIR(HUMIDITY),
                PAIR(TIME_DELTA),
                PAIR(TIME),
        };
        #undef PAIR
        return actions;
    }

    inline std::unordered_map<LogicalProto::ActionExecuteStatus, std::string>& getAllDeviceActionStatuses() {
        #define PAIR(MEMBER) { LogicalProto::ActionExecuteStatus::MEMBER, #MEMBER }
        static std::unordered_map<LogicalProto::ActionExecuteStatus, std::string> actions = {
                PAIR(UNKNOWN),
                PAIR(SUCCESS),
                PAIR(FAIL),
                PAIR(ARGUMENTS_ERROR),
                PAIR(ACTION_NOT_FOUND),
                PAIR(TIMEOUT),
        };
        #undef PAIR
        return actions;
    }
}
#endif


