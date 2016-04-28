#include <osgManipulator/CommandManager>
#include <osgManipulator/TranslateAxisDragger>
#include <osgManipulator/TabPlaneTrackballDragger>
#include <osgManipulator/ScaleAxisDragger>
#include <osgManipulator/TabBoxTrackballDragger>
#include <osgDB/ReadFile>
#include <osgViewer/Viewer>
#include <map>
#include <stdexcept>

static const char PASSTHROUGH_VERT[] =
"varying vec4 color;"
"void main(void)\n"
"{\n"
"  color = gl_FrontMaterial.diffuse;\n"
"  gl_Position = ftransform();\n"
"}\n";


static const char PASSTHROUGH_FRAG[] =
"varying vec4 color;"
"void main(void)\n"
"{\n"
"  gl_FragColor = color;\n"
"}\n";

// this is used to hide geometry e.g. manipulator
// geometry should be rendering in pre-render steps:
// 1) add manipulator geometry (asGeometry()) to group
// 2) apply this shader to the group with override attribute
static const char DISCARD_FRAG[] =
"void main(void)\n"
"{\n"
"  discard;\n"
"  //gl_FragColor = color;\n"
"}\n";


///
/// returns a group:
///   manipulatorGroup ---> 'discard' shader program with OVERRIDE option 
///          |               to discard manipulator geometry
///          |
///          -- group //-> required to handle dragger selection
///              |
///              |
///              -- manipulator : should we use asGroup() or manipulator ?
///
osg::Group* CreatePreRenderManipulatorTree( osg::Group* d )
{
    if( !d ) return 0;
    osg::ref_ptr< osg::Group > g = new osg::Group;
    g->addChild( d );
    osg::ref_ptr< osg::Program > p = new osg::Program;
    p->addShader( new osg::Shader( osg::Shader::FRAGMENT, DISCARD_FRAG ) );
	p->addShader( new osg::Shader( osg::Shader::VERTEX,   PASSTHROUGH_VERT ) );
    g->getOrCreateStateSet()->setAttributeAndModes( osg::get_pointer( p ), osg::StateAttribute::OVERRIDE );
    return g.release();
}

// class to handle events with a pick
class PickHandler : public osgGA::GUIEventHandler 
{
public: 

    /// @param d group whose first child is a dragger
    PickHandler( osg::Group* d ) : draggerGroup_( d ), transform_( 0 ) {}
            
    bool handle( const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa )
    {
        switch( ea.getEventType() )
        {
        case osgGA::GUIEventAdapter::PUSH:
        {
            osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
            if( view ) pick( view, ea );
            return false;
        }    
        default: return false;
        }
        return false;

    }
    virtual void pick( osgViewer::View* view, const osgGA::GUIEventAdapter& ea )
    {
        osgUtil::LineSegmentIntersector::Intersections intersections;
        const float x = ea.getX();
        const float y = ea.getY();
        if( view->computeIntersections( x, y, intersections ) )
        {
            for( osgUtil::LineSegmentIntersector::Intersections::iterator hitr = intersections.begin();
                 hitr != intersections.end();
                 ++hitr )
            {
                if( !hitr->nodePath.empty() )
                {
                    // if parent is dragger break
                    if( dynamic_cast< osgManipulator::Dragger* >( hitr->nodePath.back()->getParent( 0 ) ) ) break;
                    
                    // find first matrix transform above picked geode
                    osg::ref_ptr< osg::MatrixTransform > t;
                    osg::NodePath::const_reverse_iterator rit = hitr->nodePath.rbegin();
                    for( ; rit != hitr->nodePath.rend(); ++rit )
                    {
                        t = dynamic_cast< osg::MatrixTransform* >( *rit );
                        if( t ) break;
                    }
                    if( t )
                    {
                        osg::ref_ptr< osgManipulator::Dragger > dragger = draggerGroup_->getNumChildren() > 0 ?
                            dynamic_cast< osgManipulator::Dragger* >( draggerGroup_->getChild( 0 ) ) 
                            : 0;
                        if( !dragger ) break;
                        float scale = t->getBound().radius() * 1.5f;
	                    dragger->setMatrix( osg::Matrix::scale( scale, scale, scale ) *
                                             osg::Matrix::translate( t->getBound().center() ) );
                        dragger->setHandleEvents( true );
                        dragger->removeTransformUpdating( osg::get_pointer(transform_ ) );
                        transform_ = t;
                        dragger->addTransformUpdating( osg::get_pointer( transform_ ) );
                        dragger->setNodeMask( 0xffffffff );
                    }
                    break;
                }
            }
        }
        else
        {
            osg::ref_ptr< osgManipulator::Dragger > dragger = draggerGroup_->getNumChildren() > 0 ?
                    dynamic_cast< osgManipulator::Dragger* >( draggerGroup_->getChild( 0 ) ) 
                    : 0;
            if( dragger != 0 )
            {
                dragger->removeTransformUpdating( osg::get_pointer(transform_ ) );
                dragger->setHandleEvents( false );
                dragger->setNodeMask( 0x00000000 );
            }            
        }
    }
    private:
        osg::ref_ptr< osg::Group > draggerGroup_;
        osg::ref_ptr< osg::MatrixTransform > transform_;

};



