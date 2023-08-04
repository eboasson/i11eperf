#!/bin/bash

[ -f CMakeCache.txt -a -f build.ninja ] || \
    { echo "need to be in a build directory where CMake has been run successfully before with a Ninja generator" 2>&1 ; exit 2 ; }

for ntopics in {1..10} ; do
    cmake -DBATCHING=0 -DDATATYPE=a1024 -DSHM=1 -DKEEP_ALL=1 -DSLEEP_MS=10 -DNTOPICS=$ntopics .
    ninja
    for nprocs in {1..10} ; do
        echo "============= ntopics $ntopics nprocs $nprocs =============="
        k=0
        while [[ $k -lt $nprocs ]] ; do
            k=$((k + 1))
            ./asub > sub.$ntopics.$nprocs.$k &
            ./apub &
        done
        sleep 10
        killall -9 apub asub
        wait
    done
done
for nt in {1..10} ; do for np in {1..10} ; do echo -n "$nt $np " ; tail -1 sub.$np.$nt.1 ; done ; done | gawk '{print $1" "$2" "$12" "$14}' > lat-mean-90.txt
