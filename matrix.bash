trap 'kill $pids ; exit 1' SIGINT

echo '{ "cyc c (a)", "cyc c++ (c)", "fast (f)" }'
for batching in 1 0 ; do
    for datatype in ou a128 a1024 ; do
        cmake -DBATCHING=$batching -DDATATYPE=$datatype -DSHM=0 -DKEEP_ALL=1 . >cmake.log 2>&1
        ninja > ninja.log 2>&1 || exit
        echo "{ batching -> $batching, datatype -> \"$datatype\", {"
        rm -f [01].{ou,a*}.[acf].[acf].out
        for pub in a c f ; do
            for sub in a c f ; do
                [ "$pub$sub" = aa ] || echo ","
                echo -n "  (* $pub$sub *) "
                pids=""
                ./${sub}sub > $batching.$datatype.$sub.$pub.out & pids="$pids $!"
                ./${pub}pub & pids="$pids $!"
                sleep 8
                kill $pids
                wait
                # assuming no errors, lost samples ...
                perl -ane 'push @x,$F[0]; END{print "{".(join ",",@x)."}"}' $batching.$datatype.$sub.$pub.out
            done
        done
        echo "}}"
    done
done
