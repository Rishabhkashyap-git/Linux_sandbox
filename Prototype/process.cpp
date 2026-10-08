#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/resource.h>
#include <csignal>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ./process <time_limit> <program>\n";
        return 1;
    }
    int time_limit;

    try {
        time_limit = std::stoi(argv[1]);
    }
    catch (...) {
        std::cout << "Invalid time limit.\n";
        return 1;
    }

    if (time_limit <= 0) {
        std::cout << "Time limit must be greater than 0.\n";
        return 1;
    }

    std::cout << "\n========================================\n";
    std::cout << "          LIGHTWEIGHT SANDBOX\n";
    std::cout << "========================================\n";

    std::cout << "Target     : " << argv[2] << "\n";
    std::cout << "Time limit : " << time_limit << " sec\n";
    std::cout << "CPU limit  : 2 sec\n";
    std::cout << "----------------------------------------\n";

    pid_t pid = fork();
    std::cout << "[+] Process created\n";
    std::cout << "    PID: " << pid << "\n";
    if (pid < 0) {
        std::cout << "fork() failed\n";
        return 1;
    }

    else if (pid == 0) {

        std::cout << "[+] Target started\n";

        struct rlimit limit;

        limit.rlim_cur = 2;
        limit.rlim_max = 2;

        if (setrlimit(RLIMIT_CPU, &limit) != 0) {
            perror("setrlimit");
            return 1;
        }

        execl(argv[2], argv[2], (char*)NULL);

        perror("exec");
        return 1;
    }
    else {

        int status;
        int elapsed = 0;

        std::cout << "[*] Monitoring target...\n";

        while (true) {

            pid_t result = waitpid(pid, &status, WNOHANG);

            if (result == pid) {
                if (WIFEXITED(status)) {

                    int exit_code = WEXITSTATUS(status);

                    if (exit_code == 0) {
                        std::cout << "Target completed successfully.\n";
                    }
                    else {
                        std::cout << "Target exited with an error.\n";
                    }

                    std::cout << "Exit code: " << exit_code << "\n";
                }
                else if (WIFSIGNALED(status)) {

                    int signal_number = WTERMSIG(status);

                    std::cout << "Target was terminated by signal "
                            << signal_number << "\n";

                    if (signal_number == SIGXCPU) {
                        std::cout << "Reason: CPU time limit exceeded.\n";
                    }
                    else if (signal_number == SIGKILL) {
                        std::cout << "Reason: Sandbox forcibly terminated it.\n";
                    }
                }
                break;
            }

            sleep(1);
            elapsed++;


            if (elapsed >= time_limit) {

                std::cout << "[!] Time limit exceeded.\n";

                if (kill(pid, SIGKILL) == 0) {
                    waitpid(pid, &status, 0);
                    std::cout << "[!] Target terminated by sandbox.\n";
                }
                else {
                    perror("kill");
                }

                break;
            }
        }
        struct rusage usage;

        if (getrusage(RUSAGE_CHILDREN, &usage) == 0) {
            std::cout << "CPU user time: "
                    << usage.ru_utime.tv_sec
                    << " seconds\n";

            std::cout << "CPU system time: "
                    << usage.ru_stime.tv_sec
                    << " seconds\n";
        }
    }

    return 0;
}