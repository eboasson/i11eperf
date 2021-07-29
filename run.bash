trap 'kill -9 $pids ; exit 1' SIGINT

function doit () {
    for x in a c f o s ; do
        xargs=""
        [ "$x" = "o" ] && xargs="-DCPSConfigFile ../opendds.ini"
        if [ -x ${x}pub -a -x ${x}sub ] ; then
            case $x in
                a) echo "# Cyclone DDS - C" ;;
                c) echo "# Cyclone DDS - C++" ;;
                f) echo "# Fast-DDS" ;;
                o) echo "# OpenDDS" ;;
                s) echo "# OpenSplice" ;;
            esac
            [ $x = s ] && ospl start
            ./${x}sub $xargs & pids=$!
            ./${x}pub $xargs & pids="$pids $!"
            sleep 20
            if [ "$x" = "o" ] ; then
                kill -9 $pids
            else
                kill $pids
            fi
            wait
            [ $x = s ] && ospl stop
        fi
    done
}

for x in fastcdr fastrtps fastddsgen foonathan_memory_vendor ; do
    echo -n "$x: "
    (cd ~/adlink/eProsima/src/$x && git describe --tag)
done
for x in OpenDDS ; do
    echo -n "$x: "
    (cd ~/adlink/$x && git describe --tag)
done
for x in cdds cxx ; do
    echo -n "$x: "
    (cd ~/C/$x && git describe --tag)
done
echo -n "i11eperf: " ; git describe --tag || git log --oneline -n1 | cat
echo -n "machine: " ; uname -a

for cfg in "-DSHM=1 -DBATCHING=0 -DSLEEP_MS=0 -DKEEP_ALL=1" \
           "-DSHM=1 -DBATCHING=0 -DSLEEP_MS=1 -DKEEP_ALL=0" \
           "-DSHM=1 -DBATCHING=1 -DSLEEP_MS=0 -DKEEP_ALL=1" \
           "-DSHM=0 -DBATCHING=1 -DSLEEP_MS=0 -DKEEP_ALL=1" ; do
    for datatype in ou a128 a1024 a64k a1M ; do
        fullcfg="-DDATATYPE=$datatype $cfg"
        echo "#### $fullcfg"
        cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo $fullcfg .
        ninja
        doit
    done
done
