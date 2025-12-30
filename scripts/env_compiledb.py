"""
Generates compile_commands.json in the environment build directory
rather than the project root.

There are 2 ways to use this script as described below.

  #1) Add as an extra_scripts in platformio.ini

    Add to platformio.ini:
      ...
      extra_scripts = post:env_compiledb.py
      ...

    Usage:
      pio run -e MyEnvNameHere -t compiledb

    Output:
      <project_root>/.pio/build/MyEnvNameHere/compile_commands.json


  #2) Import as module in an existing 'extra_scripts' as a 'post' script
    
      Import("env")
      from env_compiledb import setup_compiledb
      ...
      setup_compiledb(env)
      ...
   
     Output:
       Same as in #1
"""


def setup_compiledb(env):
    """
    Configure compile_commands.json generation.

    Args:
        env: PlatformIO SCons environment object
    """
    # Change output directory
    env.Replace(COMPILATIONDB_PATH="$BUILD_DIR/compile_commands.json")

    # Include toolchain paths
    env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)


# Auto-run when used as extra_script
try:
    Import("env")
    setup_compiledb(env)
except NameError:
    # env not available - being imported as module
    pass
