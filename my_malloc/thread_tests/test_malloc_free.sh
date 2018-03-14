#!/bin/bash
for ((i = 0; i < 10; i++))
do
    echo ==================================================
    echo Running test $i
    ./thread_test_malloc_free
    echo ==================================================
done