/// Creates an event handler which sets the manipulator transform to the parent transform
/// of the picked object
osgGA::GUIEventHandler* CreateObjectToManipulatorTransformPicker( osg::Group* d )
{
    return new PickHandler( d );
}


/// Setup dragger geometry, shader and event handling.
osgManipulator::Dragger* SetupDraggerProperties( osgManipulator::Dragger* d )
{
    if( !d ) throw std::runtime_error( "NULL dragger" );
    if( !d ) return 0; // in case exceptions not enabled
    d->setupDefaultGeometry();
    d->setNodeMask( 0x00000000 );
    
   	//setup passthrough shader
    osg::ref_ptr< osg::Program > program = new osg::Program;
    program->setName( "Passthrough" );
	program->addShader( new osg::Shader( osg::Shader::FRAGMENT, PASSTHROUGH_FRAG ) );
	program->addShader( new osg::Shader( osg::Shader::VERTEX,   PASSTHROUGH_VERT ) );
	d->getOrCreateStateSet()->setAttributeAndModes( program.get(), osg::StateAttribute::ON );
    // we want the dragger to handle it's own events automatically
    d->setHandleEvents(false);
    // if we don't set an activation key or mod mask then any mouse click on
    // the dragger will activate it, however if do define either of ActivationModKeyMask or
    // and ActivationKeyEvent then you'll have to press either than mod key or the specified key to
    // be able to activate the dragger when you mouse click on it.  Please note the follow allows
    // activation if either the ctrl key or the 'a' key is pressed and held down.
    d->setActivationModKeyMask(osgGA::GUIEventAdapter::MODKEY_CTRL);
    d->setActivationKeyEvent('d');
    return d;
}


/// Creates manipulator from text.
osgManipulator::Dragger* CreateManipulator( const std::string& type )
{
    osg::ref_ptr< osgManipulator::Dragger > d;
    if( type == "TranslateAxisDragger" ) d = SetupDraggerProperties( new osgManipulator::TranslateAxisDragger );
    else if( type == "TabBoxDragger" ) d = SetupDraggerProperties( new osgManipulator::TabBoxDragger );
    else if( type == "TabPlaneTrackballDragger" ) d = SetupDraggerProperties( new osgManipulator::TabPlaneTrackballDragger );
    else if( type == "RotateCylinderDragger" ) d = SetupDraggerProperties( new osgManipulator::RotateCylinderDragger );
    else if( type == "RotateSphereDragger" ) d = SetupDraggerProperties( new osgManipulator::RotateSphereDragger );
    else if( type == "Scale1DDragger" ) d = SetupDraggerProperties( new osgManipulator::Scale1DDragger );
    else if( type == "Scale2DDragger" ) d = SetupDraggerProperties( new osgManipulator::Scale2DDragger );
    else if( type == "ScaleAxisDragger" ) d = SetupDraggerProperties( new osgManipulator::ScaleAxisDragger );
    else if( type == "TabBoxTrackballDragger" ) d = SetupDraggerProperties( new osgManipulator::TabBoxTrackballDragger );
    else if( type == "TabPlaneDragger" ) d = SetupDraggerProperties( new osgManipulator::TabPlaneDragger );
    else if( type == "TrackballDragger" ) d = SetupDraggerProperties( new osgManipulator::TrackballDragger );
    else if( type == "TranslatePlaneDragger" ) d = SetupDraggerProperties( new osgManipulator::TranslatePlaneDragger );
    if( !d ) 
    {
        throw std::logic_error( "Unknown dragger: " + type );
        return 0; //in case exceptions not enabled
    }
    return d.release();
}

