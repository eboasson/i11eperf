#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.17) # not sure what version is actually required
project(i11eperf LANGUAGES C CXX)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(default_build_type "RelWithDebInfo")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Note: "brew --prefix" stuff is for macOS, YMMV
#
# -- to build FastDDS --
#
# mkdir -p eProsima/src
# cd eProsima
# curl -O https://raw.githubusercontent.com/eProsima/Fast-DDS/master/fastrtps.repos
# vcs import src < fastrtps.repos
# colcon build --merge-install --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="`brew --prefix`;`brew --prefix openssl`"
# (cd src/fastddsgen && ./gradlew assemble)
#
# -- to build OpenDDS --
#
# git clone https://github.com/objectcomputing/OpenDDS.git
# cd OpenDDS
# ./configure --prefix=$PWD/install --optimize --no-debug
# bash -c '. setenv.sh && make all install'
#
# -- to configure --
# CMAKE_PREFIX_PATH will need adjusting
# fastddsgen may be in a different place
#
# cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$HOME/C/cdds/CC/install;$HOME/C/cxx/CC/install;$HOME/C/iceoryx/install;$HOME/adlink/eProsima/install;$HOME/adlink/OpenDDS/install;`brew --prefix`;`brew --prefix openssl`" ..

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

option(KEEP_ALL "Use KEEP_ALL history" ON)
option(BATCHING "Enable batching (Cyclone)" OFF)
option(LOANS "Enable loans on rd/wr (affects Cyclone C++ only)" OFF)
option(SHM "Enable use of shared memory (FastDDS)" ON)
set(NTOPICS 1 CACHE STRING "Use N topics in parallel")
set(DATATYPE "a1024" CACHE STRING "Data type to use")
set(SLEEP_MS 0 CACHE STRING "ms to sleep between two pubs")

if(KEEP_ALL)
  add_compile_definitions(HISTORY=KEEP_ALL)
else()
  add_compile_definitions(HISTORY=10)
endif()
if(BATCHING)
  add_compile_definitions(BATCHING=1)
else()
  add_compile_definitions(BATCHING=0)
endif()
if(SHM)
  add_compile_definitions(SHM=1)
else()
  add_compile_definitions(SHM=0)
endif()
if(LOANS)
  add_compile_definitions(LOANS=1)
else()
  add_compile_definitions(LOANS=0)
endif()
add_compile_definitions(DATATYPE=${DATATYPE})
add_compile_definitions(SLEEP_MS=${SLEEP_MS})
add_compile_definitions(NTOPICS=${NTOPICS})

### Cyclone DDS / C -- copy IDL file to avoid name clash with fastddsgen output
find_package(CycloneDDS QUIET)
if(CycloneDDS_FOUND)
  message(STATUS "CycloneDDS found")
  add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_a.idl
    COMMAND ${CMAKE_COMMAND} -E copy
    ARGS    ${CMAKE_SOURCE_DIR}/idl/i11eperf.idl ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_a.idl
    DEPENDS ${CMAKE_SOURCE_DIR}/idl/i11eperf.idl)
  idlc_generate(TARGET i11eperf_a FILES ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_a.idl)
  add_executable(apub src/apub.c)
  add_executable(asub src/asub.c)
  target_compile_definitions(apub PUBLIC CYCLONEDDS)
  target_compile_definitions(asub PUBLIC CYCLONEDDS)
  target_link_libraries(apub i11eperf_a CycloneDDS::ddsc)
  target_link_libraries(asub i11eperf_a CycloneDDS::ddsc)
  add_custom_target(roudi_toml ALL)
  add_custom_command(
    TARGET roudi_toml POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_LIST_DIR}/roudi.toml
            ${CMAKE_CURRENT_BINARY_DIR}/roudi.toml)
else()
  message(STATUS "CycloneDDS not found")
endif()

### Cyclone DDS / C++
find_package(CycloneDDS-CXX QUIET)
if(CycloneDDS-CXX_FOUND)
  # this shouldn't be needed, but the gods of CMake are appeased this way
  get_target_property(cyclonedds_has_shm CycloneDDS::ddsc SHM_SUPPORT_IS_AVAILABLE)
  mark_as_advanced(cyclonedds_has_shm)
  if(cyclonedds_has_shm)
    find_package(iceoryx_binding_c REQUIRED)
  endif()
  message(STATUS "CycloneDDS C++ binding found")
  idlcxx_generate(TARGET i11eperf_c FILES idl/i11eperf.idl)
  add_executable(cpub src/cpub.cpp)
  add_executable(csub src/csub.cpp)
  target_compile_definitions(cpub PUBLIC CYCLONEDDS)
  target_compile_definitions(csub PUBLIC CYCLONEDDS)
  target_link_libraries(cpub i11eperf_c CycloneDDS-CXX::ddscxx)
  target_link_libraries(csub i11eperf_c CycloneDDS-CXX::ddscxx)
else()
  message(STATUS "CycloneDDS C++ binding not found")
endif()

