#include "clangd.h"
#include <fmt/core.h>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>

using std::expected;
using std::string;
using std::string_view;
using std::unexpected;
using std::vector;

namespace fs = std::filesystem;

expected<vector<string>, string> get_envs(const string& proj_path) {
  auto ini_path = fs::path{proj_path} / "platformio.ini";

  if (!fs::exists(ini_path)) {
    return unexpected(fmt::format("{} not found", ini_path.string()));
  }

  std::ifstream file(ini_path);
  if (!file.is_open()) {
    return unexpected(fmt::format("Failed to open {}", ini_path.string()));
  }

  // parse platformio.ini where regex matches [env:name]
  // 'name' can contain alphanumeric, underscore, and hypens
  std::regex env_pattern(R"(\[env:([a-zA-Z0-9_\-]+)\])");

  vector<string> environments;
  string line;

  while (std::getline(file, line)) {
    std::smatch match;
    if (std::regex_search(line, match, env_pattern)) {
      // match[1] contains the environment name in first capture group
      environments.push_back(match[1].str());
    }
  }

  if (environments.empty()) {
    return unexpected(
        fmt::format("No environments found in {}", ini_path.string()));
  }

  return environments;
}

/*
 * What this does...
 * 1. Parses platformio.ini to extract all PlatformIO environments
 * 2. Reads compile_commands.json from each environment's build directory
 * 3. Create a unified compile_commands.json at the project root, prioritizing
 * the target environment
 * 4. Filters compiler flags to only include those essential for clangd LSP
 * analysis
 *
 * NOTE: Normalizing the .pio/libdep paths are the key to deduplication
 */
