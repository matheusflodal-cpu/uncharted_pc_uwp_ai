cmake_minimum_required(VERSION 3.24)
project(uncharted_uwp LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "The UWP target requires Windows SDK and clang-cl on Windows. See xbox/README.md.")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(UNCHARTED_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(UNCHARTED_DEPS "${CMAKE_CURRENT_LIST_DIR}/.local/deps" CACHE PATH "Pinned winpthreads dependencies")
if(NOT EXISTS "${UNCHARTED_DEPS}/winpthreads/include/pthread.h")
  message(FATAL_ERROR "Run python xbox/scripts/fetch-deps.py first")
endif()
option(UNCHARTED_GRAPHICS_PROBE "Build the Xbox graphics presentation probe" OFF)
option(UNCHARTED_COMPILE_RENDERER "Compile the experimental renderer library without enabling gameplay" OFF)
set(cpu_sources runtime runtime_content runtime_file runtime_kernel runtime_memory
  runtime_mutex runtime_rtc runtime_rwlock runtime_savedata runtime_sema runtime_services
  runtime_thread runtime_time runtime_fiber compat_win probe)
list(TRANSFORM cpu_sources PREPEND "${UNCHARTED_ROOT}/src/")
list(TRANSFORM cpu_sources APPEND ".c")
add_executable(uncharted_uwp WIN32 platform/main.cpp platform/prepare_game.cpp platform/runtime_compat.c
  platform/diagnostic_services.c ${cpu_sources})
add_executable(uncharted_prepare_contract platform/prepare_contract.cpp platform/prepare_game.cpp)
target_compile_options(uncharted_prepare_contract PRIVATE /W3 /EHsc)

add_executable(uncharted_memory_contract platform/memory_contract_host.c platform/runtime_compat.c
  platform/diagnostic_services.c "${UNCHARTED_ROOT}/tests/test_uwp_posix_memory.c" ${cpu_sources})
add_executable(uncharted_pad_contract "${UNCHARTED_ROOT}/tests/test_uwp_pad.c")
target_include_directories(uncharted_pad_contract PRIVATE "${UNCHARTED_ROOT}" "${UNCHARTED_ROOT}/src"
  platform "${UNCHARTED_DEPS}/winpthreads/include")
target_compile_definitions(uncharted_pad_contract PRIVATE _TIMESPEC_DEFINED _MODE_T_)
target_link_libraries(uncharted_pad_contract PRIVATE "${UNCHARTED_DEPS}/winpthreads/lib/libwinpthread.dll.a")
set(cpu_targets uncharted_uwp uncharted_memory_contract)
if(UNCHARTED_COMPILE_RENDERER AND UNCHARTED_GRAPHICS_PROBE)
  add_executable(uncharted_renderer_contract platform/memory_contract_host.c platform/runtime_compat.c
    platform/diagnostic_services.c graphics/resolver_contract.cpp graphics/present_bridge.cpp ${cpu_sources})
  target_compile_definitions(uncharted_renderer_contract PRIVATE UNCHARTED_RESOLVER_CONTRACT UNCHARTED_LINK_RENDERER VK_USE_PLATFORM_WIN32_KHR)
  target_include_directories(uncharted_renderer_contract BEFORE PRIVATE "${UNCHARTED_ROOT}/gpu/shim"
    "${CMAKE_BINARY_DIR}/gpu/include" "${UNCHARTED_ROOT}/gpu/third_party/gcn/include")
  target_include_directories(uncharted_renderer_contract PRIVATE graphics "${UNCHARTED_ROOT}/gpu/shadps4"
    "${UNCHARTED_DEPS}/vulkan-headers/include")
  target_compile_features(uncharted_renderer_contract PRIVATE cxx_std_23)
  target_link_libraries(uncharted_renderer_contract PRIVATE bbgpu psapi d3d12 dxgi dxguid)
  list(APPEND cpu_targets uncharted_renderer_contract)
endif()
foreach(cpu_target IN LISTS cpu_targets)
target_include_directories(${cpu_target} PRIVATE "${UNCHARTED_ROOT}" "${UNCHARTED_ROOT}/src"
  platform platform/compat "${UNCHARTED_DEPS}/winpthreads/include")
target_compile_definitions(${cpu_target} PRIVATE UNCHARTED_UWP NOMINMAX WIN32_LEAN_AND_MEAN
  _CRT_SECURE_NO_WARNINGS _TIMESPEC_DEFINED _MODE_T_)
target_compile_options(${cpu_target} PRIVATE /W3
  $<$<COMPILE_LANGUAGE:C>:/clang:-masm=att> $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
target_link_options(${cpu_target} PRIVATE /HIGHENTROPYVA:NO /STACK:16777216,16777216 /OPT:REF)
target_link_libraries(${cpu_target} PRIVATE WindowsApp "${UNCHARTED_DEPS}/winpthreads/lib/libwinpthread.dll.a")
endforeach()
# The desktop harness runs outside package activation, so it uses the desktop
# CRT; only the installed UWP app resolves CRT imports through the VCLibs framework.
target_link_directories(uncharted_uwp BEFORE PRIVATE "$ENV{VCToolsInstallDir}/lib/x64/store")
target_link_options(uncharted_uwp PRIVATE /APPCONTAINER)
target_link_libraries(uncharted_memory_contract PRIVATE psapi)
add_custom_command(TARGET uncharted_memory_contract POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "${UNCHARTED_DEPS}/libwinpthread/bin/libwinpthread-1.dll" "$<TARGET_FILE_DIR:uncharted_memory_contract>")
set(layout "${CMAKE_BINARY_DIR}/layout")
add_custom_command(TARGET uncharted_uwp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${layout}/ThirdParty"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:uncharted_uwp>" "${layout}"
  COMMAND ${CMAKE_COMMAND} -E copy "${UNCHARTED_DEPS}/libwinpthread/bin/libwinpthread-1.dll" "${layout}"
  COMMAND ${CMAKE_COMMAND} -E copy "${UNCHARTED_DEPS}/winpthreads/share/licenses/winpthreads/COPYING" "${layout}/ThirdParty/winpthreads-COPYING.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${UNCHARTED_ROOT}/LICENSE" "${layout}/LICENSE.txt"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/CREDITS.md" "${layout}"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/AppxManifest.xml" "${layout}")

# The bridge probe validates real Dozen -> D3D12 -> CoreWindow presentation.
# It deliberately does not enable the unfinished guest renderer.
if(UNCHARTED_GRAPHICS_PROBE)
  target_sources(uncharted_uwp PRIVATE graphics/present_bridge.cpp graphics/probe.cpp)
  target_include_directories(uncharted_uwp PRIVATE graphics "${UNCHARTED_ROOT}/gpu/shadps4"
    "${UNCHARTED_DEPS}/vulkan-headers/include")
  target_compile_definitions(uncharted_uwp PRIVATE UNCHARTED_GRAPHICS_PROBE VK_USE_PLATFORM_WIN32_KHR)
  target_link_libraries(uncharted_uwp PRIVATE d3d12 dxgi dxguid)
endif()

if(UNCHARTED_COMPILE_RENDERER)
  set(UNCHARTED_UWP ON)
  add_subdirectory("${UNCHARTED_ROOT}/gpu" "${CMAKE_BINARY_DIR}/gpu")
  if(UNCHARTED_GRAPHICS_PROBE)
    target_compile_definitions(uncharted_uwp PRIVATE UNCHARTED_LINK_RENDERER UNCHARTED_GUEST_INPUT)
    target_sources(uncharted_uwp PRIVATE platform/input.cpp platform/runtime_pad_uwp.c platform/input_probe.cpp)
    target_compile_features(uncharted_uwp PRIVATE cxx_std_23)
    target_sources(uncharted_uwp PRIVATE graphics/texture_probe.cpp)
    target_include_directories(uncharted_uwp BEFORE PRIVATE "${UNCHARTED_ROOT}/gpu/shim"
      "${CMAKE_BINARY_DIR}/gpu/include" "${UNCHARTED_ROOT}/gpu/third_party/gcn/include")
    target_link_libraries(uncharted_uwp PRIVATE bbgpu)
  endif()
endif()
