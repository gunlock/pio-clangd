#!/usr/bin/env python3
"""
PlatformIO clangd configuration generator.

Public API:
    gen_clangd(flags_to_skip=None, flag_transforms=None) - Generate .clangd file

Example usage as module:
    >>> from gen_config import * 
    >>> gen_clangd()
    >>> # Or with custom flags:
    >>> generate_clangd.gen_clangd(
    ...     flags_to_skip=['-my-custom-flag'],
    ...     flag_transforms={'-old-flag': '-new-flag'}
    ... )

Example usage as script:
    $ python generate_clangd.py [environment_name]
"""

import json
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional


# Default GCC-specific flags that clangd doesn't recognize
_DEFAULT_FLAGS_TO_SKIP = {
    '-MMD',  # Dependency generation
    '-fno-rtti',  # Can sometimes cause issues with clangd
    '-fno-tree-switch-conversion',  # GCC-specific optimization
    '-fno-jump-tables',  # GCC-specific optimization
    '-freorder-blocks',  # GCC-specific optimization
    '-fstrict-volatile-bitfields',  # GCC-specific
    '-Wno-old-style-declaration',  # GCC-specific warning
    '-Wno-error=unused-but-set-variable',  # GCC-specific warning
    '-mlongcalls',  # Xtensa-specific, clang complains
}

# Default flag transformations (GCC -> Clang compatible)
_DEFAULT_FLAG_TRANSFORMS = {
}


def _get_pio_metadata(cwd: Optional[str] = None) -> Dict:
    """Run pio project metadata and return parsed JSON."""
    result = subprocess.run(
        ['pio', 'project', 'metadata', '--json-output'],
        capture_output=True,
        text=True,
        check=True,
        cwd=cwd
    )
    return json.loads(result.stdout)


def _generate_clangd_config(env_data: Dict,
                            flags_to_skip: set,
                            flag_transforms: Dict[str, str]) -> str:
    """Generate .clangd configuration content from environment data."""

    # Collect all includes from all categories (build, compatlib, toolchain)
    all_includes = []
    for category in ['build', 'compatlib', 'toolchain']:
        if category in env_data['includes']:
            for include_path in env_data['includes'][category]:
                all_includes.append(f"-I{include_path}")

    # Collect all defines
    all_defines = [f"-D{define}" for define in env_data['defines']]

    # Collect and filter compiler flags
    compiler_flags = []
    for flag in env_data['cxx_flags']:
        if flag not in flags_to_skip:
            # Transform flag if needed
            transformed_flag = flag_transforms.get(flag, flag)
            compiler_flags.append(transformed_flag)

    # Build the YAML content
    lines = [
        "CompileFlags:",
        "  Add:",
    ]

    # Add defines
    for define in all_defines:
        lines.append(f"    - {define}")

    # Add includes
    for include in all_includes:
        lines.append(f"    - {include}")

    # Add compiler flags
    for flag in compiler_flags:
        lines.append(f"    - {flag}")

    # Add compiler path
    lines.append(f"  Compiler: {env_data['cxx_path']}")

    return '\n'.join(lines) + '\n'


def _write_clangd_file(config_content: str, output_path: str = '.clangd') -> Path:
    """Write .clangd configuration to file."""
    clangd_path = Path(output_path)
    clangd_path.write_text(config_content)
    return clangd_path


def _get_config_stats(env_data: Dict, flags_to_skip: set) -> Dict[str, int]:
    """Get statistics about the configuration."""
    total_includes = sum(
        len(env_data['includes'].get(cat, []))
        for cat in ['build', 'compatlib', 'toolchain']
    )
    filtered_count = sum(1 for flag in env_data['cxx_flags'] if flag not in flags_to_skip)

    return {
        'defines': len(env_data['defines']),
        'includes': total_includes,
        'flags': filtered_count,
        'filtered_flags': len(env_data['cxx_flags']) - filtered_count,
    }


def gen_clangd(flags_to_skip: Optional[List[str]] = None,
               flag_transforms: Optional[Dict[str, str]] = None,
               env_name: Optional[str] = None,
               output_path: str = '.clangd',
               verbose: bool = True) -> Path:
    """
    Generate .clangd configuration file from PlatformIO project metadata.

    Args:
        flags_to_skip: Additional GCC flags to filter out (added to defaults)
        flag_transforms: Additional flag transformations (added to defaults)
        env_name: PlatformIO environment name (auto-detected if None)
        output_path: Output file path (default: '.clangd')
        verbose: Print progress messages (default: True)

    Returns:
        Path object of the generated .clangd file

    Raises:
        subprocess.CalledProcessError: If pio command fails
        json.JSONDecodeError: If metadata is not valid JSON
        KeyError: If expected metadata keys are missing
    """
    # Merge user-provided flags with defaults
    combined_flags_to_skip = _DEFAULT_FLAGS_TO_SKIP.copy()
    if flags_to_skip:
        combined_flags_to_skip.update(flags_to_skip)

    combined_flag_transforms = _DEFAULT_FLAG_TRANSFORMS.copy()
    if flag_transforms:
        combined_flag_transforms.update(flag_transforms)

    # Get metadata
    if verbose:
        print("Fetching PlatformIO project metadata...")
    metadata = _get_pio_metadata()

    # Determine which environment to use
    env_names = list(metadata.keys())

    if env_name:
        if env_name not in env_names:
            raise ValueError(
                f"Environment '{env_name}' not found. "
                f"Available: {', '.join(env_names)}"
            )
    else:
        # Use first environment if not specified
        env_name = env_names[0]
        if verbose and len(env_names) > 1:
            print(f"Multiple environments found: {', '.join(env_names)}")
            print(f"Using '{env_name}' (specify env_name to use another)")

    if verbose:
        print(f"Generating .clangd for environment: {env_name}")

    # Generate config
    env_data = metadata[env_name]
    config_content = _generate_clangd_config(
        env_data,
        combined_flags_to_skip,
        combined_flag_transforms
    )

    # Write .clangd file
    clangd_path = _write_clangd_file(config_content, output_path)

    # Display stats
    if verbose:
        stats = _get_config_stats(env_data, combined_flags_to_skip)
        print(f"✓ Generated {clangd_path.absolute()}")
        print(f"  - {stats['defines']} defines")
        print(f"  - {stats['includes']} include paths (build + compatlib + toolchain)")
        print(f"  - {stats['flags']} compiler flags (filtered {stats['filtered_flags']} GCC-specific)")

    return clangd_path


def _main():
    """Main function for CLI usage."""
    try:
        # Parse environment name from command line if provided
        env_name = sys.argv[1] if len(sys.argv) > 1 else None

        # Generate .clangd file
        gen_clangd(env_name=env_name, verbose=True)

    except subprocess.CalledProcessError as e:
        print(f"Error running pio command: {e}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}", file=sys.stderr)
        sys.exit(1)
    except (KeyError, ValueError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    _main()
