cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(PCL_Kinect2SDK)

SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

IF(WIN32 OR win64)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES .lib .dll)
ELSE()
   SET(CMAKE_FIND_LIBRARY_SUFFIXES .a)
ENDIF()

find_package(PCL 1.7 REQUIRED)

find_package( OpenCV HINTS HINTS "$ENV{USERPROFILE}/Documents/opencv/build" "C:/Users/Public/Documents/opencv/build" "$ENV{USERPROFILE}/Documents/GitHub/BUILDS/OpenCV") 
	
string(REGEX REPLACE "/Release/Release" "/Release" OpenCV_LIB_DIR_OPT "${OpenCV_LIB_DIR_OPT}")
string(REGEX REPLACE "/Debug/Debug" "/Debug" OpenCV_LIB_DIR_DBG "${OpenCV_LIB_DIR_DBG}")

IF(OpenCV_FOUND AND ("${OpenCV_DIR}" STREQUAL "${OpenCV_CONFIG_PATH}") AND (NOT("${OpenCV_LIB_DIR_DBG}" STREQUAL "/Debug")) AND (NOT("${OpenCV_LIB_DIR_OPT}" STREQUAL "/Release")) )
	message(STATUS "OpenCV Found!")
	string(REGEX REPLACE "/lib" "/bin" OpenCV_BIN_DIR_OPT "${OpenCV_LIB_DIR_OPT}")
	string(REGEX REPLACE "/lib" "/bin" OpenCV_BIN_DIR_DBG "${OpenCV_LIB_DIR_DBG}")
ELSEIF(EXISTS "${OpenCV_DIR}/")
	message(WARNING "OpenCV not entirely found, but directory address <OpenCV_DIR> located and so assuming local build at ${OpenCV_DIR}")
	
	IF( (NOT EXISTS "${OpenCV_DIR}/lib/Debug/") AND (NOT EXISTS "${OpenCV_DIR}/lib/Release/") )
		MESSAGE(FATAL_ERROR "Provided path for OpenCV does not appear to contain valid debug OR release libraries!")
	ENDIF()
	
	SET(OpenCV_LIB_DIR_OPT "${OpenCV_DIR}/lib/Release" CACHE STRING "..." FORCE)
	SET(OpenCV_LIB_DIR_DBG "${OpenCV_DIR}/lib/Debug" CACHE STRING "..." FORCE)
	SET(OpenCV_3RDPARTY_LIB_DIR_OPT "${OpenCV_DIR}/lib/Release" CACHE STRING "..." FORCE)
	SET(OpenCV_3RDPARTY_LIB_DIR_DBG "${OpenCV_DIR}/lib/Debug" CACHE STRING "..." FORCE)
	
	LIST(APPEND _OpenCV_LIB_PATH "${OpenCV_DIR}/bin/Debug")
	LIST(APPEND _OpenCV_LIB_PATH "${OpenCV_DIR}/bin/Release")
	
	SET(OPENCV_FOUND TRUE CACHE BOOL "..." FORCE)
	SET(OpenCV_FOUND TRUE CACHE BOOL "..." FORCE)
	
	SET(OpenCV_BIN_DIR_OPT "${OpenCV_DIR}/bin/Release")
	SET(OpenCV_BIN_DIR_DBG "${OpenCV_DIR}/bin/Debug")
	SET(OpenCV_CONFIG_PATH "${OpenCV_DIR}" CACHE STRING "..." FORCE)
ELSE()
	MESSAGE(FATAL_ERROR "OpenCV not found! Please set the <OpenCV_DIR> variable to the OpenCV build directory.")
ENDIF()

IF(OpenCV_FOUND)
	include_directories(${OpenCV_INCLUDE_DIRS})
	link_directories(${_OpenCV_LIB_PATH})
	STRING(REGEX REPLACE "\\." "" OPENCV_VER "${OpenCV_VERSION}")
ENDIF()

add_subdirectory(src)

set(OPENNI_LIBRARY "C:/Program Files/OpenNI/Lib64/openNI64.lib" CACHE STRING "..." FORCE)

# THESE ARE THE CORRECT ADDRESSES FOR THE UR10 PC, AND YOU MIGHT NEED TO USE THEM TO OVERWRITE THE AUTOMATIC VALUES
#set(OpenCV_DIR "C:/Users/ur10/Downloads/opencv/build")
#set(PCL_DIR "C:/Users/ur10/Downloads/Repositories/pcl-trunk/build64")

