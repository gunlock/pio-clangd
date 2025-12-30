#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "clangd.h"

TEST_CASE("essential_flag identifies critical compiler flags", "[utilities]") {

  SECTION("Include path flags") {
    REQUIRE(essential_flag("-I/usr/include"));
    REQUIRE(essential_flag("-I"));
    REQUIRE(essential_flag("-isystem/usr/local/include"));
    REQUIRE(essential_flag("-isystem"));
    REQUIRE(essential_flag("-iquote./include"));
    REQUIRE(essential_flag("-iquote"));
    REQUIRE(essential_flag("-imacros/path/to/file"));
    REQUIRE(essential_flag("-imacros"));
    REQUIRE(essential_flag("-include/path/to/header.h"));
    REQUIRE(essential_flag("-include"));
  }

  SECTION("Macro definition flags") {
    REQUIRE(essential_flag("-DDEBUG"));
    REQUIRE(essential_flag("-D_GNU_SOURCE"));
    REQUIRE(essential_flag("-DARDUINO=10819"));
    REQUIRE(essential_flag("-D"));
    REQUIRE(essential_flag("-UTEST_MODE"));
    REQUIRE(essential_flag("-U_DEBUG"));
    REQUIRE(essential_flag("-U"));
  }

  SECTION("Standard and target flags") {
    REQUIRE(essential_flag("-std=c++23"));
    REQUIRE(essential_flag("-std=c++20"));
    REQUIRE(essential_flag("-std=gnu++17"));
    REQUIRE(essential_flag("-std=c11"));
    REQUIRE(essential_flag("--target=arm-none-eabi"));
    REQUIRE(essential_flag("--target=x86_64-linux-gnu"));
    REQUIRE(essential_flag("--sysroot=/opt/sysroot"));
    REQUIRE(essential_flag("--sysroot"));
  }

  SECTION("Architecture flags") {
    REQUIRE(essential_flag("-march=armv7-m"));
    REQUIRE(essential_flag("-march=native"));
    REQUIRE(essential_flag("-mcpu=cortex-m4"));
    REQUIRE(essential_flag("-mcpu=cortex-a53"));
    REQUIRE(essential_flag("-mthumb"));
    REQUIRE(essential_flag("-mfpu=fpv4-sp-d16"));
    REQUIRE(essential_flag("-mfpu=neon"));
    REQUIRE(essential_flag("-mfloat-abi=hard"));
    REQUIRE(essential_flag("-mfloat-abi=soft"));
    REQUIRE(essential_flag("-mabi=aapcs"));
    REQUIRE(essential_flag("-mabi=lp64"));
  }

  SECTION("Non-essential flags are rejected") {
    REQUIRE_FALSE(essential_flag("-O2"));
    REQUIRE_FALSE(essential_flag("-O0"));
    REQUIRE_FALSE(essential_flag("-Os"));
    REQUIRE_FALSE(essential_flag("-g"));
    REQUIRE_FALSE(essential_flag("-g3"));
    REQUIRE_FALSE(essential_flag("-Wall"));
    REQUIRE_FALSE(essential_flag("-Werror"));
    REQUIRE_FALSE(essential_flag("-Wextra"));
    REQUIRE_FALSE(essential_flag("-fPIC"));
    REQUIRE_FALSE(essential_flag("-fno-rtti"));
    REQUIRE_FALSE(essential_flag("-c"));
    REQUIRE_FALSE(essential_flag("-o"));
    REQUIRE_FALSE(essential_flag("-l"));
    REQUIRE_FALSE(essential_flag("-L"));
  }

  SECTION("Edge cases") {
    REQUIRE_FALSE(essential_flag(""));
    REQUIRE_FALSE(essential_flag(" "));
    REQUIRE_FALSE(essential_flag("not_a_flag"));
    REQUIRE_FALSE(essential_flag("file.cpp"));
  }
}

TEST_CASE("tokenize_command splits by spaces correctly", "[utilities]") {

  SECTION("Simple command with single spaces") {
    auto tokens = tokenize_command("g++ -c -o file.o file.cpp");
    std::vector<std::string> result;
    for (auto token : tokens) {
      result.emplace_back(token);
    }

    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == "g++");
    REQUIRE(result[1] == "-c");
    REQUIRE(result[2] == "-o");
    REQUIRE(result[3] == "file.o");
    REQUIRE(result[4] == "file.cpp");
  }

  SECTION("Command with multiple consecutive spaces") {
    auto tokens = tokenize_command("gcc   -I/path    -DDEBUG");
    std::vector<std::string> result;
    for (auto token : tokens) {
      result.emplace_back(token);
    }

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "gcc");
    REQUIRE(result[1] == "-I/path");
    REQUIRE(result[2] == "-DDEBUG");
  }

  SECTION("Empty command string") {
    auto tokens = tokenize_command("");
    std::vector<std::string> result;
    for (auto token : tokens) {
      result.emplace_back(token);
    }

    REQUIRE(result.empty());
  }

  SECTION("Command with leading/trailing spaces") {
    auto tokens = tokenize_command("  g++ -c file.cpp  ");
    std::vector<std::string> result;
    for (auto token : tokens) {
      result.emplace_back(token);
    }

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "g++");
    REQUIRE(result[1] == "-c");
    REQUIRE(result[2] == "file.cpp");
  }

  SECTION("Long command with many flags") {
    auto tokens = tokenize_command(
        "arm-none-eabi-g++ -DARDUINO=10819 -I/path1 -I/path2 -std=c++17 "
        "-march=armv7-m -O2 -Wall -c main.cpp");
    std::vector<std::string> result;
    for (auto token : tokens) {
      result.emplace_back(token);
    }

    REQUIRE(result.size() == 10);
    REQUIRE(result[0] == "arm-none-eabi-g++");
    REQUIRE(result[1] == "-DARDUINO=10819");
  }
}

