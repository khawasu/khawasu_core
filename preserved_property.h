#pragma once
#include <cstring>
#include "types.h"

constexpr uint crc32(const ubyte* message, uint size) {
    uint crc = 0xFFFFFFFF;

    for (int i = 0; i < size; ++i) {
        uint byte = message[i];            // Get next byte.
        crc = crc ^ byte;
        for (int j = 7; j >= 0; j--) {    // Do eight times.
            uint mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

#ifdef ESP_PLATFORM
#include <nvs.h>
class Storage
{
public:
    nvs_handle_t handle = -1;

    void init() {
        if(handle == -1)
            open();
    }

    void open() {
        nvs_open("preprop", NVS_READWRITE, &handle);
    }

    template<typename T>
    void save(const char* key, const T& value) {
        nvs_set_blob(handle, key, &value, sizeof(T));
        nvs_commit(handle);
    }

    template <typename T>
    bool read(const char* key, T& value) {
        size_t size = sizeof(T);
        auto err = nvs_get_blob(handle, key, &value, &size);

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return false;
        } else if(err != ESP_OK || size != sizeof(T)) {
            printf("PreProp failed NVS for %s\n", key);
            return false;
        }

        return true;
    }
};
#else
#include <unordered_map>
class Storage
{
public:
    std::unordered_map<const char*, void*> data;

    void init() {
        //
    }

    void open() {
        //
    }

    template<typename T>
    void save(const char* key, const T& value) {
        data[key] = (void*)value;
    }

    template <typename T>
    bool read(const char* key, T& value) {
        auto value_it = data.find(key);
        if (value_it == data.end())
            return false;

        value = *(T*)value_it->second;

        return true;
    }
};
#endif

inline Storage storage;

// todo: invalidate nvs on new firmware
template <typename T>
class PreservedProperty {
    ushort instance_id;
    const char* name;
    T value;
    char nvs_key[16];

public:

    template <typename... TArgs>
    explicit PreservedProperty(ushort instance_id_, const char* name_, TArgs... args) : instance_id(instance_id_), name(name_) {
        storage.init();
        getFilename(nvs_key); // Caching key
        load(args...);
    }

    // Get filename to "key"
    void getFilename(char key[16]) {
        char instance_id_str[5];
        itoa(instance_id, instance_id_str, 16);

        char name_crc_str[5];
        auto name_crc = (ushort)crc32((const uint8_t*)name, strlen(name));
        itoa(name_crc, name_crc_str, 16);

        strcpy(key, instance_id_str);
        strcat(key, ":");
        strcat(key, name_crc_str);
    }

    template <typename... TArgs>
    void load(TArgs... args) {
        auto err = storage.read(nvs_key, value);

        if (!err){
            new (&value) T { args... };
        }
    }

    const T& operator=(const T& new_value) {
        // Guard self assignment
        if (value == new_value)
            return value;

        value = new_value;
        storage.save(nvs_key, value);

        return value;
    }

    const T& operator* () {
        return value;
    }
};


#define PROPERTY(type, name, ...) PreservedProperty<type> name{self_port, #name, __VA_ARGS__};
