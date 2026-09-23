#!/usr/bin/env bash
#
# Builds MemoryMonitorDemo in every configuration, runs it, and stores the normalized
# output as a snapshot.
# Use it to prove a refactor changed nothing: capture before, capture after, compare.
#
#   ./scripts/capture-output.sh step0            capture a snapshot named step0
#   ./scripts/capture-output.sh step1 step0      capture step1 and diff it against step0
#
# Heap addresses move on every run, so they are replaced with 0xADDR before saving.
# The "% N = 0" remainder is kept, because that is the part that actually asserts something.

set -u

LABEL="${1:-}"
COMPARE_TO="${2:-}"

if [ -z "$LABEL" ]; then
    echo "usage: $0 <label> [label-to-compare-against]" >&2
    exit 2
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR" || exit 1

command -v cmake > /dev/null 2>&1 || { echo "cmake not found on PATH" >&2; exit 1; }

OUT_DIR="baselines/$LABEL"
mkdir -p "$OUT_DIR"

# config|platform|path to the exe that configuration produces
CONFIGS=(
    "Debug|x64|bin/x64/Debug/MemoryMonitorDemo.exe"
    "Debug|Win32|bin/Win32/Debug/MemoryMonitorDemo.exe"
    "Release|x64|bin/x64/Release/MemoryMonitorDemo.exe"
    "Release|Win32|bin/Win32/Release/MemoryMonitorDemo.exe"
)

# Strip color codes and pin down the addresses that move between runs
normalize() {
    sed -E 's/\x1b\[[0-9;]*m//g; s/address = 0x[0-9a-fA-F]+/address = 0xADDR/g'
}

# One build tree per platform, each holding both configurations. Written once and reused.
#
# The preset names are the platform names. CMakePresets.json is where the generator, the
# toolset and the build directory are pinned, and that pinning is the point: a snapshot is
# only worth comparing against another one built by the same compiler.
configure_tree() {
    local platform="$1"
    [ -f "build/cmake-$platform/CMakeCache.txt" ] && return 0
    cmake --preset "$platform" > "$OUT_DIR/configure-$platform.log" 2>&1
}

# Throws away one target's object files and its output, so the next build really compiles it.
# Everything else in the tree is left alone.
clean_target() {
    local platform="$1"
    local config="$2"
    local target="$3"
    rm -rf "build/cmake-$platform/$target.dir/$config"
    rm -f "bin/$platform/$config/$target.exe" "bin/$platform/$config/$target.lib"
}

failed=0

for entry in "${CONFIGS[@]}"; do
    IFS='|' read -r config platform exe <<< "$entry"
    name="${config}-${platform}"
    printf '%-16s ' "$name"

    if ! configure_tree "$platform"; then
        echo "CONFIGURE FAILED  (see $OUT_DIR/configure-$platform.log)"
        failed=1
        continue
    fi

    # A rebuild, so nothing stale can reach the snapshot, but only of the demo and the library
    # it links. cmake --build --clean-first would do it by cleaning the whole tree, which also
    # throws away the engine, SDL3.dll and everything else in bin, and rebuilds SDL from source
    # on every snapshot. MSBuild's -t:Rebuild on one .vcxproj never reached that far either.
    clean_target "$platform" "$config" MemoryMonitorDemo
    clean_target "$platform" "$config" MemoryMonitor

    if ! cmake --build "build/cmake-$platform" --config "$config" \
            --target MemoryMonitorDemo > "$OUT_DIR/$name.build.log" 2>&1; then
        echo "BUILD FAILED  (see $OUT_DIR/$name.build.log)"
        failed=1
        continue
    fi

    if [ ! -f "$exe" ]; then
        echo "BUILD OK but exe missing at $exe"
        failed=1
        continue
    fi

    "./$exe" > "$OUT_DIR/$name.raw.txt" 2>&1
    exit_code=$?
    normalize < "$OUT_DIR/$name.raw.txt" > "$OUT_DIR/$name.txt"
    echo "$exit_code" > "$OUT_DIR/$name.exit"

    printf 'built, ran (exit %s), %s lines\n' "$exit_code" "$(wc -l < "$OUT_DIR/$name.txt")"
done

echo
if [ -n "$COMPARE_TO" ]; then
    PREV_DIR="baselines/$COMPARE_TO"
    if [ ! -d "$PREV_DIR" ]; then
        echo "nothing to compare against: $PREV_DIR does not exist" >&2
        exit 1
    fi

    echo "=== $LABEL vs $COMPARE_TO ==="
    differences=0
    for entry in "${CONFIGS[@]}"; do
        IFS='|' read -r config platform _ <<< "$entry"
        name="${config}-${platform}"
        printf '%-16s ' "$name"
        if [ ! -f "$PREV_DIR/$name.txt" ] || [ ! -f "$OUT_DIR/$name.txt" ]; then
            echo "missing on one side, skipped"
            continue
        fi
        if diff -q "$PREV_DIR/$name.txt" "$OUT_DIR/$name.txt" > /dev/null; then
            echo "identical"
        else
            echo "DIFFERS  ($(diff "$PREV_DIR/$name.txt" "$OUT_DIR/$name.txt" | grep -c '^[<>]') lines)"
            differences=1
        fi
    done
    echo
    [ "$differences" -eq 0 ] && echo "No output changed." || echo "Output changed. Run: diff baselines/$COMPARE_TO/<name>.txt $OUT_DIR/<name>.txt"
    [ "$failed" -eq 0 ] || exit 1
    exit "$differences"
fi

echo "Snapshot saved to $OUT_DIR"
exit "$failed"
