cmake_minimum_required(VERSION 3.25)

project(Pawse VERSION 0.2.0 DESCRIPTION "Keyboard and mouse lock for Windows" LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "Pawse builds with MSVC (Visual Studio 2022 or later).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(resources/version.h.in generated/version.h @ONLY)

add_executable(pawse WIN32
    src/app.cpp
    src/banner.cpp
    src/hello_verifier.cpp
    src/input_blocker.cpp
    src/key_binding.cpp
    src/main.cpp
    src/settings.cpp
    src/settings_dialog.cpp
    src/updater.cpp
    resources/pawse.rc
    resources/pawse.manifest
)

target_include_directories(pawse PRIVATE
    include
    resources
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_definitions(pawse PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)

target_compile_options(pawse PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /permissive- /utf-8 /EHsc /Zc:__cplusplus>
)

# The static runtime makes pawse.exe a single file with nothing else to install.
set_property(TARGET pawse PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(pawse PRIVATE bcrypt comctl32 shcore windowsapp winhttp)
