cmake_minimum_required( VERSION 2.6 )

project( ssao )

set( OSG_INCLUDE_DIR "/usr/local/include" CACHE PATH "OpenSceneGraph include path" )
set( OSG_LIB_DIR "/usr/local/lib" CACHE PATH "OpenSceneGraph library path" )

include_directories( ${OSG_INCLUDE_DIR} )
link_directories( ${OSG_LIB_DIR} )
message( ${OSG_INCLUDE_DIR})
set( SRCS  main.cpp ssao.cpp manipulator.cpp ssao.h texture_preprocess.h manipulator.h posnormal_mrt_shaders.h )

add_executable( ssao ${SRCS} )

target_link_libraries( ssao
optimized OpenThreads debug OpenThreadsd
optimized osg debug osgd
optimized osgGA debug osgGAd
optimized osgdb debug osgdbd
optimized osgUtil debug osgUtild
optimized osgViewer debug osgViewerd
optimized osgText debug osgTextd
optimized osgManipulator debug osgManipulatord )
