# PhysicsFS; a portable, flexible file i/o abstraction.
#
# Please see the file LICENSE.txt in the source's root directory.

# The CMake project file is meant to get this compiling on all sorts of
#  platforms quickly, and serve as the way Unix platforms and Linux distros
#  package up official builds, but you don't _need_ to use this; we have
#  built PhysicsFS to (hopefully) be able to drop into your project and
#  compile, using preprocessor checks for platform-specific bits instead of
#  testing in here.

cmake_minimum_required(VERSION 2.8.4)
enable_language (CXX)

set (PHYSFS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set (PHYSFS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Urho3D - commented out CheckTypeSize, FindPkgConfig, and GNUInstallDirs modules as the corresponding macros are not being used in our modified version
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCCompilerFlag)
include(CheckStructHasMember)
include(CMakeDependentOption)



set(PHYSFS_VERSION 3.1.0)

define_source_files (GLOB_CPP_PATTERNS src/*.cpp GLOB_H_PATTERNS src/*.h)
set (INCLUDE_DIRS src)

# Increment this if/when we break backwards compatibility.
set(PHYSFS_SOVERSION 1)

# I hate that they define "WIN32" ... we're about to move to Win64...I hope!
if(WIN32 AND NOT WINDOWS)
    set(WINDOWS TRUE)
endif()

if(APPLE)
    set(OTHER_LDFLAGS ${OTHER_LDFLAGS} "-framework IOKit -framework Foundation")
    set(PHYSFS_M_SRCS src/physfs_platform_apple.m)
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    # Don't use -rpath.
    set(CMAKE_SKIP_RPATH ON CACHE BOOL "Skip RPATH" FORCE)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "SunPro")
    add_definitions(-erroff=E_EMPTY_TRANSLATION_UNIT)
    add_definitions(-xldscope=hidden)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" OR CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    set(WINRT TRUE)
endif()

if(WINRT)
    set(PHYSFS_CPP_SRCS src/physfs_platform_winrt.cpp)
endif()

if(UNIX AND NOT WINDOWS AND NOT APPLE)  # (MingW and such might be UNIX _and_ WINDOWS!)
    find_library(PTHREAD_LIBRARY pthread)
    if(PTHREAD_LIBRARY)
        set(OPTIONAL_LIBRARY_LIBS ${OPTIONAL_LIBRARY_LIBS} ${PTHREAD_LIBRARY})
    endif()
endif()

# Almost everything is "compiled" here, but things that don't apply to the
#  build are #ifdef'd out. This is to make it easy to embed PhysicsFS into
#  another project or bring up a new build system: just compile all the source
#  code and #define the things you want.


# Archivers ...
# These are (mostly) on by default now, so these options are only useful for
#  disabling them.

option(PHYSFS_ARCHIVE_ZIP "Enable ZIP support" TRUE)
if(NOT PHYSFS_ARCHIVE_ZIP)
    add_definitions(-DPHYSFS_SUPPORTS_ZIP=0)
endif()

option(PHYSFS_ARCHIVE_7Z "Enable 7zip support" TRUE)
if(NOT PHYSFS_ARCHIVE_7Z)
    add_definitions(-DPHYSFS_SUPPORTS_7Z=0)
endif()

option(PHYSFS_ARCHIVE_GRP "Enable Build Engine GRP support" TRUE)
if(NOT PHYSFS_ARCHIVE_GRP)
    add_definitions(-DPHYSFS_SUPPORTS_GRP=0)
endif()

option(PHYSFS_ARCHIVE_WAD "Enable Doom WAD support" TRUE)
if(NOT PHYSFS_ARCHIVE_WAD)
    add_definitions(-DPHYSFS_SUPPORTS_WAD=0)
endif()

option(PHYSFS_ARCHIVE_HOG "Enable Descent I/II HOG support" TRUE)
if(NOT PHYSFS_ARCHIVE_HOG)
    add_definitions(-DPHYSFS_SUPPORTS_HOG=0)
endif()

option(PHYSFS_ARCHIVE_MVL "Enable Descent I/II MVL support" TRUE)
if(NOT PHYSFS_ARCHIVE_MVL)
    add_definitions(-DPHYSFS_SUPPORTS_MVL=0)
endif()

option(PHYSFS_ARCHIVE_QPAK "Enable Quake I/II QPAK support" TRUE)
if(NOT PHYSFS_ARCHIVE_QPAK)
    add_definitions(-DPHYSFS_SUPPORTS_QPAK=0)
endif()

option(PHYSFS_ARCHIVE_SLB "Enable I-War / Independence War SLB support" TRUE)
if(NOT PHYSFS_ARCHIVE_SLB)
    add_definitions(-DPHYSFS_SUPPORTS_SLB=0)
endif()

option(PHYSFS_ARCHIVE_ISO9660 "Enable ISO9660 support" TRUE)
if(NOT PHYSFS_ARCHIVE_ISO9660)
    add_definitions(-DPHYSFS_SUPPORTS_ISO9660=0)
endif()

option(PHYSFS_ARCHIVE_VDF "Enable Gothic I/II VDF archive support" TRUE)
if(NOT PHYSFS_ARCHIVE_VDF)
    add_definitions(-DPHYSFS_SUPPORTS_VDF=0)
endif()



macro(message_bool_option _NAME _VALUE)
    if(${_VALUE})
        message(STATUS "  ${_NAME}: enabled")
    else()
        message(STATUS "  ${_NAME}: disabled")
    endif()
endmacro()

message(STATUS "PhysicsFS will build with the following options:")
message_bool_option("ZIP support" PHYSFS_ARCHIVE_ZIP)
message_bool_option("7zip support" PHYSFS_ARCHIVE_7Z)
message_bool_option("GRP support" PHYSFS_ARCHIVE_GRP)
message_bool_option("WAD support" PHYSFS_ARCHIVE_WAD)
message_bool_option("HOG support" PHYSFS_ARCHIVE_HOG)
message_bool_option("MVL support" PHYSFS_ARCHIVE_MVL)
message_bool_option("QPAK support" PHYSFS_ARCHIVE_QPAK)
message_bool_option("SLB support" PHYSFS_ARCHIVE_SLB)
message_bool_option("VDF support" PHYSFS_ARCHIVE_VDF)
message_bool_option("ISO9660 support" PHYSFS_ARCHIVE_ISO9660)

# Urho3D - anything below this line is Urho3D specific
# Define target name
set (TARGET_NAME physfs)

# Define source files
file (GLOB H_FILES src/*.h)     # Adding the headers into source files list is just for easier file browsing in the IDE

set(C_FILES
    src/physfs.c
    src/physfs_byteorder.c
    src/physfs_unicode.c
    src/physfs_platform_posix.c
    src/physfs_platform_unix.c
    src/physfs_platform_windows.c
    src/physfs_platform_os2.c
    src/physfs_platform_qnx.c
    src/physfs_platform_android.c
    src/physfs_archiver_dir.c
    src/physfs_archiver_unpacked.c
    src/physfs_archiver_grp.c
    src/physfs_archiver_hog.c
    src/physfs_archiver_7z.c
    src/physfs_archiver_mvl.c
    src/physfs_archiver_qpak.c
    src/physfs_archiver_wad.c
    src/physfs_archiver_zip.c
    src/physfs_archiver_slb.c
    src/physfs_archiver_iso9660.c
    src/physfs_archiver_vdf.c
    ${PHYSFS_CPP_SRCS}
    ${PHYSFS_M_SRCS}
)
set (SOURCE_FILES ${C_FILES} ${H_FILES})
set (INCLUDE_DIRS src)

# Setup target as STATIC library (as the result we never use EXTRA_LDFLAGS linker flags)
setup_library ()

install_header_files (DIRECTORY src/ DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty/physfs FILES_MATCHING PATTERN *.h)
# end of CMakeLists.txt ...
