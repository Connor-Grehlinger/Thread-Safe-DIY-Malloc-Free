#!/bin/bash
for ((i = 1; i <= 100; i++))
do
    echo ==================================================
    echo Running test $i
    ./thread_test_measurement
    echo ==================================================
   
done
