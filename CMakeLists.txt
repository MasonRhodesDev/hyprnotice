cmake_minimum_required(VERSION 3.19)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VERSION)

project(
  hyprnotice
  DESCRIPTION "Notification daemon for Hyprland with action-retaining inbox"
  VERSION ${VERSION})

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring hyprnotice in Debug")
  add_compile_definitions(HYPRNOTICE_DEBUG)
  set(BUILD_TESTING ON)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring hyprnotice in Release")
  set(BUILD_TESTING OFF)
endif()

add_compile_definitions(HYPRNOTICE_VERSION="${VERSION}")

include_directories(.)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
add_compile_options(
  -Wall -Wextra -Wpedantic
  -Wno-unused-parameter
  -Wno-unused-value
  -Wno-missing-field-initializers
  -Wno-narrowing -Wno-pointer-arith)

configure_file(systemd/hyprnotice.service.in systemd/hyprnotice.service @ONLY)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
  hyprutils>=0.11.0
  hyprlang>=0.6.0
  hyprtoolkit
  hyprgraphics
  pixman-1
  libdrm
  sdbus-c++>=2.0.0)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(hyprnotice ${SRCFILES})
target_link_libraries(hyprnotice PRIVATE rt Threads::Threads PkgConfig::deps)

# CLI client lives in cli/ so its sources don't get pulled into the daemon
# binary's GLOB. Only links what it actually uses (sdbus-c++).
file(GLOB CLI_SRCFILES CONFIGURE_DEPENDS "cli/*.cpp")
add_executable(hyprnotice-ctl ${CLI_SRCFILES})
pkg_check_modules(cli_deps REQUIRED IMPORTED_TARGET sdbus-c++>=2.0.0)
target_link_libraries(hyprnotice-ctl PRIVATE PkgConfig::cli_deps)
target_compile_features(hyprnotice-ctl PRIVATE cxx_std_23)

install(TARGETS hyprnotice hyprnotice-ctl)
install(FILES ${CMAKE_BINARY_DIR}/systemd/hyprnotice.service
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user
  OPTIONAL)

if(BUILD_TESTING)
  enable_testing()
  find_package(GTest CONFIG)
  if(GTest_FOUND)
    include(GoogleTest)
    file(GLOB_RECURSE TESTFILES CONFIGURE_DEPENDS "tests/*.cpp")
    if(TESTFILES)
      add_executable(hyprnotice_tests ${TESTFILES})
      target_link_libraries(hyprnotice_tests PRIVATE PkgConfig::deps GTest::gtest_main)
      gtest_discover_tests(hyprnotice_tests)
    endif()
  endif()
endif()
