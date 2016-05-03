#include <osgViewer/Viewer >
#include <osgViewer/ViewerEventHandlers>
#include <osg/TextureRectangle>
#include <osg/Geometry>
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osgUtil/SmoothingVisitor>

#include <osgDB/ReadFile>

#include <osgGA/TrackballManipulator>

#include <iostream>
#include <cassert>
#include <string>
#include <sstream>

static const std::string SHADER_PATH="C:/projects/ssao/trunk/src";
static const int MAX_FBO_WIDTH = 2048;
static const int MAX_FBO_HEIGHT = 2048;


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
	tr->setInternalFormat( GL_RGBA );
	tr->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );
	tr->setFilter( osg::Texture::MAG_FILTER, osg::Texture::NEAREST );
	tr->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
	tr->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
	return tr.release();
}

//------------------------------------------------------------------------------
osg::Camera* CreateDepthCamera( osg::Texture* tr )
{
	assert( tr != 0 );
	osg::ref_ptr< osg::Camera > camera = new osg::Camera;
	camera->setReferenceFrame( osg::Transform::ABSOLUTE_RF );
	camera->setRenderTargetImplementation( osg::Camera::FRAME_BUFFER_OBJECT );
	camera->setRenderOrder( osg::Camera::PRE_RENDER, 1 );
	camera->setClearMask( GL_DEPTH_BUFFER_BIT );
	// ATTACH TEXTURE TO CAMERA
	camera->attach( osg::Camera::DEPTH_BUFFER, tr );
	camera->setViewport( 0, 0, MAX_FBO_WIDTH, MAX_FBO_HEIGHT );
	return camera.release();
}

//------------------------------------------------------------------------------
void SetCameraCallback( osg::Camera* camera, osg::Camera* mc, osg::Uniform* vp )
{
	assert( camera );
	assert( mc );
	// sync viewer's camera with pre-render camera
	class SyncCameraNode : public osg::NodeCallback
	{
	public:
		SyncCameraNode( const osg::Camera* mc, osg::Uniform* vp )
			: mc_( mc ), uniform_( vp ), init_( true ) {}
		void operator()( osg::Node* n, osg::NodeVisitor* )
		{
			osg::Camera* sc = static_cast< osg::Camera* >( n );
			sc->setProjectionMatrix( mc_->getProjectionMatrix() );
			sc->setClearColor( mc_->getClearColor() );
			sc->setClearMask( mc_->getClearMask() );
			sc->setColorMask(  const_cast< osg::ColorMask* >( mc_->getColorMask() ) );
			sc->setViewMatrix( mc_->getViewMatrix() );
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
					sc->setViewport( const_cast< osg::Camera* >( mc_.get() )->getViewport() );
					uniform_->set( osg::Vec2( sc->getViewport()->width(), sc->getViewport()->height() ) );
				}
			}			
		}
	private:
		osg::ref_ptr< const osg::Camera > mc_;
		osg::ref_ptr< osg::Uniform > uniform_;
		int init_;
	};

	camera->setUpdateCallback( new SyncCameraNode( mc, vp ) );
}


