#pragma once
#include <algorithm>
#include <array>
#include <expected>
#include <glaze/glaze.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

// generates compile_commands.json in project root
int gen_cmds(const std::string& proj_path, const std::string& environment);

/*--------------------------------------
 *  Utility functions and structures
 *------------------------------------- */

// Data schema for glaze
struct CompileCommand {
  std::string directory{};
  std::string file{};
  std::string command{};
  std::vector<std::string> arguments{};
  std::optional<std::string> output{};

  struct glaze {
    using T = CompileCommand;
    static constexpr auto value = glz::object(
      "directory", &T::directory,
      "file", &T::file,
      "command", &T::command,
      "arguments", &T::arguments,
      "output", &T::output);
  };
};

// These prefixes should cover most of what clangd needs for semantic analysis.
// Any flag not starting with these will be removed.
// Must be sorted for std::upper_bound.
inline constexpr std::array<std::string_view, 16> STEMS = {
    "--sysroot",     // Cross-compile root
    "--target",      // Target triple
    "-D",            // Macros
    "-I",            // Include paths
    "-U",            // Undefine macros
    "-imacros",      // Macro includes
    "-include",      // Force includes
    "-iquote",       // Quote headers
    "-isystem",      // System headers
    "-mabi=",        // Architecture
    "-march=",       // Architecture
    "-mcpu=",        // Architecture
    "-mfloat-abi=",  // Architecture
    "-mfpu=",        // Architecture
    "-mthumb",       // Architecture
    "-std="          // Language standard
};

// Flags that require a separate value argument
// Must be sorted for std::binary_search.
inline constexpr std::array<std::string_view, 4> FLAGS_WITH_VALUES = {
    "--sysroot", "-I", "-include", "-isystem"};

// Determines if a flag is essential for LSP semantic analysis.
constexpr bool essential_flag(std::string_view token) {
  if (token.empty()) {
    return false;
  }

  // Find the first element that is GREATER than the token
  auto it = std::upper_bound(STEMS.begin(), STEMS.end(), token);

  // The match must be at the position immediately before 'it'
  if (it != STEMS.begin()) {
    auto prev = std::prev(it);
    if (token.starts_with(*prev)) {
      return true;
    }
  }
  return false;
}

// Tokenize command string (cmd) with space separtor
// Returns a list of views pointing directly command string
// Zero copy
inline auto tokenize_command(std::string_view cmd) {
  // 1. Split by space
  // 2. Filter out empty tokens (handles multiple spaces)
  // 3. Transform each subrange back into a string_view
  return cmd | std::views::split(' ') |
         std::views::filter([](auto&& rng) { return !rng.empty(); }) |
         std::views::transform(
             [](auto&& rng) { return std::string_view(rng); });
}

// Process tokens from a range and filter essential flags
inline void process_tokens(auto&& tokens_range,
                           std::vector<std::string>& filtered) {
  auto it = std::ranges::begin(tokens_range);
  auto end = std::ranges::end(tokens_range);
  if (it == end) {
    return;  // range is empty, nothing to do
  }
  for (++it; it != end; ++it) {
    std::string_view arg{*it};
    if (essential_flag(arg)) {
      filtered.push_back(std::string(arg));

      // Handle Flags with Separate Values (-I /path)
      if (std::binary_search(FLAGS_WITH_VALUES.begin(), FLAGS_WITH_VALUES.end(),
                             arg) &&
          std::next(it) != end &&
          !std::string_view(*std::next(it)).starts_with('-')) {
        filtered.push_back(std::string(std::string_view(*++it)));
      }
    }
  }
}

/*-------------------------------------------------------------------
 *  get_env()
 *
 *  Parses platformio.ini to extract environment names
 *
 *  Params:
 *    project_path  path to platformio.ini or directory containing it
 *  Returns result wrapped in std::expected:
 *    Success: environment names as vector<string>
 *    Error: error message as string
 *
 *-----------------------------------------------------------------*/
std::expected<std::vector<std::string>, std::string> get_envs(
    const std::string& proj_path);
