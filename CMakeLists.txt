cmake_minimum_required(VERSION 3.21.0)
project(lansend LANGUAGES CXX)

file(GLOB_RECURSE CXX_SOURCE
    src/*.cpp
    src/*.h
    src/*.hpp
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# policy set for FindBoost
if (POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# find vcpkg-installed
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONENTS system url filesystem)
find_package(OpenSSL 3.3.0 REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(lansend ${CXX_SOURCE})

if(WIN32)
  target_compile_definitions(lansend PRIVATE _WIN32_WINNT=0x0A00)
endif()

target_include_directories(lansend
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lansend
    PRIVATE
        spdlog::spdlog_header_only
        Boost::boost
        Boost::system
        Boost::filesystem
        Boost::url
        OpenSSL::Crypto
        OpenSSL::SSL
        nlohmann_json::nlohmann_json
)