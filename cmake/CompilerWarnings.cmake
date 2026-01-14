# Compiler Warning Configuration
# Sets up strict warning flags for high code quality

function(set_compiler_warnings target)
    set(GCC_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )

    set(CLANG_WARNINGS
        ${GCC_WARNINGS}
        -Wno-unknown-warning-option
    )

    set(MSVC_WARNINGS
        /W4
        /WX
        /permissive-
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${target} PRIVATE ${GCC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE ${CLANG_WARNINGS})
    elseif(MSVC)
        target_compile_options(${target} PRIVATE ${MSVC_WARNINGS})
    endif()
endfunction()

# Option to treat warnings as errors (off by default)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(WARNINGS_AS_ERRORS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Werror)
    elseif(MSVC)
        add_compile_options(/WX)
    endif()
endif()
