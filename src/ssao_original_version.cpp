#include <osgViewer/Viewer >
#include <osgViewer/ViewerEventHandlers>
#include <osg/TextureRectangle>
#include <osg/Geometry>
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osgUtil/SmoothingVisitor>
#include <osg/MatrixTransform>
#include <osgManipulator/TabBoxDragger>
#include <osgManipulator/TranslateAxisDragger>

#include <osgDB/ReadFile>

#include <osgGA/TrackballManipulator>

#include <iostream>
#include <cassert>
#include <string>
#include <sstream>
#include <fstream>
#include <set>

static const std::string SHADER_PATH="C:/projects/ssao/trunk/src";
static const int MAX_FBO_WIDTH = 2048;
static const int MAX_FBO_HEIGHT = 2048;


enum ShadingStyle
{ 
    AMBIENT_OCCLUSION_SHADING,
    AMBIENT_OCCLUSION_FLAT_SHADING,
    AMBIENT_OCCLUSION_LAMBERT_SHADING,
    AMBIENT_OCCLUSION_SPHERICAL_HARMONICS_SHADING
};

int texUnit = 2;

//------------------------------------------------------------------------------
osg::TextureRectangle* GenerateDepthTextureRectangle()
{
    osg::ref_ptr< osg::TextureRectangle > tr = new osg::TextureRectangle;
	tr->setInternalFormat( GL_DEPTH_COMPONENT );
  	tr->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );
	tr->setFilter( osg::Texture::MAG_FILTER, osg::Texture::NEAREST );
	tr->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
	tr->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
	return tr.release();
}

//------------------------------------------------------------------------------
osg::TextureRectangle* GenerateColorTextureRectangle()
{
    osg::ref_ptr< osg::TextureRectangle > tr = new osg::TextureRectangle;
    tr->setSourceFormat( GL_RGBA );
    tr->setSourceType( GL_FLOAT );
    tr->setInternalFormat( GL_RGBA32F_ARB );
	tr->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );
	tr->setFilter( osg::Texture::MAG_FILTER, osg::Texture::NEAREST );
	tr->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
	tr->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
	return tr.release();
}

//------------------------------------------------------------------------------
#include "posnormal_mrt_shaders.h"
// Attach depth or positions & normals textures to pre-render camera
osg::Camera* CreatePreRenderCamera( osg::Texture* depth,
                                    osg::Texture* positions = 0,
                                    osg::Texture* normals = 0 )
{
	osg::ref_ptr< osg::Camera > camera = new osg::Camera;
	camera->setReferenceFrame( osg::Transform::ABSOLUTE_RF );
	camera->setRenderTargetImplementation( osg::Camera::FRAME_BUFFER_OBJECT );
	camera->setRenderOrder( osg::Camera::PRE_RENDER, 1 );
	camera->setClearMask( GL_DEPTH_BUFFER_BIT );
	// ATTACH DEPTH TEXTURE TO CAMERA
	if( depth != 0 ) camera->attach( osg::Camera::DEPTH_BUFFER, depth ); 
  
	camera->setViewport( 0, 0, MAX_FBO_WIDTH, MAX_FBO_HEIGHT );
	// ATTACH TEXTURE TO STORE WORLD SPACE POSITIONS AND NORMALS WITH OPTIONAL DEPTH
    // STORED AS W COMPONENT
    if( positions ) camera->attach( osg::Camera::BufferComponent( osg::Camera::COLOR_BUFFER0 ), positions );
    if( normals   ) camera->attach( osg::Camera::BufferComponent( osg::Camera::COLOR_BUFFER0 +1 ), normals );
    if( positions || normals )
    {
        camera->setClearMask( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        // ATTACH SHADERS TO CAMERA
        osg::ref_ptr< osg::StateSet > set = camera->getOrCreateStateSet();
        assert( osg::get_pointer( set ) );
        osg::ref_ptr< osg::Program > program = new osg::Program;
	    program->setName( "Positions and Normals" );
		program->addShader( new osg::Shader( osg::Shader::FRAGMENT, POSNORMALSDEPTH_FRAG_MRT ) );
		program->addShader( new osg::Shader( osg::Shader::VERTEX,   POSNORMALS_VERT_MRT ) );
        set->setAttributeAndModes( program.get(), osg::StateAttribute::ON );
	}
    return camera.release();
}


//------------------------------------------------------------------------------
// Set node mask callback; used to enable/disable nodes in pre/post render callbacks

// sync viewer's camera with pre-render camera
class NodeMaskCBack : public osg::Camera::DrawCallback //osg::NodeCallback
{
public:
	NodeMaskCBack( osg::Node* n, osg::Node::NodeMask nm ) : node_( n ), nodeMask_( nm ) {}	
	
	void operator() ( osg::RenderInfo& /*renderInfo*/ ) const { node_->setNodeMask( nodeMask_ ); }
  
private:
	mutable osg::ref_ptr< osg::Node > node_;
	osg::Node::NodeMask nodeMask_;
};



//------------------------------------------------------------------------------
// Synchronize

// sync viewer's camera with pre-render camera
    class SyncCameraNode : public osg::Camera::DrawCallback //osg::NodeCallback
	{
	public:
		SyncCameraNode( const osg::Camera* observedCamera, osg::Camera* cameraToUpdate, osg::Uniform* vp )
			: observedCamera_( observedCamera ), cameraToUpdate_( cameraToUpdate ), uniform_( vp ), init_( true ) {}
		void operator()( osg::Node* n, osg::NodeVisitor* )
        {
            SyncCameras();
        }
		void operator() ( osg::RenderInfo& /*renderInfo*/ ) const
        {
	        SyncCameras();
		}
        void SyncCameras() const 
        {
            osg::Camera* sc = cameraToUpdate_; //static_cast< osg::Camera* >( n );
			sc->setProjectionMatrix( observedCamera_->getProjectionMatrix() );
			sc->setViewMatrix( observedCamera_->getViewMatrix() );
			// first update: do nothing; RenderStage::draw() method will set up FBO attached
			// to camera viewport to match the size of the attached FBO
			// subsequent updates: set viewport to be the same as main camera viewport
			if( init_ )
			{
				sc->setViewport( 0, 0, MAX_FBO_WIDTH, MAX_FBO_HEIGHT );
				init_ = false;
			}
			else
			{
				//if( sc->getViewport()->width() != mc_->getViewport()->width() ||
				//	sc->getViewport()->height() != mc_->getViewport()->height() )
				{
                    sc->setViewport( const_cast< osg::Camera* >( osg::get_pointer( observedCamera_ ) )->getViewport() );
                	if( uniform_ ) uniform_->set( osg::Vec2( observedCamera_->getViewport()->width(), observedCamera_->getViewport()->height() ) );
				}
			}		
        }
	private:
		osg::ref_ptr< const osg::Camera > observedCamera_;
        osg::ref_ptr< osg::Camera > cameraToUpdate_;
		osg::ref_ptr< osg::Uniform > uniform_;
		mutable int init_;
	};




void SetCameraCallback( osg::Camera* cameraToUpdate, osg::Camera* observedCamera, osg::Uniform* vp )
{
    
	cameraToUpdate->setPreDrawCallback(  new SyncCameraNode( observedCamera, cameraToUpdate, vp ) );
	//cameraToUpdate->setPostDrawCallback( new NodeMaskCBack( dragger, 0xffffffff ) );
    //camera->setPostDrawCallback(  new SyncCameraNode( mc, camera, vp ) );
    //camera->setUpdateCallback( new SyncCameraNode( mc, vp ) );
}

//------------------------------------------------------------------------------
osg::Node* CreateDefaultModel()
{
		// create spheres: this is the scene content that will be rendered into the depth texture
		// attached to the rectangle
		osg::ref_ptr< osg::Geode > ball = new osg::Geode;
		osg::ref_ptr< osg::ShapeDrawable > s1 = new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( -1.f, 1.f, 0.f ), 1.f ) );
		osg::ref_ptr< osg::TessellationHints > ti = new osg::TessellationHints;
		ti->setDetailRatio( 0.2f );
		s1->setTessellationHints( ti.get() );
		ball->addDrawable( s1.get() );
		ball->addDrawable( new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 0.f, 1.f, 1.f ), 1.f ) ) );
		ball->addDrawable( new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 1.f, 1.f, 0.f ), 1.f ) ) );
		ball->addDrawable( new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 0.f, 1.f, -1.f ), 1.f ) ) );
        ball->addDrawable( new osg::ShapeDrawable( new osg::Box( osg::Vec3( 0.f, 2.1f, 0.f ), 6.0f, 0.2f, 6.0f ) ) );
        return static_cast< osg::Node* >( ball.release() );
}

