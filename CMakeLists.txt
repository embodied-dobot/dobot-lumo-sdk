cmake_minimum_required(VERSION 3.16)
project(dobot_sdk VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_EXAMPLES "Build examples" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
endif()

include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

if(NOT TARGET_ARCH)
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
    OUTPUT_VARIABLE DOBOT_DUMPMACHINE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(DOBOT_DUMPMACHINE MATCHES "aarch64|arm64")
    set(TARGET_ARCH aarch64)
  else()
    set(TARGET_ARCH x86_64)
  endif()
endif()
message(STATUS "dobot-sdk architecture: ${TARGET_ARCH}")

set(DOBOT_SDK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/${TARGET_ARCH}/libdobot_sdk.a")
if(NOT EXISTS "${DOBOT_SDK_LIB}")
  message(FATAL_ERROR "未找到 ${DOBOT_SDK_LIB}。请确认 SDK 包完整，且当前编译器的目标架构与 lib/aarch64 或 lib/x86_64 一致。")
endif()

add_subdirectory(thirdparty)

set(DOBOT_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/${TARGET_ARCH}")
set(FASTDDS_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/${TARGET_ARCH}")
file(WRITE "${CMAKE_BINARY_DIR}/fastdds_link_test.cpp"
  "#include <fastdds/dds/domain/DomainParticipantFactory.hpp>\nint main(){return eprosima::fastdds::dds::DomainParticipantFactory::get_instance()==nullptr;}\n")
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" -std=c++17
    "-I${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include"
    "${CMAKE_BINARY_DIR}/fastdds_link_test.cpp"
    "-L${FASTDDS_LIB_DIR}"
    -lfastdds -lfastcdr
    -o "${CMAKE_BINARY_DIR}/fastdds_link_test"
  RESULT_VARIABLE DOBOT_FASTDDS_LINK_RC
  OUTPUT_QUIET ERROR_QUIET)
set(DOBOT_FASTDDS_LINK_OK FALSE)
if(DOBOT_FASTDDS_LINK_RC EQUAL 0)
  set(DOBOT_FASTDDS_LINK_OK TRUE)
endif()

find_package(Threads REQUIRED)

add_library(dobot_sdk STATIC IMPORTED GLOBAL)
set_target_properties(dobot_sdk PROPERTIES
  IMPORTED_LOCATION "${DOBOT_SDK_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include;${CMAKE_CURRENT_SOURCE_DIR}/include/dobot/idl;${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include"
  INTERFACE_LINK_LIBRARIES "Threads::Threads;fastdds;fastcdr;dobot_log")

if(BUILD_EXAMPLES)
  add_subdirectory(example)
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY thirdparty/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# 安装前缀把两类库收进同一个 lib/。导出的 dobot_sdkTargets.cmake 按这个位置查找。
install(DIRECTORY "${DOBOT_LIB_DIR}/" DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY "${FASTDDS_LIB_DIR}/" DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES cmake/dobot_sdkTargets.cmake DESTINATION lib/cmake/dobot_sdk)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  dobot_sdkConfigVersion.cmake
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)
configure_file(cmake/dobot_sdkConfig.cmake.in dobot_sdkConfig.cmake @ONLY)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/dobot_sdkConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/dobot_sdkConfigVersion.cmake"
  DESTINATION lib/cmake/dobot_sdk)
