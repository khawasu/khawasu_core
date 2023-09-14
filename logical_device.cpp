#include "logical_device.h"
#include "logical_device_manager.h"
#include "platform.h"
#include "net_utils.h"

using namespace LogicalProto;
using namespace OverlayProto;


// subscription manager
SubscriptionManager::Subscriber::Subscriber(LogicalAddress addr_, u64 end_time_, u64 next_upd_, uint period_, uint id_,
                                            ushort action_id_)
: addr(addr_), end_time(end_time_), next_periodic_update_time(next_upd_), period(period_), subscription_id(id_),
action_id(action_id_) { }

SubscriptionManager::SubscriptionManager(LogicalDevice* device_) : device(device_), self_update_next(0ull - 1) { }

void SubscriptionManager::add_subscriber(SubscriptionStartPacket* packet, uint size, LogicalAddress addr) {
    // todo validate size when extracting format data
    for (auto& subscriber : subscribers) {
        if (subscriber.subscription_id == net_load(packet->id) && subscriber.addr == addr) {
            subscriber.end_time = KhawasuOsApi::get_microseconds() + net_load(packet->duration) * 1'000'000;
            return;
        }
    }

    auto packet_period = net_load(packet->period);
    auto time = KhawasuOsApi::get_microseconds();
    subscribers.emplace_back(addr,
                             (u64) time + net_load(packet->duration) * 1'000'000,
                             packet_period ? (u64) time + packet_period * 1'000 - 1 : (0ull - 1),
                             (uint) packet_period,
                             (uint) net_load(packet->id),
                             (ushort) net_load(packet->action_id));
}

void SubscriptionManager::set_self_update_period(u64 us_period) {
    self_update_period = us_period;
    self_update_next = KhawasuOsApi::get_microseconds() + self_update_period;
}

void SubscriptionManager::stop_self_update() {
    self_update_next = 0ull - 1;
}

void SubscriptionManager::stop_subscription(SubscriptionStopPacket* packet, LogicalAddress addr) {
    auto iter = subscribers.begin();
    while (iter != subscribers.end()) {
        auto& subscriber = *iter;
        if (subscriber.subscription_id == net_load(packet->id) && subscriber.addr == addr) {
            subscribers.erase(iter++);
            continue;
        }
        iter++;
    }
}

void SubscriptionManager::update_periodic() {
    auto time = KhawasuOsApi::get_microseconds();

    if (time > self_update_next) {
        device->on_timer_update();
        self_update_next += self_update_period;
    }

    auto iter = subscribers.begin();
    while (iter != subscribers.end()) {
        auto& subscriber = *iter;
        if (time >= subscriber.end_time) {
            subscribers.erase(iter++);
            continue;
        }

        if (subscriber.next_periodic_update_time <= time) {
            subscriber.next_periodic_update_time += subscriber.period * 1'000;
            device->on_subscription_timer_update(subscriber.addr, subscriber.subscription_id, subscriber.action_id, nullptr); // todo feed format data here
        }

        iter++;
    }
}

void SubscriptionManager::send_immediate_callback_data(ushort action_id, ubyte* data, uint size) {
    for (auto& subscriber : subscribers) {
        // todo store subscribers in map with key=action_id
        if(subscriber.action_id != action_id)
            continue;

        auto log = device->dev_manager->alloc_logical_packet_ptr(
                subscriber.addr, device->self_port, size,
                OverlayProtoType::UNRELIABLE, LogicalPacketType::SUBSCRIPTION_CALLBACK);
        net_store(log.ptr()->subscription_callback.id, subscriber.subscription_id);
        net_memcpy(log.ptr()->subscription_callback.payload, data, size);
        device->dev_manager->finish_ptr(log);
    }
}


// logical device
LogicalDevice::LogicalDevice(LogicalDeviceManager* manager_, const char* name_, ushort port_)
: self_port(port_), dev_manager(manager_), name(name_) { }

void LogicalDevice::post_init() {
    send_hello_world(LogicalProto::LogicalPacketType::HELLO_WORLD, MeshProto::BROADCAST_FAR_ADDR, BROADCAST_PORT);
}

void LogicalDevice::update() {
    //
}

