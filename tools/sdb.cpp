#include <libsdb/libsdb.hpp>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <editline/readline.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
// #include <sys/ptrace.h>

namespace {
    void print_stop_reason(
        const sdb::Process& process,
        sdb::stop_reason reason
    ) {
        std::cout << "Process " << process.get_pid() << ' ';
        switch (reason.reason) {
            case sdb::ProcessState::Stopped:
                std::cout << "stopped with signal " << sigabbrev_np(reason.info);
                break;
            case sdb::ProcessState::Exited:
                std::cout << "exited with status " << static_cast<int>(reason.info);
                break;
            case sdb::ProcessState::Terminated:
                std::cout << "terminated with signal " << sigabbrev_np(reason.info);
                break;
        }
        std::cout << std::endl;
    }
    std::vector<std::string> split(std::string_view str, char delimiter) {
        std::vector<std::string> out{};
        std::stringstream ss { std::string{str} };
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            out.push_back(item);
        }
        return out;
    }

    bool is_prefix(std::string_view str, std::string_view of) {
        if (str.size() > of.size()) {
            return false;
        }
        return std::equal(str.begin(), str.end(), of.begin());
    }

    void handle_command(std::unique_ptr<sdb::Process>& process, std::string_view line) {
        auto args = split(line, ' ');
        auto command = args[0];

        if (is_prefix(command, "continue")) {
            process->resume(pid);
            auto reason = process->wait_on_signal(pid);
            print_stop_reason(*process, reason);
        } else {
            std::cerr << "Unknown command: " << command << "\n";
        }
    }

    std::unique_ptr<sdb::Process> attach(int argc, const char** argv) {
        if (argc == 3 && argv[1] == std::string_view("-p")) {
            pid_t pid = std::stoi(argv[2]);
            return sdb::Process::attach(pid);
        } else {
            const char* program_path = argv[1];
            return sdb::Process::launch(program_path);
            // return sdb::Process::launch(std::filesystem::path(program_path));
        }
    }

    void main_loop(std::unique_ptr<sdb::Process>& process) {
        char* line = nullptr;
        while ((line = readline("sdb> ")) != nullptr) {
            std::string line_str;

            if (line == std::string_view("")) {
                free(line);
                if (history_length > 0) {
                    line_str = history_list()[history_length - 1]->line;
                }
            } else {
                line_str = line;
                add_history(line);
                free(line);
            }

            if (!line_str.empty()) {
                try {
                    handle_command(process, line_str);
                } catch (const sdb::Error& e) {
                    std::cout << e.what() << std::endl;
                }
            }
        }
    }
}

int main(int argc, const char** argv) {
    if (argc == 1) {
        std::cerr << "Usage: " << argv[0] << " <pid>\n";
        return -1;
    }

    try {
        auto process = attach(argc, argv);
        main_loop(process);
    } catch (const sdb::Error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
