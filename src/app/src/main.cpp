#include "pch.hpp"
#include "app.hpp"
#include "runtime_paths.hpp"

int main(int argc, char** argv) {
    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
        scpp::InitializeRuntimePaths(argv[0]);
    }

    try {
        std::optional<std::filesystem::path> initialVideoFile;
        if (argc > 1 && argv[1] != nullptr) {
            initialVideoFile = std::filesystem::path{argv[1]};
        }

        scpp::Application app{initialVideoFile};

        app.Run();
    } catch (const std::exception& exception) {
        std::println("Scopes++ failed to start: {}", exception.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
