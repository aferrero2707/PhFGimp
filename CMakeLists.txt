cmake_minimum_required(VERSION 2.8.9)

project(phf_gimp CXX C)

# options controlling the build process
option(BABL_FLIPS_DISABLED "BABL flips disabled" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(CMAKE_POSITION_INDEPENDENT_CODE True)

find_package(PkgConfig)

pkg_check_modules(LCMS2 REQUIRED lcms2) 

pkg_check_modules (TIFF REQUIRED libtiff-4)
#find_package (TIFF REQUIRED)

#find_package (Exiv2)
pkg_check_modules(GEXIV2 REQUIRED gexiv2)
MESSAGE( STATUS "GEXIV2_INCLUDE_DIRS:         " ${GEXIV2_INCLUDE_DIRS} )
MESSAGE( STATUS "GEXIV2_LIBRARY_DIRS:         " ${GEXIV2_LIBRARY_DIRS} )

#pkg_check_modules(GIMP REQUIRED gimp-2.0>=2.9.0) 
pkg_check_modules(GIMP_2_9 REQUIRED gimp-2.0>=2.9.0)
pkg_check_modules(GIMPUI REQUIRED gimpui-2.0>=2.9.0) 
set (HAVE_GIMP_2_9 "1")
MESSAGE( STATUS "GIMP_2_9_CFLAGS:         " ${GIMP_2_9_CFLAGS} )
MESSAGE( STATUS "GIMP_2_9_INCLUDE_DIRS:         " ${GIMP_2_9_INCLUDE_DIRS} )
MESSAGE( STATUS "GIMP_2_9_LIBRARY_DIRS:         " ${GIMP_2_9_LIBRARY_DIRS} )
MESSAGE( STATUS "GIMP_2_9_LDFLAGS:         " ${GIMP_2_9_LDFLAGS} )

#set(COMPILE_FLAGS "-fpermissive -DHAVE_GIMP_2_9=${HAVE_GIMP_2_9}")
set(COMPILE_FLAGS "-DHAVE_GIMP_2_9=${HAVE_GIMP_2_9}")

if(BABL_FLIPS_DISABLED)
  set(COMPILE_FLAGS "${COMPILE_FLAGS} -DBABL_FLIPS_DISABLED=1")
endif()


include_directories(${TIFF_INCLUDE_DIRS})
link_directories(${TIFF_LIBRARY_DIRS})

include_directories(${LCMS2_INCLUDE_DIRS})
link_directories(${LCMS2_LIBRARY_DIRS})

#include_directories(${EXIV2_INCLUDE_DIRS})
#link_directories(${EXIV2_LIBRARY_DIRS})
include_directories(${GEXIV2_INCLUDE_DIRS})
link_directories(${GEXIV2_LIBRARY_DIRS})

include_directories(${GIMP_2_9_INCLUDE_DIRS})
link_directories(${GIMP_2_9_LIBRARY_DIRS})
include_directories(${GIMPUI_INCLUDE_DIRS})
link_directories(${GIMPUI_LIBRARY_DIRS})

#include(CheckFunctionExists)
#include(CheckLibraryExists)
#check_library_exists(gimp gimp_register_file_handler_raw "libgimp/gimpfileops_pdb.h" HAVE_GIMP_REGISTER_FILE_HANDLER_RAW_FUNCTION)
#if(HAVE_GIMP_REGISTER_FILE_HANDLER_RAW_FUNCTION)
#  set(COMPILE_FLAGS "${COMPILE_FLAGS} -DHAVE_GIMP_REGISTER_FILE_HANDLER_RAW_FUNCTION=1")
#endif(HAVE_GIMP_REGISTER_FILE_HANDLER_RAW_FUNCTION)



SET(CMAKE_CXX_FLAGS_DEBUG "-Wall -O0 -g -DNDEBUG -DDO_WARNINGS ${COMPILE_FLAGS}") 
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -ftree-vectorize -DNDEBUG ${COMPILE_FLAGS}  -Wno-unused-parameter") 
SET(CMAKE_CXX_FLAGS_TEST "-O3 -g -DNDEBUG ${COMPILE_FLAGS}") 


add_executable(file-photoflow file-photoflow.cc)

target_link_libraries(file-photoflow ${LIBS} 
    ${GIMP_2_9_LIBRARIES}
    ${GIMPUI_LIBRARIES}
    ${GTKMM_LIBRARIES} 
    ${GLIBMM_LIBRARIES} 
    ${GLIB_LIBRARIES} 
    ${GMODULE_LIBRARIES} 
    ${GOBJECT_LIBRARIES} 
    ${EXIV2_LIBRARIES}
    ${GEXIV2_LIBRARIES}
    ${LCMS2_LIBRARIES} 
    ${TIFF_LIBRARIES}
  )
  
add_executable(phf_gimp phf_gimp.cc)

target_link_libraries(phf_gimp ${LIBS} 
    ${GIMP_2_9_LIBRARIES}
    ${GIMPUI_LIBRARIES}
    ${GTKMM_LIBRARIES} 
    ${GLIBMM_LIBRARIES} 
    ${GLIB_LIBRARIES} 
    ${GMODULE_LIBRARIES} 
    ${GOBJECT_LIBRARIES} 
    ${EXIV2_LIBRARIES}
    ${GEXIV2_LIBRARIES}
    ${LCMS2_LIBRARIES} 
    ${TIFF_LIBRARIES}
  )
  
INSTALL(TARGETS file-photoflow phf_gimp RUNTIME DESTINATION ${GIMP_2_9_PREFIX}/lib/gimp/2.0/plug-ins)
