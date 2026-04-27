cmake_minimum_required(VERSION 3.20)
project(KeyCastOW LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Architecture suffix for output binary name
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_SUFFIX "_x64")
else()
    set(ARCH_SUFFIX "_x86")
endif()

# Detect compiler
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "GNU/MinGW compilers are not supported. Please use MSVC.")
endif()

# ── Sources ──
set(APP_SOURCES
    src/keycast.cpp
    src/keylog.cpp
)

set(RC_FILE resources/keycastow.rc)

# ── Executable ──
add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES} ${RC_FILE})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

# ── Compile settings ──
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    /W3
    /utf-8
    $<$<CONFIG:Debug>:/Od /Ob0>
    $<$<CONFIG:Release>:/O2 /Oi /GL /Gy>
)

# ── Link libraries ──
target_link_libraries(${PROJECT_NAME} PRIVATE
    gdiplus
    Comctl32
)

# ── Linker options ──
target_link_options(${PROJECT_NAME} PRIVATE
    /DEBUG
    $<$<CONFIG:Debug>:/INCREMENTAL>
    $<$<CONFIG:Release>:/OPT:REF /OPT:ICF /LTCG>
    # Enable Common Controls v6 visual theming (replaces separate .manifest file)
    "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' publicKeyToken='6595b64144ccf1df' language='*' processorArchitecture='*'"
)

# ── CRT: Debug=/MDd, Release=/MT (matches original vcxproj) ──
set_property(TARGET ${PROJECT_NAME} PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>"
)

# ── Output properties ──
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "keycastow${ARCH_SUFFIX}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$<CONFIG>"
)
