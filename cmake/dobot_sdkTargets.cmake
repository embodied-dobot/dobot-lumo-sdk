if(CMAKE_VERSION VERSION_LESS 3.16)
  message(FATAL_ERROR "dobot_sdk requires CMake 3.16 or later.")
endif()

get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
if(_IMPORT_PREFIX STREQUAL "/")
  set(_IMPORT_PREFIX "")
endif()

if(NOT TARGET fastcdr)
  add_library(fastcdr SHARED IMPORTED)
  set_target_properties(fastcdr PROPERTIES
    IMPORTED_LOCATION "${_IMPORT_PREFIX}/lib/libfastcdr.so"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
    INTERFACE_LINK_LIBRARIES "Threads::Threads")
endif()

if(NOT TARGET fastdds)
  add_library(fastdds SHARED IMPORTED)
  set_target_properties(fastdds PROPERTIES
    IMPORTED_LOCATION "${_IMPORT_PREFIX}/lib/libfastdds.so"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
    INTERFACE_LINK_LIBRARIES "fastcdr;Threads::Threads")
endif()

if(NOT TARGET dobot_sdk)
  add_library(dobot_sdk STATIC IMPORTED)
  set_target_properties(dobot_sdk PROPERTIES
    IMPORTED_LOCATION "${_IMPORT_PREFIX}/lib/libdobot_sdk.a"
    INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include;${_IMPORT_PREFIX}/include/dobot/idl"
    INTERFACE_LINK_LIBRARIES "fastdds;fastcdr;Threads::Threads"
    LINKER_LANGUAGE CXX)
endif()

unset(_IMPORT_PREFIX)
