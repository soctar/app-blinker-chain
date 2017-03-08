#! /bin/bash

make bld/gcc/depclean
make bld/gcc/clean
make VERBOSE=1 bld/gcc/all
make bld/gcc/prog
