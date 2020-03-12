echo "Testing on initial echo and trigger"
cat /sys/class/HCSR/HCSR_0/echo
cat /sys/class/HCSR/HCSR_0/trigger

echo "Trying to use an invalid echo pin"
echo -n "10" > /sys/class/HCSR/HCSR_0/echo
echo "Trying to use a pin number greater than 19"
echo -n "20" > /sys/class/HCSR/HCSR_0/echo
echo "Setting up a valid echo pin and trigger pin for device 0"
echo -n "6" > /sys/class/HCSR/HCSR_0/echo
echo -n "10" > /sys/class/HCSR/HCSR_0/trigger

echo "Setting up a occipied trigger pin for device 1"
echo -n "10" > /sys/class/HCSR/HCSR_1/trigger

echo "Setting up an available echo pin and trigger pin for device 1"
echo -n "9" > /sys/class/HCSR/HCSR_1/trigger
echo -n "5" > /sys/class/HCSR/HCSR_1/echo

echo "Setting up an invalid number of samples"
echo -n "0" > /sys/class/HCSR/HCSR_0/number_samples
echo "Setting up a valid number of samples"
echo -n "4" > /sys/class/HCSR/HCSR_0/number_samples

echo "Setting up an invalid sampling period"
echo -n "50" > /sys/class/HCSR/HCSR_0/sampling_period
echo "Setting up a valid sampling period"
echo -n "300" > /sys/class/HCSR/HCSR_0/sampling_period

cat /sys/class/HCSR/HCSR_0/number_samples
cat /sys/class/HCSR/HCSR_0/sampling_period

echo "Trigger a measurement using user space program"
./tester 0 write 0

echo "Fetch the result from the sysfs distance"
cat /sys/class/HCSR/HCSR_0/distance