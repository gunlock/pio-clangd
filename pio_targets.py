from SCons.Script import DefaultEnvironment
import subprocess
import sys
import traceback
from gen_config import *


def run_command(arg_list):
    result = subprocess.run(
        arg_list,
        capture_output=True,
        text=True,
        check=False # Allow failures
    )
    if result.returncode != 0:
        print(result.stderr)
        return 1
    
    print(result.stdout)
    return 0


# Build project without uploading
def run_build(target, source, env):
    return run_command(['pio', 'run', '-e', env.get('PIOENV')])


# Build and upload to board
def run_upload(target, source, env):
    return run_command(['pio', 'run', '-e', env.get('PIOENV'), '--target', 'upload'])


def run_upload_listen(target, source, env):
    ret = run_command(['pio', 'run', '-e', env.get('PIOENV'), '--target', 'upload'])
    if(ret != 0):
        return ret
    else:
        return run_command(['pio', 'device', 'monitor'])


def run_listen(target, source, env):
    return run_command(['pio', 'device', 'monitor'])


def run_clean(target, source, env):
    return run_command(['pio', '--target', 'clean'])


# Show project info
def run_print_info(target, source, env):
    return run_command(['pio', 'project', 'metadata'])


# Install/update deps
def run_install_deps(target, source, env):
    return run_command(['pio', 'pkg', 'install'])


def run_gen_clangd(target, source, env):
    result = 1
    print('running run_gen_clangd')

    try:
        path = gen_clangd(None, None, env_name = env.get('PIOENV'))
        result = 0

    except Exception as e:
        print("do:gen-clangd failed")
        # Get the full traceback object from the system
        # 'sys.exc_info()' returns (type, value, traceback)
        exc_type, exc_value, exc_traceback = sys.exc_info()
    
        # Extract the frames from the traceback object
        # The last frame in the list is where the error occurred
        extracted_tb = traceback.extract_tb(exc_traceback)
    
        # Get the last (most recent) frame
        last_frame = extracted_tb[-1]
    
        # Extract the necessary details
        filename = last_frame.filename
        line_number = last_frame.lineno
        function_name = last_frame.name
    
        print("--- Exception Location Details ---")
        print(f"**Exception Type:** {type(e).__name__}")
        print(f"**File Name:** {filename}")
        print(f"**Line Number:** {line_number}")
        print(f"**Function Name:** {function_name}")
        print("--------------------------------")

    return result
        
        

env = DefaultEnvironment()
env.AddTarget("do:build", None, run_build)
env.AddTarget("do:upload", None, run_upload)
env.AddTarget("do:upload-listen", None, run_upload_listen)
env.AddTarget("do:listen", None, run_listen)
env.AddTarget("do:clean", None, run_clean)
env.AddTarget("do:print-info", None, run_print_info)
env.AddTarget("do:install-deps", None, run_install_deps)
env.AddTarget("do:gen-clangd", None, run_gen_clangd)
