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
# to build FastDDS:
#
# mkdir -p eProsima/src
# cd eProsima
# curl -O https://raw.githubusercontent.com/eProsima/Fast-DDS/master/fastrtps.repos
# vcs import src < fastrtps.repos
# colcon build --merge-install --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="`brew --prefix`;`brew --prefix openssl`"
# (cd src/fastddsgen && ./gradlew assemble)

# cmake -GNinja -DCMAKE_PREFIX_PATH="$HOME/C/cdds/CC/install;$HOME/C/cxx/CC/install;$HOME/C/iceoryx/install;$HOME/adlink/eProsima/install;`brew --prefix`;`brew --prefix openssl`" ..
set(fastddsgen "$ENV{HOME}/adlink/eProsima/src/fastddsgen/scripts/fastddsgen")

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

option(KEEP_ALL "Use KEEP_ALL history" ON)
option(BATCHING "Enable batching (impl. dependent)" ON)
option(SHM "Enable use of shared memory (impl. dependent)" OFF)
set(DATATYPE "ou" CACHE STRING "Data type to use")
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

### Cyclone DDS / C
find_package(CycloneDDS REQUIRED)
add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_c.idl
  COMMAND ${CMAKE_COMMAND} -E copy
  ARGS    ${CMAKE_SOURCE_DIR}/idl/i11eperf.idl ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_c.idl
  DEPENDS ${CMAKE_SOURCE_DIR}/idl/i11eperf.idl)

idlc_generate(TARGET i11eperf_c FILES ${CMAKE_CURRENT_BINARY_DIR}/i11eperf_c.idl)

add_executable(apub src/apub.c)
add_executable(asub src/asub.c)
target_compile_definitions(apub PUBLIC CYCLONEDDS)
target_compile_definitions(asub PUBLIC CYCLONEDDS)
target_link_libraries(apub i11eperf_c CycloneDDS::ddsc)
target_link_libraries(asub i11eperf_c CycloneDDS::ddsc)

### Cyclone DDS / C++
find_package(CycloneDDS-CXX REQUIRED)
idlcxx_generate(TARGET i11eperf_cpp FILES idl/i11eperf.idl)

add_executable(cpub src/cpub.cpp)
add_executable(csub src/csub.cpp)
target_compile_definitions(cpub PUBLIC CYCLONEDDS)
target_compile_definitions(csub PUBLIC CYCLONEDDS)
target_link_libraries(cpub i11eperf_cpp CycloneDDS-CXX::ddscxx)
target_link_libraries(csub i11eperf_cpp CycloneDDS-CXX::ddscxx)

### Fast-DDS
find_package(fastcdr REQUIRED)
find_package(foonathan_memory REQUIRED)
find_package(fastrtps REQUIRED)

set(fidlgen i11eperf.cxx i11eperf.h i11eperfPubSubTypes.cxx i11eperfPubSubTypes.h)
list(TRANSFORM fidlgen PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")

add_custom_command(
  OUTPUT  ${fidlgen}
  COMMAND "${fastddsgen}"
  ARGS    "${CMAKE_CURRENT_LIST_DIR}/idl/i11eperf.idl")

add_library(i11eperf_lib STATIC ${fidlgen})
target_include_directories(i11eperf_lib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" "${fastrtps_INCLUDE_DIR}" "${fastcdr_INCLUDE_DIR}")

add_executable(fpub src/fpub.cpp)
add_executable(fsub src/fsub.cpp)
target_compile_definitions(fpub PUBLIC FASTDDS)
target_compile_definitions(fsub PUBLIC FASTDDS)
target_link_libraries(fpub i11eperf_lib fastrtps fastcdr foonathan_memory)
target_link_libraries(fsub i11eperf_lib fastrtps fastcdr foonathan_memory)
