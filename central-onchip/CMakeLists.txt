cmake_minimum_required(VERSION 3.8.2)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(central-onchip)
zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/subsys/bluetooth/host
  ../BLAKE2/ref
)


target_sources(app PRIVATE src/main.c src/leds.c src/helper.c src/spaceauth.c ../BLAKE2/ref/blake2s-ref.c)
