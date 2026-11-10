#include "caps.h"
#include <grp.h>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

bool drop_privileges() {
    for (int cap = 0; cap <= 63; cap++) {
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) != 0 && errno != EINVAL) {
            perror("PR_CAPBSET_DROP"); return false;
        }
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("PR_SET_NO_NEW_PRIVS"); return false;
    }

    const uid_t NOBODY = 65534;
    if (setgroups(0, nullptr) != 0 || setgid(NOBODY) != 0 || setuid(NOBODY) != 0) {
        perror("drop uid/gid"); return false;
    }

    __user_cap_header_struct hdr = { _LINUX_CAPABILITY_VERSION_3, 0 };
    __user_cap_data_struct data[2] = {};
    if (syscall(SYS_capset, &hdr, data) != 0) { perror("capset"); return false; }

    if (setuid(0) == 0) {
        fprintf(stderr, "privilege drop failed: still able to become root\n");
        return false;
    }
    return true;
}