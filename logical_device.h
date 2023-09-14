#pragma once


#include <cstring>
#include <list>
#include "protocols/logical_proto.h"
#include "types.h"
#include <mesh_controller.h>


struct DeviceAttrib
{
    const char* name;
    const char* value;
    ubyte name_len;
    ubyte value_len;

    template<int name_len_, int value_len_>
    constexpr DeviceAttrib(const char (&name_)[name_len_], const char (&value_)[value_len_])
    : name(name_), value(value_), name_len(name_len_ - 1), value_len(value_len_ - 1) { }
};


struct DeviceApiField
{
    const char* string;
    ubyte length;

    template <int str_len_>
    constexpr DeviceApiField(const char (&string_)[str_len_])
            : string(string_), length(str_len_ - 1) { }
};

struct DeviceApiAction
{
    const char* name;
    ubyte length;
    LogicalProto::ActionType type;

    // string_ - null-terminated string
    template <int str_len_>
    constexpr DeviceApiAction(LogicalProto::ActionType _type, const char (&string_)[str_len_])
            : name(string_), length(str_len_ - 1), type(_type) { }

    DeviceApiAction(const char* string_, LogicalProto::ActionType _type)
            : name(string_), length(strlen(string_)), type(_type) { }

};


struct LogicalAddress
{
    static constexpr MeshProto::far_addr_t NULL_ADDR = 0;

    MeshProto::far_addr_t phy;
    ushort log;

    inline LogicalAddress(MeshProto::far_addr_t phy_, ushort log_) : phy(phy_), log(log_) { }

    inline LogicalAddress() : phy(0), log(0) { }

    inline bool is_valid() {
        return phy == NULL_ADDR;
    }
} __attribute__((packed));

inline bool operator==(const LogicalAddress& a, const LogicalAddress& b) {
    return a.phy == b.phy && a.log == b.log;
}


class LogicalDevice;

class SubscriptionManager
{
public:
    struct Subscriber
    {
        LogicalAddress addr;
        u64 end_time;                  // system time, us
        u64 next_periodic_update_time; // system time, us
        uint period;                   // delta time, ms
        uint subscription_id;
        ushort action_id;

        Subscriber(LogicalAddress addr_, u64 end_time_, u64 next_upd_, uint period_, uint id_, ushort action_id_);
    };

    LogicalDevice* device;
    std::list<Subscriber> subscribers;
    u64 self_update_period;
    u64 self_update_next;

    explicit SubscriptionManager(LogicalDevice* device_);

    void add_subscriber(LogicalProto::SubscriptionStartPacket* packet, uint size, LogicalAddress addr);

    void set_self_update_period(u64 us_period);

    void stop_self_update();

    void stop_subscription(LogicalProto::SubscriptionStopPacket* packet, LogicalAddress addr);

    void send_immediate_callback_data(ushort action_id, ubyte* data, uint size);

    void update_periodic();
};


class LogicalDeviceManager;

class LogicalDevice
{
public:
    ushort self_port;
    SubscriptionManager subscriptions{this};
    LogicalDeviceManager* dev_manager;
    const char* name;

    LogicalDevice(LogicalDeviceManager* manager_, const char* name_, ushort port_);

    void post_init();

    // todo describe the meaning of these callback functions in comments
    virtual void update();

    virtual void send_hello_world(LogicalProto::LogicalPacketType type, MeshProto::far_addr_t dst_phy, ushort dst_port);

    virtual void send_field_dictionary(LogicalAddress dst_addr);

    virtual bool on_general_packet_accept(LogicalProto::LogicalPacket* packet, ushort size, MeshProto::far_addr_t src_phy); // return false to discard packet and not call other device methods

    virtual void on_device_discover(LogicalProto::LogicalPacket* packet, ushort size, MeshProto::far_addr_t src_phy);

    virtual void on_device_field_dictionary(LogicalProto::FieldDictionaryResponsePacket::ApiFieldLayout* fields, ubyte count, MeshProto::far_addr_t src_phy);