int gen_cmds(const string& proj_path, const string& environment) {
  auto environments = get_envs(proj_path);

  if (!environments) {
    fmt::println(stderr, "{}", environments.error());
    return EXIT_FAILURE;
  } else if (environments->empty()) {
    fmt::println(stderr, "No environments found in platformio.ini");
    return EXIT_FAILURE;
  }

  // If the target environment is not provided, set it to the first
  // environment found
  string target_env = environment.empty() ? (*environments)[0] : environment;

  // Validate that target_env exists in the list of environments
  auto env_exists =
      std::ranges::find(*environments, target_env) != environments->end();
  if (!env_exists) {
    fmt::println(stderr,
                 "Warning: Environment '{}' not found in platformio.ini",
                 target_env);
    target_env = (*environments)[0];
    fmt::println(stderr, "Falling back to environment '{}'", target_env);
  }

  boost::unordered_flat_map<string, vector<CompileCommand>> db;
  std::mutex db_mtx;

  vector<string> errors;
  std::mutex error_mtx;

  auto thread_proc = [&](string& env) -> void {
    auto compile_commands_path =
        fs::path{proj_path} / ".pio" / "build" / env / "compile_commands.json";
    vector<CompileCommand> compile_commands;
    auto err = glz::read_file_json(compile_commands,
                                   compile_commands_path.string(), string{});
    if (err) {
      std::scoped_lock lock(error_mtx);
      errors.push_back(fmt::format("Failed to read {}: {}",
                                   compile_commands_path.string(),
                                   glz::format_error(err)));
      return;
    }

    std::scoped_lock lock(db_mtx);
    db.emplace(env, std::move(compile_commands));
  };  // end of thread_proc()

  // scoped block provides implicit auto joins for worker threads
  // when 'workers' goes out scope
  {
    vector<std::jthread> workers;
    for (auto& env : *environments) {
      workers.emplace_back(thread_proc, std::ref(env));
    }
  }

  // Report any errors that occurred during processing
  if (!errors.empty()) {
    for (const auto& error : errors) {
      fmt::println(stderr, "{}", error);
    }
    fmt::println(stderr, "Failed to process {}/{} environment(s)",
                 errors.size(), environments->size());
    return EXIT_FAILURE;
  }

  // Calculate statistics
  size_t total_commands = 0;
  for (const auto& [env_name, commands] : db) {
    total_commands += commands.size();
  }

  auto target_it = db.find(target_env);
  size_t target_env_commands =
      (target_it != db.end()) ? target_it->second.size() : 0;

  fmt::println("Loaded {} environment(s) with {} total compile commands",
               db.size(), total_commands);
  fmt::println("Target environment: '{}' ({} commands)", target_env,
               target_env_commands);

  // Create filtered_commands map with normalized deduplication keys
  boost::unordered_flat_map<string, CompileCommand> filtered_commands;

  // Helper to create deduplication key - normalizes libdeps paths
  // to handle environment-specific library directories
  auto make_dedup_key = [](const CompileCommand& cmd) -> string {
    auto fq_path = (fs::path{cmd.directory} / cmd.file).lexically_normal();
    string path_str = fq_path.string();

    // For libdeps paths, normalize by removing environment-specific segment
    // Pattern: .pio/libdeps/ENV_NAME/LIBRARY/... -> .pio/libdeps/LIBRARY/...
    static const string libdeps_marker = ".pio/libdeps/";
    auto pos = path_str.find(libdeps_marker);
    if (pos != string::npos) {
      auto after_libdeps = pos + libdeps_marker.length();
      auto next_slash = path_str.find('/', after_libdeps);
      if (next_slash != string::npos) {
        // Remove the env name segment
        path_str.erase(after_libdeps, next_slash - after_libdeps + 1);
      }
    }

    return path_str;
  };

  // Add target_env entries first (highest priority)
  if (target_it != db.end()) {
    // Reserve capacity: estimate 150% of target env size for all environments
    filtered_commands.reserve(target_it->second.size() * 3 / 2);

    for (auto& cmd : target_it->second) {
      filtered_commands.emplace(make_dedup_key(cmd), std::move(cmd));
    }
  }

  // Add other environment entries (only if file not already present)
  for (auto& [env_name, commands] : db) {
    if (env_name == target_env)
      continue;  // Already processed

    for (auto& cmd : commands) {
      string dedup_key = make_dedup_key(cmd);
      // Only insert if this file path doesn't exist yet
      if (!filtered_commands.contains(dedup_key)) {
        filtered_commands.emplace(std::move(dedup_key), std::move(cmd));
      }
    }
  }

  fmt::println("Deduplicated to {} unique source files",
               filtered_commands.size());

  // Filter flags for each entry
  for (auto& [fq_path, cmd] : filtered_commands) {
    vector<string> filtered;
    filtered.reserve(cmd.arguments.empty() ? 20 : cmd.arguments.size());

    // compile_commands.json may use either arguments array or command string
    if (!cmd.arguments.empty()) {
      process_tokens(cmd.arguments, filtered);
    } else if (!cmd.command.empty()) {
      process_tokens(tokenize_command(cmd.command), filtered);
    }

    cmd.arguments = std::move(filtered);
    cmd.command.clear();  // Clear redundant command field
  }

  // Extract values into vector for JSON output
  vector<CompileCommand> output_commands;
  output_commands.reserve(filtered_commands.size());
  for (auto& [_, cmd] : filtered_commands) {
    output_commands.push_back(std::move(cmd));
  }

  // Write compile_commands.json to project root
  auto output_path = fs::path{proj_path} / "compile_commands.json";
  auto write_err =
      glz::write_file_json(output_commands, output_path.string(), string{});
  if (write_err) {
    fmt::println(stderr, "Failed to write {}: {}", output_path.string(),
                 glz::format_error(write_err));
    return EXIT_FAILURE;
  }

  fmt::println("Successfully wrote {} with {} entries",
               output_path.filename().string(), output_commands.size());
  fmt::println("Reduction: {} -> {} commands ({:.1f}%)", total_commands,
               output_commands.size(),
               (100 - (output_commands.size() * 100.0) / total_commands));

  return EXIT_SUCCESS;
}
