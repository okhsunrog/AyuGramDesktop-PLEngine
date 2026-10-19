cmake_minimum_required(VERSION 3.20)

project(ayu_plengine_channel_reader LANGUAGES CXX)

set(
    AYUGRAM_BUILD_DIR
    ""
    CACHE PATH
    "A configured AyuGram build directory containing Telegram/gen/scheme.h")

if(NOT AYUGRAM_BUILD_DIR)
    message(FATAL_ERROR "AYUGRAM_BUILD_DIR is required")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(ayu_plengine_channel_reader MODULE channel_reader.cpp)

target_compile_features(ayu_plengine_channel_reader PRIVATE cxx_std_20)
target_compile_definitions(
    ayu_plengine_channel_reader
    PRIVATE
        AYUPLUGIN
        CRL_FORCE_COMMON_QUEUE
        QT_NO_CAST_FROM_BYTEARRAY
        QT_NO_KEYWORDS)
target_compile_options(
    ayu_plengine_channel_reader
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -fvisibility=hidden
        -fno-strict-aliasing)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$")
    target_compile_options(ayu_plengine_channel_reader PRIVATE -mno-direct-extern-access)
endif()
target_include_directories(
    ayu_plengine_channel_reader
    PRIVATE
        ../../Telegram/SourceFiles
        ../../Telegram/lib_base
        ../../Telegram/lib_rpl
        ../../Telegram/lib_crl
        ../../Telegram/lib_tl
        ../../Telegram/ThirdParty/GSL/include
        "${AYUGRAM_BUILD_DIR}/Telegram/gen")
target_link_libraries(ayu_plengine_channel_reader PRIVATE Qt6::Core)
target_link_options(
    ayu_plengine_channel_reader
    PRIVATE
        -Wl,--unresolved-symbols=ignore-all)

set_target_properties(
    ayu_plengine_channel_reader
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "ayu-plengine-channel-reader")
