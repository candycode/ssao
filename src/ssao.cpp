#include "ssao.h"

#include <string>
#include <fstream>
#include <stdexcept>

#include <osg/Node>
#include <osg/StateSet>
#include <osg/Texture>
#include <osg/TextureRectangle>
#include <osg/Shader>
#include <osg/Program>
#include <osgGA/GUIEventHandler>
#include <osgGA/GUIEventAdapter>
#include <osgGA/GUIActionAdapter>

//------------------------------------------------------------------------------
/// Keyboard event handler for simple SSAO technique parameters.
class SSAOSimpleKbEventHandler : public osgGA::GUIEventHandler
{
public:
    SSAOSimpleKbEventHandler( osg::Uniform* ssaoU = 0,
                              osg::Uniform* samplesU = 0,
                              osg::Uniform* stepSizeU = 0,
                              osg::Uniform* occFactorU = 0,
                              osg::Uniform* textureEnabledU = 0 ) :
                                                ssaoUniform_( ssaoU ),
                                                numSamplesUniform_( samplesU ),
                                                stepSizeUniform_( stepSizeU ),
                                                occFactorUniform_( occFactorU ),
                                                textureEnabledUniform_( textureEnabledU ),
                                                enabled_( true ), numSamples_( 3 ), stepSize_( 2 ),
                                                occFactor_( 7.0f ), textureEnabled_( false )
    {
        if( ssaoU      ) ssaoU->get( enabled_    );
        if( samplesU   ) samplesU->get( numSamples_ );
        if( stepSizeU  ) stepSizeU->get( stepSize_ );
        if( occFactorU ) occFactorU->get( occFactor_ );
        if( textureEnabledU ) textureEnabledU->get( textureEnabled_ );
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
            case 't':
            case 'T':
                textureEnabled_ = !textureEnabled_;
                ToggleTextures();
                break;
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
            std::clog << std::boolalpha <<  "SSAO: " << enabled_ << "  # samples: " << numSamples_ << "  step size: " << stepSize_ << '\n';
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
    
    void ToggleTextures()
    {
        if( !textureEnabledUniform_ ) return;
        textureEnabledUniform_->set( textureEnabled_ ? 1 : 0 );
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
    osg::ref_ptr< osg::Uniform > textureEnabledUniform_;
    bool enabled_;
    float numSamples_;
    float stepSize_;
    float occFactor_;
    bool textureEnabled_;
};


//------------------------------------------------------------------------------
/// Keyboard handler for SSAO tracing algorithm parameters.
class SSAOTraceKbEventHandler : public osgGA::GUIEventHandler
{
public:
    SSAOTraceKbEventHandler( osg::Uniform* ssaoU = 0,
                             osg::Uniform* dRadiusU = 0,
                             osg::Uniform* stepMulU = 0,
                             osg::Uniform* maxRadiusPixelsU = 0,
                             osg::Uniform* occFactorU = 0,
                             osg::Uniform* samplingDirsU = 0,
                             osg::Uniform* textureEnabledU = 0,
                             osg::Uniform* minCosAngleU = 0
                            ) :
                                ssaoUniform_( ssaoU ),
                                dRadiusUniform_( dRadiusU ),
                                stepMulUniform_( stepMulU ),
                                maxRadPixelsUniform_( maxRadiusPixelsU ),
                                occFactorUniform_( occFactorU ),
                                samplingDirsUniform_( samplingDirsU ),
                                textureEnabledUniform_( textureEnabledU ),
                                minCosAngleUniform_( minCosAngleU ),
                                enabled_( true ), dRadius_( 0.1f ), stepMul_( 1 ),
                                maxRadiusPixels_( 32 ), occFactor_( 7.0f ),
                                samplingDirs_( 8 ), textureEnabled_( false ), minCosAngle_( 0.2f )
    {
        if( ssaoU      ) ssaoU->get( enabled_    );
        if( dRadiusU   ) dRadiusU->get( dRadius_ );
        if( stepMulU  ) stepMulU->get( stepMul_ );
        if( maxRadiusPixelsU ) maxRadiusPixelsU->get( maxRadiusPixels_ );
        if( occFactorU ) occFactorU->get( occFactor_ );
        if( samplingDirsU ) samplingDirsU->get( samplingDirs_ );
        if( textureEnabledU ) textureEnabledU->get( textureEnabled_ );
        if( minCosAngleU ) minCosAngleU->get( minCosAngle_ );
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
            case 't':
            case 'T':
                textureEnabled_ = !textureEnabled_;
                ToggleTextures();
                break;
            case 'a': 
            case 'A': 
                enabled_ = !enabled_;
                ToggleSSAO();
                handled = true;
                break;
            case '-': 
            case '_': 
                UpdateMinCosAngle( -0.1f );
                handled = true;
                break;
            case '+': 
            case '=': 
                UpdateMinCosAngle( +0.1f );
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
    
    void ToggleTextures()
    {
        if( !textureEnabledUniform_ ) return;
        textureEnabledUniform_->set( textureEnabled_ ? 1 : 0 );
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
    void UpdateMinCosAngle( float da )
    {
        if( !minCosAngleUniform_ ) return;
        if( minCosAngle_ + da <= -1.0f || minCosAngle_ + da >= 1.0f ) return;
        minCosAngle_ += da;
        minCosAngleUniform_->set( minCosAngle_ );
    }
    osg::ref_ptr< osg::Uniform > ssaoUniform_;
    osg::ref_ptr< osg::Uniform > dRadiusUniform_;
    osg::ref_ptr< osg::Uniform > stepMulUniform_;
    osg::ref_ptr< osg::Uniform > maxRadPixelsUniform_;
    osg::ref_ptr< osg::Uniform > occFactorUniform_;
    osg::ref_ptr< osg::Uniform > samplingDirsUniform_;
    osg::ref_ptr< osg::Uniform > textureEnabledUniform_;
    osg::ref_ptr< osg::Uniform > minCosAngleUniform_;
    bool enabled_;
    float dRadius_;
    float stepMul_;
    float maxRadiusPixels_;
    float occFactor_;
    float samplingDirs_;
    bool textureEnabled_;
    float minCosAngle_;
};



//------------------------------------------------------------------------------
osgGA::GUIEventHandler* CreateSSAOUniformsAndHandler( const osg::Node& model,
                                                      osg::StateSet& sset, 
                                                      const SSAOParameters& ssaoParams,
                                                      osg::TextureRectangle* depth,
                                                      osg::TextureRectangle* positions,
                                                      osg::TextureRectangle* normals )
{
    // MRT requested: setup positions and normals/depth
    if( ssaoParams.mrt )
    {
        if( positions )
        {
            sset.setTextureAttributeAndModes( ssaoParams.texUnit, positions );
            sset.addUniform( new osg::Uniform( "positions", ssaoParams.texUnit ) );
        }
        if( normals )
        {
            sset.setTextureAttributeAndModes( ssaoParams.texUnit + 1, normals );
            sset.addUniform( new osg::Uniform( "normals", ssaoParams.texUnit + 1 ) );
        }
    }
    else if( depth )
    {
        sset.setTextureAttributeAndModes( ssaoParams.texUnit, depth );
        sset.addUniform( new osg::Uniform( "depthMap", ssaoParams.texUnit ) );
    }
    osg::ref_ptr< osg::Uniform > ssaoUniform  = new osg::Uniform( "ssao", 1 );
    sset.addUniform( osg::get_pointer( ssaoUniform ) );
    sset.addUniform( new osg::Uniform( "shade", 1 ) );
    osg::ref_ptr< osg::Uniform > occFactorUniform = new osg::Uniform( "occlusionFactor", ssaoParams.occFact );
    sset.addUniform( occFactorUniform );
    
    osg::ref_ptr< osg::Uniform > teu = new osg::Uniform( "textureEnabled", 0 );
    sset.addUniform( osg::get_pointer( teu ) );

    if( ssaoParams.simple )
    {
        osg::ref_ptr< osg::Uniform > stepSizeUniform = new osg::Uniform( "samplingStep", ssaoParams.step );
        osg::ref_ptr< osg::Uniform > numSamplesUniform = new osg::Uniform( "halfSamples", ssaoParams.hw );
        sset.addUniform( osg::get_pointer( stepSizeUniform ) );
        sset.addUniform( osg::get_pointer( numSamplesUniform ) );
        return new SSAOSimpleKbEventHandler( 
                    osg::get_pointer( ssaoUniform ),
                    osg::get_pointer( numSamplesUniform ),
                    osg::get_pointer( stepSizeUniform ),
                    osg::get_pointer( occFactorUniform ),
                    osg::get_pointer( teu ) );

    }
    else
    {
        osg::ref_ptr< osg::Uniform > dRadiusUniform = new osg::Uniform( "dhwidth", ssaoParams.dRadius );
        osg::ref_ptr< osg::Uniform > stepMulUniform = new osg::Uniform( "dstep", ssaoParams.stepMul );
        osg::ref_ptr< osg::Uniform > maxRadiusPixelsUniform = new osg::Uniform( "hwMax", ssaoParams.maxRadius );
        osg::ref_ptr< osg::Uniform > numSamplesUniform = new osg::Uniform( "numSamples", ssaoParams.maxNumSamples );
        osg::ref_ptr< osg::Uniform > minCosAngleUniform = new osg::Uniform( "minCosAngle", ssaoParams.minCosAngle );
        
        sset.addUniform( new osg::Uniform( ssaoParams.ssaoRadiusUniform.c_str(), model.getBound().radius() ) );
        sset.addUniform( osg::get_pointer( dRadiusUniform ) );
        sset.addUniform( osg::get_pointer( stepMulUniform ) );
        sset.addUniform( osg::get_pointer( maxRadiusPixelsUniform ) );
        sset.addUniform( osg::get_pointer( numSamplesUniform ) );
        sset.addUniform( osg::get_pointer( minCosAngleUniform ) );  

        return new SSAOTraceKbEventHandler( 
                    osg::get_pointer( ssaoUniform ),
                    osg::get_pointer( dRadiusUniform ),
                    osg::get_pointer( stepMulUniform ),
                    osg::get_pointer( maxRadiusPixelsUniform ),
                    osg::get_pointer( occFactorUniform ),
                    osg::get_pointer( numSamplesUniform ),
                    osg::get_pointer( teu ),
                    osg::get_pointer( minCosAngleUniform ) );

    }

    return 0; //we never get here
}

//------------------------------------------------------------------------------
/// Return string with content of text file.
std::string ReadTextFile( const std::string& fname )
{
    std::ifstream in( fname.c_str() );
    if( !in ) {
        throw std::logic_error( "Cannot open file " + fname );
        return "";
    }
    std::string str,strTotal;
    std::getline( in, str);
    while( in ) {
        strTotal += '\n';
        strTotal += str;
        std::getline( in, str );
    }
    return strTotal;
}

//------------------------------------------------------------------------------
/// Return shader source with code prefixed.
osg::Shader* LoadShaderSource( const std::string& fname,
                               osg::Shader::Type type,
                               const std::string& prefix = "" )
{
    const std::string shaderSource = ReadTextFile( fname );
    if( shaderSource.empty() ) return 0;
    return new osg::Shader( type, prefix + shaderSource );    
}

//------------------------------------------------------------------------------
/// Create string to prefix to shader source to enable disable multiple
/// render targets and set shading style
std::string BuildShaderSourcePrefix( bool mrt,
                                     SSAOParameters::ShadingStyle ss,
                                     bool enableTextures = false )
{
    std::string ssp;
    if( enableTextures ) ssp += "#define TEXTURE_ENABLED\n";
    if( mrt ) ssp += "#define MRT_ENABLED\n";
    switch( ss )
    {    
    case SSAOParameters::AMBIENT_OCCLUSION_FLAT_SHADING:
                                         ssp += "#define AO_FLAT\n";
                                         break;
    case SSAOParameters::AMBIENT_OCCLUSION_LAMBERT_SHADING: 
                                         ssp += "#define AO_LAMBERT\n";
                                         break;
    case SSAOParameters::AMBIENT_OCCLUSION_SPHERICAL_HARMONICS_SHADING: 
                                         ssp += "#define AO_SPHERICAL_HARMONICS\n";
                                         break;
    default: break;
    }
    return ssp;
}



//------------------------------------------------------------------------------
/// Create shader program.
osg::Program* CreateSSAOProgram( const SSAOParameters& ssaoParams, const std::string& path )
{
    if( ssaoParams.vertShader.empty() && ssaoParams.fragShader.empty() ) {
        return 0;
    }
    const std::string SHADER_SOURCE_PREFIX( 
        BuildShaderSourcePrefix( ssaoParams.mrt, ssaoParams.shadeStyle, ssaoParams.enableTextures ) );
    osg::ref_ptr< osg::Shader > vertexShader = 
        LoadShaderSource( ssaoParams.vertShader, osg::Shader::VERTEX, SHADER_SOURCE_PREFIX  );
    osg::ref_ptr< osg::Shader > fragmentShader =
        LoadShaderSource( ssaoParams.fragShader, osg::Shader::FRAGMENT, SHADER_SOURCE_PREFIX );

    osg::ref_ptr< osg::Program > aprogram;
    if( vertexShader != 0 || fragmentShader != 0 )    
    {
        aprogram = new osg::Program;
        aprogram->setName( "SSAO" );
        if( vertexShader != 0 ) aprogram->addShader( osg::get_pointer( vertexShader ) );
	    if( fragmentShader != 0 ) aprogram->addShader( osg::get_pointer( fragmentShader ) );
    }
    return aprogram.release();
}

