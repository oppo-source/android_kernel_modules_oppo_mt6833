#!/bin/bash

source kernel/oplus/build/oplus_setup.sh $1
init_build_environment
source kernel/oplus/build/oplus_build_function.sh
build_start_time
build_dt_cmd
build_end_time

