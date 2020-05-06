for i in `cat patches.txt`;
do
    cd kernel;
    echo "####Building bzImage for" $i;
    patch -p1 < ../submissions/${i}/new.patch;

    cp ./.config ../.config.bak
    cp ../.config ./.config

    ARCH=x86 LOCALVERSION= CROSS_CIMPILE=i586-poky-linux- make -j4
    mv arch/x86/boot/bzImage ../submissions/${i}/

    cp ../.config.bak ./.config
    
    patch -p1 -R < ../submissions/${i}/new.patch;
    cd ..;
done