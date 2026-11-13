get_filename_component(NX_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(NX_DXVK_SOURCE "${NX_PROJECT_ROOT}/build-nx/deps/dxvk-sarek" CACHE PATH "Pinned DXVK Sarek 1.13.0 source")
set(NX_DXVK_NATIVE_HEADERS "${NX_PROJECT_ROOT}/build-nx/deps/dxvk/include/native" CACHE PATH "DXVK 3.1.1 portable COM declarations")
find_program(NX_HOST_PYTHON python3 HINTS /usr/bin NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_program(NX_GLSLANG glslangValidator HINTS /ucrt64/bin NO_CMAKE_FIND_ROOT_PATH REQUIRED)
execute_process(COMMAND "${NX_HOST_PYTHON}" "${NX_PROJECT_ROOT}/tools/nx/patch-dxvk-sarek.py" "${NX_DXVK_SOURCE}"
    COMMAND_ERROR_IS_FATAL ANY)
set(NX_DXVK_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/dxvk-generated")
file(MAKE_DIRECTORY "${NX_DXVK_GENERATED}")
file(WRITE "${NX_DXVK_GENERATED}/version.h" "#define DXVK_VERSION \"1.13.0-kisak-horizon\"\n")
file(WRITE "${NX_DXVK_GENERATED}/nx_renderer_build.h"
    "#define NX_RENDERER_LABEL \"DXVK Sarek 1.13.0 37f397e\"\n#define NX_RENDERER_CACHE \"dxvk-sarek-1.13.0\"\n#define NX_RENDERER_CONFIG \"dxvk-sarek.conf\"\n")
set(NX_SAREK_SOURCES)
foreach(part util spirv vulkan dxvk dxso d3d9)
    file(READ "${NX_DXVK_SOURCE}/src/${part}/meson.build" manifest)
    string(REGEX MATCHALL "'[^']+\\.(cpp|c)'" source_files "${manifest}")
    foreach(entry IN LISTS source_files)
        string(REPLACE "'" "" entry "${entry}")
        if(NOT entry MATCHES "(dxvk_open(vr|xr)|util_shared_res|util_monitor|util_string)\\.cpp$")
            list(APPEND NX_SAREK_SOURCES "${NX_DXVK_SOURCE}/src/${part}/${entry}")
        endif()
    endforeach()
endforeach()
list(APPEND NX_SAREK_SOURCES "${NX_PROJECT_ROOT}/src/nx/dxvk/sarek_horizon.cpp"
    "${NX_PROJECT_ROOT}/src/nx/dxvk/sarek_string.cpp" "${NX_PROJECT_ROOT}/src/nx/dxvk/horizon_alloc.cpp")
set(NX_SAREK_SHADERS)
foreach(part dxvk d3d9)
    file(READ "${NX_DXVK_SOURCE}/src/${part}/meson.build" manifest)
    string(REGEX MATCHALL "'[^']+\\.(vert|frag|geom|comp)'" shader_files "${manifest}")
    foreach(entry IN LISTS shader_files)
        string(REPLACE "'" "" entry "${entry}")
        get_filename_component(shader_name "${entry}" NAME_WE)
        set(header "${NX_DXVK_GENERATED}/${shader_name}.h")
        add_custom_command(OUTPUT "${header}"
            COMMAND "${NX_GLSLANG}" --quiet -V --vn "${shader_name}" "${NX_DXVK_SOURCE}/src/${part}/${entry}" -o "${header}"
            DEPENDS "${NX_DXVK_SOURCE}/src/${part}/${entry}" VERBATIM)
        list(APPEND NX_SAREK_SHADERS "${header}")
    endforeach()
endforeach()
add_library(nx_dxvk_native STATIC ${NX_SAREK_SOURCES} ${NX_SAREK_SHADERS})
target_compile_features(nx_dxvk_native PUBLIC cxx_std_20)
target_compile_definitions(nx_dxvk_native PUBLIC NX_DXVK_SAREK VK_USE_PLATFORM_VI_NN)
target_compile_options(nx_dxvk_native PRIVATE -O2 -fno-strict-aliasing -ffunction-sections -fdata-sections
    -Wno-unused-parameter -Wno-missing-field-initializers)
target_compile_options(nx_dxvk_native PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${NX_PROJECT_ROOT}/src/nx/dxvk/sarek_horizon.h>")
target_include_directories(nx_dxvk_native PUBLIC
    "${NX_DXVK_NATIVE_HEADERS}" "${NX_DXVK_NATIVE_HEADERS}/windows" "${NX_DXVK_NATIVE_HEADERS}/directx"
    "${NX_DXVK_SOURCE}/include" "${NX_DXVK_SOURCE}/include/vulkan/include" "${NX_DXVK_SOURCE}/include/spirv/include"
    "${NX_DXVK_SOURCE}/src" "${NX_DXVK_GENERATED}")
execute_process(COMMAND "${NX_HOST_PYTHON}" "${NX_PROJECT_ROOT}/tools/nx/gen-d3d9-bridge.py"
    "${NX_PROJECT_ROOT}/src/nx/compat/d3d9.h" "${NX_DXVK_GENERATED}/d3d9_forwarders.inc" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NX_PROJECT_ROOT}/src/nx/compat/d3d9.h" "${NX_PROJECT_ROOT}/tools/nx/gen-d3d9-bridge.py"
    "${NX_PROJECT_ROOT}/tools/nx/patch-dxvk-sarek.py")
add_library(nx_d3d9_bridge STATIC "${NX_PROJECT_ROOT}/src/nx/dxvk/d3d9_bridge.cpp"
    "${NX_PROJECT_ROOT}/src/nx/dxvk/horizon_stats.cpp" "${NX_PROJECT_ROOT}/src/nx/dxvk/horizon_threads.cpp")
target_compile_definitions(nx_d3d9_bridge PRIVATE NX_DXVK)
target_compile_options(nx_d3d9_bridge PRIVATE -O2 -fno-strict-aliasing -ffunction-sections -fdata-sections)
target_link_libraries(nx_d3d9_bridge PRIVATE nx_dxvk_native)
