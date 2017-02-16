# PhFGimp

PhFGimp is a set of plug-ins that allow to invoke the PhotoFlow editor directly from GIMP.

The **file-photoflow** plug-in calls the PhotoFlow editor to process RAW files opened with GIMP.

The **phf_gimp** plug-in calls PhotoFlow to non-destructively edit GIMP layer data.

## Compilation and installation

* install the GIMP development files (from sources or via your preferred package manager)

  The PhFGimp plug-ins are only compatible with GIMP version 2.9.x or higher.

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

## Installing PhotoFlow

In order to use the PhFGimp plug-ins, you also need to install the latest version of the PhotoFlow editor from [here](https://github.com/aferrero2707/PhotoFlow/releases/tag/continuous).

The photoflow executable is searched in different locations depending on the operating system:

* under **Linux**, it is searched in the folders included in the PATH environment variable
* under **OSX**, the plug-ins will use the ```/Applications/photoflow.app/Contents/MacOS/photoflow``` executable if available
* under **Windows**, the plug-ins will use the ```%USERPROFILE%\photoflow\bin\photoflow.exe``` executable if available

This choices can be overridden by setting the **PHOTOFLOW_PATH** environment variable to the full path of the PhotoFlow executable.
