cmake_minimum_required(VERSION 3.23)

project(il2cpp_runtime_tooling LANGUAGES CXX)

function(configure_il2cpp_target target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()

    set(target_output_dir "${CMAKE_BINARY_DIR}/il2cpp_tooling")

    foreach(output_kind RUNTIME LIBRARY ARCHIVE)
        set_target_properties(${target} PROPERTIES
            ${output_kind}_OUTPUT_DIRECTORY ${target_output_dir}
        )
        foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_target_properties(${target} PROPERTIES
                ${output_kind}_OUTPUT_DIRECTORY_${config} ${target_output_dir}
            )
        endforeach()
    endforeach()
endfunction()

add_library(il2cpp_runtime_offsets SHARED
    src/dllmain.cpp
)
configure_il2cpp_target(il2cpp_runtime_offsets)

add_library(il2cpp_action_module SHARED
    action_module/action_module.cpp
)
configure_il2cpp_target(il2cpp_action_module)
target_link_libraries(il2cpp_action_module PRIVATE
    user32
    gdi32
    shell32
    comctl32
)
set_target_properties(il2cpp_action_module PROPERTIES
    OUTPUT_NAME il2cpp_action_module
)

add_executable(il2cpp_action_controller
    action_controller/action_controller.cpp
)
configure_il2cpp_target(il2cpp_action_controller)
target_link_libraries(il2cpp_action_controller PRIVATE
    user32
    gdi32
    shell32
    ole32
    comctl32
)
set_target_properties(il2cpp_action_controller PROPERTIES
    OUTPUT_NAME il2cpp_action_controller_rig_power
    WIN32_EXECUTABLE TRUE
)

add_library(il2cpp_interaction_module SHARED
    interaction_module/interaction_module.cpp
)
configure_il2cpp_target(il2cpp_interaction_module)

add_library(il2cpp_runtime_client SHARED
    runtime_client/runtime_client.cpp
)
configure_il2cpp_target(il2cpp_runtime_client)
