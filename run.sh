
cd submissions;
for i in `ls`;
do
    cd $i;

    # Renaming the folders, unify the rest operations.
    # echo "####Renaminng for" $i;
    # if test -d "Part1"; then
    #     echo "Renaminng Part1";
    #     mv Part1 part1;
    # fi;
    # if test -d "Part2"; then
    #     echo "Renaminng Part2";
    #     mv Part2 part2;
    # fi;
    # if test -d "PART1"; then
    #     echo "Renaminng PART1";
    #     mv PART1 part1;
    # fi;
    # if test -d "PART2"; then
    #     echo "Renaminng PART2";
    #     mv PART2 part2;
    # fi;

    echo "####Checking for" $i;
    # Check for submission files & Makefile warnings.
    # [ToDo] Creat a function for below, so we can reuse the code.
    # echo -e "\n\n\n\n";
    echo -e "Files:\n";
    ls -la;

    echo -e "Makefile Wall flag?\n";
    cat Makefile | grep "Wall";
    
    # Two parts assignment.
    # if test -d "part1"; then
    #     cd part1;
    #     echo -e "Files\n";
    #     ls -la;
    #     echo -e "Makefile Wall flag\n";
    #     cat Makefile | grep "Wall";
    #     cd ..;
    # fi;
    # if test -d "part2"; then
    #     cd part2;
    #     echo -e "Files\n";
    #     ls -la;
    #     echo -e "Makefile Wall flag\n";
    #     cat Makefile | grep "Wall";
    #     cd ..;
    # fi;

    echo "####Compiling for" $i;

    dos2unix Makefile;
    # Required student to put "APP" variable in their Makefile.
    make "APP=tester -DGRADING";

    # Creat a remote directory using student's name.
    CMD="mkdir -p /home/root/students/${i}";
    ssh root@10.0.1.100 $CMD;

    # TESTER=$(cat Makefile | grep "APP =" | awk '{split($0,a," = "); print a[2];}');
    TESTER="tester";
    echo "Copying....";
    echo $TESTER;
    DIR="/home/root/students/${i}/";
    echo "to ${DIR}";
    scp $TESTER root@10.0.1.100:$DIR;
    echo "Done!";
    make clean;

    # If it's kernel image assignment.
    #echo "" >> ../../patches.txt;
    DIR="/home/root/students/${i}/";
    for j in `ls *.patch`;
    do
        mv ${j} new.patch
        echo "${i}" >> ../../patches.txt;
    done;

    # Else kernel module assignment.
    # [ToDo] Create a function, so we can resue the code.
    # if test -d "part1"; then
    #     cd part1;
    #     dos2unix Makefile;
    #     make;

    #     CMD="mkdir -p /home/root/students/${i}/part1";
    #     ssh root@10.0.1.100 $CMD;

    #     TESTER=$(cat Makefile | grep "APP =" | awk '{split($0,a," = "); print a[2];}');
    #     echo "Copying....";
    #     echo $TESTER;
    #     DIR="/home/root/students/${i}/part1";
    #     echo "to ${DIR}";
    #     scp $TESTER root@10.0.1.100:$DIR;
    #     echo "Done!";

    #     DIR="/home/root/students/${i}/part1";
    #     echo "" > ins.sh;
    #     for j in `ls *.ko`;
    #     do
    #         echo "insmod ${j} n=2" > ins.sh;
    #         scp $j root@10.0.1.100:$DIR;
    #     done;
    #     scp ins.sh root@10.0.1.100:$DIR;
    #     ssh root@10.0.1.100 "chmod a+x ${DIR}/ins.sh";
    #     cd ..;
    # fi;
    # if test -d "part2"; then
    #     cd part2;
    #     dos2unix Makefile;
    #     make;

    #     CMD="mkdir -p /home/root/students/${i}/part2";
    #     ssh root@10.0.1.100 $CMD;
    #     DIR="/home/root/students/${i}/part2";
    #     echo "" > ins.sh;
    #     for j in `ls *.ko`;
    #     do
    #         echo "insmod ${j} n=2" >> ins.sh;
    #         scp $j root@10.0.1.100:$DIR;
    #     done;
    #     for j in `ls *.sh`;
    #     do
    #         scp $j root@10.0.1.100:$DIR;
    #         ssh root@10.0.1.100 "chmod a+x ${DIR}/${j}";
    #     done;
    #     cd ..;
    # fi;

    echo -e "\n\n\n\n";
    cd ..;
done
