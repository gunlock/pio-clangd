cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(
    pio-clangd 
    VERSION 0.2.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Setup for boost
set(BOOST_INCLUDE_LIBRARIES program_options unordered CACHE STRING "")
set(BOOST_ENABLE_CMAKE ON CACHE BOOL "Enable Boost CMake support")
set(BOOST_VERSION "boost-1.90.0")
set(BOOST_URL_BASE "https://github.com/boostorg/boost/releases/download")
if(WIN32)
    set(BOOST_FILE_EXT "zip")
    set(BOOST_HASH "SHA256=89dc8dbdd61b0212dfff5286a07227cc40d0243f344037739b7c7f7f4a6c3584")
else()
    set(BOOST_FILE_EXT "tar.gz")
    set(BOOST_HASH "SHA256=913ca43d49e93d1b158c9862009add1518a4c665e7853b349a6492d158b036d4")
endif()
# Suppress verbose download progress output
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    boost
    URL "${BOOST_URL_BASE}/${BOOST_VERSION}/${BOOST_VERSION}-cmake.${BOOST_FILE_EXT}"
    URL_HASH ${BOOST_HASH}
    DOWNLOAD_NO_PROGRESS TRUE
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        12.1.0
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG        v6.2.0
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.12.0
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
)

#--------------------------------------------------------------------
# Check if AVX2 SIMD optimization are present on the build system
# for glaze optimization
#
# Assumption: This is not a cross-compile build
#--------------------------------------------------------------------
include(CheckSourceRuns)
include(CheckCXXCompilerFlag)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(AVX2_FLAG "-mavx2")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(AVX2_FLAG "/arch:AVX2")
endif()

# check if AVX2 flag is non-empty and the code runs
if(AVX2_FLAG)
    # Temporarily add the flag and check if a simple intrinsic test runs
    set(CMAKE_REQUIRED_FLAGS "${AVX2_FLAG}")

    # Use a small test program with an AVX2 intrinsic 
    # (like _mm256_setzero_si256)
    check_source_runs(
        CXX
        "
        #include <immintrin.h>
        int main() {
            __m256i v = _mm256_setzero_si256();
            (void) v;
            return 0;
        }
        "
        HAVE_AVX2_RUNTIME_SUPPORT
    )
endif()

# Set the glaze option if supported by system
if(HAVE_AVX2_RUNTIME_SUPPORT)
    # The CPU supports it and the compiler can generate code for it
    set(
        glaze_ENABLE_AVX2 ON CACHE BOOL 
        "Force AVX2 enabled because host CPU supports it." 
        FORCE)
    message(STATUS "Glaze AVX2 optimizations enabled.")
else()
    # The CPU does NOT support it, or the compiler failed
    set(
        glaze_ENABLE_AVX2 OFF CACHE BOOL 
        "Force AVX2 disabled because host CPU does not support it." 
        FORCE)
    message(STATUS "Glaze AVX2 optimizations disabled.")
endif()

#-------------------------END AVX2 Check ----------------------------

#--------------------------------------------------------------------
# Generate a status message regarding whether boost will need to
# be downloaded
#--------------------------------------------------------------------
# Query the properties
FetchContent_GetProperties(boost)

# Define the path to the internal 'complete' stamp created by CMake's
# subbuild once the download/extract is successful
set(BOOST_STAMP_DIR "${FETCHCONTENT_BASE_DIR}/boost-subbuild/boost-populate-prefix/src/boost-populate-stamp")
set(BOOST_DONE_STAMP "${BOOST_STAMP_DIR}/boost-populate-done")
if(NOT EXISTS "${BOOST_DONE_STAMP}")
    message(STATUS "Boost ${BOOST_VERSION} not found locally.")
    message(STATUS "Downloading ${BOOST_VERSION}...(this may take a moment)")
endif()
#--------------- END boost download status message ------------------

FetchContent_MakeAvailable(boost)
FetchContent_MakeAvailable(fmt)
FetchContent_MakeAvailable(glaze)
FetchContent_MakeAvailable(Catch2)

#--------------------------------------------------------------------
#  Library (for sharing code between main executable and tests)
#--------------------------------------------------------------------
set(PIO_CLANGD_LIB pio-clangd-lib)
add_library(${PIO_CLANGD_LIB} OBJECT
    src/clangd.cpp
    include/clangd.h
)

target_include_directories(${PIO_CLANGD_LIB}
    PUBLIC
        include
)

target_link_libraries(${PIO_CLANGD_LIB}
    PUBLIC
        fmt::fmt
        Boost::headers
        Boost::program_options
        Boost::unordered  # Required for Xcode generator to include header-only library paths
        glaze::glaze
)

# Enable C++23 for the library
target_compile_features(${PIO_CLANGD_LIB} PUBLIC cxx_std_23)

#--------------------------------------------------------------------
#  Main Executable
#--------------------------------------------------------------------
add_executable(pio-clangd
    src/main.cpp
)

target_link_libraries(pio-clangd PRIVATE ${PIO_CLANGD_LIB})

#--------------------------------------------------------------------
#  Testing
#--------------------------------------------------------------------
# Only build tests if this is the main project (not a subproject)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()

    # Add test executable
    add_executable(test-suite
        tests/test_utilities.cpp
        tests/test_parsing.cpp
    )

    target_link_libraries(test-suite
        PRIVATE
            ${PIO_CLANGD_LIB}
            Catch2::Catch2WithMain
    )

    # Ensure test executable uses C++23
    target_compile_features(test-suite PRIVATE cxx_std_23)

    # Register test suite with CTest
    add_test(NAME test-suite COMMAND test-suite)
endif()
