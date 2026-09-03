cmake_minimum_required(VERSION 3.20)
project(chemcpp VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------
# 基本設定
# ---------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)              # -std=c++20 (not gnu++20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)      # clangd 用

# ビルドタイプの指定忘れ対策（Makefile系だと -O0 になってしまう）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set; defaulting to Release")
endif()

option(CHEMCPP_BUILD_TESTS "Build tests"                ON)
option(CHEMCPP_BUILD_APPS  "Build command line tools"   ON)
option(CHEMCPP_SANITIZE    "Enable ASan + UBSan"        OFF)
option(CHEMCPP_NATIVE      "Optimize for this machine"  OFF)
option(CHEMCPP_LTO         "Enable link-time optimization" OFF)

# ccache があれば使う（リビルドが劇的に速くなる）
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# ---------------------------------------------------------------------
# ライブラリ本体
# ---------------------------------------------------------------------
add_library(chemcpp
    src/smiles.cpp
    src/fingerprint.cpp
    src/descriptors.cpp
    src/database.cpp
    src/search.cpp
    src/io.cpp
)
add_library(chemcpp::chemcpp ALIAS chemcpp)

target_include_directories(chemcpp
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include>
)
target_compile_features(chemcpp PUBLIC cxx_std_20)

# スレッドを使う（search.cpp）
find_package(Threads REQUIRED)
target_link_libraries(chemcpp PUBLIC Threads::Threads)

# ---------------------------------------------------------------------
# 警告設定
# ---------------------------------------------------------------------
if(MSVC)
    target_compile_options(chemcpp PRIVATE /W4 /permissive-)
else()
    target_compile_options(chemcpp PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wnull-dereference
        -Wdouble-promotion
    )
endif()

# ---------------------------------------------------------------------
# オプション: サニタイザ / ネイティブ最適化 / LTO
# ---------------------------------------------------------------------
if(CHEMCPP_SANITIZE AND NOT MSVC)
    message(STATUS "Sanitizers enabled (address, undefined)")
    target_compile_options(chemcpp PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(chemcpp PUBLIC -fsanitize=address,undefined)
    target_compile_definitions(chemcpp PUBLIC _GLIBCXX_ASSERTIONS)
endif()

if(CHEMCPP_NATIVE AND NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
    if(HAS_MARCH_NATIVE)
        message(STATUS "Building with -march=native (not portable!)")
        target_compile_options(chemcpp PUBLIC -march=native)
    endif()
endif()

if(CHEMCPP_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg)
    if(ipo_ok)
        set_property(TARGET chemcpp PROPERTY
                     INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        message(STATUS "LTO enabled")
    else()
        message(STATUS "LTO not supported: ${ipo_msg}")
    endif()
endif()

# ---------------------------------------------------------------------
# 実行ファイル
# ---------------------------------------------------------------------
if(CHEMCPP_BUILD_APPS)
    foreach(app parse_demo fp_demo screen descriptors_cli)
        add_executable(${app} apps/${app}.cpp)
        target_link_libraries(${app} PRIVATE chemcpp::chemcpp)
    endforeach()
endif()

# ---------------------------------------------------------------------
# テスト
# ---------------------------------------------------------------------
if(CHEMCPP_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    add_executable(chemcpp_tests tests/test_chemcpp.cpp)
    target_link_libraries(chemcpp_tests
        PRIVATE chemcpp::chemcpp Catch2::Catch2WithMain)

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(chemcpp_tests)
endif()

# ---------------------------------------------------------------------
message(STATUS "----------------------------------------")
message(STATUS "chemcpp ${PROJECT_VERSION}")
message(STATUS "  build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  tests      : ${CHEMCPP_BUILD_TESTS}")
message(STATUS "  sanitize   : ${CHEMCPP_SANITIZE}")
message(STATUS "  native     : ${CHEMCPP_NATIVE}")
message(STATUS "----------------------------------------")
