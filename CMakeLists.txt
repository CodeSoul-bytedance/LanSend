cmake_minimum_required(VERSION 3.21.0)
project(lansend LANGUAGES CXX)

option(BUILD_TESTING "Build the tests" ON)

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
find_package(Boost REQUIRED COMPONENTS system url filesystem asio beast)
find_package(OpenSSL 3.3.0 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)

add_executable(lansend ${CXX_SOURCE})

if(WIN32)
  target_compile_definitions(lansend PRIVATE _WIN32_WINNT=0x0A00)
endif()

# definitions for Logger
target_compile_definitions(lansend
  PRIVATE
    $<$<CONFIG:Debug>:LANSEND_DEBUG>
    $<$<CONFIG:Release>:LANSEND_RELEASE>
    BOOST_ASIO_USE_STD_COROUTINE
    BOOST_ASIO_HAS_CO_AWAIT
)

target_include_directories(lansend
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
)

if (MSVC)
  target_compile_options(lansend PRIVATE /bigobj)
endif()

target_link_libraries(lansend
    PRIVATE
        spdlog::spdlog_header_only
        Boost::boost
        Boost::system
        Boost::filesystem
        Boost::url
        Boost::asio
        Boost::beast
        OpenSSL::Crypto
        OpenSSL::SSL
        nlohmann_json::nlohmann_json
)

# if (BUILD_TESTING)
#     enable_testing()

#     # Add test executable
#     add_executable(lansend_tests
        
#     )

#     # Include directories for tests
#     target_include_directories(lansend_tests
#         PRIVATE
#             ${CMAKE_SOURCE_DIR}/src
#             ${CMAKE_SOURCE_DIR}/test
#     )
    
#     # Compile definitions for tests (similar to main executable if needed)
#     if(WIN32)
#       target_compile_definitions(lansend_tests PRIVATE _WIN32_WINNT=0x0A00)
#     endif()

#     target_compile_definitions(lansend_tests
#       PRIVATE
#         $<$<CONFIG:Debug>:LANSEND_DEBUG>
#         $<$<CONFIG:Release>:LANSEND_RELEASE>
#         BOOST_ASIO_USE_STD_COROUTINE
#         BOOST_ASIO_HAS_CO_AWAIT
#     )

#     if (MSVC)
#         target_compile_options(lansend_tests PRIVATE /bigobj)
#     endif()

#     # Link libraries to the test executable
#     target_link_libraries(lansend_tests
#         PRIVATE
#             spdlog::spdlog_header_only
#             Boost::boost
#             Boost::system
#             Boost::filesystem
#             Boost::url
#             Boost::asio
#             Boost::beast
#             OpenSSL::Crypto
#             OpenSSL::SSL
#             nlohmann_json::nlohmann_json
#             GTest::gtest
#             GTest::gtest_main
#             PkgConfig::tomlplusplus
#     )

#     # Discover and add tests
#     include(GoogleTest)
#     if(TARGET GTest::gtest)
#         gtest_discover_tests(lansend_tests)
#     else()
#         message(WARNING "GTest target not found, gtest_discover_tests will not be effective for lansend_tests. Ensure GTest is properly found and configured.")
#     endif()
# endif()
