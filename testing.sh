cd submissions;
for i in `cat ../patches.txt`;
do
    cd $i;
    echo "######Testing for ${i}"
    if test -f "bzImage"; then
        echo "Found bzImage!";
        # scp bzImage to /
        scp bzImage root@10.0.1.100:/media/realroot/bzImage
        echo "Copied to board, going to reboot.";

        # reboot
        ssh root@10.0.1.100 "reboot";
        echo "Going to sleep.";
        sleep 60;
        echo "Wake up, board should be ready.";

        # insmod mt530.ko
        CMD="insmod /home/root/mt530_drv.ko";
        ssh root@10.0.1.100 $CMD;

        # ssh "dmesg -c" clean up unrelated dmesg
        ssh root@10.0.1.100 "dmesg -c > /dev/null";

        # run test cases > /home/root/students/${i}/testreport.txt
        echo "Running test cases for ${i}";
        CMD="/home/root/syscall > /home/root/students/${i}/testreport.txt";
        ssh root@10.0.1.100 $CMD;
        echo "Testing complete";

        # ssh "dmesg" > /home/root/students/${i}/dmesg.txt
        CMD="dmesg -c > /home/root/students/${i}/dmesg.txt";
        ssh root@10.0.1.100 $CMD;

        scp root@10.0.1.100:/home/root/students/${i}/testreport.txt ./
        scp root@10.0.1.100:/home/root/students/${i}/dmesg.txt ./
    else
        echo "bzImage not exist.";
    fi;
    echo -e "\n\n\n\n";
    cd ..;
done