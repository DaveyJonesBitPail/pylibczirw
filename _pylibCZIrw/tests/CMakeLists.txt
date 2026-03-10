# Lines below are for building the test programs which can help debugging c++ execution and add sanity check.
set(TARGET_MEMTEST mem_check)
add_executable(${TARGET_MEMTEST} "read_memcheck.cpp")
target_include_directories(${TARGET_MEMTEST} PRIVATE ${libCZI_SOURCE_DIR})
target_link_libraries(${TARGET_MEMTEST} PRIVATE libCZIStatic JxrDecodeStatic _pylibCZIrw_API)

set(TARGET_EDIT_TESTS cziedit_tests)
add_executable(${TARGET_EDIT_TESTS} "edit_metadata_tests.cpp")
target_include_directories(${TARGET_EDIT_TESTS} PRIVATE ${libCZI_SOURCE_DIR})
target_link_libraries(${TARGET_EDIT_TESTS} PRIVATE libCZIStatic _pylibCZIrw_API)