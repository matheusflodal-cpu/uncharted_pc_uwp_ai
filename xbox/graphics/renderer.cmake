# Experimental compile target; linking it into the guest host is a separate milestone.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(../xbox/graphics/dependencies.cmake)
add_library(magic_enum INTERFACE)
add_library(magic_enum::magic_enum ALIAS magic_enum)
target_include_directories(magic_enum INTERFACE third_party/magic_enum/include third_party/magic_enum/include/magic_enum)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(third_party/miniz EXCLUDE_FROM_ALL)
unset(CMAKE_POLICY_VERSION_MINIMUM)
add_library(miniz::miniz ALIAS miniz)
set(SIRIT_USE_SYSTEM_SPIRV_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/sirit EXCLUDE_FROM_ALL)
get_target_property(sirit_options sirit COMPILE_OPTIONS)
if(sirit_options)
  list(REMOVE_ITEM sirit_options /MP)
  set_target_properties(sirit PROPERTIES COMPILE_OPTIONS "${sirit_options}")
endif()
set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shadps4/video_core/host_shaders)
set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(SHADER_HEADERS "")
function(add_host_shader FILENAME SHADER_NAME)
    set(OUT ${SHADER_DIR}/${SHADER_NAME}.h)
    string(TOUPPER ${SHADER_NAME} SYMBOL)
    set(DEFINES ${ARGN})
    list(TRANSFORM DEFINES PREPEND "-D")
    add_custom_command(OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
        COMMAND ${GLSLANG_COMPILER} --target-env vulkan1.2 -o ${OUT} --vn ${SYMBOL} ${DEFINES} ${SHADER_SRC}/${FILENAME}
        COMMAND ${CMAKE_COMMAND} "-DHEADER_FILE=${OUT}" -P ${SHADER_SRC}/AddShaderHeaderInclude.cmake
        DEPENDS ${SHADER_SRC}/${FILENAME} VERBATIM)
    set(SHADER_HEADERS ${SHADER_HEADERS} ${OUT} PARENT_SCOPE)
endfunction()
add_host_shader(color_to_ms_depth.frag color_to_ms_depth_frag)
add_host_shader(ms_image_blit.frag ms_image_blit_frag)
add_host_shader(ms_image_blit.frag ms_image_blit_msaa_frag SRC_MSAA=1)
add_host_shader(fault_buffer_process.comp fault_buffer_process_comp)
add_host_shader(fs_tri.vert fs_tri_vert)
add_host_shader(fsr.comp fsr_easu_comp SAMPLE_EASU=1)
add_host_shader(fsr.comp fsr_rcas_comp SAMPLE_RCAS=1)
add_host_shader(post_process.frag post_process_frag)
add_host_shader(camera_motion.comp camera_motion_comp)
add_host_shader(upscale_merge.comp upscale_merge_comp)
add_host_shader(upscale_reactive.comp upscale_reactive_comp)
add_host_shader(taa.comp taa_comp)
add_host_shader(taa_sharpen.comp taa_sharpen_comp)
add_host_shader(taa_sharpen.comp taa_sharpen_ldr_comp LDR_OUTPUT=1)
add_host_shader(buffer_multi_copy.comp buffer_multi_copy_comp)
add_host_shader(depth_resample.frag depth_resample_frag)
add_host_shader(depth_resample.frag depth_stencil_resample_frag HAS_STENCIL=1)
add_host_shader(depth_resample.frag depth_stencil_bits_frag STENCIL_BITS=1)
add_host_shader(camera_motion.comp camera_motion_debug_comp DEBUG_OVERLAY=1)
add_host_shader(frame_gen_inputs.comp frame_gen_inputs_comp)
add_host_shader(frame_gen_ui.comp frame_gen_ui_comp)
foreach(BPP IN ITEMS 8 16 32 64 96 128)
    add_host_shader(tiling.comp tiling_micro_${BPP}_comp BITS_PER_PIXEL=${BPP} IS_MACRO_TILED=0)
    add_host_shader(tiling.comp tiling_macro_${BPP}_comp BITS_PER_PIXEL=${BPP} IS_MACRO_TILED=1)