### Fast-DDS
find_package(fastrtps QUIET)
if(fastrtps_FOUND)
  message(STATUS "Fast-DDS found")
  # Assuming FastDDS was built using colcon as described in the Fast-DDS docs
  # we can make a reasonable guess at the location of fastddsgen
  # (this is with or without the "--merge-install" option of colcon)
  find_file(fastddsgen fastddsgen HINTS
    ${fastrtps_DIR}/../../../../../src/fastddsgen/scripts
    ${fastrtps_DIR}/../../../../src/fastddsgen/scripts)
  if(fastddsgen STREQUAL "fastddsgen-NOTFOUND")
    message(WARNING "Fast-DDS found, but fastddsgen not found, skipping Fast-DDS.
  fastrtps_DIR = ${fastrtps_DIR}
  try cmake -Dfastddsgen=PATH-TO-FASTDDSGEN")
  else()
    set(fidlgen i11eperf.cxx i11eperf.h i11eperfPubSubTypes.cxx i11eperfPubSubTypes.h)
    list(TRANSFORM fidlgen PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
    add_custom_command(
      OUTPUT  ${fidlgen}
      COMMAND "${fastddsgen}"
      ARGS    -replace "${CMAKE_CURRENT_LIST_DIR}/idl/i11eperf.idl"
      DEPENDS "${CMAKE_CURRENT_LIST_DIR}/idl/i11eperf.idl")
    add_library(i11eperf_f STATIC ${fidlgen})
    target_include_directories(i11eperf_f PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${fastrtps_INCLUDE_DIR}" "${fastcdr_INCLUDE_DIR}")
    add_executable(fpub src/fpub.cpp src/fcommon.cpp i11eperf.h)
    add_executable(fsub src/fsub.cpp src/fcommon.cpp i11eperf.h)
    target_compile_definitions(fpub PUBLIC FASTDDS)
    target_compile_definitions(fsub PUBLIC FASTDDS)
    target_link_libraries(fpub i11eperf_f fastrtps fastcdr foonathan_memory)
    target_link_libraries(fsub i11eperf_f fastrtps fastcdr foonathan_memory)
  endif()
else()
  message(STATUS "Fast-DDS not found")
endif()

### OpenDDS
#
# - may need to use C++98 because on macOS at least the generated code depends on
#   some template that in the OpenDDS binaries comes from the __1 namespace (I
#   guess that's TR1), but that the application expects to come from the std
#   namespace when built with C++11 or later
#
# - interoperability is really problematic ...
#   - DCPSRTISerialization is needed
#   - If you have a lot of patience and you enable tracing on the Cyclone side
#     (presumably because it slows down things a bit) it works
#   - FastDDS with OpenDDS results in "failed to deserialize data payload for SPDP"
#     errors from OpenDDS
#   - -Gxtypes-complete: following XTypes 7.6.3.2.1:
#       "The field complete.typeid_with_size shall contain the COMPLETE Hash
#       TypeIdentifier of the TopicType and the serialized size of the associated
#       TypeObject."
#     Cyclone DDS requires that both MINIMAL and COMPLETE type identifiers are
#     present, or neither are.  OpenDDS requires an IDL compiler option for that.
#   - -Ddata_representation=::OpenDDS::data_representation: OpenDDS IDL compiler
#     gives an error for @data_representation defined in XTypes 7.3.1.2.1.15 but
#     has a seemingly equivalent thing in an OpenDDS module
find_package(OpenDDS QUIET)
if(OPENDDS_FOUND)
  message(STATUS "OpenDDS found")
  add_library(i11eperf_o)
  #set_property(TARGET i11eperf_o PROPERTY CXX_STANDARD 98)
  OPENDDS_TARGET_SOURCES(i11eperf_o idl/i11eperf.idl OPENDDS_IDL_OPTIONS
    -Gxtypes-complete
    -Ddata_representation=::OpenDDS::data_representation)
  target_link_libraries(i11eperf_o OpenDDS::Dcps)
  add_executable(opub src/opub.cpp)
  add_executable(osub src/osub.cpp)
  #set_property(TARGET opub PROPERTY CXX_STANDARD 98)
  #set_property(TARGET osub PROPERTY CXX_STANDARD 98)
  target_compile_definitions(opub PUBLIC OPENDDS)
  target_compile_definitions(osub PUBLIC OPENDDS)
  target_link_libraries(opub i11eperf_o OpenDDS::OpenDDS)
  target_link_libraries(osub i11eperf_o OpenDDS::OpenDDS)
  add_custom_target(opendds_ini ALL)
  add_custom_command(
    TARGET opendds_ini POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_LIST_DIR}/opendds.ini
            ${CMAKE_CURRENT_BINARY_DIR}/opendds.ini)
else()
  message(STATUS "OpenDDS not found")
endif()

### OpenSplice
find_package(OpenSplice QUIET)
if(OpenSplice_FOUND)
  message(STATUS "OpenSplice found")
  # - need to remove @topic, add #pragma keylist
  # - renaming to i11eperf_s.idl also addresses file name clashes
  add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_s.idl
    COMMAND gawk
    ARGS    -f ${CMAKE_SOURCE_DIR}/preproc-ospl-idl.gawk ${CMAKE_SOURCE_DIR}/idl/i11eperf.idl > ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_s.idl
    DEPENDS ${CMAKE_SOURCE_DIR}/idl/i11eperf.idl ${CMAKE_SOURCE_DIR}/preproc-ospl-idl.gawk)
  osplidl_generate(i11eperf_s ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_s.idl)
  add_executable(spub src/cpub.cpp)
  add_executable(ssub src/csub.cpp)
  target_compile_definitions(spub PUBLIC OPENSPLICE)
  target_compile_definitions(ssub PUBLIC OPENSPLICE)
  target_link_libraries(spub i11eperf_s OpenSplice::cxx)
  target_link_libraries(ssub i11eperf_s OpenSplice::cxx)
else()
  message(STATUS "OpenSplice not found")
endif()
