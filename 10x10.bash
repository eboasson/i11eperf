#!/bin/bash

set +b
for ntopics in {1..10} ; do
    for nprocs in {1..10} ; do
        echo "============= ntopics $ntopics nprocs $nprocs =============="
        k=0
        pids=""
        while [[ $k -lt $nprocs ]] ; do
            k=$((k + 1))
            ./asub -k0 -n$ntopics -r4 > sub.$ntopics.$nprocs.$k -y & pids="$pids $!"
            ./apub -k0 -n$ntopics -i10 -y & pids="$pids $!"
        done
        sleep 10
        kill -9 $pids
        wait $pids >/dev/null 2>&1
    done
done
for nt in {1..10} ; do for np in {1..10} ; do echo -n "$nt $np " ; tail -1 sub.$np.$nt.1 ; done ; done | gawk '{print $1" "$2" "$12" "$14}' > lat-mean-90.txt
