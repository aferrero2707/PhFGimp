# PhFGimp

PhFGimp is a set of plug-ins that allow to invoke the PhotoFlow editor directly from GIMP.

The **file-photoflow** plug-in calls the PhotoFlow editor to process RAW files opened with GIMP.

The **phf_gimp** plug-in calls PhotoFlow to non-destructively edit GIMP layer data.

## Compilation and installation

* clone the git repository:

  git clone https://github.com/aferrero2707/PhFGimp
  
* create the build directory:

  cd PhFGimp
  mkdir -p build
  cd build
  
* configure, build and install the plug-ins:

  cmake -DBABL_FLIPS_DISABLED=OFF -DCMAKE_BUILD_TYPE=Release ..
  
  make
  
  make install (or sudo make install depending on the location of the GIMP plug-ins folder)
