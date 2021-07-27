#
# Copyright(c) 2020 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
project(i11eperf LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.17)

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
set(fastddsgen "$ENV{HOME}/adlink/eProsima/src/fastddsgen/scripts/fastddsgen")

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

option(KEEP_ALL "Use KEEP_ALL history" ON)
option(BATCHING "Enable batching (Cyclone)" OFF)
option(SHM "Enable use of shared memory (FastDDS)" ON)
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
add_compile_definitions(DATATYPE=${DATATYPE})
add_compile_definitions(SLEEP_MS=${SLEEP_MS})

### Cyclone DDS / C -- copy IDL file to avoid name clash with fastddsgen output
find_package(CycloneDDS)
if(CycloneDDS_FOUND)
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
else()
  message(STATUS "CycloneDDS not found")
endif()

### Cyclone DDS / C++
find_package(CycloneDDS-CXX)
if(CycloneDDS-CXX_FOUND)
  idlcxx_generate(TARGET i11eperf_c FILES idl/i11eperf.idl)
  add_executable(cpub src/cpub.cpp)
  add_executable(csub src/csub.cpp)
  target_compile_definitions(cpub PUBLIC CYCLONEDDS)
  target_compile_definitions(csub PUBLIC CYCLONEDDS)
  target_link_libraries(cpub i11eperf_c CycloneDDS-CXX::ddscxx)
  target_link_libraries(csub i11eperf_c CycloneDDS-CXX::ddscxx)
else()
  message(STATUS "CycloneDDS C++ not found")
endif()

### Fast-DDS
find_package(fastrtps)
if(fastrtps_FOUND)
  find_package(fastcdr REQUIRED)
  find_package(foonathan_memory REQUIRED)
  set(fidlgen i11eperf.cxx i11eperf.h i11eperfPubSubTypes.cxx i11eperfPubSubTypes.h)
  list(TRANSFORM fidlgen PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
  add_custom_command(
    OUTPUT  ${fidlgen}
    COMMAND "${fastddsgen}"
    ARGS    "${CMAKE_CURRENT_LIST_DIR}/idl/i11eperf.idl")
  add_library(i11eperf_f STATIC ${fidlgen})
  target_include_directories(i11eperf_f PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${fastrtps_INCLUDE_DIR}" "${fastcdr_INCLUDE_DIR}")
  add_executable(fpub src/fpub.cpp src/fcommon.cpp)
  add_executable(fsub src/fsub.cpp src/fcommon.cpp)
  target_compile_definitions(fpub PUBLIC FASTDDS)
  target_compile_definitions(fsub PUBLIC FASTDDS)
  target_link_libraries(fpub i11eperf_f fastrtps fastcdr foonathan_memory)
  target_link_libraries(fsub i11eperf_f fastrtps fastcdr foonathan_memory)
else()
  message(STATUS "Fast-DDS not found")
endif()

### OpenDDS
#
# - run {opub,osub} with -DCPSConfigFile ../opendds.ini
#
# - use C++98 because on macOS at least the generated code depends on some template
#   that in the OpenDDS binaries comes from the __1 namespace (I guess that's TR1),
#   but that the application expects to come from the std namespace when built with
#   C++11 or later
#
# - no errors, but no comms with Cyclone (it looks like it assumes it should send to
#   Cyclone's transmit socket's port number rather than to the advertised locator;
#   adding the transmit socket to the set of sockets that Cyclone listens to helps a
#   little but not enough)
#
# - "failed to deserialize data payload for SPDP" with Fast-DDS ...
find_package(opendds)
if(OPENDDS_FOUND)
  add_library(i11eperf_o)
  set_property(TARGET i11eperf_o PROPERTY CXX_STANDARD 98)
  OPENDDS_TARGET_SOURCES(i11eperf_o idl/i11eperf.idl)
  target_link_libraries(i11eperf_o OpenDDS::Dcps)
  add_executable(opub src/opub.cpp)
  add_executable(osub src/osub.cpp)
  set_property(TARGET opub PROPERTY CXX_STANDARD 98)
  set_property(TARGET osub PROPERTY CXX_STANDARD 98)
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
