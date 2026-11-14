cmake_minimum_required(VERSION 3.16)
project(KisakBlack)

set(BIN_NAME "KisakBlack")

set(CMAKE_CXX_STANDARD 20)
if(NOT NINTENDO_SWITCH)
    set(CMAKE_GENERATOR_PLATFORM "WIN32")
endif()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TL_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/tl")
set(DW_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/DemonWare")

include(cmake_files.cmake)
# Checkpoint components stay portable and compile in both PC and NX builds.
# SP guards keep the multiplayer executable's behavior unchanged.
target_sources(KisakBlack PRIVATE
    "${SRC_DIR}/clientscript/cscr_save.cpp"
    "${SRC_DIR}/clientscript/cscr_save_write.cpp"
    "${SRC_DIR}/clientscript/cscr_checkpoint_identity.cpp"
    "${SRC_DIR}/game_mp/checkpoint_archive.cpp"
    "${SRC_DIR}/game_mp/g_checkpoint_sp.cpp"
    "${SRC_DIR}/game_mp/g_checkpoint_records_sp.cpp"
    "${SRC_DIR}/game_mp/g_checkpoint_world_sp.cpp"
    "${SRC_DIR}/game_mp/g_checkpoint_stage_sp.cpp"
    "${SRC_DIR}/game_mp/g_checkpoint_client_sp.cpp"
)

target_sources(KisakBlack PRIVATE
    "${SRC_DIR}/live/custom_class_sync.cpp"
    "${SRC_DIR}/openblops_directory/directory_protocol.cpp"
    "${SRC_DIR}/openblops_directory/directory_client.cpp"
    "${SRC_DIR}/openblops_directory/directory_engine.cpp"
    "${SRC_DIR}/openblops_directory/directory_frontend.cpp"
    "${SRC_DIR}/openblops_directory/directory_zombies.cpp"
    "${SRC_DIR}/openblops_directory/directory_update.cpp"
)
# Default bo1_directory endpoint compiled into the game (numeric IPv4:UDP-port).
# Empty in the published source; set it per build tree, e.g.
# -DOPENBLOPS_DIRECTORY_DEFAULT=203.0.113.1:3075, so no address is committed.
set(OPENBLOPS_DIRECTORY_DEFAULT "" CACHE STRING "Default bo1_directory endpoint (numeric IPv4:UDP-port)")
if(OPENBLOPS_DIRECTORY_DEFAULT)
    set_property(SOURCE "${SRC_DIR}/openblops_directory/directory_engine.cpp" APPEND PROPERTY
        COMPILE_DEFINITIONS "BO1_DIRECTORY_DEFAULT=\"${OPENBLOPS_DIRECTORY_DEFAULT}\"")
endif()

include(cmake/direct-connect.cmake)
include(cmake/splitscreen.cmake)

if(NINTENDO_SWITCH)
    include(cmake/switch.cmake)
    return()
endif()

# phys_gjk.cpp specific flag (experimental)
set_source_files_properties(
    "${SRC_DIR}/physics/phys_gjk.cpp"
    PROPERTIES
    COMPILE_OPTIONS "/arch:IA32"
)

option(OPENBLOPS_OFFLINE_MENUS "Allow local play through the offline menu overrides" ON)
target_compile_definitions(KisakBlack PRIVATE $<$<BOOL:${OPENBLOPS_OFFLINE_MENUS}>:OPENBLOPS_OFFLINE_MENUS>)

option(KISAK_SP "Build the single-player / Zombies executable" OFF)
if(KISAK_SP)
    set(KISAK_GAME_MODE KISAK_SP)
    set(OPENBLOPS_PC_NAME OpenBLOPS)
else()
    set(KISAK_GAME_MODE KISAK_MP)
    set(OPENBLOPS_PC_NAME OpenBLOPS-MP)
endif()

set(OPENBLOPS_PC_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build-nx/PC-build"
    CACHE PATH "Directory for the Windows executables and symbols")

if(DEFINED ENV{DXSDK_DIR})
    set(DXSDK_DIR "$ENV{DXSDK_DIR}" CACHE PATH "DirectX SDK directory")
else()
    set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)" CACHE PATH "DirectX SDK directory")