//------------------------------------------------------------------------------
std::string ReadTextFile( const std::string& fname )
{
    std::ifstream in( fname.c_str() );
    if( !in ) return "";
    std::string str,strTotal;
    std::getline( in, str);
    while( in ) {
        strTotal += '\n';
        strTotal += str;
        std::getline( in, str );
    }
    return strTotal;
}
osg::Shader* LoadShaderSource( const std::string& fname,
                               osg::Shader::Type type,
                               const std::string& prefix = "" )
{
    return new osg::Shader( type, prefix + ReadTextFile( fname ) );    
}


//------------------------------------------------------------------------------
// Create string to prefix to shader source to enable disable multiple
// render targets and set shading style
std::string BuildShaderSourcePrefix( bool mrt, ShadingStyle ss, bool enableTextures = false )
{
    std::string ssp;
    if( enableTextures ) ssp += "#define TEXTURE_ENABLED\n";
    if( mrt ) ssp += "#define MRT_ENABLED\n";
    switch( ss )
    {    
    case AMBIENT_OCCLUSION_FLAT_SHADING: ssp += "#define AO_FLAT\n";
                                         break;
    case AMBIENT_OCCLUSION_LAMBERT_SHADING: ssp += "#define AO_LAMBERT\n";
                                         break;
    case AMBIENT_OCCLUSION_SPHERICAL_HARMONICS_SHADING: ssp += "#define AO_SPHERICAL_HARMONICS\n";
                                         break;
    default: break;
    }
    return ssp;
}

//------------------------------------------------------------------------------
// Configure scenegraph for simple (i.e. no automatic radius computation) ambient
// occlusion computation
osg::Camera* SSAOSimple( osgViewer::Viewer& viewer, // viewer into which SSAO scenegraph is added
                 osg::Node* node, // model
                 float halfWindowWidth, // half width of search window in number of steps
                                        // when step == 1 it corresponds to the number of pixels
                 float step, // step size in pixels
                 float occFact, // occlusion multiplier
                 const std::string& vShader, // vertex shader
		         const std::string& fShader, // fragment shader
                 const std::string& viewportUniform, // uniform viewport identifier
                 bool mrt = false, // multiple render targets
                 ShadingStyle shadeStyle = AMBIENT_OCCLUSION_SHADING
                 )

