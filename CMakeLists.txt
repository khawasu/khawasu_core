cmake_minimum_required(VERSION 3.20)

set(KHAWASU_CORE_SRCS "logical_device.cpp" "logical_device_manager.cpp")

# todo remove esp32 specific include in preserved_property.h

# ESP-IDF component
if (ESP_PLATFORM)
    # adding ESP-IDF component
    idf_component_register(
            SRCS ${KHAWASU_CORE_SRCS}
            INCLUDE_DIRS "."
            REQUIRES nvs_flash spi_flash fresh
    )

    set(KHAWASU_CORE_TARGET_NAME ${COMPONENT_LIB})

# PC library
else()
    set(KHAWASU_CORE_TARGET_NAME khawasu_core)
    project(khawasu_core)

    # adding pc library
    add_library(khawasu_core STATIC ${KHAWASU_CORE_SRCS})
    target_include_directories(khawasu_core PUBLIC ".")
    target_link_libraries(khawasu_core PUBLIC fresh_static)
endif()

set_target_properties(${KHAWASU_CORE_TARGET_NAME} PROPERTIES CXX_STANDARD 20)
