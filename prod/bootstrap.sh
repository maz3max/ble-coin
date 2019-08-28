#!/bin/bash
mkdir build
mkdir build/pre
mkdir build/coin
mkdir build/central
cd    build/pre
cmake ../../../factory-bonding-onchip
cd    ../coin
cmake ../../../coin
cd    ../central
cmake ../../../central-onchip

