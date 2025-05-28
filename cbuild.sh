#!/bin/bash

# ----------------------------------------------------------------------------
#
#                    CBuild - Simple build script for C projects
#
# ----------------------------------------------------------------------------
#
# Usage: ./cbuild.sh <file> [options]
# Options:
#   -output <dir>    : Specify output directory (default: build)
#   -compiler <name> : Specify compiler to use (default: gcc)
#   -run             : Run the executable after building
#   -silent          : Suppress build output logging
#   -clear           : Clear the console before building
#   -help            : Display this help message
#
# ----------------------------------------------------------------------------

# Parse arguments with defaults
output="build"
compiler="gcc"
run=false
silent=false
clear=false
help=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -output) output="$2"; shift 2 ;;
        -compiler) compiler="$2"; shift 2 ;;
        -run) run=true; shift ;;
        -silent) silent=true; shift ;;
        -clear) clear=true; shift ;;
        -help) help=true; shift ;;
        *) target="$1"; shift ;;
    esac
done

# Clear console if requested
$clear && clear

# Show help
if $help; then
    echo
    echo "Usage: ./cbuild.sh main.c [-output build] [-compiler gcc] [-run] [-silent] [-clear] [-help]"
    echo
    echo " - output: Output directory for build files (default: build)."
    echo " - compiler: Specify the C compiler to use (default: gcc)."
    echo " - run: Run the compiled executable after building."
    echo " - silent: Do not print build commands and output."
    echo " - clear: Clear the console before running."
    echo " - help: Print this message."
    echo
    exit 0
fi

# Ensure target is specified and exists
if [[ -z "$target" ]]; then
    echo "[error] No target file specified. See -help for usage."
    exit 1
fi

if [[ ! -f "$target" ]]; then
    echo "[error] File not found: $target"
    exit 1
fi

# Ensure compiler exists
if ! command -v "$compiler" &> /dev/null; then
    echo "[error] Compiler not found: $compiler"
    exit 1
fi

# Extract path and name components
target_dir=$(dirname "$target")
target_name=$(basename "$target")
target_name_noext="${target_name%.*}"

output_dir="$target_dir/$output"
output_exe="$output_dir/$target_name_noext"

# Extract build flags
compiler_flags=""
while IFS= read -r line; do
    if [[ "$line" =~ ^//[[:space:]]cbuild:\  ]]; then
        compiler_flags+=" ${line#// cbuild: }"
    else
        break
    fi
done < "$target"

# Create output directory if it doesn't exist
mkdir -p "$output_dir"

# Build command
build_command="$compiler \"$target\" -o \"$output_exe\" $compiler_flags"

# Time before build
start_time=$(date +%s%3N)

# Run build
cd "$target_dir" || exit 1
$silent || echo "[run] $build_command"
eval $build_command
exit_code=$?
cd - > /dev/null || exit 1

# Time after build
end_time=$(date +%s%3N)
build_time=$((end_time - start_time))

# Result
if [[ $exit_code -ne 0 ]]; then
    echo "[error] Build failed: $exit_code"
    exit $exit_code
fi

$silent || echo "[log] Build success: $output_exe (took ${build_time}ms)"

# Run if requested
if $run; then
    $silent || echo "[run] $output_exe"
    "$output_exe"
fi
