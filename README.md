# Linux_sandbox

A lightweight Linux sandbox in C++ that runs an untrusted program inside a
container-style environment and reports a verdict: OK, Time Limit Exceeded,
Memory Limit Exceeded, Process Limit Exceeded, Security Violation or Runtime Error.

It is built from the same kernel primitives Docker uses: namespaces, `pivot_root`,
cgroups v2, capabilities and seccomp-bpf.

## How it works
1. The parent creates a cgroup (memory, pids and CPU limits).
2. The parent `clone()`s the child into new PID, mount, UTS, IPC and network namespaces.
3. The child waits on a pipe until the parent has put it in the cgroup.
4. The child `pivot_root`s into the rootfs and mounts a fresh /proc, /dev and /tmp.
5. The child sets a CPU rlimit, drops all capabilities, sets no_new_privs, becomes
   uid 65534 and installs the seccomp filter (default deny, kill list for
   dangerous syscalls).
6. The child `exec`s the target. The parent enforces the wall-clock limit and turns
   the exit status plus cgroup counters into a verdict.

## Requirements
- Linux with cgroups v2 (Ubuntu 22.04 or newer), kernel 5.19+ recommended
- root access (sudo)
- `build-essential`, `libseccomp-dev`, `curl`
- Tested on: Ubuntu 26.04 LTS

## Quick start
    git clone https://github.com/<your-username>/Linux_sandbox
    cd Linux_sandbox
    sudo apt install -y build-essential libseccomp-dev curl
    ./scripts/setup_rootfs.sh     # downloads Alpine into ./alpine (not part of the repo)
    make
    sudo ./sandbox --rootfs ./alpine -- /bin/ls /

## Usage
    sudo ./sandbox --rootfs ./alpine [--mem 64M] [--cpu 2] [--time 5] [--pids 20] [--net] -- <program> [args]

| Flag | Meaning | Default |
|---|---|---|
| --rootfs | extracted root filesystem | required |
| --mem | memory limit (K/M/G) | 64M |
| --cpu | CPU seconds | 2 |
| --time | wall-clock seconds | 5 |
| --pids | maximum processes | 20 |
| --net | share the host network | off |

The target program path is relative to the rootfs (`/bin/ls` means `./alpine/bin/ls`).
Exit codes: 0 OK, 10 runtime error, 11 time limit, 12 memory limit,
13 process limit, 14 security violation.

## Tests
    make test
This compiles attack programs (fork bomb, memory hog, CPU loop, forbidden syscalls,
network attempt, an isolation audit), runs each in the sandbox, and checks that the
right layer stops it. It also checks that no cgroups or mounts are left behind.

## Threat model
| Threat | Layer | Test |
|---|---|---|
| infinite loop | RLIMIT_CPU, wall-clock timeout | cpu_loop, sleeper |
| memory exhaustion | cgroup memory.max | mem_hog |
| fork bomb | cgroup pids.max | fork_bomb |
| host file access | pivot_root | contain |
| privilege escalation | dropped caps, no_new_privs, uid 65534 | contain |
| kernel attack surface | seccomp | forbidden_syscall |
| network access | network namespace, seccomp | net_try |

## Limitations
- A learning project, not a replacement for Docker or a VM. It shares the host
  kernel, so a kernel bug could still be an escape route.
- Runs as root (no user namespace). Rootless mode is future work.
- `--net` shares the host network without filtering.
- No image management, overlay filesystems or writable bind mounts.

## Project layout
    include/   headers
    src/       implementation
    tests/     attack programs and the test runner
    scripts/   rootfs setup