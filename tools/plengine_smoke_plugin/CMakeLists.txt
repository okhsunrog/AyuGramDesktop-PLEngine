cmake_minimum_required(VERSION 3.20)

project(ayu_plengine_smoke_plugin LANGUAGES CXX)

add_library(ayu_plengine_smoke_plugin MODULE smoke_plugin.cpp)

target_compile_features(ayu_plengine_smoke_plugin PRIVATE cxx_std_17)
target_compile_options(
    ayu_plengine_smoke_plugin
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fvisibility=hidden)

set_target_properties(
    ayu_plengine_smoke_plugin
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "ayu-plengine-smoke-plugin")

target_include_directories(ayu_plengine_smoke_plugin PRIVATE ../../Telegram/SourceFiles)
