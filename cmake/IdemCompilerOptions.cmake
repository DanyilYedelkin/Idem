include_guard(GLOBAL)

function(idem_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        if(IDEM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
            -Wold-style-cast -Wcast-align -Wnon-virtual-dtor -Woverloaded-virtual
            -Wimplicit-fallthrough)
        if(IDEM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Bit-identical results across toolchains depend on every translation unit that
# touches simulation math agreeing on the floating-point model, so this is
# propagated to consumers. GCC contracts a*b+c into FMA by default whenever the
# target has it; MSVC does not under /fp:precise.
function(idem_target_strict_fp target)
    if(MSVC)
        target_compile_options(${target} PUBLIC /fp:precise)
    else()
        target_compile_options(${target} PUBLIC -ffp-contract=off -fno-fast-math)
    endif()
endfunction()

if(IDEM_SANITIZE)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()
