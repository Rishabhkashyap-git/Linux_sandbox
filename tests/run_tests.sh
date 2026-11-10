#!/bin/bash
set -u
ROOT=${ROOTFS:-./alpine}
BIN=./sandbox
BUILD=tests/bin
pass=0; fail=0

[ "$(id -u)" -eq 0 ] || { echo "run with sudo"; exit 1; }
[ -x "$BIN" ] || { echo "build first: make"; exit 1; }

mkdir -p "$BUILD" "$ROOT/tests"
for src in tests/attacks/*.c; do
    name=$(basename "$src" .c)
    gcc -static -O1 -o "$BUILD/$name" "$src" || { echo "compile failed: $src"; exit 1; }
    cp "$BUILD/$name" "$ROOT/tests/$name"
    chmod 755 "$ROOT/tests/$name"
done


check() {
    local name=$1 expect=$2; shift 2
    local out code
    out=$($BIN --rootfs "$ROOT" "$@" 2>&1); code=$?
    if [ "$code" -eq "$expect" ]; then
        printf "PASS  %-30s (exit %d)\n" "$name" "$code"; pass=$((pass+1))
    else
        printf "FAIL  %-30s (expected %d, got %d)\n" "$name" "$expect" "$code"
        echo "$out" | sed 's/^/        /'; fail=$((fail+1))
    fi
}

check "normal program"            0  -- /tests/hello
check "non-zero exit"            10  -- /bin/sh -c 'exit 3'
check "missing program"          10  -- /no/such/program
check "CPU limit"                11  --cpu 2 --time 10 -- /tests/cpu_loop
check "wall-clock limit"         11  --time 2 -- /tests/sleeper
check "memory limit"             12  --mem 32M -- /tests/mem_hog
check "fork bomb (pids limit)"   13  --pids 20 --time 3 -- /tests/fork_bomb
for s in mount ptrace unshare reboot settime; do
    check "forbidden syscall: $s" 14 -- /tests/forbidden_syscall "$s"
done
check "network blocked"           0  -- /tests/net_try
check "isolation audit"           0  -- /tests/contain


left=$(ls /sys/fs/cgroup 2>/dev/null | grep -c '^sandbox-')
if [ "$left" -eq 0 ]; then echo "PASS  no leftover cgroups"; pass=$((pass+1))
else echo "FAIL  $left leftover cgroups"; fail=$((fail+1)); fi

mnt=$(grep -c "$(basename "$ROOT")" /proc/mounts)
if [ "$mnt" -eq 0 ]; then echo "PASS  no leaked mounts"; pass=$((pass+1))
else echo "FAIL  $mnt leaked mounts"; fail=$((fail+1)); fi

echo; echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]