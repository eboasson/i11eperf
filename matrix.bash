#!/bin/bash

trap 'kill -9 $pids $jigglepids ; exit 1' SIGINT

[ -f CMakeCache.txt -a -f build.ninja ] || \
    { echo "need to be in a build directory where CMake has been run successfully before with a Ninja generator" 2>&1 ; exit 2 ; }

jiggle=false
jigglepids=""
if $jiggle ; then
    for k in {0..1} ; do
        nice bash -c 'while [[ 0 = 0 ]] ; do : ; done' & jigglepids="$jigglepids $!"
    done
fi

CYCLONEDDS_URI_INIT="$CYCLONEDDS_URI"
echo '{ "cyc c (a)", "cyc+iox c (a)", "cyc c++ (c)", "cyc+iox c++ (c)", "cyc c++ loan (cl)", "cyc+iox c++ loan (cl)", "fast (f)" }' | \
    tee result.m
for batching in 0 ; do
    for datatype in a1024 a16k a48k a64k a1M a2M a4M a8M ; do
        sz=""
        case $datatype in
            a1024) sz=1024 ;;
            a16k) sz=16384 ;;
            a48k) sz=49152 ;;
            a64k) sz=65536 ;;
            a1M) sz=1048576 ;;
            a2M) sz=2097152 ;;
            a4M) sz=4194304 ;;
            a8M) sz=8388608 ;;
        esac
        [ -z $sz ] && { echo "oopsie!" >&2 ; exit 2 ; }
        cmake -DBATCHING=$batching -DDATATYPE=$datatype -DSHM=1 -DKEEP_ALL=0 -DSLEEP_MS=20 . >cmake.log 2>&1
        ninja > ninja.log 2>&1 || exit
        echo "{ batching -> $batching, size -> $sz, {" | tee -a result.m
        rm -f [01].{ou,a*}.[acf]*.[acf]*.{false,true}.out
        for pub in a c cl f ; do
            for sub in $pub ; do # a c cl f ; do # only matched pairs
                for iox in false true ; do
                    [ $iox = "true" -a $pub = "f" ] && continue # iox makes no sense for fastdds
                    [ $iox = "false" -a $pub = "cl" ] && continue # writer loans only work if iox
                    [ "$pub$sub$iox" = aafalse ] || echo "," | tee -a result.m
                    (echo -n "  (* $pub$sub" ; if $iox ; then echo -n " +iox" ; fi ; echo -n " *) ") | tee -a result.m
                    pids=""
                    export CYCLONEDDS_URI="$CYCLONEDDS_URI_INIT,<Shared><En>$iox</></>"
                    if $iox ; then
                        $HOME/C/iceoryx/install/bin/iox-roudi --config roudi.toml & pids="$pids $!"
                        sleep 2
                    fi
                    ./${sub}sub > $batching.$datatype.$sub.$pub.$iox.out & pids="$pids $!"
                    sleep 1
                    ./${pub}pub & pids="$pids $!"
                    sleep 30
                    kill -9 $pids
                    wait $pids
                    # assuming no errors, lost samples or other problems extract mean latency
                    perl -ane 'push @x,$F[9]; END{print "{".(join ",",@x)."}"}' $batching.$datatype.$sub.$pub.$iox.out | \
                        tee -a result.m
                done
            done
        done
        echo "}}" | tee -a result.m
    done
done

[ -n "$jigglepids" ] && kill -9 $jigglepids
