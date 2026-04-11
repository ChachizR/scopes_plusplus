#include "pch.hpp"
#include "app.hpp"
#include "runtime_paths.hpp"

int main(int argc, char** argv) {
    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
        scpp::InitializeRuntimePaths(argv[0]);
    }

    scpp::Application app;

    app.Run();

    return EXIT_SUCCESS;
}
