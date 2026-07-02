cmake_minimum_required(VERSION 3.23)

project(FishingCompanionUnified LANGUAGES C CXX)

option(FC_BUILD_OVERLAY "Build the FishingCompanion overlay DLL" ON)
option(FC_BUILD_IL2CPP_TOOLING "Build standalone IL2CPP tooling modules" OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build this project as x64.")
endif()

add_subdirectory(modules/il2cpp_runtime)

if(FC_BUILD_OVERLAY)
    set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
    set(MINHOOK_DIR "${CMAKE_SOURCE_DIR}/third_party/minhook")

    if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
        message(FATAL_ERROR "Dear ImGui not found. Put it into third_party/imgui.")
    endif()

    if(NOT EXISTS "${MINHOOK_DIR}/include/MinHook.h")
        message(FATAL_ERROR "MinHook not found. Put it into third_party/minhook.")
    endif()

    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    )

    file(GLOB_RECURSE MINHOOK_SOURCES CONFIGURE_DEPENDS
        "${MINHOOK_DIR}/src/*.c"
    )

    add_library(fc_overlay_common INTERFACE)
    target_compile_features(fc_overlay_common INTERFACE cxx_std_17)
    target_include_directories(fc_overlay_common INTERFACE
        ${CMAKE_SOURCE_DIR}/src
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${MINHOOK_DIR}/include
    )
    target_compile_definitions(fc_overlay_common INTERFACE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        FCSDK_BUILD
    )

    if(MSVC)
        target_compile_options(fc_overlay_common INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        )
    endif()

    function(fc_add_object_module target)
        add_library(${target} OBJECT ${ARGN})
        target_link_libraries(${target} PRIVATE fc_overlay_common)
    endfunction()

    fc_add_object_module(fc_entry
        src/dllmain.cpp
    )

    fc_add_object_module(fc_core
        src/Core/Input.cpp
        src/Core/Overlay.cpp
    )

    fc_add_object_module(fc_actions
        src/Actions/ActionRuntime.cpp
    )

    fc_add_object_module(fc_features
        src/Features/KeyBinder.cpp
        src/Features/Settings.cpp
    )
    # Vendored nlohmann/json single-header, used only by fc_features to persist
    # user settings. Kept local so the heavy header stays out of other TUs.
    target_include_directories(fc_features PRIVATE ${CMAKE_SOURCE_DIR}/third_party/json)

    fc_add_object_module(fc_gui
        src/GUI/Menu.cpp
        src/GUI/Theme.cpp
        src/GUI/CommandPalette.cpp
        src/GUI/Tabs/ActionsTab.cpp
        src/GUI/Tabs/DashboardTab.cpp
        src/GUI/Tabs/HealthTab.cpp
        src/GUI/Tabs/LogsTab.cpp
        src/GUI/Tabs/SdkTab.cpp
        src/GUI/Tabs/SettingsTab.cpp
    )

    fc_add_object_module(fc_sdk
        src/SDK/FCSDK.cpp
        src/SDK/ModuleLoader.cpp
    )

    fc_add_object_module(fc_third_party
        ${IMGUI_SOURCES}
        ${MINHOOK_SOURCES}
    )

    add_library(FishingCompanion SHARED
        $<TARGET_OBJECTS:fc_entry>
        $<TARGET_OBJECTS:fc_core>
        $<TARGET_OBJECTS:fc_actions>
        $<TARGET_OBJECTS:fc_features>
        $<TARGET_OBJECTS:fc_gui>
        $<TARGET_OBJECTS:fc_sdk>
        $<TARGET_OBJECTS:fc_third_party>
    )

    target_link_libraries(FishingCompanion PRIVATE
        fc_overlay_common
        fc_il2cpp_runtime
        d3d11
        dxgi
        d3dcompiler
        shell32
    )

    add_dependencies(FishingCompanion
        fc_entry
        fc_core
        fc_actions
        fc_features
        fc_gui
        fc_sdk
        fc_third_party
    )

    set_target_properties(FishingCompanion PROPERTIES
        OUTPUT_NAME "FishingCompanion"
        PREFIX ""
    )
endif()

if(FC_BUILD_IL2CPP_TOOLING)
    add_subdirectory(modules/il2cpp_tooling)
endif()