/// Traverse scenegraph tree and add matrix transform above geodes.
void InsertTransform( osg::Node* n )
{
    if( !n ) return;
    if( n->asGroup() )
    {
        osg::Group* g = n->asGroup();
        for( int i = 0; i != g->getNumChildren(); ++i )
        {
            if( g->getChild( i )->asGeode() )
            {
                osg::ref_ptr< osg::MatrixTransform > t = new osg::MatrixTransform;
                osg::ref_ptr< osg::Node > c = g->getChild( i );
                t->addChild( osg::get_pointer( c ) );
                g->replaceChild( osg::get_pointer( c ), osg::get_pointer( t ) );
            }
            else InsertTransform( g->getChild( i ) );
        }
    }   
}


//------------------------------------------------------------------------------
/// Keyboard handler for SSAO tracing algorithm parameters.
class DraggerSelectorHandler : public osgGA::GUIEventHandler
{
public:
    DraggerSelectorHandler( osg::Group* parent ) : parent_( parent ) 
    {
        if( parent_->getNumChildren() > 0 )
        {
            previousDragger_ = dynamic_cast< osgManipulator::Dragger* >( parent_->getChild( 0 ) );
        }
        draggers_[ int( '1' ) ] = SetupDraggerProperties( new osgManipulator::TranslateAxisDragger );
        draggers_[ int( '2' ) ] = SetupDraggerProperties( new osgManipulator::TabBoxDragger );
        draggers_[ int( '3' ) ] = SetupDraggerProperties( new osgManipulator::TabPlaneTrackballDragger );
        draggers_[ int( '4' ) ] = SetupDraggerProperties( new osgManipulator::ScaleAxisDragger );
        draggers_[ int( '5' ) ] = SetupDraggerProperties( new osgManipulator::TabPlaneDragger );
        draggers_[ int( '6' ) ] = SetupDraggerProperties( new osgManipulator::TrackballDragger );
        draggers_[ int( '7' ) ] = SetupDraggerProperties( new osgManipulator::TranslatePlaneDragger );

    }
    bool handle( const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&  )
    {
        bool handled = false;
        if( ea.getEventType() == osgGA::GUIEventAdapter::KEYUP )
        {
            if( int( ea.getKey() ) >= int( '0' ) && int( ea.getKey() ) <= int( '9' ) )
            {
                SelectDragger( int( ea.getKey() ) );
                handled = true;
            }
        }
        return handled;            
    }
    //void accept( osgGA::GUIEventHandlerVisitor& v ) { v.visit( *this ); }
private:
    osg::ref_ptr< osg::Group > parent_;
    osg::ref_ptr< osgManipulator::Dragger > previousDragger_;
    std::map< int, osg::ref_ptr< osgManipulator::Dragger > > draggers_;
    static const int DISABLE_DRAGGER_SELECTION_KEY = int( '0' );
    void SelectDragger( int key )
    {
        if( !parent_ ) return;
        if( key == DISABLE_DRAGGER_SELECTION_KEY )
        {
            if( previousDragger_ != 0 )
            {
                parent_->removeChild( osg::get_pointer( previousDragger_ ) );
                // UV XXX this is missing !
                //previousDragger_->clearTransformUpdating();
                previousDragger_ = 0;
            }
        }
        else if( draggers_.find( key ) != draggers_.end() )
        {
            if( !previousDragger_ )
            {
                previousDragger_ = draggers_[ key ];
                parent_->addChild( draggers_[ key ] );
            }
            else 
            {
                parent_->replaceChild( osg::get_pointer( previousDragger_ ),
                                       osg::get_pointer( draggers_[ key ] ) );
                previousDragger_ = draggers_[ key ];
            }
        }
    }
    
};

osgGA::GUIEventHandler* CreateDraggerSelectorHandler( osg::Group* parent )
{
  return new DraggerSelectorHandler( parent );
}
