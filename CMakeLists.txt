cmake_minimum_required(VERSION 3.21.0)
project(lansend LANGUAGES CXX)

file(GLOB_RECURSE CORE_SOURCE src/core/*.cc)
file(GLOB_RECURSE CLI_SOURCE src/cli/*.cc)
file(GLOB_RECURSE BACKEND_SOURCE src/ipc/*.cc)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# policy set for FindBoost
if (POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# find vcpkg-installed
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED COMPONENTS system url filesystem asio beast uuid program_options)
find_package(OpenSSL 3.3.0 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)
pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)

# add_executable(lansend-cli ${CORE_SOURCE} ${CLI_SOURCE})
add_executable(lansend-backend ${CORE_SOURCE} ${BACKEND_SOURCE})

# Function to configure common settings for lansend targets
function(configure_lansend_target target)
  if(WIN32)
    target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0A00)
  endif()

  # definitions for Logger
  target_compile_definitions(${target}
    PRIVATE
      $<$<CONFIG:Debug>:LANSEND_DEBUG>
      $<$<CONFIG:Release>:LANSEND_RELEASE>
      BOOST_ASIO_USE_STD_COROUTINE
      BOOST_ASIO_HAS_CO_AWAIT
  )

  target_include_directories(${target}
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src/include
  )

  if(MSVC)
    target_compile_options(${target} PRIVATE /bigobj)
  endif()

  target_link_libraries(${target}
    PRIVATE
      spdlog::spdlog_header_only
      Boost::boost
      Boost::system
      Boost::filesystem
      Boost::url
      Boost::asio
      Boost::beast
      Boost::uuid
      Boost::program_options
      OpenSSL::Crypto
      OpenSSL::SSL
      nlohmann_json::nlohmann_json
  )
endfunction()

# configure_lansend_target(lansend-cli)
configure_lansend_target(lansend-backend)
