function(dobot_add_example target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE dobot_sdk)
  if(UNIX AND NOT APPLE)
    # 重编的程序在 build/bin，向上两级是包根。
    # 旧式 RPATH 对 libfastdds.so 再依赖的 libfastcdr.so.2 也生效。
    set_target_properties(${target} PROPERTIES
      BUILD_RPATH "\$ORIGIN/../../lib/${TARGET_ARCH};\$ORIGIN/../../thirdparty/lib/${TARGET_ARCH}")
    target_link_options(${target} PRIVATE "LINKER:--disable-new-dtags")
  endif()
endfunction()

add_subdirectory(src/sr_3588)
add_subdirectory(src/sr_thro)
