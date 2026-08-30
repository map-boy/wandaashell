cmake_minimum_required(VERSION 3.20)
project(wandaashell VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Sources
#
# core/ and builtins/ are portable C++17 and compile identically everywhere.
# Exactly one platform backend is compiled in, chosen by the host OS.
# ---------------------------------------------------------------------------
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS core/*.cpp builtins/*.cpp)

# Audio is the one part of the platform layer with a single portable
# implementation, so every platform compiles the same file.
set(SHARED_PLATFORM_SOURCES platform/audio_miniaudio.cpp)

if(WIN32)
    enable_language(RC)
    set(PLATFORM_SOURCES platform/windows/platform_windows.cpp)
    # miniaudio resolves its Windows audio backends with LoadLibrary, so
    # nothing extra needs linking here - this is where -lwinmm used to be.
    set(PLATFORM_LIBS "")
elseif(APPLE)
    message(FATAL_ERROR
        "The macOS platform backend is not implemented yet (spec phase 4). "
        "platform/macos/platform_macos.cpp does not exist on this branch.")
elseif(UNIX)
    message(FATAL_ERROR
        "The Linux platform backend is not implemented yet (spec phase 3). "
        "platform/linux/platform_linux.cpp does not exist on this branch.")
else()
    message(FATAL_ERROR "wandaashell has no platform backend for this system.")
endif()

# ---------------------------------------------------------------------------
# The shell
# ---------------------------------------------------------------------------
add_executable(wandaashell apps/desktop_main.cpp ${CORE_SOURCES}
                           ${SHARED_PLATFORM_SOURCES} ${PLATFORM_SOURCES})
target_include_directories(wandaashell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wandaashell PRIVATE ${PLATFORM_LIBS})

if(WIN32)
    # Embeds the application icon and version block.
    target_sources(wandaashell PRIVATE assets/wandaashell.rc)

    # wandaa.exe: a tiny GUI stub that opens wandaashell.exe in a new console.
    # Windows-only by nature, so it lives with the Windows backend.
    add_executable(wandaa WIN32 platform/windows/wandaa_launcher.cpp)
    target_include_directories(wandaa PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ---------------------------------------------------------------------------
# Assets
#
# platform::assetPath() looks next to the executable first, so stage the
# assets there for anyone running straight out of the build tree.
# ---------------------------------------------------------------------------
add_custom_command(TARGET wandaashell POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:wandaashell>/assets
    COMMENT "Staging assets next to the wandaashell binary")
