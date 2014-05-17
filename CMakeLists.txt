cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(PCL_Kinect2SDK)

SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

IF(WIN32 OR win64)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES .lib .dll)
ELSE()
   SET(CMAKE_FIND_LIBRARY_SUFFIXES .a)
ENDIF()

find_package(PCL 1.7 REQUIRED)
find_package( OpenCV REQUIRED )



add_subdirectory(src)

# THESE THREE ARE THE CORRECT ADDRESSES FOR THE UR10 PC, AND YOU MIGHT NEED TO USE THEM TO OVERWRITE THE AUTOMATIC VALUES
set(OPENNI_LIBRARY "C:/Program Files/OpenNI/Lib64/OpenNI64.lib")
set(OpenCV_DIR "C:/Users/ur10/Downloads/opencv/build")
set(PCL_DIR "C:/Users/ur10/Downloads/Repositories/pcl-trunk/build64")

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
# Based on suggestion from http://stackoverflow.com/questions/10671916/how-to-copy-dll-files-into-the-same-folder-as-the-executable-using-cmake

add_custom_command(TARGET Test_SDK POST_BUILD        		# Adds a post-build event to MyTest
	COMMAND ${CMAKE_COMMAND} -E copy_if_different  			# which executes "cmake - E copy_if_different..."
		"${PCL_DIR}/bin/pcl_features_debug.dll"      		# <--this is in-file
		$<TARGET_FILE_DIR:Test_SDK>)                 		# <--this is out-file path
		
add_custom_command(TARGET Test_SDK POST_BUILD        		# Adds a post-build event to MyTest
	COMMAND ${CMAKE_COMMAND} -E copy_if_different  			# which executes "cmake - E copy_if_different..."
		"${_OpenCV_LIB_PATH}/opencv_core248d.dll"      		# <--this is in-file
		$<TARGET_FILE_DIR:Test_SDK>)                 		# <--this is out-file path		
		
add_custom_command(TARGET Test_SDK POST_BUILD        		# Adds a post-build event to MyTest
	COMMAND ${CMAKE_COMMAND} -E copy_if_different  			# which executes "cmake - E copy_if_different..."
		"${_OpenCV_LIB_PATH}/opencv_highgui248d.dll"      	# <--this is in-file
		$<TARGET_FILE_DIR:Test_SDK>)                 		# <--this is out-file path
		
		
