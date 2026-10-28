cmake_minimum_required(VERSION 3.16)

project(LMECA2170-Project LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # compile_commands.json for your autocompletion

## Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# =================== Find the required libraries ========================
find_package(OpenGL REQUIRED)  # libGL on Linux, opengl32 on Windows, OpenGL.framework on macOS

# Use the installed GLFW (apt, brew, vcpkg, ...) or download and build it
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW not found, downloading it")
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz)
    FetchContent_MakeAvailable(glfw)
endif()

# PETSc, only needed by the fluid solver. ./install_petsc.sh puts a local copy
# in ./external/petsc, looked for first; $PETSC_DIR (and $PETSC_ARCH) next.
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/petsc)
if(DEFINED ENV{PETSC_DIR})
    list(APPEND CMAKE_PREFIX_PATH $ENV{PETSC_DIR} $ENV{PETSC_DIR}/$ENV{PETSC_ARCH})
endif()
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_search_module(PETSC QUIET IMPORTED_TARGET petsc PETSc)
endif()
add_library(petsc_lib INTERFACE)
if(PETSC_FOUND)
    target_link_libraries(petsc_lib INTERFACE PkgConfig::PETSC)
else()
    # No pkg-config (e.g. macOS without Homebrew's): look for the files directly
    find_path(PETSC_INCLUDE_DIR petsc.h)
    find_library(PETSC_LIBRARY petsc)
    if(PETSC_INCLUDE_DIR AND PETSC_LIBRARY)
        set(PETSC_FOUND TRUE)
        target_include_directories(petsc_lib INTERFACE ${PETSC_INCLUDE_DIR})
        target_link_libraries(petsc_lib INTERFACE ${PETSC_LIBRARY})
    endif()
endif()
if(PETSC_FOUND)
    message(STATUS "PETSc found: the fluid solver is enabled")
else()
    message(WARNING "PETSc not found: everything builds, but the fluid solver "
                    "is disabled. Run ./install_petsc.sh, then cmake again.")
endif()

# =================== Define the exec/libs ==============================

# ADD HERE ALL THE REQUIRED FILES YOU CREATE (1 per line)
set(STUDENT_PROJECT_FILES
    src/He_st.c
    src/student_mesh.c
    src/student_geometry.c
    src/predicates.c
    src/student_hilbert.c
    src/student_delaunay.c
    src/student_alpha.c
    src/student_refinement.c
    src/student_triangulation.c
    src/student_spatial_search.c
)

# ADD HERE ALL THE OTHER DIRECTORIES CONTAINING .h FILES (1 per line)
set(STUDENT_INCLUDE_DIRS
    include/
)

# These are the files required by the project (solver/display)
set(DEFAULT_PROJECT_FILES
    src/HeObject.c
    src/HeOperations.c
    src/HeGetters.c
    src/project_display.c
    src/project_solver.c
)

add_library(geonum SHARED)
target_sources(geonum PRIVATE ${STUDENT_PROJECT_FILES} ${DEFAULT_PROJECT_FILES})
target_include_directories(geonum PUBLIC ${STUDENT_INCLUDE_DIRS})  # Give access to the 'include/*.h' files

# Preserve the floating-point rounding assumptions of Shewchuk's predicates.
# Apply these settings only to the external source, including in standalone tests.
function(student_configure_predicates source)
    if(MSVC)
        set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS /O2 /fp:strict)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS
            -O2 -fno-fast-math -ffp-contract=off)
    endif()
    if(WIN32)
        # POSIX random() appears only in unused random-data benchmark helpers.
        # The orientation/incircle algorithms do not use random numbers.
        set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_DEFINITIONS random=rand)
    endif()
endfunction()
student_configure_predicates("${CMAKE_CURRENT_SOURCE_DIR}/src/predicates.c")

# Define private compilation option (given to the compiler just for this library)
if(MSVC)
    target_compile_options(geonum PRIVATE /W4)
    target_compile_definitions(geonum PRIVATE _CRT_SECURE_NO_WARNINGS)  # no "fopen is unsafe" warnings
else()
    target_compile_options(geonum PUBLIC -Wall -Wextra -pedantic -Wshadow)
endif()

# Link libraries to the mesh executable
target_link_libraries(geonum PUBLIC glfw OpenGL::GL)
if(PETSC_FOUND)
    target_link_libraries(geonum PUBLIC petsc_lib)
    target_compile_definitions(geonum PUBLIC HAVE_PETSC)
endif()
if(UNIX)  # Linux and macOS, Windows has no separate math library
    target_link_libraries(geonum PUBLIC m)
endif()
if(APPLE)
    target_compile_definitions(geonum PRIVATE GL_SILENCE_DEPRECATION)  # OpenGL is deprecated on macOS
endif()

# Look for a CMakeLists.txt in ./exec/
add_subdirectory(exec/)

