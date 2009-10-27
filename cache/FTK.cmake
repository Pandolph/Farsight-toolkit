#
# FTK.cmake
#

# Initial settings for building FARSIGHT.

set(ITK_DIR $ENV{BUILD_DIR}/itk CACHE STRING "Set to ITK build directory")
set(VTK_DIR $ENV{BUILD_DIR}/vtk CACHE STRING "Set to VTK build directory")
set(VXL_DIR $ENV{BUILD_DIR}/vxl CACHE STRING "Set to VXL build directory")

set(Boost_INCLUDE_DIR $ENV{BOOST_DIR} CACHE STRING "Set to Boost include path")
set(QT_QMAKE_EXECUTABLE $ENV{QMAKE_DIR} CACHE STRING "Set to qmake directory")

set(BUILD_BIONET OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_EXAMPLES OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_MDL OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_NUCLEI ON CACHE BOOL "Set to ON for FARSIGHT")
set(BUILD_REGISTRATION OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_RPITrace3D OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_Rendering OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_SAMPLE_EDITOR OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_SpineRing OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_TESTING OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_TISSUENETS OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_TRACING OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_TRACKER OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_TraceEdit OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_TrackEdit OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_VESSEL OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_ftl2d OFF CACHE BOOL "Set to OFF for FARSIGHT")
set(BUILD_ftl3d OFF CACHE BOOL "Set to OFF for FARSIGHT")

#set(USE_KPLS ON CACHE BOOL "Compile with proprietary KPLS library")
set(USE_KPLS OFF CACHE BOOL "Compile without proprietary KPLS library")