endif()

# ----- Preprocessor Defines -----
# Common to both Debug and Release
target_compile_definitions(KisakBlack PUBLIC
    WIN32
    _CONSOLE
    ${KISAK_GAME_MODE}
    _CRT_SECURE_NO_WARNINGS
    _WINSOCK_NO_DEPRECATED_NO_WARNINGS
)
# Debug-only
target_compile_definitions(KisakBlack PUBLIC
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug>:TRACY_ENABLE>
    $<$<CONFIG:Debug>:TRACY_ON_DEMAND>
)
# Release-only
target_compile_definitions(KisakBlack PUBLIC
    $<$<CONFIG:Release>:NDEBUG>
)

# ----- Include directories -----
target_include_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/libs")
target_include_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libtomcrypt-1.17/src/headers")
target_include_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libtommath-1.0")
target_include_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libvpx-1.5.0/include")
target_include_directories(KisakBlack PUBLIC "${DXSDK_DIR}/Include")

# ----- Library directories -----
target_link_directories(KisakBlack PUBLIC "${DXSDK_DIR}/Lib/x86")
target_link_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/libvpx-1.5.0")
target_link_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/steam/redistributable_bin")
target_link_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/binklib")
target_link_directories(KisakBlack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/nvapi")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT  /O2 /MP /W3 ${MSVC_WARNING_DISABLES} /permissive")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /MTd /Od /MP /W3 ${MSVC_WARNING_DISABLES} /permissive")

if (MSVC AND WIN32 AND NOT MSVC_VERSION VERSION_LESS 142)
    target_link_options(KisakBlack PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
    target_compile_options(KisakBlack PRIVATE $<$<CONFIG:Debug>:/ZI>)

    set(CMAKE_CXX_FLAGS_DEBUG           "${CMAKE_CXX_FLAGS_DEBUG} /SAFESEH:NO")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
endif()

# ----- Links -----
target_link_libraries(KisakBlack PUBLIC
    steam_api.lib
    binkw32.lib
    nvapi.lib
    d3d9.lib
    ws2_32.lib
    advapi32.lib
    dxguid.lib
    dsound.lib
    winmm.lib
    xinput.lib
    xapobase.lib
    dxerr.lib
    libvpx.lib
    # June-2010 DirectX SDK libs (xapobase, dxerr) reference pre-VS2015 CRT
    # symbols (_vsnprintf, etc.) that the modern UCRT no longer exports.
    # legacy_stdio_definitions.lib provides shims so those static libs link.
    legacy_stdio_definitions.lib
)
# ----- Debug Links -----
target_link_libraries(KisakBlack PUBLIC $<$<CONFIG:Debug>:d3dx9d.lib>)
# ----- Release Links -----
target_link_libraries(KisakBlack PUBLIC $<$<CONFIG:Release>:d3dx9.lib>)
target_link_options(KisakBlack PRIVATE /machine:x86)

# ----- Linker Options -----
# /DYNAMICBASE:NO - Turning this off causes directx to crash "directx has encountered an unrecoverable error". 
# /LARGEADDRESSAWARE - for convenience (4gb)
# /DEBUG - PDB for both configs 
target_link_options(KisakBlack PRIVATE /DYNAMICBASE:NO /LARGEADDRESSAWARE /DEBUG)

set_target_properties(KisakBlack PROPERTIES WIN32_EXECUTABLE TRUE)

# Set the VS debugger's working directory to the build-output dir for whichever config is active 
set_target_properties(KisakBlack PROPERTIES
    OUTPUT_NAME "${OPENBLOPS_PC_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${OPENBLOPS_PC_OUTPUT_DIR}>"
    PDB_OUTPUT_DIRECTORY "$<1:${OPENBLOPS_PC_OUTPUT_DIR}>"
    VS_DEBUGGER_WORKING_DIRECTORY "${OPENBLOPS_PC_OUTPUT_DIR}"
)
