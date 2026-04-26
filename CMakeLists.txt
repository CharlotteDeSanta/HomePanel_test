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

# Keep board support code in BSP even if CubeMX regenerates these files.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/fmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ltdc.c
    PROPERTIES HEADER_FILE_ONLY TRUE
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_debug_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi_platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_wifi_resources.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_board.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_sdram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_qspi_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Src/bsp_touch_gt911.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Examples/wifi-ap6181-h743pro-code-master/wifi_lwip_tcpecho_socket/User/43362A2_bin.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Examples/wifi-ap6181-h743pro-code-master/WiFi_SDK/include
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
