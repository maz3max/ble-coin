cmake_minimum_required(VERSION 3.8.2)

set( ENV{ZEPHYR_BASE} ${CMAKE_SOURCE_DIR}/../zephyr )
set( ENV{BOARD} nrf52840_pca10059 )
set( ENV{ZEPHYR_TOOLCHAIN_VARIANT} zephyr )
set( ENV{ZEPHYR_SDK_INSTALL_DIR} /opt/zephyr-sdk/ )

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(central-onchip)
zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/subsys/bluetooth/host
  ../BLAKE2/ref
)


target_sources(app PRIVATE src/main.c src/spaceauth.c ../BLAKE2/ref/blake2s-ref.c)
