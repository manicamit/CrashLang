cmake_minimum_required(VERSION 3.20)
project(CrashLang
    VERSION 0.1.0
    DESCRIPTION "A programming language where every runtime error explains itself"
    LANGUAGES CXX
)

# ── C++17 required ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler warnings ──────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion
    -Wshadow -Wdouble-promotion
    -Wformat=2
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wnull-dereference
)

# ── Optional AddressSanitizer ───────────────────────────────────────────────────
option(SANITIZE "Enable AddressSanitizer + UBSan" OFF)
if(SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ── Collect sources ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE CRASHLANG_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# ── Main executable ─────────────────────────────────────────────────────────────
add_executable(crashlang ${CRASHLANG_SOURCES})
target_include_directories(crashlang PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ── Install ─────────────────────────────────────────────────────────────────────
install(TARGETS crashlang RUNTIME DESTINATION bin)
