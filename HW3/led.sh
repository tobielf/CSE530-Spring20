

./spi_tester -p "\x01\x1c"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x02\x22"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x03\x41"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x04\x82"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x05\x82"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x06\x41"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x07\x22"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x08\x1c"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

sleep 1;

./spi_tester -p "\x01\x00"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x02\x1c"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x03\x22"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x04\x44"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x05\x44"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x06\x22"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x07\x1c"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

./spi_tester -p "\x08\x00"; echo 1 > /sys/class/gpio/gpio40/value ; echo 0 > /sys/class/gpio/gpio40/value

sleep 1;