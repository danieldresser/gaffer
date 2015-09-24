//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, John Haddon. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "IECore/MeshPrimitive.h"

#include "IECoreGL/CurvesPrimitive.h"
#include "IECoreGL/Group.h"
#include "IECoreGL/ShaderStateComponent.h"
#include "IECoreGL/ShaderLoader.h"
#include "IECoreGL/TextureLoader.h"
#include "IECoreGL/DiskPrimitive.h"
#include "IECoreGL/ToGLMeshConverter.h"

#include "Gaffer/Metadata.h"

#include "GafferSceneUI/StandardLightVisualiser.h"

using namespace std;
using namespace boost;
using namespace Imath;
using namespace IECore;
using namespace IECoreGL;
using namespace Gaffer;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Utility methods. We define these in an anonymouse namespace rather
// than clutter up the header with private methods.
//////////////////////////////////////////////////////////////////////////

namespace
{

template<typename T>
T parameter( InternedString metadataTarget, const Light *light, InternedString parameterNameMetadata, T defaultValue )
{
	ConstStringDataPtr parameterName = Metadata::value<StringData>( metadataTarget, parameterNameMetadata );
	if( !parameterName )
	{
		return defaultValue;
	}

	typedef IECore::TypedData<T> DataType;
	/// \todo Add a const version of Light::parametersData() so we don't need the cast.
	if( const DataType *parameterData = const_cast<Light *>( light )->parametersData()->member<DataType>( parameterName->readable() ) )
	{
		return parameterData->readable();
	}

	return defaultValue;
}

void addWireframeCurveState( IECoreGL::Group *group )
{
	group->getState()->add( new IECoreGL::Primitive::DrawWireframe( false ) );
	group->getState()->add( new IECoreGL::Primitive::DrawSolid( true ) );
	group->getState()->add( new IECoreGL::CurvesPrimitive::UseGLLines( true ) );
	group->getState()->add( new IECoreGL::CurvesPrimitive::GLLineWidth( 2.0f ) );
	group->getState()->add( new IECoreGL::LineSmoothingStateComponent( true ) );
}

V3f lightPlane( const V2f &p )
{
	return V3f( 0, p.y, -p.x );
}

// Coordinates are in the light plane.
void addRay( const V2f &start, const V2f &end, vector<int> &vertsPerCurve, vector<V3f> &p )
{
	const float arrowScale = 0.05;

	const V2f dir = end - start;
	const V2f perp( dir.y, -dir.x );

	p.push_back( lightPlane( start ) );
	p.push_back( lightPlane( end ) );
	vertsPerCurve.push_back( 2 );

	p.push_back( lightPlane( end + arrowScale * ( perp * 2 - dir * 3 ) ) );
	p.push_back( lightPlane( end ) );
	p.push_back( lightPlane( end + arrowScale * ( perp * -2 - dir * 3 ) ) );
	vertsPerCurve.push_back( 3 );
}

void addCircle( const V3f &center, float radius, vector<int> &vertsPerCurve, vector<V3f> &p )
{
	const int numDivisions = 100;
	for( int i = 0; i < numDivisions; ++i )
	{
		const float angle = 2 * M_PI * (float)i/(float)(numDivisions-1);
		p.push_back( center + radius * V3f( cos( angle ), sin( angle ), 0 ) );
	}
	vertsPerCurve.push_back( numDivisions );
}

void addSolidArc( int axis, const V3f &center, float majorRadius, float minorRadius, float startFraction, float stopFraction, vector<int> &vertsPerPoly, vector<int> &vertIds, vector<V3f> &p )
{
	const int numSegmentsForCircle = 100;
	int numSegments = (int)ceil( (stopFraction - startFraction) * numSegmentsForCircle );

	int start = p.size();
	for( int i = 0; i < numSegments + 1; ++i )
	{
		const float angle = 2 * M_PI * ( startFraction + (stopFraction - startFraction) * (float)i/(float)(numSegments) );
		V3f dir( -sin( angle ), cos( angle ), 0 );
		if( axis == 0 ) dir = V3f( 0, dir[1], -dir[0] );
		else if( axis == 1 ) dir = V3f( dir[1], 0, dir[0] );
		p.push_back( center + majorRadius * dir );
		p.push_back( center + minorRadius * dir );
	}
	for( int i = 0; i < numSegments; ++i )
	{
		vertIds.push_back( start + i * 2 );
		vertIds.push_back( start + i * 2  + 1);
		vertIds.push_back( start + i * 2  + 3);
		vertIds.push_back( start + i * 2  + 2);
		vertsPerPoly.push_back( 4 );
	}
}

void addCone( float angle, vector<int> &vertsPerCurve, vector<V3f> &p )
{
	const float halfAngle = 0.5 * M_PI * angle / 180.0;
	const float baseRadius = sin( halfAngle );
	const float baseDistance = cos( halfAngle );

	addCircle( V3f( 0, 0, -baseDistance ), baseRadius, vertsPerCurve, p );

	p.push_back( V3f( 0 ) );
	p.push_back( V3f( 0, baseRadius, -baseDistance ) );
	vertsPerCurve.push_back( 2 );

	p.push_back( V3f( 0 ) );
	p.push_back( V3f( 0, -baseRadius, -baseDistance ) );
	vertsPerCurve.push_back( 2 );
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// StandardLightVisualiser implementation.
//////////////////////////////////////////////////////////////////////////

StandardLightVisualiser::StandardLightVisualiser()
{
}

StandardLightVisualiser::~StandardLightVisualiser()
{
}

IECoreGL::ConstRenderablePtr StandardLightVisualiser::visualise( const IECore::Object *object ) const
{
	const IECore::Light *light = runTimeCast<const IECore::Light>( object );
	if( !light )
	{
		return NULL;
	}

	InternedString metadataTarget = "light:" + light->getName();
	ConstStringDataPtr type = Metadata::value<StringData>( metadataTarget, "type" );

	bool indicatorFaceCamera = false;
 
	GroupPtr result = new Group;
	if( !type || type->readable() == "point" )
	{
		result->addChild( const_pointer_cast<IECoreGL::Renderable>( pointRays() ) );
		indicatorFaceCamera = true;
	}
	else if( type->readable() == "spot" )
	{
		float coneAngle = parameter<float>( metadataTarget, light, "coneAngleParameter", 0.0f );
		float penumbraAngle = parameter<float>( metadataTarget, light, "penumbraAngleParameter", 0.0f );

		if( ConstStringDataPtr angleUnit = Metadata::value<StringData>( metadataTarget, "angleUnit" ) )
		{
			if( angleUnit->readable() == "radians" )
			{
				coneAngle *= 180.0 / M_PI;
				penumbraAngle *= 180 / M_PI;
			}
		}

		ConstStringDataPtr penumbraType = Metadata::value<StringData>( metadataTarget, "penumbraType" );

		float innerAngle = 0;
		float outerAngle = 0;

		if( !penumbraType || penumbraType->readable() == "inset" )
		{
			outerAngle = coneAngle;
			innerAngle = coneAngle - 2.0f * penumbraAngle;
		}
		else if( penumbraType->readable() == "outset" )
		{
			outerAngle = coneAngle + 2.0f * penumbraAngle;
			innerAngle = coneAngle ;
		}
		else if( penumbraType->readable() == "absolute" )
		{
			outerAngle = coneAngle;
			innerAngle = penumbraAngle;
		}

		result->addChild( const_pointer_cast<IECoreGL::Renderable>( spotlightCone( innerAngle, outerAngle ) ) );
		result->addChild( const_pointer_cast<IECoreGL::Renderable>( ray() ) );
	}
	else if( type->readable() == "distant" )
	{
		for ( int i = 0; i < 3; i++ )
		{	
			IECoreGL::GroupPtr rayGroup = new IECoreGL::Group();

			Imath::M44f trans;
			trans.rotate( V3f( 0, 0, 2.0 * M_PI / 3.0 * i ) );
			trans.translate( V3f( 0, 0.4, 0.5 ) );
			rayGroup->addChild( const_pointer_cast<IECoreGL::Renderable>( ray() ) );
			rayGroup->setTransform( trans );
			
			result->addChild( rayGroup );
		}
	}

	const Color3f color = parameter<Color3f>( metadataTarget, light, "colorParameter", Color3f( 1.0f ) );
	const float intensity = parameter<float>( metadataTarget, light, "intensityParameter", 1 );
	const float exposure = parameter<float>( metadataTarget, light, "exposureParameter", 0 );

	result->addChild( const_pointer_cast<IECoreGL::Renderable>( colorIndicator( color * intensity * pow( 2.0f, exposure ), indicatorFaceCamera ) ) );

	return result;
}

const char *StandardLightVisualiser::faceCameraVertexSource()
{
	return

		"#version 120\n"
		""
		"#if __VERSION__ <= 120\n"
		"#define in attribute\n"
		"#define out varying\n"
		"#endif\n"
		""
		"uniform int aimType;"
		""
		"uniform vec3 Cs = vec3( 1, 1, 1 );"
		"uniform bool vertexCsActive = false;"
		""
		"in vec3 vertexP;"
		"in vec3 vertexN;"
		"in vec2 vertexst;"
		"in vec3 vertexCs;"
		""
		"out vec3 geometryI;"
		"out vec3 geometryP;"
		"out vec3 geometryN;"
		"out vec2 geometryst;"
		"out vec3 geometryCs;"
		""
		"out vec3 fragmentI;"
		"out vec3 fragmentP;"
		"out vec3 fragmentN;"
		"out vec2 fragmentst;"
		"out vec3 fragmentCs;"
		""
		"void main()"
		"{"
		""
		""
		"	vec3 aimedXAxis, aimedYAxis, aimedZAxis;"
		"	if( aimType == 0 )"
		"	{"
		"		vec4 viewDirectionInObjectSpace = gl_ModelViewMatrixInverse * vec4( 0, 0, -1, 0 );"
		"		aimedYAxis = normalize( cross( viewDirectionInObjectSpace.xyz, vec3( 0, 0, -1 ) ) );"
		"		aimedXAxis = normalize( cross( aimedYAxis, vec3( 0, 0, -1 ) ) );"
		"		aimedZAxis = vec3( 0, 0, 1 );"
		"	}"
		"	else"
		"	{"
		"		aimedXAxis = normalize( gl_ModelViewMatrixInverse * vec4( 0, 0, -1, 0 ) ).xyz;"
		"		aimedYAxis = normalize( gl_ModelViewMatrixInverse * vec4( 0, 1, 0, 0 ) ).xyz;"
		"		aimedZAxis = normalize( gl_ModelViewMatrixInverse * vec4( 1, 0, 0, 0 ) ).xyz;"
		"	}"
		""
		"	vec3 pAimed = vertexP.x * aimedXAxis + vertexP.y * aimedYAxis + vertexP.z * aimedZAxis;"
		""
		"	vec4 pCam = gl_ModelViewMatrix * vec4( pAimed, 1 );"
		"	gl_Position = gl_ProjectionMatrix * pCam;"
		"	geometryP = pCam.xyz;"
		"	geometryN = normalize( gl_NormalMatrix * vertexN );"
		"	if( gl_ProjectionMatrix[2][3] != 0.0 )"
		"	{"
		"		geometryI = normalize( -pCam.xyz );"
		"	}"
		"	else"
		"	{"
		"		geometryI = vec3( 0, 0, -1 );"
		"	}"
		""
		"	geometryst = vertexst;"
		"	geometryCs = mix( Cs, vertexCs, float( vertexCsActive ) );"
		""
		"	fragmentI = geometryI;"
		"	fragmentP = geometryP;"
		"	fragmentN = geometryN;"
		"	fragmentst = geometryst;"
		"	fragmentCs = geometryCs;"
		"}"

	;
}

IECoreGL::ConstRenderablePtr StandardLightVisualiser::ray()
{
	IECoreGL::GroupPtr group = new IECoreGL::Group();
	addWireframeCurveState( group.get() );

	IECore::CompoundObjectPtr parameters = new CompoundObject;
	parameters->members()["aimType"] = new IntData( 0 );
	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCameraVertexSource(), "", Shader::constantFragmentSource(), parameters )
	);

