#include "../include/filesystem.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <climits>
#include <cstdio>
#include <cstdlib>

static bool fail(const char* what) { perror(what); return false; }

static bool make_dev_nodes() {
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_NOEXEC, "mode=755,size=64k") != 0)
        return fail("mount /dev");
    struct { const char* path; int major, minor; } nodes[] = {
        {"/dev/null", 1, 3}, {"/dev/zero", 1, 5},
        {"/dev/random", 1, 8}, {"/dev/urandom", 1, 9},
    };
    for (auto& n : nodes)
        if (mknod(n.path, S_IFCHR | 0666, makedev(n.major, n.minor)) != 0)
            return fail(n.path);
    return true;
}

bool setup_filesystem(const std::string& rootfs) {
    char root[PATH_MAX];
    if (!realpath(rootfs.c_str(), root)) return fail("rootfs path");
    if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0)
        return fail("make / private");
    if (mount(root, root, nullptr, MS_BIND | MS_REC, nullptr) != 0)
        return fail("bind rootfs");
    if (chdir(root) != 0) return fail("chdir rootfs");
    if (syscall(SYS_pivot_root, ".", ".") != 0) return fail("pivot_root");
    if (umount2(".", MNT_DETACH) != 0) return fail("umount old root");
    if (chdir("/") != 0) return fail("chdir /");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) != 0)
        return fail("mount /proc");
    if (mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "size=16m") != 0)
        return fail("mount /tmp");
    return make_dev_nodes();
}