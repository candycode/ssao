#include <osgViewer/Viewer >
#include <osgViewer/ViewerEventHandlers>
#include <osg/Texture>
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

#include "ssao.h"
#include "texture_preprocess.h"
#include "manipulator.h"
#include "posnormal_mrt_shaders.h"

static const std::string SHADER_PATH="C:/projects/ssao/trunk/src";
static const int MAX_FBO_WIDTH = 2048;
static const int MAX_FBO_HEIGHT = 2048;

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
// Attach depth or positions & normals textures to pre-render camera
osg::Camera* CreatePreRenderCamera( osg::Texture* depth,
                                    osg::Texture* positions,
                                    osg::Texture* normals )
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
    if( normals   ) camera->attach( osg::Camera::BufferComponent( osg::Camera::COLOR_BUFFER0 + 1 ), normals );
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
		    sc->setViewport( const_cast< osg::Camera* >( osg::get_pointer( observedCamera_ ) )->getViewport() );
         	if( uniform_ ) uniform_->set( osg::Vec2( observedCamera_->getViewport()->width(), observedCamera_->getViewport()->height() ) );
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
	osg::ref_ptr< osg::Geode > ball1 = new osg::Geode;
    osg::ref_ptr< osg::Geode > ball2 = new osg::Geode;
    osg::ref_ptr< osg::Geode > ball3 = new osg::Geode;
    osg::ref_ptr< osg::Geode > ball4 = new osg::Geode;
    osg::ref_ptr< osg::Geode > box = new osg::Geode;
	osg::ref_ptr< osg::ShapeDrawable > s1 = new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( -1.f, 1.f, 0.f ), 1.f ) );
	osg::ref_ptr< osg::TessellationHints > ti = new osg::TessellationHints;
	ti->setDetailRatio( 0.2f );
	s1->setTessellationHints( ti.get() );
	ball1->addDrawable( s1.get() );
	ball2->addDrawable( new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 0.f, 1.f, 1.f ), 1.f ) ) );
	ball3->addDrawable( new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 1.f, 1.f, 0.f ), 1.f ) ) );
	ball4->addDrawable( new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 0.f, 1.f, -1.f ), 1.f ) ) );
    box->addDrawable( new osg::ShapeDrawable( new osg::Box( osg::Vec3( 0.f, 2.2f, 0.f ), 7.0f, 0.2f, 7.0f ) ) );
    osg::ref_ptr< osg::Group > group = new osg::Group;
    group->addChild( osg::get_pointer( ball1 ) );
    group->addChild( osg::get_pointer( ball2 ) );
    group->addChild( osg::get_pointer( ball3 ) );
    group->addChild( osg::get_pointer( ball4 ) );
    group->addChild( osg::get_pointer( box ) );
    return group.release();
}

/// Pre-draw callback used to set the viewport uniform. 
class SetViewportUniformCBack : public osg::Camera::DrawCallback 
{
public:
	SetViewportUniformCBack( const osg::Camera* observedCamera, osg::Uniform* vp )
		: observedCamera_( observedCamera ), uniform_( vp ) {}
	void operator()( osg::Node* , osg::NodeVisitor* )
    {
        SetUniform();
    }
	void operator() ( osg::RenderInfo& /*renderInfo*/ ) const
    {
        SetUniform();
	}
    void SetUniform() const 
    {
        if( observedCamera_ != 0 && observedCamera_->getViewport() != 0 )
        {
            if( uniform_ ) uniform_->set( osg::Vec2( observedCamera_->getViewport()->width(), observedCamera_->getViewport()->height() ) );
        }
    }
private:
	osg::ref_ptr< const osg::Camera > observedCamera_;
	osg::ref_ptr< osg::Uniform > uniform_;
};

/// Returns command line parser
osg::ArgumentParser GetCmdLineParser( int* argc, char** argv )
{
    osg::ArgumentParser arguments( argc, argv );
    arguments.getApplicationUsage()->addCommandLineOption( "--options", "Pass options to loader(s)" );
   
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
                                                           "                     'ao_lambert' ambient occlusion with lambert shading\n"
                                                           "                     'ao_sph_harm' spherical harmonics" ); 
    arguments.getApplicationUsage()->addCommandLineOption( "-textures",  "[advanced] enable textures" );
    arguments.getApplicationUsage()->addCommandLineOption( "-manip",  "[all] enable manipulators; select manipulator with 1-7 keys" );

    return arguments;
}

