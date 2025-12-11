from SCons.Script import DefaultEnvironment
import sys
import traceback
from gen_config import *


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
env.AddTarget("gen-clangd", None, run_gen_clangd)