endforeach()
add_host_shader(../../../../xbox/graphics/texture_sample.comp uwp_texture_sample_comp)
add_custom_target(host_shaders DEPENDS ${SHADER_HEADERS})
file(GLOB_RECURSE VIDEO_CORE CONFIGURE_DEPENDS shadps4/video_core/*.cpp)
file(GLOB_RECURSE RECOMPILER CONFIGURE_DEPENDS shadps4/shader_recompiler/*.cpp)
list(FILTER VIDEO_CORE EXCLUDE REGEX "renderdoc\\.cpp$")
set(COMMON
    shadps4/common/decoder.cpp shadps4/common/error.cpp
    shadps4/common/io_file.cpp shadps4/common/native_clock.cpp shadps4/common/number_utils.cpp
    shadps4/common/path_util.cpp shadps4/common/rdtsc.cpp shadps4/common/spin_lock.cpp shadps4/common/recursive_lock.cpp
    shadps4/common/string_util.cpp shadps4/common/thread.cpp shadps4/common/signal_context.cpp
    shim/common/logging/log.cpp)
file(GLOB AVPLAYER CONFIGURE_DEPENDS shadps4/core/libraries/avplayer/*.cpp)

file(GLOB SHIM_SOURCES CONFIGURE_DEPENDS shim/*.cpp shim/*/*.cpp)
list(FILTER VIDEO_CORE EXCLUDE REGEX "vk_temporal_upscaler[.]cpp$|vk_fsr4[.]cpp$|/fsr411/|vk_dlss[.]cpp$|vk_frame_generation[.]cpp$|vk_swapchain[.]cpp$")
list(FILTER SHIM_SOURCES EXCLUDE REGEX "shim/window[.]cpp$")
list(APPEND VIDEO_CORE ../xbox/graphics/native_resolution.cpp ../xbox/graphics/swapchain.cpp ../xbox/graphics/dense_buffers.cpp)
list(APPEND SHIM_SOURCES ../xbox/graphics/window.cpp ../xbox/graphics/offline_network.cpp)
# Windows: a DLL cannot leave the runtime_* symbols for bb-probe.exe to resolve, so the
# library is static there and linked into the executable.
if (WIN32)
    set(BBGPU_KIND STATIC)
else()
    set(BBGPU_KIND SHARED)
endif()
add_library(bbgpu ${BBGPU_KIND} ${VIDEO_CORE} ${RECOMPILER} ${COMMON} ${SHIM_SOURCES}
    shadps4/core/libraries/gnmdriver/gnmdriver.cpp
    shadps4/core/libraries/videoout/driver.cpp shadps4/core/libraries/videoout/video_out.cpp
    shadps4/core/libraries/kernel/equeue.cpp ${AVPLAYER}
    shadps4/core/libraries/videodec/video_utils.cpp
    shadps4/core/libraries/videodec/videodec2.cpp shadps4/core/libraries/videodec/videodec2_impl.cpp)
add_dependencies(bbgpu host_shaders)

target_sources(bbgpu PRIVATE third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp ../xbox/graphics/descriptor_fallback.cpp)
target_include_directories(bbgpu PRIVATE "${UNCHARTED_ROOT}/src" shim shadps4 ${SHADER_INCLUDE}
    third_party/gcn/include third_party/half/include third_party/xbyak
    third_party/imgui third_party/imgui/backends ../xbox/platform ../xbox/platform/compat
    ../xbox/graphics "${UNCHARTED_DEPS}/winpthreads/include")
target_compile_definitions(bbgpu PRIVATE UNCHARTED_UWP NOMINMAX WIN32_LEAN_AND_MEAN
    _TIMESPEC_DEFINED _CRT_SECURE_NO_WARNINGS VK_USE_PLATFORM_WIN32_KHR BOOST_ASIO_STANDALONE NDEBUG
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
    IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
    BB_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans.ttf")
target_compile_options(bbgpu PRIVATE /EHsc /Oy- "/FI${CMAKE_CURRENT_SOURCE_DIR}/../xbox/graphics/compat.h"
    /clang:-Wno-deprecated-declarations /clang:-Wno-missing-field-initializers)
target_link_libraries(bbgpu PUBLIC Vulkan::Headers fmt::fmt Boost::headers magic_enum::magic_enum
    tsl::robin_map GPUOpen::VulkanMemoryAllocator PkgConfig::XXHASH PkgConfig::SDL3
    PkgConfig::FFMPEG miniz::miniz Zydis::Zydis sirit WindowsApp)

# Host-side shader contracts use the same production emitter and packaged ICD.
add_executable(uncharted_integer_sample_contract ${UNCHARTED_ROOT}/tests/test_uwp_integer_sample.cpp)
add_executable(uncharted_integer_driver_contract ${UNCHARTED_ROOT}/tests/test_uwp_integer_driver.cpp ${UNCHARTED_ROOT}/tests/test_uwp_integer_sample.cpp)
target_compile_definitions(uncharted_integer_driver_contract PRIVATE UNCHARTED_INTEGER_TEST_NO_MAIN NOMINMAX WIN32_LEAN_AND_MEAN)
foreach(integer_target uncharted_integer_sample_contract uncharted_integer_driver_contract)
  target_include_directories(${integer_target} PRIVATE shadps4 ${UNCHARTED_DEPS}/vulkan-headers/include)
  set_target_properties(${integer_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(${integer_target} PRIVATE sirit kernel32)
  target_compile_options(${integer_target} PRIVATE /EHsc)
endforeach()
