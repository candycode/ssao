/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

/* Written by Don Burns */

#include "FirstPersonManipulator.h"
#include <osgUtil/LineSegmentIntersector>

#include <osg/io_utils>

#ifndef M_PI
# define M_PI       3.14159265358979323846  /* pi */
#endif

using namespace osgGA;

FirstPersonManipulator::FirstPersonManipulator(bool grounded):
            _t0(0.0),
            _shift(false),
            _jump(false),
            _grounded(grounded)
{ 
    _minHeightAboveGround          = 2.0;
    _minDistanceAside              = 1.0;
    _minDistanceInFront            = 5.0;

    _speedAccelerationFactor       = 9;
    _speedDecelerationFactor       = 0.40;

    _maxSpeed                      = 15;
    _speedEpsilon                  = 0.02;
    


    _forwardDirection.set( 0,1,0);
    _viewDirection.set( 0,1,0);
    _upwardDirection.set( 0,0,1);
    _homeUp = _upwardDirection;
    _x=0;
    _y=0;
    _stop();
    home(0);
}

FirstPersonManipulator::~FirstPersonManipulator()
{
}

bool FirstPersonManipulator::intersect(const osg::Vec3d& start, const osg::Vec3d& end, osg::Vec3d& intersection) const
{
    osg::ref_ptr<osgUtil::LineSegmentIntersector> lsi = new osgUtil::LineSegmentIntersector(start,end);

    osgUtil::IntersectionVisitor iv(lsi.get());
    iv.setTraversalMask(_intersectTraversalMask);
    
    _node->accept(iv);
    
    if (lsi->containsIntersections())
    {
        intersection = lsi->getIntersections().begin()->getWorldIntersectPoint();
        return true;
    }
    return false;
}

void FirstPersonManipulator::setNode( osg::Node *node )
{
    _node = node;

    if (getAutoComputeHomePosition()) 
        computeHomePosition();

    home(0.0);
}

const osg::Node* FirstPersonManipulator::getNode() const
{
    return _node.get();
}

osg::Node* FirstPersonManipulator::getNode()
{
    return _node.get();
}


const char* FirstPersonManipulator::className() const 
{ 
    return "FirstPerson"; 
}

void FirstPersonManipulator::setByMatrix( const osg::Matrixd &mat ) 
{
    _inverseMatrix = mat;
    _matrix.invert( _inverseMatrix );

    _position.set( _inverseMatrix(3,0), _inverseMatrix(3,1), _inverseMatrix(3,2 ));
    osg::Matrix R(_inverseMatrix);
    R(3,0) = R(3,1) = R(3,2) = 0.0;
    _forwardDirection = osg::Vec3(0,0,-1) * R; // camera up is +Z, regardless of CoordinateFrame
    _viewDirection =_forwardDirection;
    _stop();
}

void FirstPersonManipulator::setByInverseMatrix( const osg::Matrixd &invmat) 
{
    _matrix = invmat;
    _inverseMatrix.invert( _matrix );

    _position.set( _inverseMatrix(3,0), _inverseMatrix(3,1), _inverseMatrix(3,2 ));
    osg::Matrix R(_inverseMatrix);
    R(3,0) = R(3,1) = R(3,2) = 0.0;
    _forwardDirection = osg::Vec3(0,0,-1) * R; // camera up is +Z, regardless of CoordinateFrame
    _viewDirection =_forwardDirection;
    _stop();
}

osg::Matrixd FirstPersonManipulator::getMatrix() const
{
    return (_matrix);
}

osg::Matrixd FirstPersonManipulator::getInverseMatrix() const 
{
    return (_inverseMatrix );
}

bool
FirstPersonManipulator::intersectDownWard(osg::Vec3d& intersection, bool atNodeCenter)
{
    osg::BoundingSphere bs = _node->getBound();

    /*
       * Find the ground - Assumption: The ground is the hit of an intersection
       * from a line segment extending from above to below the database at its 
       * horizontal center, that intersects the database closest to zero. */


    osg::Vec3 center= _position;
    if(atNodeCenter)
        center = bs.center();
    else
        center[2]=bs.center()[2];
    osg::Vec3 A = center + (_upwardDirection*(bs.radius()*2));
    osg::Vec3 B = center + (-_upwardDirection*(bs.radius()*2));

    if( (B-A).length() == 0.0)
    {
        return false;
    }

    // start with it high
    double ground = bs.radius() * 3;

    osg::Vec3d ip;
    if (intersect(A, B, ip))
    {
        double d = ip*_upwardDirection;
        if( d < ground )
            ground = d;
    }
    else
    {
        //osg::notify(osg::WARN)<<"FirstPersonManipulator : I can't find the ground!"<<std::endl;
        ground = 0.0;
        return false;
    }


    intersection = osg::Vec3d(center + _upwardDirection*( ground + _minHeightAboveGround ) );
    return true;
}

