#! /usr/bin/env bash

if [ "$1" = --description ]; then
  echo "compile a .cu file into a shared library"
  exit 0
fi

if [ "$1" = --options ]; then
  echo "--description --options"
  exit 0
fi

source "$PLUMED_ROOT"/src/config/compile_options.sh

if [ $# != 1 ] || [[ "$1" != *.cu ]]; then
  echo "ERROR"
  echo "type 'mklib file.cu'"
  exit 1
fi

file="$1"
obj="${file%%.cu}".o
lib="${file%%.cu}".$soext

if [ ! -f "$file" ]; then
  echo "ERROR: I cannot find file $file"
  exit 1
fi

rm -f "$obj" "$lib"

#nvcc "$2" -Xcompiler -fPIC -c -o "$kernel"
compile="nvcc -ccbin ${compile}"

if [[ ${SILENT_CUDA_COMPILATION} ]]; then
  #echo "disabled warning"
  compile=${compile//-Wall/}
  compile=${compile//-pedantic/}
  #-w suppress the warnings
  compile=${compile/-c /-w -c }
fi

for opt in -W -pedantic -f; do
  compile=${compile//${opt}/-Xcompiler ${opt}}
done

link_command=$link_uninstalled

if [[ -z ${link_command:+x} ]]; then
  link_command=$link_installed
fi

link_command="nvcc -shared${link_command#*-shared}"
link_command=${link_command/-rdynamic/-Xcompiler -rdynamic}
link_command=${link_command/-Wl,/-Xlinker }
#link_command=${link_command/-fopenmp/-Xcompiler -fopenmp}
for opt in -f; do
  link_command=${link_command//${opt}/-Xcompiler ${opt}}
done

eval "$compile" "$obj" "$file" && \
eval "$compile" "ndReduction1D.o" "ndReduction1D.cu" && \
eval "$link_command" "$lib" "ndReduction1D.o" "$obj"

