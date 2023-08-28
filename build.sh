#!/bin/bash
script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

mkdir -p ${script_dir}/Build
pushd ${script_dir}/Build
gcc ${script_dir}/asan_example.cpp -g -fsanitize=address -o asan_example
popd


