dobot_add_example(sr_3588_rpc_call rpc_call.cpp)
dobot_add_example(sr_3588_motion_demo motion_demo.cpp)
dobot_add_example(sr_3588_walk_demo walk_demo.cpp)

if(DOBOT_FASTDDS_LINK_OK)
  dobot_add_example(sr_3588_dds_echo dds_echo.cpp)
  dobot_add_example(sr_3588_dds_publish_cmd dds_publish_cmd.cpp)
  dobot_add_example(sr_3588_tts_demo tts_demo.cpp)
  dobot_add_example(sr_3588_upper_state_rate upper_state_rate.cpp)
  dobot_add_example(sr_3588_upper_arm_swing upper_arm_swing.cpp)
else()
  message(WARNING
    "The current system cannot link libfastdds.so in thirdparty/lib/${TARGET_ARCH} (GLIBC 2.34 and GLIBCXX 3.4.30 or later are required)."
    "The DDS examples are skipped; sr_3588_rpc_call, sr_3588_motion_demo and sr_3588_walk_demo are still built.")
endif()

set(DOBOT_MODEL sr_3588)
