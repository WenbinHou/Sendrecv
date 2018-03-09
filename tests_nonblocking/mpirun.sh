#!/bin/bash
maxpid=2
src_path="/home/cuixiang/hbx/Sendrecv/build"
((i=0))
while read hostip port;do
    echo "start process on ${hostip}"
    echo "cd ${src_path} && $1 -i ${i} -f $2 -n 8196"
    ssh -n $hostip "cd ${src_path} && $1 -i ${i} -f $2 -n 8196" > out/$i-outf.out 2>&1 &
    ((i++))
done < start_hostlist
