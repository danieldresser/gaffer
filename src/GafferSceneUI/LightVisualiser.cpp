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
#include "GafferSceneUI/StandardLightVisualiser.h"

using namespace std;
using namespace Imath;
using namespace GafferSceneUI;

//////////////////////////////////////////////////////////////////////////
// Internal implementation details
//////////////////////////////////////////////////////////////////////////

namespace
{

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
	else
	{
		static StandardLightVisualiserPtr g_defaultVisualiser = new StandardLightVisualiser();
		return g_defaultVisualiser->visualise( object );
	}
}

void LightVisualiser::registerLightVisualiser( const IECore::InternedString &name, ConstLightVisualiserPtr visualiser )
{
	lightVisualisers()[name] = visualiser;
}
