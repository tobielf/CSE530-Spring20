README
Team1, Xiangyu 1211550272

1. How to make
cd ./EOSI-team1-assgn02
make

2. How to use & sample run on part1.
Load the module into the kernel first.

insmod hcsr04.ko n=2

There are two user space program, "hcsr04" and "tester".
"hcsr04" will run all the unit test cases I wrote and exit, check the test-report.txt.
"tester" allows you to perform individual instruction step by step.

The demo here, using "tester".
Execute the user space program, you will receive a brief usage information.
root@quark:~# ./tester
Number of HCSR devices detected: 2
===============================================================
|    HCSR Usage:                                              |
|        read : ./tester <dev> read                           |
|        write: ./tester <dev> write <integer_value>          |
|        pins : ./tester <dev> pins <trigger_pin> <echo_pin>  |
|        param: ./tester <dev> param <m_samples> <delta>      |
|    More Instructions see README                             |
|       Contact: Xiangyu.Guo@asu.edu                          |
===============================================================

The user space program will detect the number of the device automatically for you.
Let's try to use a device without configure the pins.

root@quark:~# ./tester 0 read
Number of HCSR devices detected: 2
No data

It shows no data.
Let's configure a pin to device 0, (10 for trigger, and 6 for echo)

root@quark:~# ./tester 0 pins 10 6
Number of HCSR devices detected: 2
root@quark:~# d
[25009.971288] Registering 2 Devices
[25009.974931] Creating HCSR_0
[25009.990404] Creating HCSR_1
[26269.005074] shield: 10, logic: 10, dir: 26, mux1: 74, mux2: -1, mux: 0
[26269.012362] shield: 6, logic: 1, dir: 20, mux1: 68, mux2: -1, mux: 0
root@quark:~#

The default parameter of the device is m = 2, and delta = 200ms.
Let's do some sampling on this device

root@quark:~# ./tester 0 read
Number of HCSR devices detected: 2
0 7018216 116898721
Distance 298(centi-meter)
root@quark:~# d
[26376.970135] sampling 3
[26377.180131] sampling 2
[26377.390132] sampling 1
[26377.392533] Total interrupts 8
[26377.395616] diff 7013060
[26377.398173] diff 7004164
[26377.400848] diff 7023372
[26377.403415] diff 7033550
[26377.405976] large: 7033550 small: 7004164 sum 28074146
[26377.411230] Result: 7018216
root@quark:~#

Cool, the real distance of the sensor to the wall in my room is 3 meters.
Let's try with two devices. Initialize the device 1, (9 for trigger, and 5 for echo)

You can use write to invoke many measurements, the device will keep the most
recently five measurements. I am not demo it here, you can run the "hcsr" program.
The "hcsr" program will perform more test cases.

root@quark:~# ./tester 1 pins 9 5
Number of HCSR devices detected: 2
root@quark:~# d
[26538.122752] shield: 9, logic: 4, dir: 22, mux1: 70, mux2: -1, mux: 0
[26538.129787] shield: 5, logic: 0, dir: 18, mux1: 66, mux2: -1, mux: 0
root@quark:~#

Do some measurement on this device.

root@quark:~# ./tester 1 read
Number of HCSR devices detected: 2
0 128521 1182576401
Distance 5(centi-meter)
root@quark:~#

Oops! The distance looks not good, there must be some problem with the sensor!
Anyway, we still can play something interesting with these two devices.

Let's set the param of the device 0 to m = 100, delta = 2000.

root@quark:~# ./tester 0 param 100 2000
Number of HCSR devices detected: 2

And we give it a sampling task using write, then give one more, you will receive
a warning. However, we still can do the sampling using device 1.

root@quark:~# ./tester 0 write 0
Number of HCSR devices detected: 2
root@quark:~# ./tester 0 write 0
Number of HCSR devices detected: 2
Write error, Device or resource busy
root@quark:~# ./tester 1 read
Number of HCSR devices detected: 2
0 128188 2385204987
Distance 5(centi-meter)
root@quark:~#

Even after this step, let's go back using read on the device 0 to check the result.
The read I/O will be blocked, waiting for the sampling finish and fetch the result.

root@quark:~# ./tester 0 read
Number of HCSR devices detected: 2
<Wait for about 200 seconds>
0 7027713 2909080331
Distance 299(centi-meter)

You can also reconfigure the device pin after you changed it phsically.

