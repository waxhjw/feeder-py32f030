cmake_minimum_required(VERSION 3.20)
project(feeder_py32f030 LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDK "${CMAKE_CURRENT_SOURCE_DIR}/.deps/PY32F0xx_Firmware_V1.5.0")
set(DEVICE "${SDK}/Drivers/CMSIS/Device/PY32F0xx")
set(STARTUP "${SDK}/Projects/PY32F030-STK/Example/GPIO/GPIO_Toggle/EIDE/startup_py32f030xx.s")
if(NOT EXISTS "${STARTUP}")
    message(FATAL_ERROR "Run tools/dev.py setup first: Puya SDK is missing")
endif()

add_executable(feeder_env_0.1.0
    src/main.c src/runtime.c
    "${DEVICE}/Source/system_py32f0xx.c"
    "${STARTUP}")
set_target_properties(feeder_env_0.1.0 PROPERTIES SUFFIX ".elf")
target_include_directories(feeder_env_0.1.0 PRIVATE board)
target_include_directories(feeder_env_0.1.0 SYSTEM PRIVATE
    "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(feeder_env_0.1.0 PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION)
target_compile_options(feeder_env_0.1.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -ffunction-sections -fdata-sections -g3 -Og
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-fstack-usage>")
target_link_options(feeder_env_0.1.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/feeder_env_0.1.0.map")
set_property(TARGET feeder_env_0.1.0 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET feeder_env_0.1.0 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:feeder_env_0.1.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_env_0.1.0.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:feeder_env_0.1.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_env_0.1.0.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:feeder_env_0.1.0>"
    VERBATIM)


# 只初始化并保持电机关闭；独立产物，不链接点动驱动和动作状态机。
add_executable(feeder_init_only_0.2.1 EXCLUDE_FROM_ALL
    src/init_only_main.c src/init_only_early.c src/runtime.c bsp/motor_off.c
    "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(feeder_init_only_0.2.1 PROPERTIES SUFFIX ".elf")
target_include_directories(feeder_init_only_0.2.1 PRIVATE board bsp)
target_include_directories(feeder_init_only_0.2.1 SYSTEM PRIVATE
    "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(feeder_init_only_0.2.1 PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION)
target_compile_options(feeder_init_only_0.2.1 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -ffunction-sections -fdata-sections -g3 -Og
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-fstack-usage>")
target_link_options(feeder_init_only_0.2.1 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/feeder_init_only_0.2.1.map")
set_property(TARGET feeder_init_only_0.2.1 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET feeder_init_only_0.2.1 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:feeder_init_only_0.2.1>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_init_only_0.2.1.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:feeder_init_only_0.2.1>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_init_only_0.2.1.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:feeder_init_only_0.2.1>"
    VERBATIM)

# 人工单次点动为独立目标；默认环境构建不包含、不覆盖它。
add_executable(feeder_motor_jog_0.2.0 EXCLUDE_FROM_ALL
    src/motor_bench_main.c src/runtime.c app/motor_bench.c bsp/motor_pulse.c
    "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(feeder_motor_jog_0.2.0 PROPERTIES SUFFIX ".elf")
target_include_directories(feeder_motor_jog_0.2.0 PRIVATE board app bsp)
target_include_directories(feeder_motor_jog_0.2.0 SYSTEM PRIVATE
    "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(feeder_motor_jog_0.2.0 PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION)
target_compile_options(feeder_motor_jog_0.2.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -ffunction-sections -fdata-sections -g3 -Og
    "$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-fstack-usage>")
target_link_options(feeder_motor_jog_0.2.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/feeder_motor_jog_0.2.0.map")
set_property(TARGET feeder_motor_jog_0.2.0 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET feeder_motor_jog_0.2.0 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:feeder_motor_jog_0.2.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_motor_jog_0.2.0.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:feeder_motor_jog_0.2.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_motor_jog_0.2.0.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:feeder_motor_jog_0.2.0>"
    VERBATIM)

# WBR3只接收诊断；复用0.2.1的早期关闭，不链接点动驱动。
add_executable(feeder_wbr_rx_0.3.0 EXCLUDE_FROM_ALL
    src/wbr_rx_main.c src/init_only_early.c src/runtime.c
    bsp/motor_off.c bsp/wbr_uart_rx.c app/wbr_rx_observer.c
    "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(feeder_wbr_rx_0.3.0 PROPERTIES SUFFIX ".elf")
target_include_directories(feeder_wbr_rx_0.3.0 PRIVATE board bsp app)
target_include_directories(feeder_wbr_rx_0.3.0 SYSTEM PRIVATE
    "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(feeder_wbr_rx_0.3.0 PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION)
get_target_property(INIT_COMPILE_OPTIONS feeder_init_only_0.2.1 COMPILE_OPTIONS)
target_compile_options(feeder_wbr_rx_0.3.0 PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(feeder_wbr_rx_0.3.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_rx_0.3.0.map")
set_property(TARGET feeder_wbr_rx_0.3.0 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET feeder_wbr_rx_0.3.0 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:feeder_wbr_rx_0.3.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_rx_0.3.0.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:feeder_wbr_rx_0.3.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_rx_0.3.0.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:feeder_wbr_rx_0.3.0>"
    VERBATIM)

# 心跳应答独立版本；复用已验证的接收队列及电机关闭，不替换0.3.0。
add_executable(feeder_wbr_heartbeat_0.4.0 EXCLUDE_FROM_ALL
    src/wbr_heartbeat_main.c src/init_only_early.c src/runtime.c
    bsp/motor_off.c bsp/wbr_uart_rx.c bsp/wbr_uart_hb.c
    app/tuya_frame.c app/wbr_heartbeat.c
    "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(feeder_wbr_heartbeat_0.4.0 PROPERTIES SUFFIX ".elf")
target_include_directories(feeder_wbr_heartbeat_0.4.0 PRIVATE board bsp app)
target_include_directories(feeder_wbr_heartbeat_0.4.0 SYSTEM PRIVATE
    "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(feeder_wbr_heartbeat_0.4.0 PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION)
target_compile_options(feeder_wbr_heartbeat_0.4.0 PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(feeder_wbr_heartbeat_0.4.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_heartbeat_0.4.0.map")
set_property(TARGET feeder_wbr_heartbeat_0.4.0 APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET feeder_wbr_heartbeat_0.4.0 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:feeder_wbr_heartbeat_0.4.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_heartbeat_0.4.0.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:feeder_wbr_heartbeat_0.4.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_heartbeat_0.4.0.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:feeder_wbr_heartbeat_0.4.0>"
    VERBATIM)

# PID初始化独立目标；256字节接收载荷支持DP1最大128字节加DP头。
# 旧0.4.0仍采用128默认值，旧产物必须核对冻结基线后才能作为回滚。
add_executable(feeder_wbr_init_0.5.0 EXCLUDE_FROM_ALL
    src/wbr_init_main.c src/init_only_early.c src/runtime.c
    bsp/motor_off.c bsp/wbr_uart_rx.c bsp/wbr_uart_tx.c
    app/tuya_frame.c app/wbr_init.c
    "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(feeder_wbr_init_0.5.0 PROPERTIES SUFFIX ".elf")
target_include_directories(feeder_wbr_init_0.5.0 PRIVATE board bsp app)
target_include_directories(feeder_wbr_init_0.5.0 SYSTEM PRIVATE
    "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(feeder_wbr_init_0.5.0 PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION TUYA_PAYLOAD_LIMIT=256U)
target_compile_options(feeder_wbr_init_0.5.0 PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(feeder_wbr_init_0.5.0 PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_init_0.5.0.map")
set_property(TARGET feeder_wbr_init_0.5.0 APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET feeder_wbr_init_0.5.0 POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:feeder_wbr_init_0.5.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_init_0.5.0.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:feeder_wbr_init_0.5.0>" "${CMAKE_CURRENT_BINARY_DIR}/feeder_wbr_init_0.5.0.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:feeder_wbr_init_0.5.0>"
    VERBATIM)

# 同一初始化实现增加可选的人工配网请求；旧版本编译时完全排除此能力。
get_target_property(WBR_INIT_SOURCES feeder_wbr_init_0.5.0 SOURCES)
set(WBR_PAIR_IMAGE feeder_wbr_pair_0.6.8)
add_executable(${WBR_PAIR_IMAGE} EXCLUDE_FROM_ALL ${WBR_INIT_SOURCES} app/wbr_pair.c)
set_target_properties(${WBR_PAIR_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${WBR_PAIR_IMAGE} PRIVATE board bsp app)
target_include_directories(${WBR_PAIR_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${WBR_PAIR_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1)
target_compile_options(${WBR_PAIR_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${WBR_PAIR_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${WBR_PAIR_IMAGE}.map")
set_property(TARGET ${WBR_PAIR_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${WBR_PAIR_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${WBR_PAIR_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${WBR_PAIR_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${WBR_PAIR_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${WBR_PAIR_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${WBR_PAIR_IMAGE}>"
    VERBATIM)

# LED专项：完整复用0.6.8通信/配网/喂食拦截，仅增加PB4显示测试。
set(LED_IMAGE feeder_led_test_0.7.0)
add_executable(${LED_IMAGE} EXCLUDE_FROM_ALL ${WBR_INIT_SOURCES} app/wbr_pair.c
    app/led_pattern.c bsp/led_tm1652.c)
set_target_properties(${LED_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${LED_IMAGE} PRIVATE board bsp app)
target_include_directories(${LED_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${LED_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1)
target_compile_options(${LED_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${LED_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${LED_IMAGE}.map")
set_property(TARGET ${LED_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${LED_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${LED_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${LED_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${LED_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${LED_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${LED_IMAGE}>"
    VERBATIM)

# 0.8.0完整保留LED/涂鸦诊断，仅增加独立5V电机的人工单次200ms入口。
set(PUMP_IMAGE feeder_pump_test_0.8.0)
get_target_property(LED_SOURCES ${LED_IMAGE} SOURCES)
add_executable(${PUMP_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES} app/pump_test.c bsp/motor_pulse.c)
set_target_properties(${PUMP_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${PUMP_IMAGE} PRIVATE board bsp app)
target_include_directories(${PUMP_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${PUMP_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1 FEEDER_PUMP_TEST=1)
target_compile_options(${PUMP_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${PUMP_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PUMP_IMAGE}.map")
set_property(TARGET ${PUMP_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${PUMP_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${PUMP_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${PUMP_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${PUMP_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${PUMP_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${PUMP_IMAGE}>"
    VERBATIM)

# 用户明确将电机改为TOP1长按3秒控制持续运行；保留LED/WBR，不链接旧SWD点动入口。
set(TOUCH_IMAGE feeder_top1_motor_0.9.0)
add_executable(${TOUCH_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES}
    app/touch_motor.c bsp/touch_motor_io.c bsp/motor_pulse.c)
set_target_properties(${TOUCH_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${TOUCH_IMAGE} PRIVATE board bsp app)
target_include_directories(${TOUCH_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${TOUCH_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1 FEEDER_TOUCH_MOTOR=1)
target_compile_options(${TOUCH_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${TOUCH_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TOUCH_IMAGE}.map")
set_property(TARGET ${TOUCH_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${TOUCH_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${TOUCH_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${TOUCH_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${TOUCH_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${TOUCH_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${TOUCH_IMAGE}>"
    VERBATIM)

# TOP1短按反转，独立0.9.1目标，0.9.0保留。
set(REVERSE_IMAGE feeder_top1_reverse_0.9.1)
add_executable(${REVERSE_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES}
    app/touch_motor.c bsp/touch_motor_io.c bsp/motor_pulse.c)
set_target_properties(${REVERSE_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${REVERSE_IMAGE} PRIVATE board bsp app)
target_include_directories(${REVERSE_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${REVERSE_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1 FEEDER_TOUCH_MOTOR=1 FEEDER_TOUCH_REVERSE=1)
target_compile_options(${REVERSE_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${REVERSE_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${REVERSE_IMAGE}.map")
set_property(TARGET ${REVERSE_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${REVERSE_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${REVERSE_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${REVERSE_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${REVERSE_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${REVERSE_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${REVERSE_IMAGE}>"
    VERBATIM)

# 仅新App目标开放DP101/102/103。
set(APP_IMAGE feeder_app_motor_0.10.0)
add_executable(${APP_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES}
    app/touch_motor.c bsp/touch_motor_io.c bsp/motor_pulse.c app/app_motor.c)
set_target_properties(${APP_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${APP_IMAGE} PRIVATE board bsp app)
target_include_directories(${APP_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${APP_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1 FEEDER_TOUCH_MOTOR=1 FEEDER_TOUCH_REVERSE=1 FEEDER_APP_MOTOR=1)
target_compile_options(${APP_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${APP_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${APP_IMAGE}.map")
set_property(TARGET ${APP_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${APP_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${APP_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${APP_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${APP_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${APP_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${APP_IMAGE}>"
    VERBATIM)

# 用户授权DP2定时近似测试，旧App目标不变。
set(QUICK_IMAGE feeder_quick_timed_0.10.1)
add_executable(${QUICK_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES}
    app/touch_motor.c bsp/touch_motor_io.c bsp/motor_pulse.c app/app_motor.c)
set_target_properties(${QUICK_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${QUICK_IMAGE} PRIVATE board bsp app)
target_include_directories(${QUICK_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${QUICK_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    TUYA_PAYLOAD_LIMIT=256U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1 FEEDER_TOUCH_MOTOR=1 FEEDER_TOUCH_REVERSE=1 FEEDER_APP_MOTOR=1 FEEDER_QUICK_TIMED=1)
target_compile_options(${QUICK_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${QUICK_IMAGE} PRIVATE
    -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${QUICK_IMAGE}.map")
set_property(TARGET ${QUICK_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/py32f030x8.ld")
add_custom_command(TARGET ${QUICK_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${QUICK_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${QUICK_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${QUICK_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${QUICK_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${QUICK_IMAGE}>"
    VERBATIM)

# MCU OTA完整继承0.10.1；旧目标及固定地址保持不变。
foreach(OTA_VERSION IN ITEMS 0.11.0 0.11.1 0.12.0 0.12.1 0.12.2 0.12.3 0.13.0 0.13.1 0.13.2 0.14.0 0.15.0 0.15.1 0.15.2)
    set(OTA_IMAGE feeder_ota_${OTA_VERSION})
    add_executable(${OTA_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES}
        app/touch_motor.c bsp/touch_motor_io.c bsp/motor_pulse.c app/app_motor.c
        app/ota_update.c app/ota_image.c bsp/ota_flash.c)
    set_target_properties(${OTA_IMAGE} PROPERTIES SUFFIX ".elf")
    target_include_directories(${OTA_IMAGE} PRIVATE board bsp app)
    target_include_directories(${OTA_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
    target_compile_definitions(${OTA_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
        TUYA_PAYLOAD_LIMIT=320U FEEDER_PAIR_MODE=1 FEEDER_LED_TEST=1 FEEDER_TOUCH_MOTOR=1
        FEEDER_TOUCH_REVERSE=1 FEEDER_APP_MOTOR=1 FEEDER_QUICK_TIMED=1 FEEDER_OTA_APP=1)
    if(OTA_VERSION STREQUAL "0.11.1")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_OTA_NEXT=1)
    endif()
    if(OTA_VERSION MATCHES "^0\\.12\\.")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_INPUT_DIAG=1)
        target_sources(${OTA_IMAGE} PRIVATE app/input_observer.c bsp/input_observer_io.c)
    endif()
    if(OTA_VERSION MATCHES "^0\\.12\\.[123]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_ADC_DIAG=1)
        target_sources(${OTA_IMAGE} PRIVATE bsp/adc_sampler.c)
    endif()
    if(OTA_VERSION MATCHES "^0\\.12\\.[23]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_ADC_RS_FIX=1)
    endif()
    if(OTA_VERSION STREQUAL "0.12.3")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_ADC_REARM=1)
    endif()
    if(OTA_VERSION MATCHES "^0\\.13\\." OR OTA_VERSION STREQUAL "0.14.0" OR OTA_VERSION MATCHES "^0\\.15\\.[012]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_PLAN_DIAG=1 FEEDER_INPUT_DIAG=1
            FEEDER_ADC_DIAG=1 FEEDER_ADC_RS_FIX=1 FEEDER_ADC_REARM=1)
        target_sources(${OTA_IMAGE} PRIVATE app/input_observer.c bsp/input_observer_io.c bsp/adc_sampler.c
            app/meal_plan.c app/product_store.c app/product_runtime.c bsp/plan_diag.c)
    endif()
    if(OTA_VERSION MATCHES "^0\\.13\\.[12]$" OR OTA_VERSION STREQUAL "0.14.0" OR OTA_VERSION MATCHES "^0\\.15\\.[012]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_STORE_PROFILE=1)
    endif()
    if(OTA_VERSION STREQUAL "0.13.2" OR OTA_VERSION STREQUAL "0.14.0" OR OTA_VERSION MATCHES "^0\\.15\\.[012]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_STORE_CACHED=1)
    endif()
    if(OTA_VERSION STREQUAL "0.14.0" OR OTA_VERSION MATCHES "^0\\.15\\.[012]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_LOCAL_UI=1)
        target_sources(${OTA_IMAGE} PRIVATE app/local_controls.c bsp/local_controls_io.c)
    endif()
    if(OTA_VERSION MATCHES "^0\\.15\\.[012]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_PLAN_APP=1)
        target_sources(${OTA_IMAGE} PRIVATE app/plan_app.c)
    endif()
    if(OTA_VERSION MATCHES "^0\\.15\\.[12]$")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_PLAN_APP_EMPTY=1)
    endif()
    if(OTA_VERSION STREQUAL "0.15.2")
        target_compile_definitions(${OTA_IMAGE} PRIVATE FEEDER_PLAN_APP_TABLE=1)
    endif()
    target_compile_options(${OTA_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
    target_link_options(${OTA_IMAGE} PRIVATE -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
        -nostartfiles --specs=nano.specs --specs=nosys.specs
        "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-app.ld" -Wl,--wrap=SystemInit
        -Wl,--gc-sections -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${OTA_IMAGE}.map")
    set_property(TARGET ${OTA_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-app.ld")
    add_custom_command(TARGET ${OTA_IMAGE} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${OTA_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${OTA_IMAGE}.hex"
        COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${OTA_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${OTA_IMAGE}.bin"
        COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${OTA_IMAGE}>" VERBATIM)
endforeach()
set(BOOT_IMAGE feeder_ota_boot_1.0.0)
set(CYCLE_BENCH_IMAGE feeder_cycle_diag_0.15.3)
add_executable(${CYCLE_BENCH_IMAGE} EXCLUDE_FROM_ALL
    src/cycle_bench_main.c src/init_only_early.c src/runtime.c
    app/cycle_bench.c app/feed_cycle.c app/input_observer.c app/led_pattern.c
    bsp/input_observer_io.c bsp/led_tm1652.c bsp/motor_off.c
    "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(${CYCLE_BENCH_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${CYCLE_BENCH_IMAGE} PRIVATE board bsp app)
target_include_directories(${CYCLE_BENCH_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${CYCLE_BENCH_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    FEEDER_OTA_APP=1 FEEDER_CYCLE_BENCH=1)
target_compile_options(${CYCLE_BENCH_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${CYCLE_BENCH_IMAGE} PRIVATE -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-app.ld" -Wl,--wrap=SystemInit
    -Wl,--gc-sections -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${CYCLE_BENCH_IMAGE}.map")
set_property(TARGET ${CYCLE_BENCH_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-app.ld")
add_custom_command(TARGET ${CYCLE_BENCH_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${CYCLE_BENCH_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${CYCLE_BENCH_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${CYCLE_BENCH_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${CYCLE_BENCH_IMAGE}.bin"
    VERBATIM)

# 联网CN6台架：保留真实DP/校时/计划存储，替换为无驱动的执行边界。
set(CYCLE_MANUAL_DP "104" CACHE STRING "Custom quantity DP for manual-feed bench only")
if(NOT CYCLE_MANUAL_DP MATCHES "^[0-9]+$" OR CYCLE_MANUAL_DP LESS 104 OR CYCLE_MANUAL_DP GREATER 255)
    message(FATAL_ERROR "CYCLE_MANUAL_DP must be a custom DP ID in 104..255")
endif()
foreach(CYCLE_NET_VERSION IN ITEMS 0.15.4 0.15.5 0.15.6)
set(CYCLE_NET_IMAGE feeder_cycle_net_diag_${CYCLE_NET_VERSION})
add_executable(${CYCLE_NET_IMAGE} EXCLUDE_FROM_ALL ${LED_SOURCES}
    app/cycle_bench.c app/feed_cycle.c app/input_observer.c bsp/input_observer_io.c
    app/meal_plan.c app/product_store.c app/product_runtime.c app/plan_app.c
    bsp/plan_diag.c bsp/ota_flash.c bsp/cycle_bench_net.c)
set_target_properties(${CYCLE_NET_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${CYCLE_NET_IMAGE} PRIVATE board bsp app)
target_include_directories(${CYCLE_NET_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${CYCLE_NET_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION
    FEEDER_CYCLE_NET=1 FEEDER_APP_SLOT=1 FEEDER_LED_TEST=1 FEEDER_INPUT_DIAG=1 FEEDER_PAIR_MODE=1
    FEEDER_LOCAL_UI=1 FEEDER_APP_MOTOR=1 FEEDER_PLAN_DIAG=1 FEEDER_STORE_CACHED=1
    FEEDER_PLAN_APP=1 FEEDER_PLAN_APP_EMPTY=1 FEEDER_PLAN_APP_TABLE=1 TUYA_PAYLOAD_LIMIT=320U)
if(CYCLE_NET_VERSION STREQUAL "0.15.5")
    target_compile_definitions(${CYCLE_NET_IMAGE} PRIVATE FEEDER_CYCLE_MANUAL_DP=${CYCLE_MANUAL_DP})
endif()
if(CYCLE_NET_VERSION STREQUAL "0.15.6")
    target_compile_definitions(${CYCLE_NET_IMAGE} PRIVATE FEEDER_CYCLE_MANUAL_DP=104 FEEDER_CYCLE_SUITE=1 FEEDER_OTA_APP=1)
    target_sources(${CYCLE_NET_IMAGE} PRIVATE app/bench_panel.c bsp/bench_buzzer.c app/ota_update.c app/ota_image.c)
endif()
target_compile_options(${CYCLE_NET_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${CYCLE_NET_IMAGE} PRIVATE -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -nostartfiles --specs=nano.specs --specs=nosys.specs
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-app.ld" -Wl,--wrap=SystemInit
    -Wl,--gc-sections -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${CYCLE_NET_IMAGE}.map")
set_property(TARGET ${CYCLE_NET_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-app.ld")
add_custom_command(TARGET ${CYCLE_NET_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${CYCLE_NET_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${CYCLE_NET_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${CYCLE_NET_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${CYCLE_NET_IMAGE}.bin"
    VERBATIM)
endforeach()
add_executable(${BOOT_IMAGE} EXCLUDE_FROM_ALL src/ota_boot_main.c src/init_only_early.c src/runtime.c
    bsp/motor_off.c bsp/ota_flash.c app/ota_image.c "${DEVICE}/Source/system_py32f0xx.c" "${STARTUP}")
set_target_properties(${BOOT_IMAGE} PROPERTIES SUFFIX ".elf")
target_include_directories(${BOOT_IMAGE} PRIVATE board bsp app)
target_include_directories(${BOOT_IMAGE} SYSTEM PRIVATE "${SDK}/Drivers/CMSIS/Include" "${DEVICE}/Include")
target_compile_definitions(${BOOT_IMAGE} PRIVATE PY32F030x8 FORBID_VECT_TAB_MIGRATION)
target_compile_options(${BOOT_IMAGE} PRIVATE ${INIT_COMPILE_OPTIONS})
target_link_options(${BOOT_IMAGE} PRIVATE -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft
    -nostartfiles --specs=nano.specs --specs=nosys.specs "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-boot.ld"
    -Wl,--wrap=SystemInit -Wl,--gc-sections -Wl,--print-memory-usage "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${BOOT_IMAGE}.map")
set_property(TARGET ${BOOT_IMAGE} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/ota-boot.ld")
add_custom_command(TARGET ${BOOT_IMAGE} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${BOOT_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${BOOT_IMAGE}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${BOOT_IMAGE}>" "${CMAKE_CURRENT_BINARY_DIR}/${BOOT_IMAGE}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${BOOT_IMAGE}>" VERBATIM)
