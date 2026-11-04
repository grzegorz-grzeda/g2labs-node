# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(g2labs_node)

include(${CMAKE_CURRENT_SOURCE_DIR}/core/core.cmake)

target_sources(app PRIVATE
  src/main.c
  src/shell.c
  src/adapters/image_mcuboot.c
  src/adapters/memory.c
  src/adapters/mqtt.c
  src/adapters/settings.c
  src/adapters/system.c
  src/adapters/wifi.c
)
