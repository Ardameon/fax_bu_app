#!/bin/bash
set -e

echo
echo "build fax BU application: $*"
echo

BUILD=$1

CFLAGS=
LIBS=
DEBUG=
CLEAN=

check_opt()
{
    echo "checl_opt: '$OPTION'"

    case "$OPTION" in
    dbg)
        DEBUG=yes
    ;;
    clean)
        CLEAN=yes
    ;;
    *)
    ;;
    esac

    return 0;
}

check_spandsp_lib()
{
	CURDIR="`pwd`"
	if [ ! -e $SPANDSP_DIR/lib/libspandsp.a ] || [ -n "$CLEAN" ]; then
		echo ""
		echo "spandsp wasn't found. try build..."
		echo ""
		cd $CURDIR/spandsp/
		./configure.sh $ARCH
		./make-and-install.sh $ARCH

		if [ -e $ARCH/deploy/usr/local/lib/libspandsp.a ]; then
			echo ""
			echo "spandsp was successfully built"
			echo ""
		else
			echo ""
			echo "spandsp wasn't found after build attempt!"
			echo ""
			exit 1;
		fi

		cd -
	fi;
	CFLAGS+=`pkg-config --cflags-only-other $CURDIR/spandsp/$ARCH/deploy/usr/local/lib/pkgconfig/spandsp.pc`
    CFLAGS+=" -I$SPANDSP_DIR/include"
    CFLAGS+=" -I$LIBTIFF_DIR/include"
    LDFLAGS+=" -L$SPANDSP_DIR/lib"
    LDFLAGS+=" -L$LIBTIFF_DIR/lib"
	LIBS+=`pkg-config --libs-only-l --static $CURDIR/spandsp/$ARCH/deploy/usr/local/lib/pkgconfig/spandsp.pc`
}

OPTION=$2 && check_opt
OPTION=$3 && check_opt
OPTION=$4 && check_opt

if [ -n "$DEBUG" ]; then
    CFLAGS="${CFLAGS} -O0 -g"
else
    CFLAGS="${CFLAGS} -O2 -DNDEBUG -g"
fi

case "$BUILD" in
    mv)
        ARCH=mv
        ;;
    mv2)
        ARCH=mv2
        ;;
    x64)
        ARCH=x86_64
        ;;
    *)
        echo "platform is not defined"
        exit 1
        ;;
esac

export SPANDSP_DIR="`pwd`/spandsp/$ARCH/deploy/usr/local"
export LIBTIFF_DIR="`pwd`/libtiff/$ARCH/deploy/usr/local"
export INCLUDE_PATH="`pwd`/include"

[ -n "$CLEAN" ] && make -C ./src clean

case "$BUILD" in
    mv)
        check_spandsp_lib
#        export CFLAGS="${CFLAGS} -std=c99 -D_BSD_SOURCE"
        export CFLAGS="${CFLAGS}"
        export LDFLAGS="${LDFLAGS}"
        export CROSS_COMPILER='arm-mv5sft-linux-gnueabi-'
        export LIBS="${LIBS}"
        make -C ./src 
        ;;
    mv2)
        check_spandsp_lib
        export CFLAGS="${CFLAGS}"
        export LDFLAGS="${LDFLAGS}"
        export LIBS="${LIBS}"
        export CROSS_COMPILER='arm-marvell-linux-gnueabi-'
        make -C ./src 
        ;;
    x64)
        check_spandsp_lib
        export CFLAGS="${CFLAGS} -m64"
        export LDFLAGS="${LDFLAGS}"
        export LIBS="${LIBS}"
        make -C ./src 
        ;;
    *)
        echo "platform is not defined"
        exit 1
        ;;
esac

