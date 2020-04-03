echo 24 > /sys/class/gpio/export
echo 44 > /sys/class/gpio/export
echo 72 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio24/direction
echo "out" > /sys/class/gpio/gpio44/direction
echo 0 > /sys/class/gpio/gpio24/value
echo 1 > /sys/class/gpio/gpio44/value
echo 0 > /sys/class/gpio/gpio72/value

echo 30 > /sys/class/gpio/export
echo 46 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio30/direction
echo "out" > /sys/class/gpio/gpio46/direction
echo 0 > /sys/class/gpio/gpio30/value
echo 1 > /sys/class/gpio/gpio46/value