TEST_CASE("process_tokens filters compiler flags correctly", "[utilities]") {

  SECTION("Filters essential flags from vector") {
    std::vector<std::string> args = {
        "arm-none-eabi-g++",
        "-DARDUINO=10819",
        "-I/path/to/include",
        "-O2",     // Should be filtered out
        "-Wall",   // Should be filtered out
        "-std=c++17",
        "-march=armv7-m",
        "-g"       // Should be filtered out
    };

    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    REQUIRE(filtered.size() == 4);
    REQUIRE(filtered[0] == "-DARDUINO=10819");
    REQUIRE(filtered[1] == "-I/path/to/include");
    REQUIRE(filtered[2] == "-std=c++17");
    REQUIRE(filtered[3] == "-march=armv7-m");
  }

  SECTION("Handles flags with separate value arguments") {
    std::vector<std::string> args = {
        "g++",
        "-I", "/usr/include",             // -I with separate path
        "-isystem", "/usr/local/include", // -isystem with separate path
        "-DDEBUG",
        "-o", "output.o"                  // -o is not essential
    };

    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    REQUIRE(filtered.size() == 5);
    REQUIRE(filtered[0] == "-I");
    REQUIRE(filtered[1] == "/usr/include");
    REQUIRE(filtered[2] == "-isystem");
    REQUIRE(filtered[3] == "/usr/local/include");
    REQUIRE(filtered[4] == "-DDEBUG");
  }

  SECTION("Handles mixed flag formats") {
    std::vector<std::string> args = {
        "gcc",
        "-I/combined/path",     // Combined format
        "-I", "/separate/path", // Separate format
        "-DFOO",                // Combined format (only way -D works)
        "-DBAR=123",            // Combined with value
        "-std=c++20"
    };

    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    REQUIRE(filtered.size() == 6);
    REQUIRE(filtered[0] == "-I/combined/path");
    REQUIRE(filtered[1] == "-I");
    REQUIRE(filtered[2] == "/separate/path");
    REQUIRE(filtered[3] == "-DFOO");
    REQUIRE(filtered[4] == "-DBAR=123");
    REQUIRE(filtered[5] == "-std=c++20");
  }

  SECTION("Handles empty input") {
    std::vector<std::string> args;
    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    REQUIRE(filtered.empty());
  }

  SECTION("Filters out all non-essential flags") {
    std::vector<std::string> args = {
        "gcc",
        "-O3",
        "-Wall",
        "-Wextra",
        "-fPIC",
        "-c"
    };

    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    REQUIRE(filtered.empty());
  }

  SECTION("Handles flag at end requiring value") {
    std::vector<std::string> args = {
        "gcc",
        "-DFOO",
        "-I"  // Flag that expects value but none provided
    };

    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    // Should include -DFOO and -I (but no value after -I)
    REQUIRE(filtered.size() == 2);
    REQUIRE(filtered[0] == "-DFOO");
    REQUIRE(filtered[1] == "-I");
  }

  SECTION("Handles --sysroot with separate value") {
    std::vector<std::string> args = {
        "arm-none-eabi-gcc",
        "--sysroot", "/opt/cross/sysroot",
        "-DTEST"
    };

    std::vector<std::string> filtered;
    process_tokens(args, filtered);

    REQUIRE(filtered.size() == 3);
    REQUIRE(filtered[0] == "--sysroot");
    REQUIRE(filtered[1] == "/opt/cross/sysroot");
    REQUIRE(filtered[2] == "-DTEST");
  }

  SECTION("Works with tokenize_command output") {
    auto tokens = tokenize_command(
        "gcc -I/path -DFOO -O2 -Wall -std=c++17 -march=native -g");

    std::vector<std::string> filtered;
    process_tokens(tokens, filtered);

    REQUIRE(filtered.size() == 4);
    REQUIRE(filtered[0] == "-I/path");
    REQUIRE(filtered[1] == "-DFOO");
    REQUIRE(filtered[2] == "-std=c++17");
    REQUIRE(filtered[3] == "-march=native");
  }
}
