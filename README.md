# Blackjack

A cross-platform, feature-rich C++ implementation of classic casino Blackjack with SDL2 graphics, AI opponents, local & network multiplayer, achievements, and full persistence.

## Features

- **Complete Casino Rules Engine** — hit, stand, double down, split, insurance, surrender, soft-17 configuration, and configurable payouts
- **Four AI Personalities** — Conservative, Aggressive, Card Counter (Hi-Lo), and Random
- **Local Multiplayer** — 2–4 players in pass-and-play (hot-seat) mode
- **Network Multiplayer** — Host-based authoritative multiplayer with room codes and reconnection
- **Save/Load System** — Full mid-round state persistence, multiple player profiles, and cross-platform save directories
- **Achievement System** — First Blackjack, High Roller, Lucky Streak, Win Streaks, Veteran, and more
- **Tutorial Mode** — Step-by-step guided introduction to Blackjack strategy
- **Procedural Card Rendering** — Clean, scalable vector-style card rendering with animations
- **Responsive SDL2 UI** — Themed widget toolkit with buttons, sliders, modals, and toasts
- **Audio System** — Ambient casino atmosphere, SFX for all actions, and volume controls

## Building

### Requirements

- CMake 3.16+
- C++20 compiler (Clang, GCC, or MSVC)
- SDL2, SDL2_image, SDL2_ttf, SDL2_mixer
- nlohmann/json (fetched automatically)
- Catch2 v3 (fetched automatically for tests)

### macOS (Homebrew)

```bash
brew install cmake sdl2 sdl2_image sdl2_ttf sdl2_mixer
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.logicalcpu)
```

### Linux

```bash
sudo apt-get install cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Windows

```bash
# Using vcpkg for dependencies
vcpkg install sdl2 sdl2-image sdl2-ttf sdl2-mixer
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## Running

```bash
./build/bin/blackjack
```

## Testing

```bash
cd build
make -j$(sysctl -n hw.logicalcpu)

# Run all test suites
./bin/core_tests
./bin/game_tests
./bin/integration_tests
./bin/ai_tests
./bin/persistence_tests
```

## Architecture

The project is structured in clean layers:

- **core/** — Pure C++ engine (Card, Hand, Shoe, RuleSet). No SDL2.
- **game/** — Round state machine, action validation, turn handling
- **ai/** — AI strategy implementations (Basic Strategy tables, Hi-Lo card counting)
- **persistence/** — JSON save/load, profiles, statistics, achievements
- **gui/** — SDL2 rendering, screens, widgets, and theming
- **audio/** — SDL2_mixer sound management
- **network/** — Host-authoritative TCP multiplayer
- **platform/** — Platform-specific utilities

## License

MIT License — see [LICENSE](LICENSE.md)
