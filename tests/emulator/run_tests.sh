#!/usr/bin/env bash
# Build and run the RMACXX tests on the CPU emulator (no GPU / NVSHMEM / MPI
# needed). Usage: tests/emulator/run_tests.sh [test.cpp ...]
# The number of PEs is taken from the file name (name.N.cpp), default 2.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EMU="$ROOT/tests/emulator"
OUT="${EMU_BUILD_DIR:-$ROOT/build/emulator}"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -O1 -g -pthread}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
mkdir -p "$OUT"

if [ $# -gt 0 ]; then
    TESTS=("$@")
else
    mapfile -t TESTS < <(cd "$ROOT" && find tests -name '*.cpp' -not -path 'tests/emulator/*' | sort)
fi

"$CXX" $CXXFLAGS -I"$EMU/include" -c "$EMU/emu_main.cpp" -o "$OUT/emu_main.o" || exit 2

run_one() {
    local src="$1" name np exe log extra=""
    name="$(echo "${src#tests/}" | sed 's/\.cpp$//; s#/#.#g')"
    np="$(basename "$src" .cpp | sed -n 's/.*\.\([0-9][0-9]*\)$/\1/p')"
    [ -z "$np" ] && np=2
    case "$src" in tests/futures/*) extra="-DRMACXX_USE_FUTURES -DDEFAULT_EEXPR_SIZE=4096 -DDEFAULT_BEXPR_SIZE=16384";; esac
    exe="$OUT/$name"
    log="$OUT/$name.log"
    if ! $CXX $CXXFLAGS $extra -I"$EMU/include" -I"$ROOT/include" -Dmain=rmacxx_test_main \
            "$ROOT/$src" "$OUT/emu_main.o" -o "$exe" > "$log" 2>&1; then
        echo "BUILD-FAIL $src"
        return
    fi
    if EMU_NP="$np" timeout 120 "$exe" >> "$log" 2>&1; then
        echo "PASS       $src (np=$np)"
    else
        echo "FAIL($?)    $src (np=$np)"
    fi
}
export -f run_one
export CXX CXXFLAGS EMU ROOT OUT

printf '%s\n' "${TESTS[@]}" | xargs -P "$JOBS" -I{} bash -c 'cd "$ROOT" && run_one "{}"' | sort -k2
