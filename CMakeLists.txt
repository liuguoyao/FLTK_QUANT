cmake_minimum_required(VERSION 3.15)
project(FlOpDeskDAG CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: suppress C4996 warnings for strdup/sprintf etc.
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Fl_OpDesk uses strndup; MSVC has it but under a different guard
    add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
    # Source files are UTF-8 (Chinese comments); tell MSVC to interpret them as UTF-8
    add_compile_options(/utf-8)
endif()

# --------------------------------------------------------------------------
# FLTK: two strategies depending on the compiler
#
#   MSVC  : build FLTK from source via FetchContent (no prebuilt MSVC FLTK)
#   MinGW : use the system FLTK installed via pacman (GCC ABI)
#
#   The two ABIs are incompatible, so we must never mix them.
# --------------------------------------------------------------------------
if(MSVC)
    include(FetchContent)
    # FLTK 1.4.x official repo
    set(FLTK_QUIET ON CACHE BOOL "" FORCE)
    set(OPTION_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_TEST     OFF CACHE BOOL "" FORCE)
    set(OPTION_BUILD_GL     OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        fltk
        GIT_REPOSITORY https://github.com/fltk/fltk.git
        GIT_TAG        release-1.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(fltk)

    set(FLTK_LIBS fltk fltk_images)
    message(STATUS "FLTK: built from source via FetchContent (MSVC)")
else()
    find_package(FLTK REQUIRED NO_MODULE)
    set(FLTK_LIBS ${FLTK_LIBRARIES})
    message(STATUS "Found system FLTK: ${FLTK_VERSION} (libs: ${FLTK_LIBRARIES})")
endif()

# --------------------------------------------------------------------------
# Build Fl_OpDesk widget sources into a static library
#   (vendored from https://github.com/etorth/Fl_OpDesk)
# --------------------------------------------------------------------------
set(OPDESK_DIR ${CMAKE_SOURCE_DIR}/third_party/Fl_OpDesk)

add_library(fl_opdesk STATIC
    ${OPDESK_DIR}/Fl_OpDesk.C
    ${OPDESK_DIR}/Fl_OpBox.C
    ${OPDESK_DIR}/Fl_OpButton.C
)

target_include_directories(fl_opdesk PUBLIC ${OPDESK_DIR})
target_link_libraries(fl_opdesk PUBLIC ${FLTK_LIBS})

# FLTK 1.3.x used the legacy .C extension; treat it as C++
set_source_files_properties(
    ${OPDESK_DIR}/Fl_OpDesk.C
    ${OPDESK_DIR}/Fl_OpBox.C
    ${OPDESK_DIR}/Fl_OpButton.C
    PROPERTIES LANGUAGE CXX)

# --------------------------------------------------------------------------
# Main program: interactive DAG editor
#   WIN32_EXECUTABLE 让程序使用 GUI 子系统,启动时不弹出 cmd 黑窗
# --------------------------------------------------------------------------
add_executable(dag_editor
    src/main.cpp
    src/CustomControl.cpp
)
set_target_properties(dag_editor PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(dag_editor PRIVATE
    fl_opdesk
    ${FLTK_LIBS}
)

# Link additional system libs that FLTK needs on Windows/MinGW
if(WIN32 AND NOT MSVC)
    target_link_libraries(dag_editor PRIVATE
        gdi32 ole32 uuid comdlg32 comctl32 wsock32)
endif()

# MSVC FLTK target also needs the Windows system libs
if(MSVC)
    target_link_libraries(dag_editor PRIVATE
        gdi32 ole32 uuid comdlg32 comctl32)
endif()
