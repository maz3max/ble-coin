cmake_minimum_required(VERSION 3.8.2)

IF(NOT DEFINED ENV{ZEPHYR_BASE})
  set( ENV{ZEPHYR_BASE} "${CMAKE_SOURCE_DIR}/../zephyr" )
ENDIF()
IF(NOT DEFINED ENV{BOARD})
  set( ENV{BOARD} nrf52840_pca10059 )
ENDIF()
IF(NOT DEFINED ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  set( ENV{ZEPHYR_TOOLCHAIN_VARIANT} zephyr )
ENDIF()
IF(NOT DEFINED ENV{ZEPHYR_SDK_INSTALL_DIR})
  set( ENV{ZEPHYR_SDK_INSTALL_DIR} /opt/zephyr-sdk/ )
ENDIF()

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(central-onchip)
zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/subsys/bluetooth/host
  ../BLAKE2/ref
)


target_sources(app PRIVATE src/main.c src/leds.c src/helper.c src/spaceauth.c ../BLAKE2/ref/blake2s-ref.c)