	IntVectorDataPtr vertsPerCurve = new IntVectorData;
	V3fVectorDataPtr p = new V3fVectorData;
	addRay( V2f( 0 ), V2f( 1, 0 ), vertsPerCurve->writable(), p->writable() );

	IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurve );
	curves->addPrimitiveVariable( "P", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Vertex, p ) );
	curves->addPrimitiveVariable( "Cs", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( Color3f( 1.0f, 0.835f, 0.07f ) ) ) );

	group->addChild( curves );

	return group;
}

IECoreGL::ConstRenderablePtr StandardLightVisualiser::pointRays()
{
	IECoreGL::GroupPtr group = new IECoreGL::Group();
	addWireframeCurveState( group.get() );

	IECore::CompoundObjectPtr parameters = new CompoundObject;
	parameters->members()["aimType"] = new IntData( 1 );
	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCameraVertexSource(), "", Shader::constantFragmentSource(), parameters )
	);

	IntVectorDataPtr vertsPerCurve = new IntVectorData;
	V3fVectorDataPtr p = new V3fVectorData;

	const int numRays = 8;
	for( int i = 0; i < numRays; ++i )
	{
		const float angle = M_PI * 2.0f * float(i)/(float)numRays;
		const V2f dir( cos( angle ), sin( angle ) );
		addRay( dir * .5, dir * 1, vertsPerCurve->writable(), p->writable() );
	}

	IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurve );
	curves->addPrimitiveVariable( "P", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Vertex, p ) );
	curves->addPrimitiveVariable( "Cs", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( Color3f( 1.0f, 0.835f, 0.07f ) ) ) );

	group->addChild( curves );

	return group;
}

