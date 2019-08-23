#!/bin/bash
mkdir build
mkdir build/pre
mkdir build/coin
mkdir build/central
cd build/pre
cmake ../../factory-bonding-onchip
cd ../../build/coin
cmake ../../coin
cd ../../build/central
cmake ../../central-onchip

