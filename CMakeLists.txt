cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME HomePanel_test)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add touchgfx generated sources and libraries
add_subdirectory(cmake/touchgfx)

# Keep custom TouchGFX helper sources outside Designer-generated CMake files.
target_sources(TouchGFX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/TouchGFX/gui/src/common/GuiTime.cpp
)

# Keep board support code in BSP even if CubeMX regenerates these files.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/fmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ltdc.c
    PROPERTIES HEADER_FILE_ONLY TRUE
)

set(APP_AP6181_ROOT
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/ThirdParty/AP6181
)

set(APP_WIFI_LWIP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/lwip_sys_arch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi_lwip.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/api_lib.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/api_msg.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/err.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/netbuf.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/netdb.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/netifapi.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/sockets.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/api/tcpip.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/def.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/dns.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/inet_chksum.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/init.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ip.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/mem.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/memp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/netif.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/pbuf.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/raw.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/stats.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/sys.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/tcp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/tcp_in.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/tcp_out.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/timeouts.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/udp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/autoip.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/dhcp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/etharp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/icmp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/igmp.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/ip4.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/ip4_addr.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/core/ipv4/ip4_frag.c
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/netif/ethernet.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_WIFI_LWIP_SOURCES}
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_debug_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_home_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_home_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi_platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi_resources.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_board.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_sdram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_qspi_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_touch_gt911.c
    ${APP_AP6181_ROOT}/Firmware/43362A2_bin.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Inc
    ${APP_AP6181_ROOT}/WiFi_SDK/include
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/platform/GCC
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/include
    ${APP_AP6181_ROOT}/WiFi_SDK/WICED/network/LwIP/ver2.0.3/src/include/posix
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