IECoreGL::ConstRenderablePtr StandardLightVisualiser::spotlightCone( float innerAngle, float outerAngle )
{
	IECoreGL::GroupPtr group = new IECoreGL::Group();
	addWireframeCurveState( group.get() );

	group->getState()->add( new IECoreGL::CurvesPrimitive::GLLineWidth( 1.0f ) );

	IECore::CompoundObjectPtr parameters = new CompoundObject;
	parameters->members()["aimType"] = new IntData( 0 );
	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCameraVertexSource(), "", Shader::constantFragmentSource(), parameters )
	);

	IntVectorDataPtr vertsPerCurve = new IntVectorData;
	V3fVectorDataPtr p = new V3fVectorData;
	addCone( innerAngle, vertsPerCurve->writable(), p->writable() );

	IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurve );
	curves->addPrimitiveVariable( "P", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Vertex, p ) );
	curves->addPrimitiveVariable( "Cs", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( Color3f( 1.0f, 0.835f, 0.07f ) ) ) );

	group->addChild( curves );

	if( fabs( innerAngle - outerAngle ) > 0.1 )
	{
		IECoreGL::GroupPtr outerGroup = new Group;
		outerGroup->getState()->add( new IECoreGL::CurvesPrimitive::GLLineWidth( 0.5f ) );

		IntVectorDataPtr vertsPerCurve = new IntVectorData;
		V3fVectorDataPtr p = new V3fVectorData;
		addCone( outerAngle, vertsPerCurve->writable(), p->writable() );

		IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurve );
		curves->addPrimitiveVariable( "P", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Vertex, p ) );
		curves->addPrimitiveVariable( "Cs", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( Color3f( 1.0f, 0.835f, 0.07f ) ) ) );

		outerGroup->addChild( curves );

		group->addChild( outerGroup );
	}

	return group;
}

