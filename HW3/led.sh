

./spi_tester -p "\x1c\x01" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x22\x02" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x41\x03" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x82\x04" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x82\x05" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x41\x06" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x22\x07" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x1c\x08" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

sleep 1;

./spi_tester -p "\x00\x01" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x1c\x02" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x22\x03" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x44\x04" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x44\x05" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x22\x06" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x1c\x07" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x00\x08" -s 10 -b16; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

sleep 1;