void LogicalDevice::send_hello_world(LogicalPacketType type, MeshProto::far_addr_t dst_phy, ushort dst_port) {
    // getting info
    auto name = get_name();
    auto [attribs, attrib_cnt] = get_attribs();
    auto [actions, action_cnt] = get_api_actions();
    auto name_len = strlen(name);

    // calculating packet size
    auto additional_size = name_len + sizeof(LogicalProto::HelloWorldPacket::HelloWorldDeviceAttrib) * attrib_cnt
                                    + sizeof(LogicalProto::HelloWorldPacket::ActionData) * action_cnt;

    for (int i = 0; i < attrib_cnt; ++i)
        additional_size += attribs[i].name_len + attribs[i].value_len;

    for (int i = 0; i < action_cnt; ++i)
        additional_size += actions[i].length;

    // building packet
    auto log = dev_manager->alloc_logical_packet_ptr({dst_phy, dst_port}, self_port, additional_size,
                                                     OverlayProtoType::UNRELIABLE, type);

    // writing packet parameters
    net_store(log.ptr()->hello_world.name_len, name_len);
    net_store(log.ptr()->hello_world.special_attrib_count, attrib_cnt);
    net_store(log.ptr()->hello_world.device_class, get_device_class());
    net_store(log.ptr()->hello_world.action_count, action_cnt);
    net_memcpy(log.ptr()->hello_world.name, name, name_len);  // without \0

    // writing variable-length parameters
    auto attrib_offset_byte = log.ptr()->hello_world.name + name_len;
    for (int i = 0; i < attrib_cnt; ++i) {
        auto attrib_offset = (HelloWorldPacket::HelloWorldDeviceAttrib*) attrib_offset_byte;
        net_store(attrib_offset->key_len, attribs[i].name_len);
        net_store(attrib_offset->value_len, attribs[i].value_len);
        net_memcpy(attrib_offset->key, attribs[i].name, attribs[i].name_len);
        net_memcpy(attrib_offset->value + attribs[i].name_len, attribs[i].value, attribs[i].value_len);

        attrib_offset_byte = &attrib_offset->value[attribs[i].name_len + attribs[i].value_len];
    }

    // writing action data
    auto action_offset_byte = attrib_offset_byte;
    for (int i = 0; i < action_cnt; ++i) {
        auto action_offset = (HelloWorldPacket::ActionData*) action_offset_byte;
        net_store(action_offset->type, actions[i].type);
        net_store(action_offset->name_length, actions[i].length);
        net_memcpy(action_offset->name, actions[i].name, actions[i].length);

        action_offset_byte = &action_offset->name[actions[i].length];
    }

    dev_manager->finish_ptr(log);
    free_api_actions(actions);
    free_attribs(attribs);
    free_name(name);
}

void LogicalDevice::send_field_dictionary(LogicalAddress dst_addr) {
    auto [fields, field_cnt] = get_api_fields();

    auto additional_size = field_cnt * sizeof(FieldDictionaryResponsePacket::ApiFieldLayout);
    for (int i = 0; i < field_cnt; ++i)
        additional_size += fields[i].length;

    // building packet
    auto log = dev_manager->alloc_logical_packet_ptr(dst_addr, self_port, additional_size,
                                                     OverlayProtoType::UNRELIABLE, LogicalPacketType::FIELD_DICTIONARY_RESPONSE);
    log.ptr()->field_dictionary_response.field_count = field_cnt;

    // writing variable-length parameters
    auto field_offset_byte = (ubyte*) log.ptr()->field_dictionary_response.fields;
    for (int i = 0; i < field_cnt; ++i) {
        auto field_write = (FieldDictionaryResponsePacket::ApiFieldLayout*) field_offset_byte;
        net_store(field_write->length, fields[i].length);
        net_memcpy(field_write->string, fields[i].string, fields[i].length); // without \0
        field_offset_byte = field_write->string + fields[i].length;
    }

    dev_manager->finish_ptr(log);
    free_api_fields(fields);
}

bool LogicalDevice::on_general_packet_accept(LogicalPacket* packet, ushort size, MeshProto::far_addr_t src_phy) {
    return true;
}

void LogicalDevice::on_device_discover(LogicalPacket* packet, ushort size, MeshProto::far_addr_t src_phy) {
    printf("default device discovery: im %d, i found %d (type %d)\n", self_port, net_load(packet->src_addr),
           (int) net_load(packet->hello_world.device_class));
}

void LogicalDevice::on_device_field_dictionary(FieldDictionaryResponsePacket::ApiFieldLayout* fields, ubyte count, MeshProto::far_addr_t src_phy) {
    //
}

void LogicalDevice::on_subscription_data(ubyte* data, uint size, LogicalAddress addr, uint sub_id) {
    //
}

void LogicalDevice::on_subscription_timer_update(LogicalAddress addr, uint sub_id, ushort act_id, const void* format) {
    //
}


const char* LogicalDevice::get_name() {
    return name;
}

std::pair<DeviceAttrib*, ubyte> LogicalDevice::get_attribs() {
    return {nullptr, 0};
}

std::pair<DeviceApiField*, ubyte> LogicalDevice::get_api_fields() {
    return {nullptr, 0};
}

std::pair<DeviceApiAction*, ubyte> LogicalDevice::get_api_actions() {
    return {nullptr, 0};
}

LogicalProto::DeviceClassEnum LogicalDevice::get_device_class() {
    return LogicalProto::DeviceClassEnum::UNKNOWN;
}

void LogicalDevice::free_name(const char* name) {
    // do nothing, currently it is a string literal
}

void LogicalDevice::free_attribs(DeviceAttrib*) {
    // do nothing, currently it is a static array
}

void LogicalDevice::free_api_fields(DeviceApiField*) {
    // do nothing, currently it is a static array
}

void LogicalDevice::free_api_actions(DeviceApiAction*) {
    // do nothing, currently it is a static array
}

void LogicalDevice::on_timer_update() {
    //
}

ActionExecuteStatus LogicalDevice::on_action_set(int action_id, const ubyte* data, uint size, LogicalAddress addr) {
    return ActionExecuteStatus::UNKNOWN;
}

void LogicalDevice::on_action_get(int action_id, const ubyte* data, uint size, LogicalAddress addr, ubyte request_id) {
    //
}

void LogicalDevice::on_action_get_response(int action_id, const ubyte* data, uint size, LogicalAddress addr, ubyte request_id) {
    //
}
