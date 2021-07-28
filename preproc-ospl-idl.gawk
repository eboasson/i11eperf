/@topic/ {next}
{print}
/struct/ {n=$2}
/};/ && n!="" {print "#pragma keylist "n; n=""}
