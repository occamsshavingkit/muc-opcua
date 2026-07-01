# cmake/MucOpcUaCodegen.cmake
#
# Size-oriented code generation. Embedded firmware wants the linker to drop any
# function/object it doesn't reference ("compile in only what you use"). That
# requires per-symbol sections at compile time AND --gc-sections at the final
# executable link. We compile the library with -ffunction-sections/-fdata-sections
# and export --gc-sections as an INTERFACE link option, so every executable that
# links muc_opcua strips unreferenced code automatically.
#
# MUC_OPCUA_OPTIMIZE_SIZE additionally compiles with -Os. It defaults ON
# because this library targets flash-constrained devices; use
# -DMUC_OPCUA_OPTIMIZE_SIZE=OFF for local debug or speed-oriented experiments.

option(MUC_OPCUA_OPTIMIZE_SIZE "Optimize the library for size (-Os)" ON)

# Apply size-friendly compile options to a target the library owns.
function(muc_opcua_apply_codegen target_name)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE -ffunction-sections -fdata-sections)
        if(MUC_OPCUA_OPTIMIZE_SIZE)
            target_compile_options(${target_name} PRIVATE -Os)
        endif()
        # Strip unreferenced sections at the final link of any consumer.
        target_link_options(${target_name} INTERFACE -Wl,--gc-sections)
    endif()

    if(MUC_OPCUA_LTO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_err)
        if(_ipo_ok)
            set_target_properties(${target_name} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
        else()
            message(WARNING "MUC_OPCUA_LTO requested, but IPO/LTO is not supported: ${_ipo_err}")
        endif()
    endif()
endfunction()