//------------------------------------------------------------------------------
void SSAO( osgViewer::Viewer& viewer, osg::Node* node,
		   float dhwidth,
		   float dstep,
		   float sm,
		   float hwm,
		   float mins,
		   float minhw,
		   bool simple,
		   const std::string& vShader,
		   const std::string& fShader,
		   float steps,
		   float numSamples,
           float occFact,
           const std::string& viewportUniform )
{ 
	osg::ref_ptr< osg::Node > model = node;
	
	std::string fragShader( fShader );
	std::string vertShader( vShader );

	if( !node )
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
		model = osg::static_pointer_cast< osg::Node >( ball );
	}
	
	// create depth texture 
	osg::ref_ptr< osg::TextureRectangle > tr = GenerateDepthTextureRectangle();
	
	// CREATE CAMERA: this is the camera used to pre-render to depth texture
	osg::ref_ptr< osg::Camera > camera = CreateDepthCamera( tr.get() );

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
		return;
	}

	// create depth texture state set and shaders and attach to camera
	osg::StateSet* set = mc->getOrCreateStateSet();   	
	osg::ref_ptr< osg::Program > aprogram = new osg::Program;
	aprogram->setName( "SSAO" );

		
    osg::ref_ptr< osg::Uniform > stepSizeUniform = new osg::Uniform( "samplingStep", dstep );
    osg::ref_ptr< osg::Uniform > numSamplesUniform = new osg::Uniform( "halfSamples", dhwidth );
	if( !simple )
	{
		fragShader = fragShader.empty() ? SHADER_PATH + "/ssao2.frag" : fragShader;
	    vertShader = vertShader.empty() ? SHADER_PATH + "/ssao2.vert" : vertShader;
		aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::FRAGMENT, fragShader ) );
		aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::VERTEX,   vertShader ) );
		set->setAttributeAndModes( aprogram.get(), osg::StateAttribute::ON );
		set->setTextureAttributeAndModes( texUnit, tr.get() );
		set->addUniform( new osg::Uniform( "depthMap", texUnit ) );
		set->addUniform( new osg::Uniform( "radius", model->getBound().radius() ) );
		set->addUniform( new osg::Uniform( "dhwidth", dhwidth ) );
		set->addUniform( new osg::Uniform( "dstep", dstep ) );
		set->addUniform( new osg::Uniform( "stepMax", sm ) );
		set->addUniform( new osg::Uniform( "hwMax", hwm ) );
		set->addUniform( new osg::Uniform( "hwMin", minhw ) );
		set->addUniform( new osg::Uniform( "stepMin", mins ) );
		set->addUniform( new osg::Uniform( "steps", steps ) );
		set->addUniform( new osg::Uniform( "numSamples", numSamples ) );
	}
	else
	{
		const bool ssaoOnly = false;
		if( ssaoOnly )
		{
			fragShader = fragShader.empty() ? SHADER_PATH + "/ssao_simple.frag" : fragShader;
			aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::FRAGMENT, fragShader ) );
		} 
		else
		{
			fragShader = fragShader.empty() ? SHADER_PATH + "/ssao_in_vertex_shader.frag" : fragShader;
	        vertShader = vertShader.empty() ? SHADER_PATH + "/ssao_in_vertex_shader.vert" : vertShader;
			//aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::VERTEX,   SHADER_PATH + "/ssao_simple.vert" ) );
			//aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::FRAGMENT,   SHADER_PATH + "/ssao_simple_shading.frag" ) );
			aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::VERTEX, vertShader ) );
			aprogram->addShader( osg::Shader::readShaderFile( osg::Shader::FRAGMENT, fragShader ) );
		}
		set->setAttributeAndModes( aprogram.get(), osg::StateAttribute::ON );
		set->setTextureAttributeAndModes( texUnit, tr.get() );
		set->addUniform( new osg::Uniform( "depthMap", texUnit ) );
        std::clog << "samplingStep: " << dstep   << '\n';
        std::clog << "halfSamples:  " << dhwidth << '\n';
        set->addUniform( osg::get_pointer( stepSizeUniform ) );
        set->addUniform( osg::get_pointer( numSamplesUniform ) );
	}
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
	
	// disable shading and ssao for depth camera
	set = camera->getOrCreateStateSet();
	//set->addUniform( new osg::Uniform( "ssao", 0 ) );
	//set->addUniform( new osg::Uniform( "shade", 0 ) );

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
            std::clog << std::boolalpha << '\r' << "SSAO: " << enabled_ 
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
                std::clog << std::boolalpha << '\r' << "SSAO: " << enabled_ << "  # samples: " << numSamples_ << "  step size: " << stepSize_ << '\r';
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

		arguments.getApplicationUsage()->addCommandLineOption( "-hw",    "Half kernel width" );
		arguments.getApplicationUsage()->addCommandLineOption( "-step",  "Step" );
		arguments.getApplicationUsage()->addCommandLineOption( "-ms",    "Max step multiplier" );
		arguments.getApplicationUsage()->addCommandLineOption( "-mhw",   "Max half kernel window" );
		arguments.getApplicationUsage()->addCommandLineOption( "-mins",  "Min step in pixels" );
		arguments.getApplicationUsage()->addCommandLineOption( "-minhw", "Min kernel half width in pixels" );
		arguments.getApplicationUsage()->addCommandLineOption( "-s",     "Simple, no automatic computation of kernel width" );
		arguments.getApplicationUsage()->addCommandLineOption( "-vert",  "Vertex shader file" );
		arguments.getApplicationUsage()->addCommandLineOption( "-frag",  "Fragment shader file" );
		arguments.getApplicationUsage()->addCommandLineOption( "-steps", "Number of marching steps per ray" );
		arguments.getApplicationUsage()->addCommandLineOption( "-numSamples",  "Number of rays" );
		arguments.getApplicationUsage()->addCommandLineOption( "-texUnit",  "Depth buffer Texture Unit" );
        arguments.getApplicationUsage()->addCommandLineOption( "-occFact",  "Occlusion factor" );
        arguments.getApplicationUsage()->addCommandLineOption( "-viewportUniform",  "Name of uniform used for specifying the viewport" );

		std::string hw;
		std::string step;
		std::string stepMax;
		std::string hwMax;
		std::string hwMin;
		std::string stepMin;
		std::string simp;
		std::string vertShader;
		std::string fragShader;
		std::string st;
		std::string ns;
		std::string tu;
        std::string occFact;
        std::string viewportUniform( "viewport" );
		arguments.read( "-hw", hw );
		arguments.read( "-step",  step );
		arguments.read( "-mhw",   hwMax );
		arguments.read( "-ms",    stepMax );
		arguments.read( "-mins",  stepMin );
		arguments.read( "-minhw", hwMin );
		arguments.read( "-vert", vertShader );
		arguments.read( "-frag", fragShader );
		arguments.read( "-steps", st );
		arguments.read( "-numSamples", ns );
		arguments.read( "-texUnit", tu );
        arguments.read( "-occFact", occFact );
        arguments.read( "-viewportUniform", viewportUniform );
		
		float dhw = 0.03f;
		float ds = 0.005f;
		float sm = 32.0f;
		float hwm = 8.0f;
		float mins = 1.0f;
		float minhw = 1.0f;
		bool simple = false;
		float steps = 10.0f;
		float numSamples = 8.0f;
        float of = 4.0f;
		if( arguments.read( "-s" ) )
 		{	
			simple = true;
			dhw = 4;
			ds  = 2;
		}
		if( !hw.empty() )
		{
			std::istringstream is( hw );
			is >> dhw;
		}
		if( !step.empty() )
		{	
			std::istringstream is( step );
			is >> ds;
		}
		if( !stepMax.empty() )
		{	
			std::istringstream is( stepMax );
			is >> sm;
		}
		if( !hwMax.empty() )
		{	
			std::istringstream is( hwMax );
			is >> hwm;
		}
		if( !hwMin.empty() )
		{	
			std::istringstream is( hwMin );
			is >> minhw;
		}
		if( !stepMin.empty() )
		{	
			std::istringstream is( stepMin );
			is >> mins;
		}
		if( !st.empty() )
		{	
			std::istringstream is( st );
			is >> steps;
		}
		if( !ns.empty() )
		{	
			std::istringstream is( ns );
			is >> numSamples;
		}
		if( !tu.empty() )
		{	
			std::istringstream is( tu );
			is >> texUnit;
		}
        if( !occFact.empty() )
		{	
			std::istringstream is( occFact );
			is >> of;
		}
				
		osg::ref_ptr<osg::Node> loadedModel = osgDB::readNodeFiles( arguments, new osgDB::ReaderWriter::Options( options ) );
		
		if( loadedModel != 0 && arguments.read( "-normals" ) )
		{
			osgUtil::SmoothingVisitor tsv;
			loadedModel->accept( tsv );
		}
		
		SSAO( viewer, loadedModel.get(), dhw, ds, sm, hwm, mins, minhw, simple, vertShader, fragShader, steps, numSamples, of, viewportUniform );
	
		return viewer.run();
	}
	catch( const std::exception& e )
	{
		std::cerr << e.what() << std::endl;
	}
	return 1;
}
