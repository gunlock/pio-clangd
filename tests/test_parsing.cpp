#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include "clangd.h"
#include <vector>
#include <string>
#include "test_fixtures.hpp"

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::VectorContains;

TEST_CASE("get_envs parses platformio.ini correctly", "[parsing][file-io]") {

  SECTION("Single environment") {
    TempProjectFixture fixture;
    fixture.create_platformio_ini({"esp32dev"});

    auto result = get_envs(fixture.get_path_string());

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    REQUIRE(result->at(0) == "esp32dev");
  }

  SECTION("Multiple environments") {
    TempProjectFixture fixture;
    fixture.create_platformio_ini({"esp32", "esp32s3", "esp32c3"});

    auto result = get_envs(fixture.get_path_string());

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE_THAT(*result, VectorContains(std::string("esp32")));
    REQUIRE_THAT(*result, VectorContains(std::string("esp32s3")));
    REQUIRE_THAT(*result, VectorContains(std::string("esp32c3")));
  }

  SECTION("Five environments") {
    TempProjectFixture fixture;
    fixture.create_platformio_ini({
        "dev",
        "staging",
        "production",
        "test",
        "debug"
    });

    auto result = get_envs(fixture.get_path_string());

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 5);
    REQUIRE_THAT(*result, VectorContains(std::string("dev")));
    REQUIRE_THAT(*result, VectorContains(std::string("staging")));
    REQUIRE_THAT(*result, VectorContains(std::string("production")));
    REQUIRE_THAT(*result, VectorContains(std::string("test")));
    REQUIRE_THAT(*result, VectorContains(std::string("debug")));
  }

  SECTION("Environment names with underscores and hyphens") {
    TempProjectFixture fixture;
    fixture.create_platformio_ini({"my_env", "test-env-1", "prod_2024"});

    auto result = get_envs(fixture.get_path_string());

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE_THAT(*result, VectorContains(std::string("my_env")));
    REQUIRE_THAT(*result, VectorContains(std::string("test-env-1")));
    REQUIRE_THAT(*result, VectorContains(std::string("prod_2024")));
  }

  SECTION("Environment names with numbers") {
    TempProjectFixture fixture;
    fixture.create_platformio_ini({"env123", "test2", "abc456def"});

    auto result = get_envs(fixture.get_path_string());

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE_THAT(*result, VectorContains(std::string("env123")));
    REQUIRE_THAT(*result, VectorContains(std::string("test2")));
    REQUIRE_THAT(*result, VectorContains(std::string("abc456def")));
  }

  SECTION("Error: platformio.ini not found") {
    TempProjectFixture fixture;
    // Don't create platformio.ini

    auto result = get_envs(fixture.get_path_string());

    REQUIRE_FALSE(result.has_value());
    REQUIRE_THAT(result.error(), ContainsSubstring("not found"));
  }

  SECTION("Error: no environments in platformio.ini") {
    TempProjectFixture fixture;
    fixture.create_empty_ini();

    auto result = get_envs(fixture.get_path_string());

    REQUIRE_FALSE(result.has_value());
    REQUIRE_THAT(result.error(), ContainsSubstring("No environments found"));
  }

  SECTION("Error: non-existent directory") {
    auto result = get_envs("/this/path/does/not/exist/at/all");

    REQUIRE_FALSE(result.has_value());
    REQUIRE_THAT(result.error(), ContainsSubstring("not found"));
  }

  SECTION("Error: empty path") {
    // Empty path should fail
    auto result = get_envs("");

    REQUIRE_FALSE(result.has_value());
  }

  SECTION("Environment order is preserved") {
    TempProjectFixture fixture;
    fixture.create_platformio_ini({"first", "second", "third"});

    auto result = get_envs(fixture.get_path_string());

    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    // Check order is preserved
    REQUIRE(result->at(0) == "first");
    REQUIRE(result->at(1) == "second");
    REQUIRE(result->at(2) == "third");
  }
}