    // todo replace with const ubyte* data
    virtual void on_subscription_data(ubyte* data, uint size, LogicalAddress addr, uint sub_id);

    virtual void on_subscription_timer_update(LogicalAddress addr, uint sub_id, ushort act_id, const void* format);

    virtual void on_timer_update();

    virtual LogicalProto::ActionExecuteStatus on_action_set(int action_id, const ubyte* data, uint size, LogicalAddress addr);

    virtual void on_action_get(int action_id, const ubyte* data, uint size, LogicalAddress addr, ubyte request_id);

    virtual void on_action_get_response(int action_id, const ubyte* data, uint size, LogicalAddress addr, ubyte request_id);

    virtual const char* get_name();

    virtual std::pair<DeviceAttrib*, ubyte> get_attribs();

    virtual std::pair<DeviceApiField*, ubyte> get_api_fields();

    virtual std::pair<DeviceApiAction*, ubyte> get_api_actions();

    virtual LogicalProto::DeviceClassEnum get_device_class();

    virtual void free_name(const char* name);

    virtual void free_attribs(DeviceAttrib*);

    virtual void free_api_fields(DeviceApiField*);

    virtual void free_api_actions(DeviceApiAction*);
};


#define OVERRIDE_ATTRIBS(...)                                               \
std::pair<DeviceAttrib*, ubyte> get_attribs() override {                    \
    return {(DeviceAttrib*) attribs, sizeof(attribs) / sizeof(attribs[0])}; \
}                                                                           \
protected:                                                                  \
constexpr static const DeviceAttrib attribs[] = {                           \
    __VA_ARGS__                                                             \
};                                                                          \
public:

#define OVERRIDE_FIELDS(...)                                                           \
std::pair<DeviceApiField*, ubyte> get_api_fields() override {                          \
    return {(DeviceApiField*) api_fields, sizeof(api_fields) / sizeof(api_fields[0])}; \
}                                                                                      \
protected:                                                                             \
constexpr static const DeviceApiField api_fields[] = {                                 \
    __VA_ARGS__                                                                        \
};                                                                                     \
                                                                                       \
template <int string_len>                                                              \
constexpr static uint get_field_id(const char (&name)[string_len]) {                   \
    for (auto i = 0; i < sizeof(api_fields) / sizeof(api_fields[0]); ++i) {            \
        if (api_fields[i].length != string_len - 1)                                    \
            continue;                                                                  \
                                                                                       \
        int j;                                                                         \
        for (j = 0; j < string_len && api_fields[i].string[j] == name[j]; ++j) ;       \
        if (j >= string_len)                                                           \
            return i;                                                                  \
    }                                                                                  \
    return 254;                                                                        \
}                                                                                      \
public:

#define OVERRIDE_DEV_CLASS(value) \
LogicalProto::DeviceClassEnum get_device_class() override { return value; }

#define OVERRIDE_ACTIONS(...)                                                              \
std::pair<DeviceApiAction*, ubyte> get_api_actions() override {                            \
    return {(DeviceApiAction*) api_actions, sizeof(api_actions) / sizeof(api_actions[0])}; \
}                                                                                          \
protected:                                                                                 \
constexpr static const DeviceApiAction api_actions[] = {                                   \
    __VA_ARGS__                                                                            \
};                                                                                         \
                                                                                           \
template <int string_len>                                                                  \
constexpr static uint get_action_id(const char (&name)[string_len]) {                      \
    for (auto i = 0; i < sizeof(api_actions) / sizeof(api_actions[0]); ++i) {              \
        if (api_actions[i].length != string_len - 1)                                       \
            continue;                                                                      \
                                                                                           \
        int j;                                                                             \
        for (j = 0; j < string_len && api_actions[i].name[j] == name[j]; ++j) ;            \
        if (j >= string_len)                                                               \
            return i;                                                                      \
    }                                                                                      \
    return 254;                                                                            \
}                                                                                          \
public:
