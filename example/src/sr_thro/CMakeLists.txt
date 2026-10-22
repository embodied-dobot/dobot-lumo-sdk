dobot_add_example(sr_thro_rpc_call rpc_call.cpp)

if(DOBOT_FASTDDS_LINK_OK)
  dobot_add_example(sr_thro_dds_echo dds_echo.cpp)
  dobot_add_example(sr_thro_dds_publish_cmd dds_publish_cmd.cpp)
  dobot_add_example(sr_thro_upper_state_rate upper_state_rate.cpp)
  dobot_add_example(sr_thro_upper_arm_swing upper_arm_swing.cpp)
else()
  message(WARNING
    "当前系统链不上 thirdparty/lib/${TARGET_ARCH} 里的 libfastdds.so（需要 GLIBC 2.34 和 GLIBCXX 3.4.30 或更高）。"
    "已跳过 sr_thro_dds_echo 和 sr_thro_dds_publish_cmd，sr_thro_rpc_call 仍可编译。")
endif()

set(DOBOT_MODEL sr_thro)