{
    osg::ref_ptr< osg::Node > model = node;
	
	std::string fragShader( fShader );
	std::string vertShader( vShader );

    if( !node ) model = CreateDefaultModel();

    // depth/position/normal textures 
	osg::ref_ptr< osg::TextureRectangle > depth; 
    osg::ref_ptr< osg::TextureRectangle > positions;
    osg::ref_ptr< osg::TextureRectangle > normals;
    // if multiple render targets enabled z component will be available in 
    // w component of positions or normals
    if( mrt )
    {
        positions = GenerateColorTextureRectangle();
        normals   = GenerateColorTextureRectangle();
    }
    else depth = GenerateDepthTextureRectangle();

	// CREATE CAMERA: this is the camera used to pre-render to depth texture
    osg::ref_ptr< osg::Camera > camera = CreatePreRenderCamera( osg::get_pointer( depth ),
                                                            osg::get_pointer( positions ),
                                                            osg::get_pointer( normals ) );

	// ADD STUFF TO VIEWER
	osg::ref_ptr< osg::Group > root = new osg::Group;
	root->addChild( model.get() );
	root->addChild( camera.get() );

	// add a viewport to the viewer and attach the scene graph.
	viewer.setSceneData( root.get() );
 
	// get reference to viewer's main camera 
	osg::Camera* mc = viewer.getCamera();
	if( !mc )
	{
		std::cerr << "NULL CAMERA" << std::endl;
		return 0;
	}

	//  attach shaders to camera
	osg::StateSet* set = mc->getOrCreateStateSet();   	
	osg::ref_ptr< osg::Program > aprogram = new osg::Program;
	aprogram->setName( "SSAO Simple" );
		
    osg::ref_ptr< osg::Uniform > stepSizeUniform = new osg::Uniform( "samplingStep", step );
    osg::ref_ptr< osg::Uniform > numSamplesUniform = new osg::Uniform( "halfSamples", halfWindowWidth );
	fragShader = fragShader.empty() ? SHADER_PATH + "/ssao_in_vertex_shader.frag" : fragShader;
	vertShader = vertShader.empty() ? SHADER_PATH + "/ssao_in_vertex_shader.vert" : vertShader;

    const std::string SHADER_SOURCE_PREFIX( BuildShaderSourcePrefix( mrt, shadeStyle ) );  
    osg::ref_ptr< osg::Shader > vertexShader   = LoadShaderSource( vertShader, osg::Shader::VERTEX, SHADER_SOURCE_PREFIX  );
    osg::ref_ptr< osg::Shader > fragmentShader = LoadShaderSource( fragShader, osg::Shader::FRAGMENT, SHADER_SOURCE_PREFIX );
    aprogram->addShader( osg::get_pointer( vertexShader ) );
	aprogram->addShader( osg::get_pointer( fragmentShader ) );
	set->setAttributeAndModes( aprogram.get(), osg::StateAttribute::ON );
    if( depth )
    {
        set->setTextureAttributeAndModes( texUnit, osg::get_pointer( depth ) );
        set->addUniform( new osg::Uniform( "depthMap", texUnit ) );
    }
    if( positions )
    {
        set->setTextureAttributeAndModes( texUnit + 1, osg::get_pointer( positions ) );
        set->addUniform( new osg::Uniform( "positions", texUnit + 1 ) );
    }
    if( normals )
    {
        set->setTextureAttributeAndModes( texUnit + 2, osg::get_pointer( normals ) );
        set->addUniform( new osg::Uniform( "normals", texUnit + 2 ) );
    }
	
    set->addUniform( osg::get_pointer( stepSizeUniform ) );
    set->addUniform( osg::get_pointer( numSamplesUniform ) );

    osg::ref_ptr< osg::Uniform > ssaoUniform  = new osg::Uniform( "ssao", 1 );
    osg::ref_ptr< osg::Uniform > occFactorUniform = new osg::Uniform( "occlusionFactor", occFact );
    set->addUniform( osg::get_pointer( ssaoUniform ) );
	set->addUniform( new osg::Uniform( "shade", 1 ) );
	set->addUniform( occFactorUniform );
	
	// viewport uniform
	osg::ref_ptr< osg::Uniform > vp = new osg::Uniform( viewportUniform.c_str(), osg::Vec2( MAX_FBO_WIDTH, MAX_FBO_HEIGHT ) );
    set->addUniform( osg::get_pointer( vp ) );

	// sync depth camera with main camera
    SetCameraCallback( camera.get(), mc, osg::get_pointer( vp ) );
	
	// add nodes to be rendered to depth texture
	camera->addChild( model.get() );

    class SSAOKbEventHandler : public osgGA::GUIEventHandler
    {
    public:
        SSAOKbEventHandler( osg::Uniform* ssaoU = 0,
                            osg::Uniform* samplesU = 0,
                            osg::Uniform* stepSizeU = 0,
                            osg::Uniform* occFactorU = 0 ) :
                                                    ssaoUniform_( ssaoU ),
                                                    numSamplesUniform_( samplesU ),
                                                    stepSizeUniform_( stepSizeU ),
                                                    occFactorUniform_( occFactorU ),
                                                    enabled_( true ), numSamples_( 3 ), stepSize_( 2 ), occFactor_( 7.0f )
        {
            if( ssaoU      ) ssaoU->get( enabled_    );
            if( samplesU   ) samplesU->get( numSamples_ );
            if( stepSizeU  ) stepSizeU->get( stepSize_ );
            if( occFactorU ) occFactorU->get( occFactor_ ); 
            std::clog << std::boolalpha << "SSAO: " << enabled_ 
                << "  # samples: " << numSamples_ << "  step size: " << stepSize_ << " occlusion factor:  " << occFactor_ << " ";
        }
        bool handle( const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&  )
        {
            bool handled = false;
            switch( ea.getEventType() )
            {
            case osgGA::GUIEventAdapter::KEYDOWN:
                switch( ea.getKey() )
                {
                case 'a': 
                case 'A': 
                    enabled_ = !enabled_;
                    ToggleSSAO();
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Up:
                    UpdateNumSamples( 1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Down:
                    UpdateNumSamples( -1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Left:
                    UpdateStepSize( -1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Right:
                    UpdateStepSize( 1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Page_Up:
                    UpdateOccFactor( 1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Page_Down:
                    UpdateOccFactor( -1 );
                    handled = true;
                    break;

                default:
                    break;
                }
                break;
            default:
                break;
            }
            if( handled ) 
            {
                std::clog << std::boolalpha <<  "SSAO: " << enabled_ << "  # samples: " << numSamples_ << "  step size: " << stepSize_ << '\r';
            }
            return handled;            
        }
        void accept( osgGA::GUIEventHandlerVisitor& v ) { v.visit( *this ); }
    private:
        
        void ToggleSSAO()
        {
            if( !ssaoUniform_ ) return;
            ssaoUniform_->set( enabled_ ? 1 : 0 );
        }
        
        void UpdateNumSamples( int inc )
        {
            if( !numSamplesUniform_ ) return;
            numSamples_ = std::max( 1.f , numSamples_ + inc );
            numSamplesUniform_->set( numSamples_ );
        }
        void UpdateStepSize( int inc )
        {
            if( !stepSizeUniform_ ) return;
            stepSize_ = std::max( 1.f, stepSize_ + inc );
            stepSizeUniform_->set( stepSize_ );
        }
        void UpdateOccFactor( int inc )
        {
            if( !occFactorUniform_ ) return;
            occFactor_ = std::max( 0.f, occFactor_ + inc );
            occFactorUniform_->set( occFactor_ );
        }

        osg::ref_ptr< osg::Uniform > ssaoUniform_;
        osg::ref_ptr< osg::Uniform > numSamplesUniform_;
        osg::ref_ptr< osg::Uniform > stepSizeUniform_;
        osg::ref_ptr< osg::Uniform > occFactorUniform_;
        bool enabled_;
        float numSamples_;
        float stepSize_;
        float occFactor_;
    };
    viewer.addEventHandler( new SSAOKbEventHandler( 
        osg::get_pointer( ssaoUniform ),
        osg::get_pointer( numSamplesUniform ),
        osg::get_pointer( stepSizeUniform ),
        osg::get_pointer( occFactorUniform ) ) );
    return camera.release();	
}

#include "manipulator.h"
//------------------------------------------------------------------------------
// Configure scenegraph for advanced (i.e. automatic radius computation) ambient
// occlusion computation
osg::Camera* SSAOTrace( osgViewer::Viewer& viewer, 
           osg::Node* node,
           float dRadius, // Fraction of object radius in world coordinates
                          // used to determine the area used for AO computation
           float stepMul, // Step multiplier: applied to the step in screen coordinates (i.e. pixels)
           float samplingDirs, // Number of sampling directions
           float maxFilterPixelRadius, // Max size of radius in pixels
           /* float maxNumberOfRaysPerDirection,*/ // max number of AO rays per direction
           float occFact, // occlusion multiplier
           const std::string& vShader, // vertex shader
		   const std::string& fShader, // fragment shader
           const std::string& viewportUniform, // uniform viewport identifier
           const std::string& ssaoRadiusUniform, // ssao radius identifier
           bool mrt = false, // multiple render target: world vertex positions and normals computed in pre-render step
           ShadingStyle shadeStyle = AMBIENT_OCCLUSION_SHADING,
           bool enableTextures = false
           )
{
//#ifdef MANIPULATOR
	osg::ref_ptr< osg::MatrixTransform > model = new osg::MatrixTransform;
	
	
	std::string fragShader( fShader );
	std::string vertShader( vShader );

	if( !node ) model->addChild( CreateDefaultModel() );
	else model->addChild( node );

    // depth, positions, normals & depth texture 
	osg::ref_ptr< osg::TextureRectangle > depth;
    osg::ref_ptr< osg::TextureRectangle > positions;
    osg::ref_ptr< osg::TextureRectangle > normals;
    // if multiple render targets enabled z component will be available in 
    // w component of positions or normals
    if( mrt )
    {
        positions = GenerateColorTextureRectangle();
        normals   = GenerateColorTextureRectangle();
    }
    else depth = GenerateDepthTextureRectangle();

	
	// CREATE CAMERA: this is the camera used to pre-render to depth texture
	 osg::ref_ptr< osg::Camera > camera = CreatePreRenderCamera( osg::get_pointer( depth ),
                                                             osg::get_pointer( positions ),
                                                             osg::get_pointer( normals ) );
	// ADD STUFF TO VIEWER
    osg::ref_ptr< osg::Group > root = new osg::Group;
#ifdef MANIPULATOR
    typedef osgManipulator::TranslateAxisDragger //  TabBoxDragger();
        Dragger;
    osg::ref_ptr< Dragger > dragger = new Dragger;
    dragger->setupDefaultGeometry();
	float scale = model->getBound().radius();// * .0001;
	dragger->setMatrix(osg::Matrix::scale(scale, scale, scale) *
                       osg::Matrix::translate(model->getBound().center()));
	dragger->addTransformUpdating(osg::get_pointer(model));
	//dragger->getOrCreateStateSet()->setMode( GL_NORMALIZE, osg::StateAttribute::ON );
	{
		osg::ref_ptr< osg::Program > program = new osg::Program;
	    program->setName( "Passthrough" );
		program->addShader( new osg::Shader( osg::Shader::FRAGMENT, PASSTHROUGH_FRAG ) );
		program->addShader( new osg::Shader( osg::Shader::VERTEX,   PASSTHROUGH_VERT ) );
		dragger->getOrCreateStateSet()->setAttributeAndModes( program.get(), osg::StateAttribute::ON );
	}

    // we want the dragger to handle it's own events automatically
    dragger->setHandleEvents(true);

    // if we don't set an activation key or mod mask then any mouse click on
    // the dragger will activate it, however if do define either of ActivationModKeyMask or
    // and ActivationKeyEvent then you'll have to press either than mod key or the specified key to
    // be able to activate the dragger when you mouse click on it.  Please note the follow allows
    // activation if either the ctrl key or the 'a' key is pressed and held down.
    dragger->setActivationModKeyMask(osgGA::GUIEventAdapter::MODKEY_CTRL);
    dragger->setActivationKeyEvent('d');

    // put manipulator geometry under group and assign 'disard' shader to hide it
    // from pre-render pass
    osg::ref_ptr< osg::Group > dg = new osg::Group;
    {
		osg::ref_ptr< osg::Program > program = new osg::Program;
	    program->setName( "Discard" );
		program->addShader( new osg::Shader( osg::Shader::FRAGMENT, DISCARD_FRAG ) );
		program->addShader( new osg::Shader( osg::Shader::VERTEX,   PASSTHROUGH_VERT ) );
		dg->getOrCreateStateSet()->setAttributeAndModes( program.get(), osg::StateAttribute::OVERRIDE );
	}
    dg->addChild( dragger->asGroup() );
    camera->addChild( dg.get() );
	root->addChild( camera.get() );
	root->addChild( model );
    root->addChild( dragger.get() );
#else
	root->addChild( camera.get() );
	root->addChild( model.get() );
	
#endif
	// add a viewport to the viewer and attach the scene graph.
	viewer.setSceneData( root.get() );
 
	// get reference to viewer's main camera 
	osg::Camera* mc = viewer.getCamera();
	if( !mc )
	{
		std::cerr << "NULL CAMERA" << std::endl;
		return 0;
	}

	// create depth texture state set and shaders and attach to camera
	osg::StateSet* set = mc->getOrCreateStateSet();   	
	osg::ref_ptr< osg::Program > aprogram = new osg::Program;
	aprogram->setName( "SSAO" );

    osg::ref_ptr< osg::Uniform > dRadiusUniform = new osg::Uniform( "dhwidth", dRadius );
    osg::ref_ptr< osg::Uniform > stepMulUniform = new osg::Uniform( "dstep", stepMul );
    osg::ref_ptr< osg::Uniform > maxRadiusPixelsUniform = new osg::Uniform( "hwMax", maxFilterPixelRadius );
    osg::ref_ptr< osg::Uniform > numSamplesUniform = new osg::Uniform( "numSamples", samplingDirs );
    fragShader = fragShader.empty() ? SHADER_PATH + "/ssao2.frag" : fragShader;
	vertShader = vertShader.empty() ? SHADER_PATH + "/ssao2.vert" : vertShader;

	const std::string SHADER_SOURCE_PREFIX( BuildShaderSourcePrefix( mrt, shadeStyle, enableTextures ) );
    osg::ref_ptr< osg::Shader > vertexShader   = LoadShaderSource( vertShader, osg::Shader::VERTEX, SHADER_SOURCE_PREFIX  );
    osg::ref_ptr< osg::Shader > fragmentShader = LoadShaderSource( fragShader, osg::Shader::FRAGMENT, SHADER_SOURCE_PREFIX );
  
    aprogram->addShader( osg::get_pointer( vertexShader ) );
	aprogram->addShader( osg::get_pointer( fragmentShader ) );
	set->setAttributeAndModes( aprogram.get(), osg::StateAttribute::ON );
    if( depth )
    {
        set->setTextureAttributeAndModes( texUnit, osg::get_pointer( depth ) );
        set->addUniform( new osg::Uniform( "depthMap", texUnit ) );
    }
    if( positions )
    {
        set->setTextureAttributeAndModes( texUnit + 1, osg::get_pointer( positions ) );
        set->addUniform( new osg::Uniform( "positions", texUnit + 1 ) );
    }
    if( normals )
    {
        set->setTextureAttributeAndModes( texUnit + 2, osg::get_pointer( normals ) );
        set->addUniform( new osg::Uniform( "normals", texUnit + 2 ) );
    }
	
	set->addUniform( new osg::Uniform( ssaoRadiusUniform.c_str(), model->getBound().radius() ) );
    set->addUniform( osg::get_pointer( dRadiusUniform ) );
    set->addUniform( osg::get_pointer( stepMulUniform ) );
    set->addUniform( osg::get_pointer( maxRadiusPixelsUniform ) );
    set->addUniform( osg::get_pointer( numSamplesUniform ) );

    osg::ref_ptr< osg::Uniform > ssaoUniform  = new osg::Uniform( "ssao", 1 );
    osg::ref_ptr< osg::Uniform > occFactorUniform = new osg::Uniform( "occlusionFactor", occFact );
    set->addUniform( osg::get_pointer( ssaoUniform ) );
	set->addUniform( new osg::Uniform( "shade", 1 ) );
	set->addUniform( occFactorUniform );
	
	// viewport uniform
	osg::ref_ptr< osg::Uniform > vp = new osg::Uniform( viewportUniform.c_str(),
                                                        osg::Vec2( MAX_FBO_WIDTH, MAX_FBO_HEIGHT ) );
    set->addUniform( osg::get_pointer( vp ) );

	// sync depth camera with main camera
    SetCameraCallback( camera.get(), mc, osg::get_pointer( vp ) );
	
	// disable shading and ssao for depth camera
	set = camera->getOrCreateStateSet();
	set->addUniform( new osg::Uniform( "ssao", 0 ) );
	set->addUniform( new osg::Uniform( "shade", 0 ) );

	// add nodes to be rendered to depth texture
	camera->addChild( model.get() );

    class SSAOKbEventHandler : public osgGA::GUIEventHandler
    {
    public:
        SSAOKbEventHandler( osg::Uniform* ssaoU = 0,
                            osg::Uniform* dRadiusU = 0,
                            osg::Uniform* stepMulU = 0,
                            osg::Uniform* maxRadiusPixelsU = 0,
                            osg::Uniform* occFactorU = 0,
                            osg::Uniform* samplingDirsU = 0
                            ) :
                                                    ssaoUniform_( ssaoU ),
                                                    dRadiusUniform_( dRadiusU ),
                                                    stepMulUniform_( stepMulU ),
                                                    maxRadPixelsUniform_( maxRadiusPixelsU ),
                                                    occFactorUniform_( occFactorU ),
                                                    samplingDirsUniform_( samplingDirsU ),
                                                    enabled_( true ), dRadius_( 0.1f ), stepMul_( 1 ),
                                                    maxRadiusPixels_( 32 ), occFactor_( 7.0f ),
                                                    samplingDirs_( 8 )
        {
            if( ssaoU      ) ssaoU->get( enabled_    );
            if( dRadiusU   ) dRadiusU->get( dRadius_ );
            if( stepMulU  ) stepMulU->get( stepMul_ );
            if( maxRadiusPixelsU ) maxRadiusPixelsU->get( maxRadiusPixels_ );
            if( occFactorU ) occFactorU->get( occFactor_ );
            if( samplingDirsU ) samplingDirsU->get( samplingDirs_ ); 
            std::clog << std::boolalpha << "SSAO: " << enabled_ 
                << "  # %radius: " << dRadius_ * 100.0f << "  step mult: " << stepMul_ << 
                "  max radius: " << maxRadiusPixels_ << "px  " << "# sampling dirs:  " << samplingDirs_ << '\n';
        }
        bool handle( const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter&  )
        {
            bool handled = false;
            switch( ea.getEventType() )
            {
            case osgGA::GUIEventAdapter::KEYDOWN:
                switch( ea.getKey() )
                {
                case 'a': 
                case 'A': 
                    enabled_ = !enabled_;
                    ToggleSSAO();
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Up:
                    UpdatedRadius( 0.01f );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Down:
                    UpdatedRadius( -0.01f );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Left:
                    UpdateStepMul( -1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Right:
                    UpdateStepMul( 1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Page_Up:
                    UpdateOccFactor( 0.1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Page_Down:
                    UpdateOccFactor( -0.1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_Home:
                    UpdateMaxPixelRadius( 1 );
                    handled = true;
                    break;
                case osgGA::GUIEventAdapter::KEY_End:
                    UpdateMaxPixelRadius( -1 );
                    handled = true;
                    break;
                case '<':
                case ',':
                    UpdateSamplingDirs( -1 );
                    handled = true;
                    break;
                case '>':
                case '.':
                    UpdateSamplingDirs( 1 );
                    handled = true;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
            if( handled ) 
            {
                std::clog << std::boolalpha <<  "SSAO: " << enabled_ 
                << "  # %radius: " << dRadius_ * 100.0f << "  step mult: " << stepMul_ << 
                "  max radius: " << maxRadiusPixels_ << "px  " << "# sampling dirs:  " << samplingDirs_ << '\n';
            }
            return handled;            
        }
        void accept( osgGA::GUIEventHandlerVisitor& v ) { v.visit( *this ); }
    private:
        
        void ToggleSSAO()
        {
            if( !ssaoUniform_ ) return;
            ssaoUniform_->set( enabled_ ? 1 : 0 );
        }
        
        void UpdatedRadius( float inc )
        {
            if( !dRadiusUniform_ ) return;
            dRadius_ = std::max( 0.001f , dRadius_ + inc );
            dRadiusUniform_->set( dRadius_ );
        }
        void UpdateStepMul( int inc )
        {
            if( !stepMulUniform_ ) return;
            stepMul_ = std::max( 1.f, stepMul_ + inc );
            stepMulUniform_->set( stepMul_ );
        }
        void UpdateOccFactor( float inc )
        {
            if( !occFactorUniform_ ) return;
            occFactor_ = std::max( 0.f, occFactor_ + inc );
            occFactorUniform_->set( occFactor_ );
        }
        void UpdateSamplingDirs( int inc )
        {
            if( !samplingDirsUniform_ ) return;
            samplingDirs_ = std::max( 1.f, samplingDirs_ + inc );
            samplingDirsUniform_->set( samplingDirs_ );
        }
        void UpdateMaxPixelRadius( int inc )
        {
            if( !maxRadPixelsUniform_ ) return;
            maxRadiusPixels_ = std::max( 1.f, maxRadiusPixels_ + inc );
            maxRadPixelsUniform_->set( maxRadiusPixels_ );
        }

        osg::ref_ptr< osg::Uniform > ssaoUniform_;
        osg::ref_ptr< osg::Uniform > dRadiusUniform_;
        osg::ref_ptr< osg::Uniform > stepMulUniform_;
        osg::ref_ptr< osg::Uniform > maxRadPixelsUniform_;
        osg::ref_ptr< osg::Uniform > occFactorUniform_;
        osg::ref_ptr< osg::Uniform > samplingDirsUniform_;

        bool enabled_;
        float dRadius_;
        float stepMul_;
        float maxRadiusPixels_;
        float occFactor_;
        float samplingDirs_;
    };
    viewer.addEventHandler( new SSAOKbEventHandler( 
        osg::get_pointer( ssaoUniform ),
        osg::get_pointer( dRadiusUniform ),
        osg::get_pointer( stepMulUniform ),
        osg::get_pointer( maxRadiusPixelsUniform ),
        osg::get_pointer( occFactorUniform ),
        osg::get_pointer( numSamplesUniform ) ) );

    return camera.release();
}

/// Iterates through nodes and adds texture uniforms.
class TextureToUniformVisitor : public osg::NodeVisitor
    {
    public:
        template < class FwdIt >
        TextureToUniformVisitor( const std::string& tun,
                                 const std::string& tn, FwdIt b, FwdIt e  )
            : osg::NodeVisitor( osg::NodeVisitor::TRAVERSE_ALL_CHILDREN ),
            textureUnitName_( tun ), textureName_( tn ), excludedTexUnits_( b, e )
    {}

    void Apply(osg::Drawable* drawable)
    {
        if( !drawable ) return;
        osg::Geometry* geometry = drawable->asGeometry();
        osg::StateSet* ss = drawable->getStateSet();
        if( ss == 0  ) return;
        SetUniform( *ss );
    }

    virtual void apply(osg::Geode& node)
    {
        osg::StateSet* ss = node.getStateSet();
        if( ss != 0 ) SetUniform( *ss );
        for( unsigned int i = 0; i != node.getNumDrawables(); ++i )
        {
            Apply( node.getDrawable( i ) );
        }
        traverse( node );
    }
    private:
        std::string textureUnitName_;
        std::string textureName_;
        typedef std::set< int > TexUnits;
        TexUnits excludedTexUnits_;
        void SetUniform( osg::StateSet& ss )
        {
            int textureUnit = -1;
            
            // iterate over texture unit: the first unit NOT in the exclusion
            // list is assumed to be the one to use
            for( int i = 0; i != 7; ++i )
            {
                if( ss.getTextureAttribute( i, osg::StateAttribute::TEXTURE ) != 0 &&
                    excludedTexUnits_.find( i ) == excludedTexUnits_.end() )
                {
                    textureUnit = i; std::clog << i << ' ';
                    break;
                }
            }
            if( textureUnit >= 0 ) ss.addUniform( new osg::Uniform( textureName_.c_str(), textureUnit ) );
            ss.addUniform( new osg::Uniform( textureUnitName_.c_str(), textureUnit  ) );
        }
    };


/// Adds uniform to map textures in statesets, it is possible to specify an exclusion list
/// to only allow non-excluded texture units to be taken into account; this is needed
/// in cases where a number of texture units is used for RTT tasks.
template < class FwdIt >
void TextureToUniform( osg::Node& root, 
                       const std::string& textureUnitName,
                       const std::string& textureName,
                       FwdIt texUnitExcludedBegin,
                       FwdIt texUnitExcludedEnd )
{
     
    TextureToUniformVisitor ttuv( textureUnitName, textureName, 
                                  texUnitExcludedBegin, texUnitExcludedEnd );
    root.accept( ttuv );

}

// Renders subgraph into depth texture rectangle then renders textured quad
// using data from depth texture for shading.
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
	
	try
	{
		// use an ArgumentParser object to manage the program arguments.
		osg::ArgumentParser arguments(&argc,argv);

		// construct the viewer.
		osgViewer::Viewer viewer;
		// add the stats handler
		viewer.addEventHandler(new osgViewer::StatsHandler);

		
		// Add OB extensions
		osgDB::Registry* r = osgDB::Registry::instance();
		assert( r );
		r->addFileExtensionAlias( "pdb",  "molecule" );
		r->addFileExtensionAlias( "ent",  "molecule" );
		r->addFileExtensionAlias( "hin",  "molecule" );
		r->addFileExtensionAlias( "xyz",  "molecule" );
		r->addFileExtensionAlias( "mol",  "molecule" );
		r->addFileExtensionAlias( "mol2", "molecule" );
		r->addFileExtensionAlias( "cube", "molecule" );
		r->addFileExtensionAlias( "g03",  "molecule" );
		r->addFileExtensionAlias( "g98",  "molecule" );
		r->addFileExtensionAlias( "gam",  "molecule" );
		r->addFileExtensionAlias( "coor", "molecule" );
		r->addFileExtensionAlias( "ref",  "molecule" );

		arguments.getApplicationUsage()->addCommandLineOption( "--options", "Pass options to loader(s)" );
		std::string options;
        arguments.read( "--options", options );

        //simple options
        arguments.getApplicationUsage()->addCommandLineOption( "-s",        "[simple] Simple, no automatic computation of kernel width" );
        arguments.getApplicationUsage()->addCommandLineOption( "-hw",       "[simple] Half window width in # of steps" );
        arguments.getApplicationUsage()->addCommandLineOption( "-step",     "[simple] Step size in pixels" );
        arguments.getApplicationUsage()->addCommandLineOption( "-occFact",  "[all] Occlusion factor" );
        arguments.getApplicationUsage()->addCommandLineOption( "-texUnit",  "[all] Depth buffer Texture Unit" );
        arguments.getApplicationUsage()->addCommandLineOption( "-viewportUniform",  "[all] Name of uniform used for specifying the viewport" );
        arguments.getApplicationUsage()->addCommandLineOption( "-ssaoRadiusUniform",  "[all] Name of uniform used for specifying the ssao radius" );
        arguments.getApplicationUsage()->addCommandLineOption( "-vert",  "[all] Vertex shader file" );
		arguments.getApplicationUsage()->addCommandLineOption( "-frag",  "[all] Fragment shader file" );
		arguments.getApplicationUsage()->addCommandLineOption( "-stepMul", "[advanced] Pixel step multiplier" );
		arguments.getApplicationUsage()->addCommandLineOption( "-maxRadius", "[advanced] Max radius size in steps" );
        arguments.getApplicationUsage()->addCommandLineOption( "-dRadius", "[advanced] fraction of object radius used as max length of rays" );
		//arguments.getApplicationUsage()->addCommandLineOption( "-steps", "Max number of marching steps per ray" );
		arguments.getApplicationUsage()->addCommandLineOption( "-maxNumSamples",  "[advanced] Maximum number of rays" );
        arguments.getApplicationUsage()->addCommandLineOption( "-normals",  "[all] Compute normals" );
        arguments.getApplicationUsage()->addCommandLineOption( "-mrt",  "[all] Multiple render targets: save depth, position and normals in pre-rendering step" );
        arguments.getApplicationUsage()->addCommandLineOption( "-shade", 
                                                               "[all] Shading style: 'ao' ambient occlusion only\n"
                                                               "                     'ao_flat' ambient occlusion with flat shading\n"
                                                               "                     'ao_lambert' ambient occlusion with lambert shading" );
        arguments.getApplicationUsage()->addCommandLineOption( "-textures",  "[advanced] enable textures" );

        //command line parameter value
        std::string cmdParStr; 
        // defaults for command line parameters
        bool enableTextures = false;
        bool simple = false;
        float hw = 16.0f;
        float step = 1.0f;
        float occFact = 1.0f;
        int texUnit = 2;
        std::string viewportUniform( "viewport" );
        std::string ssaoRadiusUniform( "radius" );
        std::string vertShader;
        std::string fragShader;
        float stepMul = 1.0f;
        float maxRadius = 32.0f;
        float dRadius = 0.1f;
        int maxNumSamples = 8;
        bool mrt = false;
        ShadingStyle shadeStyle = AMBIENT_OCCLUSION_SHADING;
       
		// read command line parameters
        simple =  arguments.read( "-s" );
		if( arguments.read( "-hw",  cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> hw;
        }
		if( arguments.read( "-step", cmdParStr  ) )
        {
            std::istringstream is( cmdParStr );
			is >> step;
        }
        if( arguments.read( "-occFact", cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> occFact;
        }
        if( arguments.read( "-texUnit", cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> texUnit;
        }
        arguments.read( "-viewportUniform", viewportUniform );
        arguments.read( "-ssaoRadiusUniform", ssaoRadiusUniform );
        arguments.read( "-vert", vertShader );
        arguments.read( "-frag", fragShader );
        if( arguments.read( "-stepMul", cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> stepMul;
        }
        if( arguments.read( "-maxRadius", cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> maxRadius;
        }
        if( arguments.read( "-dRadius", cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> dRadius;
        }
        if( arguments.read( "-maxNumSamples",  cmdParStr ) )
        {
            std::istringstream is( cmdParStr );
			is >> maxNumSamples;
        }
        if( arguments.read( "-shade", cmdParStr ) )
        {
            if( cmdParStr == "ao" )
            {
                shadeStyle = AMBIENT_OCCLUSION_SHADING;
            }
            else if( cmdParStr == "ao_flat" )
            {
                shadeStyle = AMBIENT_OCCLUSION_FLAT_SHADING;
            }
            else if( cmdParStr == "ao_lambert" )
            {
                shadeStyle = AMBIENT_OCCLUSION_LAMBERT_SHADING;
            }
            else if( cmdParStr == "ao_sph_harm" )
            {
                shadeStyle = AMBIENT_OCCLUSION_SPHERICAL_HARMONICS_SHADING;
            }
            else throw std::runtime_error( "Invalid shading model: " + cmdParStr );
        }
        mrt = arguments.read( "-mrt" );  
		// load model		
		osg::ref_ptr<osg::Node> loadedModel = osgDB::readNodeFiles( arguments, new osgDB::ReaderWriter::Options( options ) );
		if( loadedModel != 0 && arguments.read( "-normals" ) )
		{
			osgUtil::SmoothingVisitor tsv;
			loadedModel->accept( tsv );
		}
        enableTextures = arguments.read( "-textures" );
        if( loadedModel != 0 )
        {
            if( enableTextures )
            {
                // create reserved texture unit list
                std::vector< int > tu( 3 );
                tu[ 0 ] = texUnit;
                tu[ 1 ] = tu[ 0 ] + 1;
                tu[ 2 ] = tu[ 1 ] + 1;
                // make textures in scenegraph accessible from shaders
                TextureToUniform( *loadedModel, "textureUnit", "tex", tu.begin(), tu.end() );
            }
            else
            {
                void RemoveTextures( osg::Node& );
                RemoveTextures( *loadedModel );
            }
        }
        osg::ref_ptr< osg::Camera > preRenderCamera;
        if( simple )
        {
            preRenderCamera = SSAOSimple( viewer,
                                          osg::get_pointer( loadedModel ),
                                          hw, step, occFact, vertShader, fragShader, viewportUniform, mrt, shadeStyle );
        }
        else
        {
            preRenderCamera = SSAOTrace( viewer,
                                         osg::get_pointer( loadedModel ),
                                         dRadius, stepMul, maxNumSamples, maxRadius,
                                         occFact, vertShader, fragShader, viewportUniform, ssaoRadiusUniform, mrt, shadeStyle, enableTextures );
        }
        if( !enableTextures ) viewer.getSceneData()->getOrCreateStateSet()->addUniform( new osg::Uniform( "textureUnit", -1 ) );
        // since the pre render camera needs to be synchronized with the main camera
        // we need to perform the synchronization before the actual rendering takes place
        //return viewer.run();
        viewer.setThreadingModel( osgViewer::Viewer::SingleThreaded );     
		viewer.setCameraManipulator(new osgGA::TrackballManipulator());
        viewer.setReleaseContextAtEndOfFrameHint( false );
        viewer.realize();
        SyncCameraNode sn( viewer.getCamera(), osg::get_pointer( preRenderCamera ), 0 );

        while( !viewer.done() ) 
        {
            viewer.advance();
            viewer.eventTraversal();
            viewer.updateTraversal();
            sn.SyncCameras();
            viewer.renderingTraversals();
        }   
        return 0;
	}
	catch( const std::exception& e )
	{
		std::cerr << e.what() << std::endl;
	}
	return 1;
    
}


class StripTexturesVisitor : public osg::NodeVisitor 
{ 
public: 

   StripTexturesVisitor() 
      : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) 
   {} 

   void Strip(osg::Drawable* drawable) 
   { 
      osg::Geometry* geometry = drawable->asGeometry(); 

      osg::StateSet* ss = drawable->getStateSet(); 
      if(ss != NULL) 
      { 
         for(unsigned int i = 0; i < 2; ++i) 
         { 
            ss->removeTextureAttribute(i, osg::StateAttribute::TEXTURE); 
            if(geometry != NULL) 
            { 
               geometry->setTexCoordArray(i, NULL);          
            } 
         }        
         ss->releaseGLObjects(); 
      } 
   } 

   virtual void apply(osg::Geode& node) 
   { 
      for(unsigned int i = 0; i < node.getNumDrawables(); ++i) 
      { 
         Strip(node.getDrawable(i)); 
      } 
      
      traverse(node); 

   } 
}; 

void RemoveTextures( osg::Node& n )
{
    StripTexturesVisitor stv;
    n.accept( stv );
}