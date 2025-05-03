# Getting started

## Build and Installation

### Prerequisites

- **CMake >= 3.16**

- **C++ compiler with C++20 support (C++23 required for Glaze JSON)**

- **Boost 1.84.0 or higher (system, filesystem, iostreams components)**

- **OpenSSL**

- **IO-Uring**

- **Installing via git modules**
    - fmt library
    - Nlohmann/json and Glaze JSON

- **Google Test (for building tests)**

- **Linux operating system (Windows is not supported for now, it will be fixed later)**

## CMake Options

**CXXAPI** provides several configuration options that can be enabled or disabled during the build process:

| Option | Description | Default |
|--------|-------------|---------|
| `CXXAPI_USE_NLOHMANN_JSON` | Enable nlohmann/json support | ON |
| `CXXAPI_USE_GLAZE_JSON` | Enable Glaze JSON support (requires C++23) | OFF |
| `CXXAPI_USE_REDIS_IMPL` | Enable Redis implementation support | ON |
| `CXXAPI_USE_LOGGING_IMPL` | Enable Logging implementation support | ON |
| `CXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL` | Enable builtin middlewares | ON |
| `CXXAPI_BUILD_TESTS` | Build CXXAPI tests | OFF |

### Building with tests

```bash
cmake -DCXXAPI_BUILD_TESTS=ON -S . -B build && cmake --build build --target all
```

### JSON Library Selection

**CXXAPI** requires at least one JSON library to be enabled. 

- You can choose between:
    - nlohmann/json (C++20)
    - Glaze JSON (C++23)

**At least one of `CXXAPI_USE_NLOHMANN_JSON` or `CXXAPI_USE_GLAZE_JSON` must be set to ON.**

To enable Glaze JSON (which requires C++23 support):
```bash
cmake -DCXXAPI_USE_GLAZE_JSON=ON ..
```

### Optional Components

To disable Redis support:
```bash
cmake -DCXXAPI_USE_REDIS_IMPL=OFF ..
```

To disable Logging support:
```bash
cmake -DCXXAPI_USE_LOGGING_IMPL=OFF ..
```

To disable builtin middlewares:
```bash
cmake -DCXXAPI_USE_BUILTIN_MIDDLEWARES_IMPL=OFF ..
```

## Installation

**Installation via FetchContent from SOURCE_DIR:**

```cmake
include(FetchContent)

FetchContent_Declare(
    cxxapi
    SOURCE_DIR YOUR_PATH_TO_CXXAPI
)

FetchContent_MakeAvailable(cxxapi)

target_link_libraries(YOUR_TARGET PRIVATE cxxapi::cxxapi)
```

**Installation via FetchContent from GIT_REPOSITORY:**

```cmake
FetchContent_Declare(
    cxxapi
    GIT_REPOSITORY https://github.com/cluedesc/cxxapi.git
    GIT_TAG 1.1.0
)

FetchContent_MakeAvailable(cxxapi)

target_link_libraries(YOUR_TARGET PRIVATE cxxapi::cxxapi)
```