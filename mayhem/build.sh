#!/usr/bin/env bash
#
# mayhem/build.sh — Hypersomnia fuzz harnesses + the upstream unit-test runner.
#
# Upstream builds one monolithic executable with CMake. We configure upstream's CMake as a
# HEADLESS (dedicated-server) build with WebRTC/OpenSSL off, build every object of the
# Hypersomnia target except the few TUs that do not compile without WebRTC (listed below),
# archive them and link each harness against that archive. Two trees:
#   mayhem/build-fuzz  ASan+UBSan+SanCov, DWARF 3  -> fuzz targets + -standalone reproducers
#   mayhem/build-test  upstream's normal flags, BUILD_UNIT_TESTS=ON -> mayhem/build-test/unit_tests
# Submodules (not in a plain checkout) come from /opt/toolchains/deps/<name>-<commit>/src, fetched
# at upstream's pinned commits by mayhem/Dockerfile; each is copied into its path only where empty.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${HS_DEPS:=/opt/toolchains/deps}"
export CC CXX

cd "$SRC"

# 0) Submodules.
for sub in cmake/Introspector-generator src/3rdparty/lodepng src/3rdparty/rapidjson src/3rdparty/yojimbo \
           src/3rdparty/imgui src/3rdparty/polypartition src/3rdparty/rectpack2D src/3rdparty/Catch; do
	name=$(basename "$sub")
	if [ -z "$(ls -A "$sub" 2>/dev/null)" ]; then
		dep=$(ls -d "$HS_DEPS/$name"-*/src 2>/dev/null | head -n1)
		[ -n "$dep" ] || { echo "missing submodule source $HS_DEPS/$name-<commit>/src" >&2; exit 1; }
		mkdir -p "$sub"
		cp -a "$dep/." "$sub/"
		chmod -R u+w "$sub"
	fi
done

# TUs of the Hypersomnia target left out: masterserver.cpp and server_setup.cpp need WebRTC
# (libdatachannel) to compile; main.cpp and work.cpp are the application's entry point and main
# loop, which reference those two. No harness or unit test needs any of them.
EXCLUDED_OBJS='src/main\.cpp\.o|src/work\.cpp\.o|src/application/masterserver/masterserver\.cpp\.o|src/application/setups/server/server_setup\.cpp\.o'

COMMON_CMAKE=(-G Ninja -DHEADLESS=1 -DBUILD_WEBRTC=OFF -DBUILD_OPENSSL=OFF -DPREFER_LIBCXX=OFF
	-DBUILD_VERSION_FILE_GENERATOR=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DARCHITECTURE=x64)
# Without WebRTC upstream leaves two parameters unused (network_types.cpp); not an error for us.
WARN_FLAGS="-Wno-error=unused-parameter"

# Builds the objects into $1 and archives them into $1/libhs.a.
build_objects() {
	local dir=$1
	local objs
	objs=$(ninja -C "$dir" -t targets all | sed -n 's|^\(CMakeFiles/Hypersomnia\.dir/[^:]*\.o\): .*|\1|p' | grep -Ev "$EXCLUDED_OBJS")
	# shellcheck disable=SC2086
	ninja -C "$dir" -j"$MAYHEM_JOBS" streflop blake3 $objs
	rm -f "$dir/libhs.a"
	(cd "$dir" && echo "$objs" | xargs llvm-ar qc libhs.a && llvm-ranlib libhs.a)
}

# The compile command of one upstream TU, minus its output/input/depfile arguments: the harness
# flags (includes, defines, -std) then match the project exactly.
tu_flags() {
	python3 - "$1/compile_commands.json" <<'PY'
import json, shlex, sys
cmds = json.load(open(sys.argv[1]))
c = next(c for c in cmds if c["file"].endswith("src/application/intercosm.cpp"))
a = shlex.split(c["command"])
out, skip = [], 0
for i, x in enumerate(a[1:]):
    if skip: skip -= 1; continue
    if x in ("-o", "-c", "-MT", "-MF"): skip = 1; continue
    if x == "-MD": continue
    if x == "/usr/bin/nice" or x in ("-n", "10"): continue
    out.append(x)
print(" ".join(shlex.quote(x) for x in out))
PY
}

LINK_LIBS() { echo "$1/app_globals.o -Wl,--start-group $1/libhs.a $1/src/3rdparty/streflop/libstreflop.a $1/libblake3.a -Wl,--end-group -lsodium -ldl -pthread -fuse-ld=lld"; }

# 1) Sanitized, SanCov-instrumented project build.
F=mayhem/build-fuzz
cmake -S . -B "$F" "${COMMON_CMAKE[@]}" -DCMAKE_BUILD_TYPE=Debug -DBUILD_UNIT_TESTS=OFF \
	-DCMAKE_C_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS $WARN_FLAGS" \
	-DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS $WARN_FLAGS"
build_objects "$F"
FFLAGS=$(tu_flags "$F")
eval "$CXX $FFLAGS -c mayhem/harnesses/app_globals.cpp -o $F/app_globals.o"

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$F/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$F/standalone_main.o"

HARNESSES="project_json project_meta image config_json net_message masterserver_packets stun_response identity_cert"
for h in $HARNESSES; do
	eval "$CXX $FFLAGS -c mayhem/harnesses/fuzz_$h.cpp -o $F/fuzz_$h.o"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$F/fuzz_$h.o" "$F/lsan_off.o" $(LINK_LIBS "$F") -o "/mayhem/fuzz_$h"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$F/fuzz_$h.o" "$F/standalone_main.o" "$F/lsan_off.o" $(LINK_LIBS "$F") -o "/mayhem/fuzz_$h-standalone"
done

# 2) Upstream's unit tests with its normal flags (no sanitizers), run by mayhem/test.sh.
T=mayhem/build-test
cmake -S . -B "$T" "${COMMON_CMAKE[@]}" -DCMAKE_BUILD_TYPE=Debug -DBUILD_UNIT_TESTS=ON \
	-DCMAKE_C_FLAGS="$WARN_FLAGS $COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$WARN_FLAGS $COVERAGE_FLAGS"
build_objects "$T"
TFLAGS=$(tu_flags "$T")
eval "$CXX $TFLAGS -c mayhem/harnesses/unit_tests_main.cpp -o $T/unit_tests_main.o"
eval "$CXX $TFLAGS -c mayhem/harnesses/app_globals.cpp -o $T/app_globals.o"
# The Catch TEST_CASEs register themselves from TUs nothing else references: link those objects
# directly (found by their source), the rest from the archive. tournament_coordinator.cpp's tests
# drive a whole server_setup (excluded above), so they are left out.
TEST_OBJS=$(cd "$SRC" && grep -rl --include='*.cpp' -E '^\s*TEST_CASE' src | grep -v -e '^src/3rdparty/' -e 'tournament_coordinator\.cpp$' \
	| sed 's|^|CMakeFiles/Hypersomnia.dir/|; s|$|.o|' | while read -r o; do [ -f "$T/$o" ] && echo "$T/$o"; done || true)
# shellcheck disable=SC2086
$CXX $COVERAGE_FLAGS "$T/unit_tests_main.o" "$T/app_globals.o" $TEST_OBJS -Wl,--start-group "$T/libhs.a" \
	"$T/src/3rdparty/streflop/libstreflop.a" "$T/libblake3.a" -Wl,--end-group -lsodium -ldl -pthread -fuse-ld=lld -o "$T/unit_tests"
