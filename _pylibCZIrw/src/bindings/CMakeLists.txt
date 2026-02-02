# _pylibCZIrw is the actual python module - it links to the _pylibCZIrw_API library (and in turn to libCZI and other dependencies)

pybind11_add_module(_pylibCZIrw MODULE 
        CZIrw.cpp 
        PbHelper.h 
        PbHelper.cpp
        include_python.h)

target_link_libraries(_pylibCZIrw PRIVATE _pylibCZIrw_API)
target_include_directories(_pylibCZIrw PRIVATE ${libCZI_SOURCE_DIR})
target_compile_features(_pylibCZIrw PRIVATE cxx_std_17)

# After the bindings module is built, copy the resulting .pyd to the desired location
if(MSVC)
    # Custom target that always runs when explicitly built
    add_custom_target(copy_pyd ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "V:/Repos/pylibczirw/out/build/x64-Release/_pylibCZIrw/src/bindings/_pylibCZIrw.cp314-win_amd64.pyd"
            "V:/Repos/pylibczirw/pylibCZIrw/_pylibCZIrw.pyd"
        COMMENT "Copying built _pylibCZIrw .pyd to pylibCZIrw package directory"
        VERBATIM
    )

    # Ensure the copy happens after the module is built
    add_dependencies(copy_pyd _pylibCZIrw)
endif()