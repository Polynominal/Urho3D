#!/bin/bash
cd build_debug
make -j 5 
cd .. 

cd build_release
make -j 5 
cd .. 

cd build_release_crosscompile
make -j 5 
cd .. 

cd build_rel_debug
make -j 5 
cd .. 
echo "==== COMPLETED BUILD ===="


