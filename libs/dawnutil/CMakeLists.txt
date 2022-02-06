fips_begin_lib(dawnutil)
    fips_files(SampleUtils.cpp SampleUtils.h)
    fips_libs(
        SPIRV-Tools
        SPIRV-Tools-opt
        tint
        tint_diagnostic_utils
        absl_str_format_internal
        absl_strings
        dawncpp_headers
        dawn_headers
        dawncpp
        dawn_proc
        dawn_native
        dawn_utils
        dawn_platform
        dawn_wire
        dawn_common
        glfw3
    )
fips_end_lib(dawnutil)
if (FIPS_OSX)
    target_compile_definitions(dawnutil PRIVATE DAWN_ENABLE_BACKEND_METAL)
endif()
target_include_directories(dawnutil INTERFACE .)

