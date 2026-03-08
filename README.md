# Simulation Foundry Render

![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white)

Simulation Foundry Render is a developer friendly repository for rendering simulation data generated in Simulation Foundry Core.

## Prerequisites

- Vulkan SDK
- A supported C++ compiler (MSVC on Windows, clang or gcc on macOS/Linux)
- vcpkg for dependency management
- clang-format and clang-tidy for code formatting and static analysis
- pre-commit for git hooks

## Build Instructions

1. Set vcpkg root environment variable (replace `<vcpkg-root>`):

    Linux:

    ```bash
    export VCPKG_ROOT="<vcpkg-root>"
    ```

    Windows (Powershell):

    ```Powershell
    $env:VCPKG_ROOT="<vcpkg-root>"
    ```

1. Build with CMake workflow presets (replace `<workflow-preset>`):

    ```bash
    cmake --workflow --preset "<workflow-preset>"
    ```

    Workflow presets can be listed with:

    ```bash
    cmake --workflow --list-presets
    ```

## Examples

After building, example binaries can be found under `out/build/<preset>/examples`.

## Git Hooks

Install hooks locally:

```bash
pre-commit install
```

Run hooks on the whole repository:

```bash
pre-commit run --all-files
```

## License

This project is released under the terms in the `LICENSE` file: [LICENSE](LICENSE)
