#include <fmt/core.h>
#include <boost/program_options.hpp>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include "clangd.h"

using std::ostringstream;
using std::string;

namespace po = boost::program_options;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  // parse command line args
  string proj_path;
  string environment;

  po::options_description desc(
      "Optimizes PlatformIO compile_commands.json for clangd");
  desc.add_options()("help,h", "Help message")(
      "path,p", po::value<string>(&proj_path),
      "Optional. Directory containing platformio.ini. Defaults to working "
      "directory.")("env,e", po::value<string>(&environment),
                    "Optional. Configure clangd to this environment. Defaults "
                    "to first environment if omitted.");

  // var map to store results
  po::variables_map var_map;

  try {
    po::store(po::parse_command_line(argc, argv, desc), var_map);

    // if --help or -h, hten output help message and exit
    if (var_map.count("help")) {
      ostringstream ss;
      ss << desc;
      fmt::println("{}", ss.str());
      return EXIT_SUCCESS;
    }
    po::notify(var_map);

  } catch (const po::error& e) {
    ostringstream ss;
    ss << desc;
    fmt::println(stderr, "Error: {}", e.what());
    fmt::println(stderr, "{}", ss.str());
    return EXIT_FAILURE;
  }

  proj_path = proj_path.empty() ? fs::current_path().string() : proj_path;
  proj_path = fs::absolute(proj_path);

  return gen_cmds(proj_path, environment);
}
