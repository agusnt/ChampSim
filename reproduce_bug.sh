#!/bin/bash

#rm -r bin
#make clean
./config.sh berti.json
make
#./bin/no --warmup_instructions 50000000 --simulation_instructions 200000000\
./bin/no --warmup_instructions 0 --simulation_instructions 10000000\
    /home/ant_uz/HW_Prefetch/Trazas/spec2k17/602.gcc_s-1850B.champsimtrace.xz