void FirstPersonManipulator::computeHomePosition()
{
    _forwardDirection=osg::Vec3(1,0,0)*(osg::Vec3(1,0,0)*_forwardDirection)+osg::Vec3(0,1,0)*(osg::Vec3(0,1,0)*_forwardDirection);
    _forwardDirection.normalize();
    _viewDirection =_forwardDirection;
    _upwardDirection=_homeUp;
    if( !_node.valid() )
        return;

    osg::Vec3d p;
    intersectDownWard(p);
    setHomePosition( p, p + _forwardDirection, _homeUp );
}

void FirstPersonManipulator::init(const GUIEventAdapter&, GUIActionAdapter&)
{
    //home(ea.getTime());

    _stop();
}

void FirstPersonManipulator::home(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us) 
{
    home(ea.getTime());
    us.requestRedraw();
    us.requestContinuousUpdate(false);

}

void FirstPersonManipulator::home(double) 
{
    if (getAutoComputeHomePosition()) 
        computeHomePosition();

    _position = _homeEye;
    _forwardDirection=osg::Vec3(1,0,0)*(osg::Vec3(1,0,0)*_forwardDirection)+osg::Vec3(0,1,0)*(osg::Vec3(0,1,0)*_forwardDirection);
    _forwardDirection.normalize();
    _viewDirection =_forwardDirection;
    _upwardDirection=_homeUp;
    
    _inverseMatrix.makeLookAt( _position, _position+_viewDirection, _homeUp );
    _matrix.invert( _inverseMatrix );


    _forwardSpeed = 0.0;
    _sideSpeed = 0.0;
    _upSpeed = 0.0;
}

bool FirstPersonManipulator::handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter &aa)
{
    switch(ea.getEventType())
    {
        case(osgGA::GUIEventAdapter::FRAME):
            _frame(ea,aa);
            return false;
        default:
            break;
    }

    if (ea.getHandled()) return false;

    switch(ea.getEventType())
    {
        case(osgGA::GUIEventAdapter::KEYUP):
            _keyUp( ea, aa );
            return false;
            break;

        case(osgGA::GUIEventAdapter::KEYDOWN):
            _keyDown(ea, aa);
            return false;
            break;
            
        case(osgGA::GUIEventAdapter::PUSH):
            _x = ea.getXnormalized();
            _y = ea.getYnormalized();
            return false;
            break;
            
        case(osgGA::GUIEventAdapter::DRAG):
            aa.requestContinuousUpdate(true);
            if(_drag(ea, aa))
                aa.requestRedraw();
            return false;
            break;

        case(osgGA::GUIEventAdapter::FRAME):
            _frame(ea,aa);
            return false;
            break;

        default:
            return false;
    }
}

void FirstPersonManipulator::getUsage(osg::ApplicationUsage& usage) const
{

    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <SpaceBar>",        "Reset the view to the home position.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <Shift/SpaceBar>",  "Reset the up vector to the vertical.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <UpArrow>",         "Run forward.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <DownArrow>",       "Run backward.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <LeftArrow>",       "Step to the left.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <RightArrow>",      "Step to the right.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <Shift/UpArrow>",   "Move up.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <Shift/DownArrow>", "Move down.");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <PageDown>",        "Switch Mode(Free vs Grounded).");
    usage.addKeyboardMouseBinding("FirstPerson Manipulator: <DragMouse>",       "Rotate the moving and looking direction.");
}



void FirstPersonManipulator::_keyUp( const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter & )
{
    switch( ea.getKey() )
    {
        case osgGA::GUIEventAdapter::KEY_Up:
        case osgGA::GUIEventAdapter::KEY_Down:
            if(!_shift)
                _decelerateForwardRate = true;
            else 
                _decelerateUpRate = true;
            break;

        case osgGA::GUIEventAdapter::KEY_Shift_L:
        case osgGA::GUIEventAdapter::KEY_Shift_R:
            _shift = false;
            _decelerateUpRate = true;
            break;

            
        case osgGA::GUIEventAdapter::KEY_Left:
        case osgGA::GUIEventAdapter::KEY_Right:
            if(!_shift)
                _decelerateSideRate = true;
            break;
    }
}