set(KinectSDK_INCLUDE "$ENV{KINECTSDK20_DIR}inc" CACHE PATH "Include directory for Kinect2 SDK")
set(KinectSDK_LIB "$ENV{KINECTSDK20_DIR}lib/x64" CACHE PATH "Library directory for Kinect2 SDK")


include_directories(${OpenCV_DIR})
include_directories(${PCL_INCLUDE_DIRS})
include_directories(${PCL_Kinect2SDK_SOURCE_DIR}/include)
include_directories(${KinectSDK_INCLUDE})


link_directories(${PCL_LIBRARY_DIRS})
link_directories(${KinectSDK_LIB})


add_definitions(${PCL_DEFINITIONS})

file(GLOB_RECURSE PCL_Kinect2SDK_HEADERS include/*.h)
file(GLOB_RECURSE PCL_Kinect2SDK_SOURCES src/*.cpp)
set (PCL_Kinect2SDK_INCLUDE_DIRS "")
foreach (_headerFile ${PCL_Kinect2SDK_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND PCL_Kinect2SDK_INCLUDE_DIRS ${_dir})
endforeach()
list(REMOVE_DUPLICATES PCL_Kinect2SDK_INCLUDE_DIRS)

include_directories(${PCL_Kinect2SDK_INCLUDE_DIRS})
add_library(PCL_Kinect2SDK STATIC src/Microsoft_grabber2.cpp)
target_link_libraries (PCL_Kinect2SDK ${PCL_LIBRARIES} ${OpenCV_LIBS} Kinect20.lib)

add_executable (Test_SDK src/PCL_Kinect2SDK.cpp)
target_link_libraries (Test_SDK PCL_Kinect2SDK ${PCL_LIBRARIES} ${OpenCV_LIBS} Kinect20.lib)

# Custom build steps to move DLL into execution directory
IF(EXISTS "${PCL_DIR}/bin/pcl_common_debug.dll")
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_common_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_features_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_search_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_visualization_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_octree_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_kdtree_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_io_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_io_ply_debug.dll" $<TARGET_FILE_DIR:Test_SDK>)
ELSE()
	MESSAGE(WARNING "Could not find PCL Debug libraries - has PCL been built in Debug mode?")
ENDIF()

IF(EXISTS "${PCL_DIR}/bin/pcl_common_release.dll")
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_common_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_features_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_search_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_visualization_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_octree_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_kdtree_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_io_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PCL_DIR}/bin/pcl_io_ply_release.dll" $<TARGET_FILE_DIR:Test_SDK>)
ELSE()
	MESSAGE(WARNING "Could not find PCL Release libraries - has PCL been built in Release mode?")
ENDIF()

IF(EXISTS "${OpenCV_BIN_DIR_OPT}/opencv_core${OPENCV_VER}.dll")
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenCV_BIN_DIR_OPT}/opencv_core${OPENCV_VER}.dll" $<TARGET_FILE_DIR:Test_SDK>)
ENDIF()
IF(EXISTS "${OpenCV_BIN_DIR_DBG}/opencv_core${OPENCV_VER}d.dll")
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenCV_BIN_DIR_DBG}/opencv_core${OPENCV_VER}d.dll" $<TARGET_FILE_DIR:Test_SDK>)
ENDIF()

IF(EXISTS "${OpenCV_BIN_DIR_OPT}/opencv_highgui${OPENCV_VER}.dll")
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenCV_BIN_DIR_OPT}/opencv_highgui${OPENCV_VER}.dll" $<TARGET_FILE_DIR:Test_SDK>)
ENDIF()
IF(EXISTS "${OpenCV_BIN_DIR_DBG}/opencv_highgui${OPENCV_VER}d.dll")
add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenCV_BIN_DIR_DBG}/opencv_highgui${OPENCV_VER}d.dll" $<TARGET_FILE_DIR:Test_SDK>)
ENDIF()

IF(EXISTS "${OpenCV_BIN_DIR_OPT}/opencv_highgui${OPENCV_VER}.dll")
	add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenCV_BIN_DIR_OPT}/opencv_imgproc${OPENCV_VER}.dll" $<TARGET_FILE_DIR:Test_SDK>)
ENDIF()
IF(EXISTS "${OpenCV_BIN_DIR_DBG}/opencv_highgui${OPENCV_VER}d.dll")
add_custom_command(TARGET Test_SDK POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OpenCV_BIN_DIR_DBG}/opencv_imgproc${OPENCV_VER}d.dll" $<TARGET_FILE_DIR:Test_SDK>)
ENDIF()
