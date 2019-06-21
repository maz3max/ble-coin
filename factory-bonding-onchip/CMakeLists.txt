cmake_minimum_required(VERSION 3.8.2)

set( ENV{ZEPHYR_BASE} ${CMAKE_SOURCE_DIR}/../zephyr )
set( ENV{BOARD} nrf52_coin )
set( BOARD_ROOT ${CMAKE_SOURCE_DIR}/../coin )
set( ENV{ZEPHYR_TOOLCHAIN_VARIANT} zephyr )
set( ENV{ZEPHYR_SDK_INSTALL_DIR} /opt/zephyr-sdk/ )

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(coin-bonding)
zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/subsys/bluetooth/host
)


target_sources(app PRIVATE src/main.c)
