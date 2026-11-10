#include "caps.h"
#include <grp.h>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

bool drop_privileges() {
    // 1. Bounding set: the ceiling of what any future exec could ever gain.
    //    EINVAL just means "that capability number doesn't exist", so ignore it.
    for (int cap = 0; cap <= 63; cap++) {
        if (prctl(PR_CAPBSET_DROP, cap, 0, 0, 0) != 0 && errno != EINVAL) {
            perror("PR_CAPBSET_DROP"); return false;
        }
    }

    // 2. No gaining privileges through exec (setuid binaries, file capabilities).
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("PR_SET_NO_NEW_PRIVS"); return false;
    }

    // 3. Stop being root. Order matters: groups, then gid, then uid LAST.
    const uid_t NOBODY = 65534;
    if (setgroups(0, nullptr) != 0 || setgid(NOBODY) != 0 || setuid(NOBODY) != 0) {
        perror("drop uid/gid"); return false;
    }

    // 4. Belt and braces: explicitly empty the permitted/effective/inheritable sets.
    __user_cap_header_struct hdr = { _LINUX_CAPABILITY_VERSION_3, 0 };
    __user_cap_data_struct data[2] = {};
    if (syscall(SYS_capset, &hdr, data) != 0) { perror("capset"); return false; }

    // 5. Verify: getting root back must fail.
    if (setuid(0) == 0) {
        fprintf(stderr, "privilege drop failed: still able to become root\n");
        return false;
    }
    return true;
}