#!/bin/bash

brew install  little-cms2 || exit 1
brew install  openexr || exit 1
brew reinstall gettext || exit 1
brew install  intltool json-c json-glib glib-networking gexiv2 librsvg poppler gtk+ py2cairo pygtk gtk-mac-integration || exit 1


instdir=/usr/local
cd "$TRAVIS_BUILD_DIR"
mkdir -p build && cd build

export PATH="/usr/local/opt/gettext/bin:$PATH"
export LD_LIBRARY_PATH="/usr/local/opt/gettext/lib:$LD_LIBRARY_PATH"
#export PKG_CONFIG_PATH="$HOME/homebrew/opt/jpeg-turbo/lib/pkgconfig:$HOME/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export ACLOCAL_PATH="/usr/local/opt/gettext/share/aclocal:$ACLOCAL_PATH"
export ACLOCAL_FLAGS="-I /usr/local/opt/gettext/share/aclocal"

#export LD_LIBRARY_PATH="$HOME/homebrew/opt/libffi/lib:$LD_LIBRARY_PATH"
#export PKG_CONFIG_PATH="$HOME/homebrew/opt/libffi/lib/pkgconfig:$PKG_CONFIG_PATH"

#export PATH="$instdir/bin:$PATH"
#export LD_LIBRARY_PATH="$instdir/lib:$LD_LIBRARY_PATH"
#export PKG_CONFIG_PATH="$instdir/lib/pkgconfig:$instdir/share/pkgconfig:$PKG_CONFIG_PATH"

#export LIBRARY_PATH="$LD_LIBRARY_PATH"

#export LIBTOOLIZE=glibtoolize

export CC="gcc -march=nocona -mno-sse3 -mtune=generic -I/usr/local/opt/gettext/include  -I/usr/X11/include"
export CXX="g++ -march=nocona -mno-sse3 -mtune=generic -I/usr/local/opt/gettext/include -I/usr/X11/include"
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
	(git clone -b BABL_0_1_56 https://git.gnome.org/browse/babl) || exit 1
fi
(cd babl && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./autogen.sh --disable-gtk-doc --enable-sse3=no --enable-sse4_1=no --enable-f16c=no --enable-altivec=no --prefix=${instdir} && make && make -j 3 install) || exit 1

if [ ! -e gegl ]; then
	(git clone -b GEGL_0_4_8 https://git.gnome.org/browse/gegl) || exit 1
fi
(cd gegl && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./autogen.sh --disable-docs --prefix=${instdir} --enable-gtk-doc-html=no --enable-introspection=no && make -j 3 install) || exit 1

#exit

if [ ! -e gimp ]; then
	(git clone -b GIMP_2_10_6 http://git.gnome.org/browse/gimp) || exit 1
	(cd gimp && patch -p1 < "$TRAVIS_BUILD_DIR/gimp-icons-osx.patch") || exit 1
fi
(cd gimp && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./autogen.sh --disable-gtk-doc --enable-sse=no --disable-python --prefix=${instdir} && make -j 3 install) || exit 1
#(cd gimp && TIFF_LIBS="-ltiff -ljpeg -lz" JPEG_LIBS="-ljpeg" ./configure --disable-gtk-doc --enable-sse=no --prefix=${instdir} && make -j 1 install) || exit 1

#if [ ! -e PhFGimp ]; then
#	(git clone https://github.com/aferrero2707/PhFGimp.git) || exit 1
#fi
cd "$TRAVIS_BUILD_DIR"
(mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON .. && make) || exit 1


cd "$TRAVIS_BUILD_DIR"
mkdir -p tools || exit 1
cd tools || exit 1
rm -rf macdylibbundler
git clone https://github.com/aferrero2707/macdylibbundler.git || exit 1
cd macdylibbundler || exit 1
make || exit 1

for f in "file-photoflow" "phf_gimp";  do
	./dylibbundler -od -of -x "$TRAVIS_BUILD_DIR/build/$f" -p @rpath
	install_name_tool -add_rpath "@loader_path/../../.." "$TRAVIS_BUILD_DIR/build/$f"
	install_name_tool -add_rpath "/tmp/McGimp-2.10.6/Contents/Resources/lib" "$TRAVIS_BUILD_DIR/build/$f"
	install_name_tool -add_rpath "/tmp/lib-std" "$TRAVIS_BUILD_DIR/build/$f"
done

cd "$TRAVIS_BUILD_DIR/build" || exit 1
tar czvf "$TRAVIS_BUILD_DIR/PhFGimp.tgz" file-photoflow phf_gimp
