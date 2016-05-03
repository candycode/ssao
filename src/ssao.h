#ifndef SSAO_H_
#define SSAO_H_

#include <string>
#include <iostream>

// forward declarations
namespace osg
{
    class Camera;
    class Program;
    class Node;
    class TextureRectangle;
    class StateSet;
}

namespace osgGA
{
    class GUIEventHandler;
}

/// SSAO parameters
struct SSAOParameters
{
    enum ShadingStyle
    { 
        AMBIENT_OCCLUSION_SHADING,
        AMBIENT_OCCLUSION_FLAT_SHADING,
        AMBIENT_OCCLUSION_LAMBERT_SHADING,
        AMBIENT_OCCLUSION_SPHERICAL_HARMONICS_SHADING
    };

    SSAOParameters() :
        enableTextures( false ),
        simple( false ),
        hw( 16.0f ),
        step( 1.0f ),
        occFact( 1.0f ),
        texUnit( 2 ),
        viewportUniform( "viewport" ),
        ssaoRadiusUniform( "radius" ),
        vertShader(""),
        fragShader(""),
        stepMul( 1.0f ),
        maxRadius( 32.0f ),
        dRadius( 0.1f ),
        maxNumSamples( 8 ),
        mrt( false ),
        shadeStyle( AMBIENT_OCCLUSION_SHADING ),
        minCosAngle( 0.2f ) // ~78 deg
        {}

        bool enableTextures;
        bool simple;
        float hw;
        float step;
        float occFact;
        int texUnit;
        std::string viewportUniform;
        std::string ssaoRadiusUniform;
        std::string vertShader;
        std::string fragShader;
        float stepMul;
        float maxRadius;
        float dRadius;
        float maxNumSamples;
        bool mrt;
        ShadingStyle shadeStyle;
        float minCosAngle;
};

inline std::ostream& operator<<( std::ostream& os, const SSAOParameters& ssaoParams )
{
    os << '\n';
    os << std::boolalpha;
    os << "  enableTextures:    " << ssaoParams.enableTextures
        << "\n  simple:            " << ssaoParams.simple
        << "\n  hw:                " << ssaoParams.hw
        << "\n  step:              " << ssaoParams.step
        << "\n  occFact:           " << ssaoParams.occFact
        << "\n  texUnit:           " << ssaoParams.texUnit
        << "\n  viewportUniform:   " << ssaoParams.viewportUniform
        << "\n  ssaoRadiusUniform: " << ssaoParams.ssaoRadiusUniform
        << "\n  vertShader:        " << ssaoParams.vertShader
        << "\n  fragShader:        " << ssaoParams.fragShader
        << "\n  stepMul:           " << ssaoParams.stepMul
        << "\n  maxRadius:         " << ssaoParams.maxRadius
        << "\n  dRadius:           " << ssaoParams.dRadius
        << "\n  maxNumSamples:     " << ssaoParams.maxNumSamples
        << "\n  mrt:               " << ssaoParams.mrt
        << "\n  shadeStyle         " << ssaoParams.shadeStyle;
    os << std::endl;
    return os;
}

osg::Program* CreateSSAOProgram( const SSAOParameters&, const std::string& path );

osgGA::GUIEventHandler* CreateSSAOUniformsAndHandler( const  osg::Node&,
                                                      osg::StateSet&,
                                                      const SSAOParameters&,
                                                      osg::TextureRectangle*,
                                                      osg::TextureRectangle*,
                                                      osg::TextureRectangle* );


#endif // SSAO_H_