void FirstPersonManipulator::_keyDown( const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter & )
{
    switch( ea.getKey() )
    {
        case osgGA::GUIEventAdapter::KEY_Shift_L :
        case osgGA::GUIEventAdapter::KEY_Shift_R :
            _shift = true;
            break;

        case osgGA::GUIEventAdapter::KEY_Up:
            if( _shift )
            {
                if(_grounded)
                    break;
                _decelerateForwardRate = true;
                if(_upSpeed<_maxSpeed)
                    _upSpeed += _speedAccelerationFactor;
                _decelerateUpRate = false;
            }
            else
            {
                
                if(_forwardSpeed<_maxSpeed*2.f)
                    _forwardSpeed += _speedAccelerationFactor;
                _decelerateForwardRate = false;
            }
            break;

        case osgGA::GUIEventAdapter::KEY_Down:
            if( _shift )
            {
                if(_grounded)
                    break;
                _decelerateForwardRate = true;
                if(_upSpeed>-_maxSpeed)
                    _upSpeed -= _speedAccelerationFactor;
                _decelerateUpRate = false;
            }
            else
            {
                
                if(_forwardSpeed>-_maxSpeed*2.f)
                    _forwardSpeed -= _speedAccelerationFactor;
                _decelerateForwardRate = false;
            }
            break;

        case osgGA::GUIEventAdapter::KEY_Right:
            if( _shift )
            {
                _decelerateSideRate = true;
            }
            else
            {
                if(_sideSpeed<_maxSpeed)
                    _sideSpeed += _speedAccelerationFactor;
                _decelerateSideRate = false;
            }
            break;

        case osgGA::GUIEventAdapter::KEY_Left:
            if( _shift )
            {
                _decelerateSideRate = true;
            }
            else
            {
                if(_sideSpeed>-_maxSpeed)
                    _sideSpeed -= _speedAccelerationFactor;
                _decelerateSideRate = false;
            }
            break;
            

        case '0':
            if(_grounded)
            {
                if(!_jump)
                {
                    _jump=true;
                    _upSpeed=_maxSpeed;
                }
            }
            break;

        case osgGA::GUIEventAdapter::KEY_Page_Down:
            _grounded=!_grounded;
            if(_grounded)
            {
                std::cout<<"Current Mode : GROUNDED"<<std::endl;
                osg::Vec3d position=_position;
                osg::Vec3d viewDirection=_viewDirection;
                home(ea.getTime());
                _position=position;
                _viewDirection=viewDirection;
                
            }
            else
                std::cout<<"Current Mode : FREE"<<std::endl;
            break;

        case ' ':
            if(_shift)
            {
                osg::Vec3d position=_position;
                home(ea.getTime());
                _position=position;
            }
            else
            {
                home(ea.getTime());
            }
            break;
    }

}


bool FirstPersonManipulator::_drag( const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter & )
{
    float dx = (ea.getXnormalized()-_x)*100.f;
    float dy = (ea.getYnormalized()-_y)*40.f; // less sensitivity
    _x = ea.getXnormalized();
    _y = ea.getYnormalized();

        
    osg::Vec3d up = _upwardDirection;
    osg::Vec3d sv = _forwardDirection;
    osg::Vec3d lv = sv^up;
    lv.normalize();

    double yaw = osg::inDegrees(dx*50.0f*_dt);
    double pitch = osg::inDegrees(dy*50.0f*_dt);

    osg::Quat delta_rotate;

    osg::Quat pitch_rotate;
    osg::Quat yaw_rotate;

    yaw_rotate.makeRotate(-yaw,up.x(),up.y(),up.z());
    pitch_rotate.makeRotate(pitch,lv.x(),lv.y(),lv.z());

    delta_rotate = yaw_rotate*pitch_rotate;

    // update the direction
    osg::Matrixd rotation_matrix;
    if(_grounded)
    {
        rotation_matrix.makeRotate(yaw_rotate);
        _forwardDirection = _forwardDirection * rotation_matrix;
    }
    else
    {
        rotation_matrix.makeRotate(delta_rotate);
        _upwardDirection = _upwardDirection * rotation_matrix;
        _forwardDirection = _forwardDirection * rotation_matrix;
    }
    rotation_matrix.makeRotate(delta_rotate);
    _viewDirection = _viewDirection * rotation_matrix;    
    return true;
    
}