IECoreGL::ConstRenderablePtr StandardLightVisualiser::colorIndicator( const Imath::Color3f &color, bool faceCamera )
{

	float maxChannel = std::max( color[0], std::max( color[1], color[2] ) );
	float exposure = 0;
	Imath::Color3f indicatorColor = color;
	if( maxChannel > 1 )
	{
		indicatorColor = color / maxChannel;
		exposure = log( maxChannel ) / log( 2 );
	}
	IECoreGL::GroupPtr group = new IECoreGL::Group();
	IECoreGL::GroupPtr wirelessGroup = new IECoreGL::Group();

	IECore::CompoundObjectPtr parameters = new CompoundObject;
	parameters->members()["aimType"] = new IntData( 1 );
	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCamera ? faceCameraVertexSource() : "", "", Shader::constantFragmentSource(), parameters )
	);

	wirelessGroup->getState()->add( new IECoreGL::Primitive::DrawWireframe( false ) );

	float indicatorRad = 0.3;
	int indicatorAxis = faceCamera ? 0 : 2;

	{
		IntVectorDataPtr vertsPerPoly = new IntVectorData;
		IntVectorDataPtr vertIds = new IntVectorData;
		V3fVectorDataPtr p = new V3fVectorData;

		addSolidArc( indicatorAxis, V3f( 0 ), indicatorRad, indicatorRad * 0.9, 0, 1, vertsPerPoly->writable(), vertIds->writable(), p->writable() );

		IECore::MeshPrimitivePtr mesh = new IECore::MeshPrimitive( vertsPerPoly, vertIds, "linear", p );
		mesh->variables["N"] = IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new V3fData( V3f( 0 ) ) );
		mesh->variables["Cs"] = IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( indicatorColor ) );
		ToGLMeshConverterPtr meshConverter = new ToGLMeshConverter( mesh );
		group->addChild( IECore::runTimeCast<IECoreGL::Renderable>( meshConverter->convert() ) );
	}
	{
		IntVectorDataPtr vertsPerPoly = new IntVectorData;
		IntVectorDataPtr vertIds = new IntVectorData;
		V3fVectorDataPtr p = new V3fVectorData;

		addSolidArc( indicatorAxis, V3f( 0 ), indicatorRad * 0.4, 0.0, 0, 1, vertsPerPoly->writable(), vertIds->writable(), p->writable() );

		for( int i = 0; i < exposure && i < 20; i++ )
		{
			float startAngle = 1 - pow( 0.875, i );
			float endAngle = 1 - pow( 0.875, std::min( i+1.0, (double)exposure ) );
			float maxEndAngle = 1 - pow( 0.875, i+1.0);
			float sectorScale = ( maxEndAngle - startAngle - 0.008 ) / ( maxEndAngle - startAngle );
			addSolidArc( indicatorAxis, V3f( 0 ), indicatorRad * 0.85, indicatorRad * 0.45, startAngle, startAngle + ( endAngle - startAngle ) * sectorScale, vertsPerPoly->writable(), vertIds->writable(), p->writable() );
		}

		IECore::MeshPrimitivePtr mesh = new IECore::MeshPrimitive( vertsPerPoly, vertIds, "linear", p );
		mesh->variables["N"] = IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new V3fData( V3f( 0 ) ) );
		mesh->variables["Cs"] = IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( indicatorColor ) );
		ToGLMeshConverterPtr meshConverter = new ToGLMeshConverter( mesh );
		wirelessGroup->addChild( IECore::runTimeCast<IECoreGL::Renderable>( meshConverter->convert() ) );
	}

	// For exposures greater than 20, draw an additional solid bar of a darker color at the very end, without any segment dividers
	if( exposure > 20 )
	{
		IntVectorDataPtr vertsPerPoly = new IntVectorData;
		IntVectorDataPtr vertIds = new IntVectorData;
		V3fVectorDataPtr p = new V3fVectorData;

		float startAngle = 1 - pow( 0.875, 20 );
		float endAngle = 1 - pow( 0.875, (double)exposure );
		addSolidArc( indicatorAxis, V3f( 0 ), indicatorRad * 0.85, indicatorRad * 0.45, startAngle, endAngle, vertsPerPoly->writable(), vertIds->writable(), p->writable() );

		IECore::MeshPrimitivePtr mesh = new IECore::MeshPrimitive( vertsPerPoly, vertIds, "linear", p );
		mesh->variables["N"] = IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new V3fData( V3f( 0 ) ) );
		mesh->variables["Cs"] = IECore::PrimitiveVariable( IECore::PrimitiveVariable::Constant, new Color3fData( 0.5f * indicatorColor ) );
		ToGLMeshConverterPtr meshConverter = new ToGLMeshConverter( mesh );
		wirelessGroup->addChild( IECore::runTimeCast<IECoreGL::Renderable>( meshConverter->convert() ) );
	}

	group->addChild( wirelessGroup );

	return group;
}
