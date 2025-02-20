#/usr/bin/bash

args="-Wall -Wextra -Werror -Wpedantic -std=c++23 -D _DEBUG -D _LOCAL -lstdc++_libbacktrace"

file=$1
binary=$2
compiler=$3
echo=$4
# append all additional arguments
args="$args ${@:5}"

function usage {

echo "Usage: compile.bash <file> <binary> <compiler> <echo> [args]"
exit 1

}

if [ -z $file ]; then
    echo "No file specified"
    usage
fi

if [ -z $binary ]; then
    echo "No binary specified"
    usage
fi


if [ -z $compiler ]; then
    compiler="g++"
fi

if [ $compiler = "gcc" ]; then
    compiler="g++"
fi

if [ $compiler = "clang" ]; then
    compiler="clang++"
fi

if [ $compiler != "g++" ] && [ $compiler != "clang++" ]; then
    echo "Invalid compiler $compiler"
    exit 1
fi

if [ $compiler = "clang" ]; then
    compiler="clang++"
    args="$args -lstdc++"
fi

# if the string is echo
if [ $echo = "echo" ]; then
    echo "==== Start of compile commands ===="
    echo "  $compiler $file -o $binary $args"
    echo "====  End of compile commands  ===="
fi

if [ $echo = "only" ]; then
    echo "$compiler $file -o $binary $args"
    exit 0
fi

$compiler $file -o $binary $args
