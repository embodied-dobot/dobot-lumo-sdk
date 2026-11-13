function(dobot_add_example target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE dobot_sdk)
  if(UNIX AND NOT APPLE)
    # Rebuilt programs are in build/bin; two levels up is the package root.
    # The legacy RPATH also applies to libfastcdr.so.2, which libfastdds.so depends on.
    set_target_properties(${target} PROPERTIES
      BUILD_RPATH "\$ORIGIN/../../lib/${TARGET_ARCH};\$ORIGIN/../../thirdparty/lib/${TARGET_ARCH}")
    target_link_options(${target} PRIVATE "LINKER:--disable-new-dtags")
  endif()
endfunction()

add_subdirectory(src/sr_3588)
add_subdirectory(src/sr_thor)
