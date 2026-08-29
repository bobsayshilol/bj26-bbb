if (EMSCRIPTEN)
    message(WARNING "Bug in SDL 3.2.4 with sanitizers enabled")
    set(BUILD_WITH_SANITIZERS OFF)
else()
    option(BUILD_WITH_SANITIZERS "Enable sanitizers" ON)
endif()

# Language standards.
set(CMAKE_C_STANDARD 20)
set(CMAKE_CXX_STANDARD 23)

# Faster builds.
set(CMAKE_OPTIMIZE_DEPENDENCIES ON)

# Always have assertions
add_compile_options(-UNDEBUG)

# Per-compiler flags.
if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
    add_compile_options(
        # Warnings.
        -Wall -Wextra -Werror #-Wpedantic
        -Wno-unused-const-variable
        -Wno-c99-designator
        # Enable debug info.
        -g
    )
    add_link_options(
        # Enable debug info.
        -g
    )

elseif (MSVC)
    add_compile_options(
        # Warnings.
        /W1 /WX
        # Be standard compliant.
        /permissive-
    )
    add_compile_definitions(
        # See list of NO* macros in Windows.h.
        WIN32_LEAN_AND_MEAN NOMINMAX
    )

endif()

if (BUILD_WITH_SANITIZERS)
    if (MSVC)
        add_compile_options(/fsanitize=address)
    else()
        set(_sanitizer_flags -fsanitize=address,undefined)
        add_compile_options(${_sanitizer_flags})
        add_link_options(${_sanitizer_flags})
    endif()
endif()

if (EMSCRIPTEN)
    # Output as a HTML.
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    add_compile_options(
        -O3
    )
    add_link_options(
        # The linker also needs optimisations enabled.
        -O3

        # Allow sleeping in the main loop.
        -sASYNCIFY

        # Debug checks
        #-sSTACK_OVERFLOW_CHECK=2
        #-sASSERTIONS=1

        # Source file mappings in debug data.
        #-gsource-map

        # Single HTML.
        -sSINGLE_FILE=1
    )
endif()
