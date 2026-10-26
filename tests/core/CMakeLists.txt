add_executable(msa_core_tests
    smoke_test.cpp
)

target_compile_features(msa_core_tests PRIVATE cxx_std_20)
target_link_libraries(msa_core_tests PRIVATE msa_core GTest::gtest_main)
msa_apply_warnings(msa_core_tests)

include(GoogleTest)
gtest_discover_tests(msa_core_tests)
