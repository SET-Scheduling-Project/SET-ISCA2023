for ir in `ls results/json/*.json`
do
    echo $ir
    filename=${ir#*json/}
    dirname=results/${filename%.*}
    if [ -d ${dirname} ]
    then
        echo ${dirname} Exists.
    else
        mkdir -p ${dirname}
        ./build/parser $ir ${dirname} > ${dirname}/parserlog.txt 2> ${dirname}/parsererr.txt
        n=$?
        for((i=0;i<n;++i)) do
            if [ -f ${dirname}/wlist/$i.json ]; then
                echo core $i
                cur_dir=${dirname}/wlist/$i/
                mkdir -p $cur_dir
                ./build/allocator $i ${dirname} > ${cur_dir}log.txt;
            fi
        done
        ./build/refill $n $ir ${dirname}
    fi
done