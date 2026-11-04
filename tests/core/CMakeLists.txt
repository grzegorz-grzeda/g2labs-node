# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(node_core_test)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../core/core.cmake)

target_sources(app PRIVATE
  src/fake_ports.c
  src/test_core.c
  src/test_known_networks.c
  src/test_wifi_failover.c
  src/test_broker.c
  src/test_broker_failover.c
  src/test_commands.c
)

# Host side of the fake ports' publish dump (needs the host's stdio).
target_sources(native_simulator INTERFACE src/publish_dump_bottom.c)
