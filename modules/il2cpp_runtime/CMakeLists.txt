add_library(fc_il2cpp_runtime INTERFACE)

target_include_directories(fc_il2cpp_runtime INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(fc_il2cpp_runtime INTERFACE cxx_std_17)

target_compile_definitions(fc_il2cpp_runtime INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
