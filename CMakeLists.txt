cmake_minimum_required(VERSION 3.6)
include(CMakeForceCompiler)
SET(CMAKE_SYSTEM_NAME "Generic")
SET(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)

#
# Set Toolchain Path Below
#

if (UNIX)
    set(tools "$ENV{HOME}/opt/gcc-arm-none-eabi/")
    set(CMAKE_C_COMPILER ${tools}/bin/arm-none-eabi-gcc)
    set(CMAKE_C_COMPILER ${tools}/bin/arm-none-eabi-g++)
else ()
    set(tools "D:/Programy/GNU Tools ARM Embedded/5.4 2016q3/")
    set(CMAKE_C_COMPILER ${tools}/bin/arm-none-eabi-gcc.exe)
    set(CMAKE_C_COMPILER ${tools}/bin/arm-none-eabi-g++.exe)
endif ()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

project(RS41HUP C CXX)

add_definitions(-DSTM32F100C8)
add_definitions(-DSTM32F10X_MD_VL)
add_definitions(-DUSE_STDPERIPH_DRIVER)
add_definitions(-DSUPPORT_CPLUSPLUS)
add_definitions(-D__ASSEMBLY__)

SET(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/arm-gcc-link.ld)
SET(COMMON_FLAGS " -mcpu=cortex-m3 -Wall -ffunction-sections -O3 -g -nostartfiles ")
SET(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=c++11")
SET(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=gnu99")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map -lstdc++ -O3 -Wl,--gc-sections -T ${LINKER_SCRIPT}")

file(GLOB_RECURSE USER_SOURCES "*.c")
file(GLOB_RECURSE USER_SOURCES_CXX "*.cpp")
file(GLOB_RECURSE USER_HEADERS "*.h")

include_directories(cmsis
        cmsis_boot
        stm_lib/inc
        .)

add_executable(${PROJECT_NAME}.elf ${USER_SOURCES} ${USER_SOURCES_CXX} ${USER_HEADERS} ${HAL_SOURCES} ${LINKER_SCRIPT})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map")
set(HEX_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
        COMMENT "Building ${HEX_FILE} \nBuilding ${BIN_FILE}"
        COMMAND ${tools}/bin/arm-none-eabi-size ${PROJECT_NAME}.elf)

set(CMAKE_CXX_STANDARD 11)

add_custom_target(program
        DEPENDS ${PROJECT_NAME}.elf

        if (UNIX)
            COMMAND st-flash --reset write ${BIN_FILE} 0x08000000
        else ()
            COMMAND D:/Programy/stlink-1.3.0-win64/bin/st-flash --reset write ${BIN_FILE} 0x08000000
        endif ()
        COMMENT "flashing ${BIN_FILE}")
