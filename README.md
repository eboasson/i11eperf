# Small interoperable DDS benchmark

This provides a set of small simple benchmark programs capable of doing throughput or latency measurements (assuming a well-synchronized) clock using

* [Eclipse Cyclone DDS C](https://github.com/eclipse-cyclonedds/cyclonedds) (`apub` and `asub`)
* [Eclipse Cyclone DDS C++11](https://github.com/eclipse-cyclonedds/cyclonedds-cxx) (`cpub` and `csub`)
* [eProsima Fast-DDS C++](https://github.com/eProsima/Fast-DDS) (`fpub` and `fsub`)
* [OpenDDS C++](https://github.com/OpenDDS/OpenDDS) (`opub` and `osub`)
* [ADLINK OpenSplice C++11](https://github.com/ADLINK-IST/opensplice) (`spub` and `ssub`)

It uses CMake `find_package` to find these implementations, skipping any that it can't find. Cyclone, Fast-DDS and OpenDDS provide support for this when installed, for OpenSplice there is a module included in this configuration that relies on the `OSPL_HOME` variable (optionally `OSPL_HOME_NORMALIZED` and `SPLICE_TARGET` so it can target a build tree rather than an install tree). If all the DDS implementations are built and installed according to their instructions, and `CMAKE_PREFIX_PATH` includes the installation roots of all (except OpenSplice, which relies on the `OSPL_HOME` environment) it should work.

The idea was that for C++ a single source should suffice, but it turns out that Fast-DDS and OpenDDS do not implement the "new", C++11 DDS API, and that the mapping of QoS and data types in Fast-DDS doesn't correspond to any standard. There are also small differences in IDL handling, which will result in a few warnings from the eProsima IDL compiler.

# Configuration

A CMake option is used to configure the data type:

* `DATATYPE` string (`a1024`): one of the built-in datatypes
  * `ou`: "one unsigned long":
  ```
  @topic
  struct ou {
    unsigned long long ts;
    unsigned long s;
  };
  ```
  The intent was for there to be nothing but the sequence number `s`, but it turns out that Fast-DDS doesn't support the `write_w_timestamp` operation and so a source timestamp had to be added to the data out of necessity.
  * `a32`: a 32-bytes large data type by appending a 20-bytes large octet array to `ou`
  * `a128`: a 128-bytes large one, analogous to `a32`
  * `a1024`, `a16k`, `a48k`, `a64k`, `a1M`, `a2M`, `a4M`, `a8M`: analogous to `a32` and `a128`

All other options are set via comand line parameters of the various pub/sub programs:

* `-n` integer (`1`): the number of topics to use (max = 20 to protect the stack in C version)
* `-k` integer (`0`): the history depth, 0 means keep-all
* `-i` integer (`0`): milliseconds between writing a burst of samples (one of each topic)
* `-r` integer (`1`): report interval for the subscriber
* `-o` string (`""`): write (some) raw latencies to the specified, disabled if the empty string
* `-b`: enable writer batching (Cyclone, C++ only on 0.10)
* `-l`: enable loans (Cyclone, Fast-DDS)
* `-y`: restrict Fast-DDS to loopback interface
* `-z`: disable Fast-DDS shared memory transport
* `-h`: print help and exit

The defaults are in parenthesis and correspond to an out-of-the-box throughput test with 1024-byte large samples. OpenDDS requires an initialization file or it won't use the interoperable DDSI protocol, this is copied into the build directly and used automatically.

# Running

The publishers have no options and start publishing immediately.

The subscriber has an optional argument, the name of a file to store the first 10M <timestamp, latency> pairs as double-precision floating-point numbers on graceful termination. Both are in seconds, timestamps are normalized to the first timestamp received, latency is computed by subtracting the received time stamp from the curren time and so assumes that the clocks are well synchronized.

The subscriber further more prints one line of output when it first receives a sample after the at least one second has passed since the previous line of output:

* sample rate in kS/s
* data rate in Gb/s
* number of lost samples (this should be 0 unless a keep-last history is used, "lost")
* number of times the sequence number jumped back ("errs", this should be 0 unless the publisher is restarted)
* 90-percentile latency of the first (at most) 1000 samples received in microseconds ("us90%lat")

All of these except the number of backward jumps are computed over the samples received since the previous line of output. The backward jump counter is never reset.

SIGTERM is handled properly. This is the only way to gracefully terminate the programs and to get the output file containing raw timing data.
