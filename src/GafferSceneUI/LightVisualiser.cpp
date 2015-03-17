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

#include "boost/container/flat_map.hpp"

#include "IECoreGL/CurvesPrimitive.h"
#include "IECoreGL/Group.h"

#include "GafferSceneUI/LightVisualiser.h"

using namespace std;
using namespace Imath;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Internal implementation details
//////////////////////////////////////////////////////////////////////////

namespace
{

IECoreGL::ConstRenderablePtr fallbackVisualisation()
{
	static IECoreGL::GroupPtr group = NULL;
	if( !group )
	{
		group = new IECoreGL::Group;

		group->getState()->add( new IECoreGL::Primitive::DrawWireframe( true ) );
		group->getState()->add( new IECoreGL::Primitive::DrawSolid( false ) );
		group->getState()->add( new IECoreGL::CurvesPrimitive::UseGLLines( true ) );
		group->getState()->add( new IECoreGL::WireframeColorStateComponent( Color4f( 0.5, 0, 0, 1 ) ) );

		const float a = 0.5f;
		const float phi = 1.0f + sqrt( 5.0f ) / 2.0f;
		const float b = 1.0f / ( 2.0f * phi );

		// icosahedron points
		IECore::V3fVectorDataPtr pData = new IECore::V3fVectorData;
		vector<V3f> &p = pData->writable();
		p.resize( 24 );
		p[0] = V3f( 0, b, -a );
		p[2] = V3f( b, a, 0 );
		p[4] = V3f( -b, a, 0 );
		p[6] = V3f( 0, b, a );
		p[8] = V3f( 0, -b, a );
		p[10] = V3f( -a, 0, b );
		p[12] = V3f( 0, -b, -a );
		p[14] = V3f( a, 0, -b );
		p[16] = V3f( a, 0, b );
		p[18] = V3f( -a, 0, -b );
		p[20] = V3f( b, -a, 0 );
		p[22] = V3f( -b, -a, 0 );

		for( size_t i = 0; i<12; i++ )
		{
			p[i*2] = 2.0f * p[i*2].normalized();
			p[i*2+1] = V3f( 0 );
		}

		IECore::IntVectorDataPtr vertsPerCurve = new IECore::IntVectorData;
		vertsPerCurve->writable().resize( 12, 2 );

		IECoreGL::CurvesPrimitivePtr curves = new IECoreGL::CurvesPrimitive( IECore::CubicBasisf::linear(), false, vertsPerCurve );
		curves->addPrimitiveVariable( "P", IECore::PrimitiveVariable( IECore::PrimitiveVariable::Vertex, pData ) );

		group->addChild( curves );
	}

	return group;
}

typedef boost::container::flat_map<IECore::InternedString, ConstLightVisualiserPtr> LightVisualisers;
LightVisualisers &lightVisualisers()
{
	static LightVisualisers l;
	return l;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// LightVisualiser class
//////////////////////////////////////////////////////////////////////////

Visualiser::VisualiserDescription<LightVisualiser> LightVisualiser::g_visualiserDescription;

LightVisualiser::LightVisualiser()
{
}

LightVisualiser::~LightVisualiser()
{
}

IECoreGL::ConstRenderablePtr LightVisualiser::visualise( const IECore::Object *object ) const
{
	const IECore::Light *light = IECore::runTimeCast<const IECore::Light>( object );
	if( !light )
	{
		return NULL;
	}

	const LightVisualisers &l = lightVisualisers();
	LightVisualisers::const_iterator it = l.find( light->getName() );
	if( it != l.end() )
	{
		return it->second->visualise( object );
	}

	return fallbackVisualisation();
}

void LightVisualiser::registerLightVisualiser( const IECore::InternedString &name, ConstLightVisualiserPtr visualiser )
{
	lightVisualisers()[name] = visualiser;
}
