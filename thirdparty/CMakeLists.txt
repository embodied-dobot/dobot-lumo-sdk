find_package(Threads REQUIRED)

set(DOBOT_FAST_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/${TARGET_ARCH}")
set(DOBOT_SDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/${TARGET_ARCH}")

add_library(fastcdr SHARED IMPORTED GLOBAL)
set_target_properties(fastcdr PROPERTIES
  IMPORTED_LOCATION "${DOBOT_FAST_LIB_DIR}/libfastcdr.so"
  IMPORTED_NO_SONAME TRUE
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  INTERFACE_LINK_LIBRARIES "Threads::Threads")
target_link_directories(fastcdr INTERFACE "${DOBOT_FAST_LIB_DIR}")

add_library(fastdds SHARED IMPORTED GLOBAL)
set_target_properties(fastdds PROPERTIES
  IMPORTED_LOCATION "${DOBOT_FAST_LIB_DIR}/libfastdds.so"
  IMPORTED_NO_SONAME TRUE
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  INTERFACE_LINK_LIBRARIES "fastcdr;Threads::Threads")
target_link_directories(fastdds INTERFACE "${DOBOT_FAST_LIB_DIR}")

set(DOBOT_LOG_LIB "${DOBOT_SDK_LIB_DIR}/libdobot_log.so")
if(NOT EXISTS "${DOBOT_LOG_LIB}")
  message(FATAL_ERROR "未找到 ${DOBOT_LOG_LIB}。请先在 dobot-sdk-src 编译并执行 stage_public_sdk。")
endif()
add_library(dobot_log SHARED IMPORTED GLOBAL)
set_target_properties(dobot_log PROPERTIES
  IMPORTED_LOCATION "${DOBOT_LOG_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  INTERFACE_LINK_LIBRARIES "Threads::Threads")
target_link_directories(dobot_log INTERFACE "${DOBOT_SDK_LIB_DIR}")
