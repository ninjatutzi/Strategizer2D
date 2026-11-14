#include <iostream>
#include <string>

#include "App.hpp"
#include "Data.hpp"

int main(int argc, char** argv) {
    std::string shotDir;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--screenshots" && i + 1 < argc) shotDir = argv[++i];

    std::string root = findGameRoot(argv[0]);
    if (root.empty()) {
        std::cerr << "Cannot find the game files (data/units.csv). Run the game from the project folder.\n";
        return 1;
    }
    GameData data;
    try {
        data.load(root);
    } catch (const std::exception& e) {
        std::cerr << "Error in game data: " << e.what() << "\n";
        return 1;
    }
    App app(data, root);
    app.run(shotDir);
    return 0;
}
