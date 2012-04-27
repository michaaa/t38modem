#!/bin/sh


WORKING_DIR=`pwd`

echo "This builds a t38modem v2.0 and ptlib/OPAL depending lib as a"
echo "standalone installation to /opt/t38modem"
echo
read -p "...continue? (y/n)"  ans

if [ "$ans" != "y" ]; then 
	exit 0
fi

echo
echo "Building ptlib..."

cd ptlib
./configure --prefix /opt/t38modem
make
make install
cd $WORKING_DIR


echo
echo "Building OPAL..."

cd opal
PKG_CONFIG_PATH=/opt/t38modem/lib/pkgconfig ./configure --prefix /opt/t38modem
make
make install
cd $WORKING_DIR

echo
echo "Building t38modem..."

cd t38modem
make PTLIBDIR=$WORKING_DIR/ptlib OPALDIR=$WORKING_DIR/opal USE_OPAL=1 USE_UNIX98_PTY=1 opt
cp -v obj_linux_x86_64_opal/t38modem /opt/t38modem/bin
cd $WORKING_DIR

echo
read -p "Install an init-Script to /etc/init.d/ and configuration to /etc/default? (y/n)"  ans

if [ "$ans" != "y" ]; then
        exit 0
fi

cp t38modem.init /etc/init.d/t38modem
chown -v 0:0 /etc/init.d/t38modem
chmod -v +x /etc/init.d/t38modem
cp -v  t38modem.default /etc/default/t38modem
