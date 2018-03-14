#!/bin/bash
for ((i = 1; i <= 1000; i++))
do
    echo ==================================================
    echo Running test $i
    ./thread_test_malloc_free_change_thread
    echo ==================================================

done