void FirstPersonManipulator::_frame( const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter & )
{
    double t1 = ea.getTime();
    if( _t0 == 0.0 )
    {
        _t0 = ea.getTime();
        _dt = 0.0;
    }
    else
    {
        _dt = t1 - _t0;
        _t0 = t1;
    }

    _sideDirection = _forwardDirection * osg::Matrix::rotate( -M_PI*0.5, _upwardDirection);

    _position += ( (_forwardDirection       * _forwardSpeed) + 
                   (_sideDirection         * _sideSpeed) +
                   (_upwardDirection        * _upSpeed) )* _dt;



    _adjustPosition();

    _inverseMatrix.makeLookAt( _position, _position + _viewDirection, _upwardDirection); 
    _matrix.invert(_inverseMatrix);

    if( _decelerateUpRate )
    {
        _upSpeed   *= _speedDecelerationFactor;
    }
    if( _decelerateSideRate )
    {
        _sideSpeed   *= _speedDecelerationFactor;
    }
    if( _decelerateForwardRate )
    {
        _forwardSpeed   *= _speedDecelerationFactor;
    } 
}

void FirstPersonManipulator::_adjustPosition()
{
    if( !_node.valid() )
        return;

    // Forward line segment at 3 times our intersect distance


    typedef std::vector<osg::Vec3d> Intersections;
    Intersections intersections;

    // Check intersects infront.
    osg::Vec3d ip;
    osg::Vec3d bottomPosition = _position + _upwardDirection*0.8*_minHeightAboveGround;
    if (intersect(bottomPosition, 
                  bottomPosition + (_forwardDirection * (_minDistanceInFront * 1.1f)),
                  ip ))
    {
        double d = (ip - bottomPosition).length();

        if( d < _minDistanceInFront )
        {
            _position += _forwardDirection*(d - _minDistanceInFront);
            if(_forwardSpeed<0)
                _forwardSpeed=0;
        }
    }
    
    // Check intersects behind.
    if (intersect(bottomPosition, 
                  bottomPosition - (_forwardDirection * (_minDistanceInFront * 1.1f)),
                  ip ))
    {
        double d = (ip - bottomPosition).length();

        if( d < _minDistanceInFront )
        {
            _position -= _forwardDirection*(d - _minDistanceInFront);
            if(_forwardSpeed>0)
                _forwardSpeed=0;
        }
    }

    // Check intersects right.
    if (intersect(bottomPosition, 
                  bottomPosition + (_sideDirection * (_minDistanceAside * 1.f)),
                  ip ))
    {
        double d = (ip - bottomPosition).length();

        if( d < _minDistanceAside )
        {
            _position += _sideDirection*(d - _minDistanceAside);
            if(_sideSpeed<0)
                _sideSpeed=0;
        }
    }
    
    // Check intersects left.
    if (intersect(bottomPosition, 
                  bottomPosition - (_sideDirection * (_minDistanceAside * 1.f)),
                  ip ))
    {
        double d = (ip - bottomPosition).length();

        if( d < _minDistanceAside )
        {
            _position -= _sideDirection*(d - _minDistanceAside);
            if(_sideSpeed>0)
                _sideSpeed=0;
        }
    }
    
    // Check intersects below.

    if (intersect(_position, 
                  _position - _upwardDirection*_minHeightAboveGround*2.f, 
                  ip ))
    {
        double d = (ip - _position).length();

        if( d <= _minHeightAboveGround*1 )
        {
          _position = ip + (_upwardDirection * _minHeightAboveGround);
          if(_grounded)
          {
              _upSpeed = 0;
              _jump=false;
          }
        }
        else if(_grounded) // stick to the ground
        {
            _decelerateUpRate = false;
            _upSpeed-=_speedDecelerationFactor;
        }
    }
    else if(_grounded)
    {
        osg::Vec3d pos;
        bool aboveGround=false;
        if(intersectDownWard(pos,false))
        {
            double d=(_position-pos)*_upwardDirection;
            if(d>0)
            {
                _decelerateUpRate = false;
                _upSpeed-=_speedDecelerationFactor;//gravity effect
                aboveGround=true;
            }
        }
        if(!aboveGround)
        {
            std::cout<<"you are lost in space!"<<std::endl;
            home(0);
        }
    }
    
    // Check intersects above.

    if (intersect(_position, 
                  _position + _upwardDirection*_minHeightAboveGround*1.f, 
                  ip ))
    {
        double d = (ip - _position).length();

        if( d < _minHeightAboveGround )
          _position = ip - (_upwardDirection * _minHeightAboveGround);
    }
}


void FirstPersonManipulator::_stop()
{
    _forwardSpeed = 0.0;
    _sideSpeed = 0.0;
    _upSpeed = 0.0;
}

void FirstPersonManipulator::getCurrentPositionAsLookAt( osg::Vec3 &eye, osg::Vec3 &center, osg::Vec3 &up )
{
    eye = _position;
    center = _position + _viewDirection;
    up.set(getUpVector(getCoordinateFrame(_position)));
}

