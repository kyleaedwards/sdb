#ifndef SDB_PROCESS_HPP
#define SDB_PROCESS_HPP

#include <filesystem>
#include <memory>
#include <sys/types.h>

namespace sdb {
    enum class ProcessState {
        Stopped,
        Running,
        Exited,
        Terminated
    };

    class Process {
    public:
        static std::unique_ptr<Process> launch(std::filesystem::path program_path);
        static std::unique_ptr<Process> attach(pid_t pid);

        void resume();
        stop_reason wait_on_signal();
        pid_t get_pid() const { return pid_; };
        ProcessState get_state() const { return state_; };

        // Delete public constructors
        Process() = delete;
        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;
        ~Process();
    private:
        pid_t pid_ = 0;
        bool terminate_on_end_ = true;
        ProcessState state_ = ProcessState::Stopped;

        // Private constructor
        Process(pid_t pid, bool terminate_on_end)
            : pid_(pid), terminate_on_end_(terminate_on_end) {}
    };

    struct stop_reason {
        stop_reason(int wait_status);

        ProcessState reason;
        std::uint8_t info;
    };
}

#endif