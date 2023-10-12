cmake_minimum_required(VERSION 3.26)
project(bone_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(bone_bot
    src/main.cpp
    src/insults.h src/insults.cpp
    src/teams.h src/teams.cpp
)

add_executable(tests
    src/tests.cpp
    src/insults.h src/insults.cpp
    src/teams.h src/teams.cpp
)

target_compile_features(bone_bot PUBLIC cxx_std_20)
# Enable DPP coroutine features
target_compile_definitions(bone_bot PUBLIC DPP_CORO)
option(DPP_CORO "Experimental support for C++20 coroutines" OFF)
set(DPP_CORO ON)

add_subdirectory(lib/DPP)
target_link_libraries(bone_bot PRIVATE dpp)
target_link_libraries(tests PRIVATE dpp)

# Extras for DPP
find_package(ZLIB REQUIRED)
target_link_libraries(bone_bot PRIVATE ZLIB::ZLIB)
target_link_libraries(tests PRIVATE ZLIB::ZLIB)

find_package(Opus CONFIG REQUIRED)
target_link_libraries(bone_bot PRIVATE Opus::opus)
target_link_libraries(tests PRIVATE Opus::opus)

find_package(OpenSSL REQUIRED)
target_link_libraries(bone_bot PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_package(unofficial-sodium CONFIG REQUIRED)
target_link_libraries(bone_bot PRIVATE unofficial-sodium::sodium unofficial-sodium::sodium_config_public)
target_link_libraries(tests PRIVATE unofficial-sodium::sodium unofficial-sodium::sodium_config_public)

# Nice to have extras
find_package(fmt CONFIG REQUIRED)
target_link_libraries(bone_bot PRIVATE fmt::fmt)
target_link_libraries(tests PRIVATE fmt::fmt)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(bone_bot PRIVATE spdlog::spdlog)
target_link_libraries(tests PRIVATE spdlog::spdlog)

find_package(tomlplusplus CONFIG REQUIRED)
target_link_libraries(bone_bot PRIVATE tomlplusplus::tomlplusplus)
target_link_libraries(tests PRIVATE tomlplusplus::tomlplusplus)

# Tests
find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
