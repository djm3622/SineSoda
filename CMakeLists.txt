cmake_minimum_required(VERSION 3.25)

if(CMAKE_HOST_APPLE)
  set(_sinesoda_universal_default ON)
else()
  set(_sinesoda_universal_default OFF)
endif()
option(SINESODA_UNIVERSAL_MACOS "Build arm64 and x86_64 macOS slices"
       ${_sinesoda_universal_default})
if(SINESODA_UNIVERSAL_MACOS)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
      "Architectures built for macOS" FORCE)
endif()

project(
  SineSoda
  VERSION 1.0.0
  DESCRIPTION "A research-grade real-time time-frequency analyzer"
  LANGUAGES C CXX)

option(SINESODA_BUILD_PLUGIN "Build the VST3 plug-in" ON)
option(SINESODA_BUILD_TESTS "Build the DSP and streaming tests" ON)
option(SINESODA_BUILD_BENCHMARKS "Build manual DSP performance benchmarks" ON)
option(SINESODA_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
option(SINESODA_DEPLOY_PLUGIN "Copy the built plug-in to the per-user VST3 directory" OFF)
option(SINESODA_ADHOC_SIGN_MACOS "Ad-hoc sign local macOS plug-in builds" ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
  SineSodaDSP
  src/dsp/AnalysisSettings.cpp
  src/dsp/Fft.cpp
  src/dsp/FrequencyScale.cpp
  src/dsp/SpectralFrame.cpp
  src/dsp/StreamingAnalyzer.cpp
  src/dsp/Transforms.cpp
  src/runtime/AnalysisEngine.cpp)
add_library(SineSoda::DSP ALIAS SineSodaDSP)

target_include_directories(
  SineSodaDSP
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(SineSodaDSP PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(SineSodaDSP PRIVATE /W4 /permissive-)
else()
  target_compile_options(
    SineSodaDSP
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow)
endif()

if(SINESODA_ENABLE_SANITIZERS AND NOT MSVC)
  target_compile_options(SineSodaDSP PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(SineSodaDSP PUBLIC -fsanitize=address,undefined)
endif()

if(SINESODA_BUILD_TESTS)
  enable_testing()
  add_executable(
    SineSodaTests
    tests/TestMain.cpp
    tests/TestAxisLayout.cpp
    tests/TestFft.cpp
    tests/TestScales.cpp
    tests/TestStreaming.cpp
    tests/TestTransforms.cpp
    tests/TestRuntime.cpp)
  target_link_libraries(SineSodaTests PRIVATE SineSoda::DSP)
  target_include_directories(SineSodaTests PRIVATE tests)
  add_test(NAME SineSodaTests COMMAND SineSodaTests)
endif()

if(SINESODA_BUILD_BENCHMARKS)
  add_executable(SineSodaBenchmark tools/benchmark_transforms.cpp)
  target_link_libraries(SineSodaBenchmark PRIVATE SineSoda::DSP)

  add_executable(SineSodaExhaustiveQA tools/exhaustive_qa.cpp)
  target_link_libraries(SineSodaExhaustiveQA PRIVATE SineSoda::DSP)
endif()

set(
  SINESODA_FORMAT_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/dsp/AnalysisSettings.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/dsp/Fft.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/dsp/FrequencyScale.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/dsp/SpectralFrame.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/dsp/StreamingAnalyzer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/dsp/Transforms.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/runtime/AnalysisEngine.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/runtime/SpscRing.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/ui/AxisLayout.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/sinesoda/ui/SpectrogramRaster.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/AnalysisSettings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/Fft.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/FrequencyScale.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/SpectralFrame.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/StreamingAnalyzer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/Transforms.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/AnalysisEngine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/SineSoda.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/SineSoda.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/SpectrogramControl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/SpectrogramControl.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestFft.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestAxisLayout.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMain.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRuntime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestScales.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestStreaming.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestSupport.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestTransforms.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/benchmark_transforms.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/exhaustive_qa.cpp)
find_program(
  SINESODA_CLANG_FORMAT_EXECUTABLE
  NAMES clang-format clang-format-18
  HINTS /opt/homebrew/opt/llvm/bin /Library/Developer/CommandLineTools/usr/bin)
if(SINESODA_CLANG_FORMAT_EXECUTABLE)
  add_custom_target(
    format
    COMMAND ${SINESODA_CLANG_FORMAT_EXECUTABLE} -i ${SINESODA_FORMAT_FILES}
    VERBATIM)
  add_custom_target(
    format-check
    COMMAND ${SINESODA_CLANG_FORMAT_EXECUTABLE} --dry-run --Werror
            ${SINESODA_FORMAT_FILES}
    VERBATIM)
else()
  add_custom_target(
    format
    COMMAND ${CMAKE_COMMAND} -E echo
            "clang-format not found - formatting was not modified"
    VERBATIM)
  add_custom_target(
    format-check
    COMMAND ${CMAKE_COMMAND} -E echo
            "clang-format not found - format check was skipped"
    VERBATIM)
endif()

if(SINESODA_BUILD_PLUGIN)
  include(FetchContent)

  set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps" CACHE PATH
      "Shared source cache for pinned third-party dependencies")

  if(NOT DEFINED IPLUG2_DIR)
    FetchContent_Declare(
      iPlug2
      GIT_REPOSITORY https://github.com/iPlug2/iPlug2.git
      GIT_TAG 5c2df9dce3f5258acfeff3846a6a9563f382212c
      GIT_SHALLOW FALSE
      SOURCE_SUBDIR _sinesoda_population_only)
    FetchContent_MakeAvailable(iPlug2)
    set(IPLUG2_DIR "${iplug2_SOURCE_DIR}" CACHE PATH "iPlug2 root directory")
  endif()

  set(_sinesoda_vst3_sdk_dir "${IPLUG2_DIR}/Dependencies/IPlug/VST3_SDK")
  if(NOT EXISTS "${_sinesoda_vst3_sdk_dir}/base/source/baseiids.cpp")
    FetchContent_Declare(
      vst3sdk
      GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
      GIT_TAG 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
      GIT_SHALLOW FALSE
      GIT_SUBMODULES base cmake pluginterfaces public.sdk
      GIT_SUBMODULES_RECURSE TRUE
      SOURCE_SUBDIR _sinesoda_population_only)
    FetchContent_MakeAvailable(vst3sdk)
    file(COPY
         "${vst3sdk_SOURCE_DIR}/base"
         "${vst3sdk_SOURCE_DIR}/cmake"
         "${vst3sdk_SOURCE_DIR}/pluginterfaces"
         "${vst3sdk_SOURCE_DIR}/public.sdk"
         "${vst3sdk_SOURCE_DIR}/LICENSE.txt"
         DESTINATION "${_sinesoda_vst3_sdk_dir}")
  endif()
  if(NOT EXISTS "${_sinesoda_vst3_sdk_dir}/base/source/baseiids.cpp")
    message(FATAL_ERROR "Pinned VST3 SDK population failed at ${_sinesoda_vst3_sdk_dir}")
  endif()

  set(IPLUG2_CXX_STANDARD 20 CACHE STRING "C++ standard used by iPlug2")
  set(IPLUG2_UNIVERSAL ${SINESODA_UNIVERSAL_MACOS} CACHE BOOL
      "Build iPlug2 targets for arm64 and x86_64" FORCE)
  set(IPLUG_DEPLOY_PLUGINS ${SINESODA_DEPLOY_PLUGIN} CACHE BOOL
      "Deploy iPlug2 plug-in targets after building" FORCE)
  include("${IPLUG2_DIR}/iPlug2.cmake")
  find_package(iPlug2 REQUIRED)

  iplug_add_plugin(
    SineSoda
    SOURCES
      src/plugin/SineSoda.cpp
      src/plugin/SineSoda.h
      src/plugin/SpectrogramControl.cpp
      src/plugin/SpectrogramControl.h
      resources/resource.h
    RESOURCES
      resources/fonts/Roboto-Regular.ttf
    FORMATS VST3
    LINK SineSoda::DSP)

  if(APPLE AND SINESODA_ADHOC_SIGN_MACOS)
    find_program(SINESODA_CODESIGN_EXECUTABLE codesign REQUIRED)
    add_custom_command(
      TARGET SineSoda-vst3 POST_BUILD
      COMMAND "${SINESODA_CODESIGN_EXECUTABLE}" --force --deep --sign -
              "$<TARGET_BUNDLE_DIR:SineSoda-vst3>"
      COMMENT "Ad-hoc signing SineSoda.vst3 for local validation"
      VERBATIM)
  endif()
endif()
