#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

std::unique_ptr<sdb::Process> sdb::Process::launch(std::filesystem::path path) {
    pid_t pid;
    if ((pid = fork()) < 0) {
        Error::send_errno("fork failed");
    }

    if (pid == 0) {
        // execute the program
        if (ptrace(PTRACE_TRACEME, 0, /*addr=*/nullptr, /*data=*/nullptr) < 0) {
            Error::send_errno("ptrace traceme failed");
        }
        if (execlp(path.c_str(), path.c_str(), nullptr) < 0) {
            Error::send_errno("execlp failed");
        }
    }

    std::unique_ptr<Process> process (new Process(pid, true));
    process->wait_on_signal();

    return process;
}

std::unique_ptr<sdb::Process> sdb::Process::attach(pid_t pid) {
    if (pid == 0) {
        Error::send("Invalid PID");
    }

    if (ptrace(PTRACE_ATTACH, pid, /*addr=*/nullptr, /*data=*/nullptr) < 0) {
        Error::send_errno("ptrace attach failed");
    }

    std::unique_ptr<Process> process (new Process(pid, false));
    process->wait_on_signal();

    return process;
}

sdb::Process::~Process() {
    if (pid_ != 0) {
        int status;
        if (state_ == ProcessState::Running) {
            kill(pid_, SIGSTOP);
            waitpid(pid_, &status, 0);
        }
        ptrace(PTRACE_DETACH, pid_, /*addr=*/nullptr, /*data=*/nullptr);
        kill(pid_, SIGCONT);
        if (terminate_on_end_) {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

void sdb::Process::resume() {
    if (ptrace(PTRACE_CONT, pid_, /*addr=*/nullptr, /*data=*/nullptr) < 0) {
        Error::send_errno("ptrace continue failed");
    }
    state_ = ProcessState::Running;
}

sdb::stop_reason sdb::Process::wait_on_signal() {
    int wait_status;
    int options = 0;
    if (waitpid(pid_, &wait_status, options) < 0) {
        Error::send_errno("waitpid failed");
    }
    stop_reason reason(wait_status);
    state_ = reason.reason;
    return reason;
}

sdb::stop_reason::stop_reason(int wait_status) {
    if (WIFSTOPPED(wait_status)) {
        reason = ProcessState::Stopped;
        info = WSTOPSIG(wait_status);
    } else if (WIFEXITED(wait_status)) {
        reason = ProcessState::Exited;
        info = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        reason = ProcessState::Terminated;
        info = WTERMSIG(wait_status);
    }
}
