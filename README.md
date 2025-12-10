# pio-clangd

This project attempts to address the issues with using `clangd` as the LSP for PlatformIO projects. The primary use case is to use `clangd` as the LSP in Neovim and other editors.

## The Problem

PlatformIO has a CLI command to generate a `compile_commands.json` file (`pio run -t combiledb`), however, it has been my experience the generated `compile_commands.json` does not provide a working configuration for `clangd`.  There are two issues with the generated `compile_commands.json`.

 - GCC compiler flags that are not recognized by clangd
 - Missing paths for the toolchain and framework

## The Approach

The initial thought was to take the `compile_commands.json` file, parse it, and make the needed corrections and additions to the file.  However, I discovered the output from `pio project metadata` provided seemingly all of the information needed to create a functional `.clangd` configuration file...and the parsing was trivial.

## Integration with PlatformIO

This can be integrated into a PlatformIO project using the `extra_scripts` option for `platformio.ini` where targets are added to the pio CLI.  The `pio_targets.py` script can be added to a project's `extra_scripts` option, which adds a target to generate the `.clangd` configuration file along with some other convenience targets.

## Usage

Here are 2 approaches of how to integrate this into a PlatformIO project.

### Option 1: Git Submodule

1.  Create a PlatformIO project.  For example...

```bash
pio project init --board featheresp32 --project-dir .
```

2. Initialize git and create the initial commit

```bash
git init && git add . && git commit -m "Initial commit"
```

3. Add repo `pio-clangd` repo as a submodule to your git project

```bash
git submodule add https://github.com/gunlock/pio-clangd.git pio-clangd
git commit -m "Added pio-clangd as a submodule"
```

4. Add the following to `platformio.ini`

```ini
extra_scripts = pre:./pio-clangd/pio_targets.py
```

5. Generate the `.clangd` configuration file for the project/environment. If the environment is not specified in the pio CLI command, it will fall back to the default environment. Run this command in the project root.

```bash
pio run -t do:gen-clangd
```

6. The `.clangd` file will be generated for the project in the project root directory.

**NOTE**: It is straightforward to modify the `pio_targets.py` to add compiler flags to ignore or to transform.  See the `run_gen_clangd()` in `pio_targets.py` and the documentation in `gen_config.py` for the `gen_clangd()` function.

### Option 2: Copy and paste these scripts into your project root

The `gen_config.py` can be invoked directly as shell script or invoked with python3.

## Pull Requests

Pull requests are welcome given the small number of tested platforms.


### Platforms Tested to Date

 - ESP32
 - Teensy
