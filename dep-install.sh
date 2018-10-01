#!/bin/bash

brew install  little-cms2
brew install  openexr
brew reinstall gettext
brew install  intltool json-c json-glib glib-networking gexiv2
brew install  gtk+


instdir=/usr/local
cd "$TRAVIS_BUILD_DIR"
mkdir -p build && cd build

export PATH="/usr/local/opt/gettext/bin:$PATH"
export LD_LIBRARY_PATH="/usr/local/opt/gettext/lib:$LD_LIBRARY_PATH"
#export PKG_CONFIG_PATH="$HOME/homebrew/opt/jpeg-turbo/lib/pkgconfig:$HOME/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
#export ACLOCAL_PATH="$HOME/homebrew/share/aclocal:$ACLOCAL_PATH"
#export ACLOCAL_FLAGS="-I $HOME/homebrew/share/aclocal -I $HOME/homebrew/Cellar/gettext/0.19.8.1/share/aclocal/"

#export LD_LIBRARY_PATH="$HOME/homebrew/opt/libffi/lib:$LD_LIBRARY_PATH"
#export PKG_CONFIG_PATH="$HOME/homebrew/opt/libffi/lib/pkgconfig:$PKG_CONFIG_PATH"

#export PATH="$instdir/bin:$PATH"
#export LD_LIBRARY_PATH="$instdir/lib:$LD_LIBRARY_PATH"
#export PKG_CONFIG_PATH="$instdir/lib/pkgconfig:$instdir/share/pkgconfig:$PKG_CONFIG_PATH"

#export LIBRARY_PATH="$LD_LIBRARY_PATH"

#export LIBTOOLIZE=glibtoolize

#export CC="gcc -march=nocona -mno-sse3 -mtune=generic -I$HOME/homebrew/opt/gettext/include  -I$HOME/homebrew/include -I/usr/X11/include"
#export CXX="g++ -march=nocona -mno-sse3 -mtune=generic -I$HOME/homebrew/include -I$HOME/homebrew/opt/gettext/include -I/usr/X11/include"
#export CFLAGS="-g -O2 -I $HOME/homebrew/include -I /usr/X11/include"
export CXXFLAGS="-stdlib=libc++"
export CPPFLAGS="-I $/usr/local/opt/gettext/include"
export LDFLAGS="-L/usr/local/opt/gettext/lib"


#HOMEBREW_NO_AUTO_UPDATE=1 brew reinstall --build-from-source --verbose exiv2
#HOMEBREW_NO_AUTO_UPDATE=1 brew install intltool gettext json-c json-glib glib-networking gexiv2
#HOMEBREW_NO_AUTO_UPDATE=1 brew info json-glib glib glib-networking gexiv2
#HOMEBREW_NO_AUTO_UPDATE=1 brew install --HEAD babl
#HOMEBREW_NO_AUTO_UPDATE=1 brew install --HEAD gegl
#HOMEBREW_NO_AUTO_UPDATE=1 brew info gcc
#ls -lh $HOME/homebrew/bin/gcc*

#ls $HOME/homebrew/opt
#ls $HOME/homebrew/opt/gettext/bin
#ls $HOME/homebrew/bin
ls /usr/local
which autopoint
which gcc
which libtool
which xgettext
#$HOME/homebrew/bin/pkg-config --exists --print-errors "pygtk-2.0 >= 2.10.4"
#ls $HOME/homebrew/Cellar/exiv2/0.26/lib
#exit


if [ ! -e libmypaint-1.3.0 ]; then
	curl -L https://github.com/mypaint/libmypaint/releases/download/v1.3.0/libmypaint-1.3.0.tar.xz -O
	tar xvf libmypaint-1.3.0.tar.xz
fi
(cd libmypaint-1.3.0 && ./configure --enable-introspection=no --prefix=${instdir} && make -j 3 install) || exit 1

if [ ! -e mypaint-brushes ]; then
	git clone -b v1.3.x https://github.com/Jehan/mypaint-brushes
fi
(cd mypaint-brushes && ./autogen.sh && ./configure --prefix=${instdir} && make -j 3 install) || exit 1


if [ ! -e babl ]; then
	(git clone https://git.gnome.org/browse/babl) || exit 1
fi
(cd babl && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./autogen.sh --disable-gtk-doc --enable-sse3=no --enable-sse4_1=no --enable-f16c=no --enable-altivec=no --prefix=${instdir} && make && make -j 3 install) || exit 1

if [ ! -e gegl ]; then
	(git clone https://git.gnome.org/browse/gegl) || exit 1
fi
(cd gegl && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./autogen.sh --disable-docs --prefix=${instdir} --enable-gtk-doc-html=no --enable-introspection=no && make -j 3 install) || exit 1

#exit

if [ ! -e gimp ]; then
	(git clone -b gimp-2-10 http://git.gnome.org/browse/gimp) || exit 1
fi
(cd gimp && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./autogen.sh --disable-gtk-doc --enable-sse=no --prefix=${instdir} && make -j 3 install) || exit 1
#(cd gimp && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./configure --disable-gtk-doc --enable-sse=no --prefix=${instdir} && make -j 1 install) || exit 1

#if [ ! -e PhFGimp ]; then
#	(git clone https://github.com/aferrero2707/PhFGimp.git) || exit 1
#fi
cd "$TRAVIS_BUILD_DIR/build"
(cd PhFGimp && mkdir -p build && cd build && cmake .. && make install)
