# SPDX-License-Identifier: MIT
#
# The Node core: all decision-making, plain C, no Zephyr Wi-Fi, Bluetooth,
# MQTT or MCUboot APIs. Included by the firmware and by tests/core.
set(NODE_CORE_DIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(app PRIVATE ${NODE_CORE_DIR}/include)
target_sources(app PRIVATE
  ${NODE_CORE_DIR}/src/core.c
  ${NODE_CORE_DIR}/src/json.c
)
