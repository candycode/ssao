#ifndef TEXTURE_PREPROCESS_H_
#define TEXTURE_PREPROCESS_H_

#include <string>

#include <osg/Texture>
#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Geometry>


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
                    textureUnit = i; 
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



#endif //TEXTURE_PREPROCESS_H_