#!/bin/bash

mkdir Debug
cd Debug
rm -Rf *
cmake -DCMAKE_BUILD_TYPE=Debug ../
make -j`nproc`
cd ..

mkdir Release
cd Release
rm -Rf *
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j`nproc`
cd ..
