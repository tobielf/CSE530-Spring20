
# Enable Shield pin 11 as SPI MOSI
echo 24 > /sys/class/gpio/export
echo 44 > /sys/class/gpio/export
echo 72 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio24/direction
echo "out" > /sys/class/gpio/gpio44/direction
echo 0 > /sys/class/gpio/gpio24/value
echo 1 > /sys/class/gpio/gpio44/value
echo 0 > /sys/class/gpio/gpio72/value

# Enable Shield pin 13 as SPI SCK
echo 30 > /sys/class/gpio/export
echo 46 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio30/direction
echo "out" > /sys/class/gpio/gpio46/direction
echo 0 > /sys/class/gpio/gpio30/value
echo 1 > /sys/class/gpio/gpio46/value

# Use Shield pin 8 as SPI CS/LOAD
echo 40 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio40/direction

# Turn off shutdown mode
./spi_tester -p "\x01\x0c" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

# Set scan limit to 8
./spi_tester -p "\x08\x0b" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

# Test Display
./spi_tester -p "\x00\x0f" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value
