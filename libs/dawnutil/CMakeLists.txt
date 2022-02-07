fips_begin_lib(dawnutil)
    fips_files(SampleUtils.cpp SampleUtils.h)
fips_end_lib()
dawn_add_deps(dawnutil)
if (FIPS_OSX)
    target_compile_definitions(dawnutil PRIVATE DAWN_ENABLE_BACKEND_METAL)
endif()
target_include_directories(dawnutil INTERFACE .)

