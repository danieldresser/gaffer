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

#include "IECoreGL/CurvesPrimitive.h"
#include "IECoreGL/Group.h"
#include "IECoreGL/ShaderStateComponent.h"
#include "IECoreGL/ShaderLoader.h"
#include "IECoreGL/TextureLoader.h"

#include "GafferSceneUI/StandardLightVisualiser.h"

using namespace std;
using namespace boost;
using namespace Imath;
using namespace IECore;
using namespace IECoreGL;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Utility methods. We define these in an anonymouse namespace rather
// than clutter up the header with private methods.
//////////////////////////////////////////////////////////////////////////

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

void addCone( float angle, vector<int> &vertsPerCurve, vector<V3f> &p )
{
	addCircle( V3f( 0, 0, -1 ), sin( M_PI * angle / 180.0 ), vertsPerCurve, p );

	p.push_back( V3f( 0 ) );
	p.push_back( V3f( 0, sin( M_PI * angle / 180.0 ), -1 ) );
	vertsPerCurve.push_back( 2 );

	p.push_back( V3f( 0 ) );
	p.push_back( V3f( 0, -sin( M_PI * angle / 180.0 ), -1 ) );
	vertsPerCurve.push_back( 2 );
}

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
	GroupPtr result = new Group;

	result->addChild( const_pointer_cast<IECoreGL::Renderable>( spotlightCone( 20, 25 ) ) );
	result->addChild( const_pointer_cast<IECoreGL::Renderable>( ray() ) );

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
		"	vec4 viewDirectionInObjectSpace = gl_ModelViewMatrixInverse * vec4( 0, 0, -1, 0 );"
		"	vec3 aimedYAxis = normalize( cross( viewDirectionInObjectSpace.xyz, vec3( 0, 0, -1 ) ) );"
		"	vec3 aimedXAxis = normalize( cross( aimedYAxis, vec3( 0, 0, -1 ) ) );"
		""
		"	vec3 pAimed = vertexP.x * aimedXAxis + vertexP.y * aimedYAxis + vertexP.z * vec3( 0, 0, 1 );"
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

	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCameraVertexSource(), "", Shader::constantFragmentSource(), new CompoundObject )
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

	/// \todo NEED TO MAKE THIS FACE CAMERA FULLY
	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCameraVertexSource(), "", Shader::constantFragmentSource(), new CompoundObject )
	);

	IntVectorDataPtr vertsPerCurve = new IntVectorData;
	V3fVectorDataPtr p = new V3fVectorData;

	const int numRays = 8;
	for( int i = 0; i < numRays; ++i )
	{
		const float angle = M_PI * 2.0f * float(i)/(float)numRays;
		const V2f dir( cos( angle ), sin( angle ) );
		addRay( dir * 1, dir * 2, vertsPerCurve->writable(), p->writable() );
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

	group->getState()->add(
		new IECoreGL::ShaderStateComponent( ShaderLoader::defaultShaderLoader(), TextureLoader::defaultTextureLoader(), faceCameraVertexSource(), "", Shader::constantFragmentSource(), new CompoundObject )
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
