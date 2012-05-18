cmake_minimum_required( VERSION 2.4 )

PROJECT( ssao )

SET( OSG_INCLUDE_DIR "/usr/local/include" CACHE PATH "OpenSceneGraph include path" )
SET( OSG_LIB_DIR "/usr/local/lib" CACHE PATH "OpenSceneGraph library path" )

INCLUDE_DIRECTORIES( ${OSG_INCLUDE_DIR} )
LINK_DIRECTORIES( ${OSG_LIB_DIR} )

SET( SRCS  main.cpp ssao.cpp manipulator.cpp ssao.h texture_preprocess.h manipulator.h posnormal_mrt_shaders.h )

ADD_EXECUTABLE( ssao ${SRCS} )

TARGET_LINK_LIBRARIES( ssao
optimized OpenThreads debug OpenThreadsd
optimized osg debug osgd
optimized osgGA debug osgGAd
optimized osgdb debug osgdbd
optimized osgUtil debug osgUtild
optimized osgViewer debug osgViewerd
optimized osgText debug osgTextd
optimized osgManipulator debug osgManipulatord )