root@quark:~# ./tester 1 pins 8 4
Number of HCSR devices detected: 2
root@quark:~# d
[28445.366471] shield: 8, logic: 40, dir: -1, mux1: -1, mux2: -1, mux: 0
[28445.373666] shield: 4, logic: 6, dir: 36, mux1: -1, mux2: -1, mux: 0
root@quark:~#

If you try to configure pin to the pin already occupied by others, you will fail.

root@quark:~# ./tester 1 pins 10 6
Number of HCSR devices detected: 2
Setup pins error, Bad address
root@quark:~# d
[28445.366471] shield: 8, logic: 40, dir: -1, mux1: -1, mux2: -1, mux: 0
[28445.373666] shield: 4, logic: 6, dir: 36, mux1: -1, mux2: -1, mux: 0
[28509.051872] Pin(s) already in use!

That's completed the part one.
Have fun!

One more thing......

There is a hidden easter(backdoor?) command, named "fun"(Didn't show up in the usage).
It will continues read data from the device, I took it walk around the room to
detect the distance.

root@quark:~# ./tester 0 param 2 60
Number of HCSR devices detected: 2
root@quark:~# ./tester 0 fun
Number of HCSR devices detected: 2
Distance 298(centi-meter)
Distance 298(centi-meter)
Distance 299(centi-meter)
Distance 226(centi-meter)
Distance 298(centi-meter)
Distance 298(centi-meter)
Distance 133(centi-meter)
Distance 99(centi-meter)
Distance 82(centi-meter)
Distance 65(centi-meter)
Distance 48(centi-meter)
Distance 78(centi-meter)
Distance 21(centi-meter)
Distance 84(centi-meter)
Distance 66(centi-meter)
^C

EOL

3. How to use & sample run on part2.
Load the module into the kernel first.
You can load the module in any order.

Let's try to insert device module first, then driver module.

root@quark:~# insmod hcsr-dev.ko
root@quark:~# insmod hcsr04.ko
root@quark:~# d
[179762.191317] Found the device -- HCSR_0  249561088
[179762.196319] Creating HCSR_0
[179762.199290] Going to run the thread
[179762.240849] Found the device -- HCSR_1  249561089
[179762.245849] Creating HCSR_1
[179762.248821] Going to run the thread

You can try any combination of the ordering you want.

root@quark:~# rmmod hcsr04.ko
root@quark:~# rmmod hcsr-dev.ko
root@quark:~# insmod hcsr-dev.ko
root@quark:~# insmod hcsr04.ko
root@quark:~# rmmod hcsr-dev.ko
root@quark:~# rmmod hcsr04.ko
root@quark:~# insmod hcsr04.ko
root@quark:~# insmod hcsr-dev.ko
root@quark:~# rmmod hcsr04.ko
root@quark:~# rmmod hcsr-dev.ko
root@quark:~# insmod hcsr04.ko
root@quark:~# insmod hcsr-dev.ko
root@quark:~# rmmod hcsr-dev.ko
root@quark:~# rmmod hcsr04.ko
root@quark:~# d -n 30
[179797.843922] Removing the device -- HCSR_1 249561089
[179797.920878] Removing the device -- HCSR_0 249561088
[179804.172001] Goodbye, unregister the device
[179816.821301] Found the device -- HCSR_0  248512512
[179816.826300] Creating HCSR_0
[179816.829267] Going to run the thread
[179816.879564] Found the device -- HCSR_1  248512513
[179816.884786] Creating HCSR_1
[179816.888967] Going to run the thread
[179822.862782] Removing the device -- HCSR_0 248512512
[179822.911926] Removing the device -- HCSR_1 248512513
[179822.952035] Goodbye, unregister the device
[179840.759440] Found the device -- HCSR_0  247463936
[179840.764663] Creating HCSR_0
[179840.805130] Going to run the thread
[179840.836663] Found the device -- HCSR_1  247463937
[179840.841890] Creating HCSR_1
[179840.877342] Going to run the thread
[179846.085423] Removing the device -- HCSR_1 247463937
[179846.170890] Removing the device -- HCSR_0 247463936
[179854.031989] Goodbye, unregister the device
[179862.097511] Found the device -- HCSR_0  246415360
[179862.102734] Creating HCSR_0
[179862.145383] Going to run the thread
[179862.183097] Found the device -- HCSR_1  246415361
[179862.188097] Creating HCSR_1
[179862.214247] Going to run the thread
[179870.792635] Removing the device -- HCSR_0 246415360
[179870.851870] Removing the device -- HCSR_1 246415361
[179870.921912] Goodbye, unregister the device
root@quark:~#

All the test cases passed in part1 still works, you can play around with it.

Let's focus on the newly added sysfs interface.

root@quark:~# cat /sys/class/HCSR/HCSR_0/echo
-1
root@quark:~# cat /sys/class/HCSR/HCSR_0/trigger
-1
root@quark:~#
root@quark:~# echo "Trying to use an invalid echo pin"
Trying to use an invalid echo pin
root@quark:~# echo -n "10" > /sys/class/HCSR/HCSR_0/echo
-sh: echo: write error: Invalid argument
root@quark:~# echo "Trying to use a pin number greater than 19"
Trying to use a pin number greater than 19
root@quark:~# echo -n "20" > /sys/class/HCSR/HCSR_0/echo
-sh: echo: write error: Invalid argument
root@quark:~# echo "Setting up a valid echo pin and trigger pin for device 0"
Setting up a valid echo pin and trigger pin for device 0
root@quark:~# echo -n "6" > /sys/class/HCSR/HCSR_0/echo
-sh: echo: write error: Invalid argument
root@quark:~# echo -n "10" > /sys/class/HCSR/HCSR_0/trigger
-sh: echo: write error: Invalid argument
root@quark:~#
root@quark:~# echo "Setting up a occipied trigger pin for device 1"
Setting up a occipied trigger pin for device 1
root@quark:~# echo -n "10" > /sys/class/HCSR/HCSR_1/trigger
-sh: echo: write error: Invalid argument
root@quark:~#
root@quark:~# echo "Setting up an available echo pin and trigger pin for device 1"
Setting up an available echo pin and trigger pin for device 1
root@quark:~# echo -n "9" > /sys/class/HCSR/HCSR_1/trigger
root@quark:~# echo -n "5" > /sys/class/HCSR/HCSR_1/echo
root@quark:~#
root@quark:~# echo "Setting up an invalid number of samples"
Setting up an invalid number of samples
root@quark:~# echo -n "0" > /sys/class/HCSR/HCSR_0/number_samples
-sh: echo: write error: Invalid argument
root@quark:~# echo "Setting up a valid number of samples"
Setting up a valid number of samples
root@quark:~# echo -n "4" > /sys/class/HCSR/HCSR_0/number_samples
root@quark:~#
root@quark:~# echo "Setting up an invalid sampling period"
Setting up an invalid sampling period
root@quark:~# echo -n "50" > /sys/class/HCSR/HCSR_0/sampling_period
-sh: echo: write error: Invalid argument
root@quark:~# echo "Setting up a valid sampling period"
Setting up a valid sampling period
root@quark:~# echo -n "300" > /sys/class/HCSR/HCSR_0/sampling_period
root@quark:~#
root@quark:~# cat /sys/class/HCSR/HCSR_0/number_samples
4
root@quark:~# cat /sys/class/HCSR/HCSR_0/sampling_period
300
root@quark:~#
root@quark:~# echo "Trigger a measurement using user space program"
Trigger a measurement using user space program
root@quark:~# ./tester 0 write 0
Number of HCSR devices detected: 2
root@quark:~#
root@quark:~# echo "Fetch the result from the sysfs distance"
Fetch the result from the sysfs distance
root@quark:~# cat /sys/class/HCSR/HCSR_0/distance
76

Note: the description of enable was a little bit vague, it could be 
1. set to one trigger only one measurement, 
then stop (change the enable value back to 0 by driver)
or
2. set to one and trigger a endless measurement,
keep measuring the result, until the user echo 0 to enable.

I talked with Professor Lee, he said either one is fine, as long as I specify in
the README, so I choose case 2.

root@quark:~# echo -n "1" > /sys/class/HCSR/HCSR_0/enable

Start sampling, and put my hand in front of the sensor, changing the distance.

root@quark:~# while true; do cat /sys/class/HCSR/HCSR_0/distance; sleep 1; done
76
72
68
66
62
58
52
^C

root@quark:~# echo -n "0" > /sys/class/HCSR/HCSR_0/enable

This will stop sampling. The distance variable stores the value from last time.

EOF