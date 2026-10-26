#include "forge/cli.h"
#include "forge/forge.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        forge::cli::print_usage();
        return 1;
    }

    std::string command = argv[1];

    // Handle --version and --help before dispatch
    if (command == "--version" || command == "-v") {
        std::cout << "forge version " << forge::VERSION << "\n";
        return 0;
    }
    if (command == "--help" || command == "-h") {
        forge::cli::print_usage();
        return 0;
    }

    // Collect remaining arguments
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    return forge::cli::dispatch(command, args);
}