/// Parse SSAO parameters
SSAOParameters ParseSSAOParameters( osg::ArgumentParser& arguments )
{
    SSAOParameters p;
    
    //command line parameter value
    std::string cmdParStr; 
      
	// read command line parameters
    p.simple =  arguments.read( "-s" );
	if( arguments.read( "-hw",  cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.hw;
    }
	if( arguments.read( "-step", cmdParStr  ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.step;
    }
    if( arguments.read( "-occFact", cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.occFact;
    }
    if( arguments.read( "-texUnit", cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.texUnit;
    }
    arguments.read( "-viewportUniform", p.viewportUniform );
    arguments.read( "-ssaoRadiusUniform", p.ssaoRadiusUniform );
    arguments.read( "-vert", p.vertShader );
    arguments.read( "-frag", p.fragShader );
    if( arguments.read( "-stepMul", cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.stepMul;
    }
    if( arguments.read( "-maxRadius", cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.maxRadius;
    }
    if( arguments.read( "-dRadius", cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.dRadius;
    }
    if( arguments.read( "-maxNumSamples",  cmdParStr ) )
    {
        std::istringstream is( cmdParStr );
		is >> p.maxNumSamples;
    }
    if( arguments.read( "-shade", cmdParStr ) )
    {
        if( cmdParStr == "ao" )
        {
            p.shadeStyle = SSAOParameters::AMBIENT_OCCLUSION_SHADING;
        }
        else if( cmdParStr == "ao_flat" )
        {
            p.shadeStyle = SSAOParameters::AMBIENT_OCCLUSION_FLAT_SHADING;
        }
        else if( cmdParStr == "ao_lambert" )
        {
            p.shadeStyle = SSAOParameters::AMBIENT_OCCLUSION_LAMBERT_SHADING;
        }
        else if( cmdParStr == "ao_sph_harm" )
        {
            p.shadeStyle = SSAOParameters::AMBIENT_OCCLUSION_SPHERICAL_HARMONICS_SHADING;
        }
        else throw std::runtime_error( "Invalid shading model: " + cmdParStr );
    }
    p.mrt = arguments.read( "-mrt" );
    p.enableTextures = arguments.read( "-textures" );
    return p;
}


// Renders subgraph into depth texture rectangle or position & normal/depth texturs
// then renders scene using data from pre-rendered textures for shading.
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
	
	try
	{
        /// *** I/O *** ///
        // ADD EXTENSIONS //
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

        // READ PARAMETERS //
		// read and parse ssao parameters
		osg::ArgumentParser arguments = GetCmdLineParser(&argc,argv);
	    SSAOParameters ssaoParams = ParseSSAOParameters( arguments );
	    // read additional options to pass to reader
        std::string options;
        arguments.read( "--options", options );
       	
        // LOAD MODEL //
	    osg::ref_ptr<osg::Node> model = osgDB::readNodeFiles( arguments, new osgDB::ReaderWriter::Options( options ) );
	    if( model != 0 && arguments.read( "-normals" ) )
	    {
		    osgUtil::SmoothingVisitor tsv;
		    model->accept( tsv );
	    }
        if( model == 0 ) model = CreateDefaultModel();
   
        // add group: useful for adding transform in case mainpulator requested
        if( !model->asGroup() )
        {
            osg::ref_ptr< osg::Group > group = new osg::Group;
            group->addChild( osg::get_pointer( model ) );
            model = group;
        }

        // PROCESS TEXTURES//
        // enable or remove textures
        if( model != 0 )
        {
            if( ssaoParams.enableTextures )
            {
                // create reserved texture unit list
                std::vector< int > tu( 3 );
                tu[ 0 ] = ssaoParams.texUnit;
                tu[ 1 ] = tu[ 0 ] + 1;
                tu[ 2 ] = tu[ 1 ] + 1;
                // make textures in scenegraph accessible from shaders
                TextureToUniform( *model, "textureUnit", "tex", tu.begin(), tu.end() );
            }
            else RemoveTextures( *model );
        }
      
        InsertTransform( osg::get_pointer( model ) );

        /// *** CREATE VIEWER *** ///
        // construct the viewer.
		osgViewer::Viewer viewer;
		// add the stats handler
		viewer.addEventHandler( new osgViewer::StatsHandler );

        /// *** SSAO *** ///
        // CREATE TEXTURES 
        // depth/position/normal textures 
	    osg::ref_ptr< osg::TextureRectangle > depth; 
        osg::ref_ptr< osg::TextureRectangle > positions;
        osg::ref_ptr< osg::TextureRectangle > normals;
        // if multiple render targets enabled z component will be available in 
        // w component of positions or normals
        if( ssaoParams.mrt )
        {
            positions = GenerateColorTextureRectangle();
            normals   = GenerateColorTextureRectangle();
        }
        else depth = GenerateDepthTextureRectangle();
        
        // CREATE PRE-RENDER CAMERA
        osg::ref_ptr< osg::Camera > preRenderCamera = 
            CreatePreRenderCamera( osg::get_pointer( depth ),
                                   osg::get_pointer( positions ),
                                   osg::get_pointer( normals ) );
        // model to pre-render: used to generate depth map or depth-position-normal data
        preRenderCamera->addChild( osg::get_pointer( model ) ); 
              
        // setup camera callback
	    osg::ref_ptr< osg::Uniform > vp = new osg::Uniform( ssaoParams.viewportUniform.c_str(),
                                                            osg::Vec2( MAX_FBO_WIDTH, MAX_FBO_HEIGHT ) );
        preRenderCamera->getOrCreateStateSet()->addUniform( osg::get_pointer( vp ) );
	    // sync depth camera with main camera
        SetCameraCallback( osg::get_pointer( preRenderCamera ), viewer.getCamera(), osg::get_pointer( vp ) );

        // SETUP MAIN CAMERA & SSAO EVENT HANDLER
        osg::ref_ptr< osg::Program > ssaoProgram = CreateSSAOProgram( ssaoParams, SHADER_PATH );
        osg::ref_ptr< osg::Camera > mainCamera = viewer.getCamera();
        if( ssaoProgram != 0 )
        {
            mainCamera->getOrCreateStateSet()->setAttributeAndModes( osg::get_pointer( ssaoProgram ) );
        }
        osg::ref_ptr< osgGA::GUIEventHandler > uniformHandler = 
            CreateSSAOUniformsAndHandler( *model, *mainCamera->getOrCreateStateSet(), ssaoParams,
                    osg::get_pointer( depth ), osg::get_pointer( positions ), osg::get_pointer( normals ) );
        viewer.addEventHandler( osg::get_pointer( uniformHandler ) );
        // set up uniform
        osg::ref_ptr< osg::Uniform > vpu = new  osg::Uniform( ssaoParams.viewportUniform.c_str(),
                                                 osg::Vec2( MAX_FBO_WIDTH, MAX_FBO_HEIGHT ) );
        mainCamera->setPreDrawCallback( 
            new SetViewportUniformCBack( osg::get_pointer( mainCamera ), osg::get_pointer( vpu ) ) );
        mainCamera->getOrCreateStateSet()->addUniform( osg::get_pointer( vpu ) );

        /// *** ADD TO VIEWER *** ///
        osg::ref_ptr< osg::Group > root = new osg::Group;
        root->addChild( osg::get_pointer( preRenderCamera ) );
        root->addChild( osg::get_pointer( model ) );
        viewer.setSceneData( osg::get_pointer( root ) );
        if( !ssaoParams.enableTextures ) root->getOrCreateStateSet()->addUniform( new osg::Uniform( "textureUnit", -1 ) );
         
        /// *** MANIPULATOR *** ///
        if( arguments.read( "-manip" ) )
        {
            //osg::ref_ptr< osgManipulator::Dragger > manip = CreateManipulator( "TranslateAxisDragger" );
            //if( !manip ) throw std::runtime_error( "Cannot create manipulator" );
            osg::ref_ptr< osg::Group > manipGroup = new osg::Group;
            //manipGroup->addChild( osg::get_pointer( manip ) );
            //model = InsertTransform( osg::get_pointer( model ) ); // insert transform node above each child node 
            root->addChild( osg::get_pointer( manipGroup ) );
            preRenderCamera->addChild( CreatePreRenderManipulatorTree( osg::get_pointer( manipGroup ) ) );
            // add picker to select manipulator transform: selected transform
            // is the the parent of the selected node
            viewer.addEventHandler( CreateObjectToManipulatorTransformPicker( osg::get_pointer( manipGroup ) ) );
            viewer.addEventHandler( CreateDraggerSelectorHandler( osg::get_pointer( manipGroup ) ) );
        }
     
        /// *** RENDERING LOOP ***///
        //return viewer.run();
        
        // since the pre render camera needs to be synchronized with the main camera
        // we need to perform the synchronization before the actual rendering takes place
        // Fixed by properly setting a pre-draw callback to update both pre-render and
        // main camera
        viewer.setThreadingModel( osgViewer::Viewer::SingleThreaded );     
		viewer.setCameraManipulator(new osgGA::TrackballManipulator());
        viewer.setReleaseContextAtEndOfFrameHint( false );
        viewer.realize();
        SyncCameraNode sn( mainCamera, osg::get_pointer( preRenderCamera ), 0 );
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

