#ifndef MANIPULATOR_H_
#define MANIPULATOR_H_

#include <string>

namespace osg
{
    class Node;
    class Group;
}

namespace osgManipulator
{
    class Dragger;
}

/// @param dg group whose first child is a Dragger.
osg::Group* CreatePreRenderManipulatorTree( osg::Group* dg );

/// @param dg group whose first child is a Dragger.
osgGA::GUIEventHandler* CreateObjectToManipulatorTransformPicker( osg::Group* dg );

osgManipulator::Dragger* CreateManipulator( const std::string& type );

void InsertTransform( osg::Node* );

/// @param dg group whose first child is a Dragger.
osgGA::GUIEventHandler* CreateDraggerSelectorHandler( osg::Group* );

#endif //MANIPULATOR_H_