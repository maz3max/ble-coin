cmake_minimum_required(VERSION 3.8.2)

IF(NOT DEFINED ENV{ZEPHYR_BASE})
  set( ENV{ZEPHYR_BASE} "${CMAKE_SOURCE_DIR}/../zephyr" )
ENDIF()
IF(NOT DEFINED ENV{BOARD})
  set( ENV{BOARD} nrf52_coin )
  set( BOARD_ROOT ${CMAKE_SOURCE_DIR} )
ENDIF()
IF(NOT DEFINED ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  set( ENV{ZEPHYR_TOOLCHAIN_VARIANT} zephyr )
ENDIF()
IF(NOT DEFINED ENV{ZEPHYR_SDK_INSTALL_DIR})
  set( ENV{ZEPHYR_SDK_INSTALL_DIR} /opt/zephyr-sdk/ )
ENDIF()

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(coin)
zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/subsys/bluetooth/host
  ../BLAKE2/ref
)


target_sources(app PRIVATE src/main.c src/bas.c src/io.c src/spaceauth.c ../BLAKE2/ref/blake2s-ref.c)
