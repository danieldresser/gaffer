//////////////////////////////////////////////////////////////////////////
//  
//  Copyright (c) 2011-2012, Image Engine Design Inc. All rights reserved.
//  Copyright (c) 2011, John Haddon. All rights reserved.
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

#ifndef GAFFER_PARAMETERHANDLER_H
#define GAFFER_PARAMETERHANDLER_H

#include "boost/function.hpp"

#include "IECore/Parameter.h"

#include "Gaffer/Plug.h"

namespace Gaffer
{

IE_CORE_FORWARDDECLARE( ParameterHandler );

/// ParameterHandlers manage a mapping between IECore::Parameter objects
/// and Plugs on a Node.
class ParameterHandler : public IECore::RefCounted
{

	public :

		IE_CORE_DECLAREMEMBERPTR( ParameterHandler );

		virtual ~ParameterHandler();
		
		virtual IECore::ParameterPtr parameter() = 0;
		virtual IECore::ConstParameterPtr parameter() const = 0;

		virtual void restore( GraphComponent *plugParent ) = 0;
		virtual Gaffer::PlugPtr setupPlug( GraphComponent *plugParent, Plug::Direction direction=Plug::In ) = 0;

		virtual Gaffer::PlugPtr plug() = 0;
		virtual Gaffer::ConstPlugPtr plug() const = 0;
		
		virtual void setParameterValue() = 0;
		virtual void setPlugValue() = 0;
		
		/// Returns a handler for the specified parameter.
		static ParameterHandlerPtr create( IECore::ParameterPtr parameter );
		/// A function for creating ParameterHandlers which will represent a Parameter with a plug on a given
		/// parent.
		typedef boost::function<ParameterHandlerPtr ( IECore::ParameterPtr )> Creator;	
		/// Registers a function which can return a ParameterHandler for a given Parameter type.
		static void registerParameterHandler( IECore::TypeId parameterType, Creator creator );
		
	protected :
		
		ParameterHandler();
		
		/// Should be called by derived classes in setupPlug().
		void setupPlugFlags( Plug *plug );
		
		/// Create a static instance of this to automatically register a derived class
		/// with the factory mechanism. Derived class must have a constructor of the form
		/// Derived( ParameterType::Ptr parameter, GraphComponentPtr plugParent ).
		template<typename HandlerType, typename ParameterType>
		struct ParameterHandlerDescription
		{
				ParameterHandlerDescription() { ParameterHandler::registerParameterHandler( ParameterType::staticTypeId(), &creator ); };
			private :
				static ParameterHandlerPtr creator( IECore::ParameterPtr parameter ) { return new HandlerType( IECore::staticPointerCast<ParameterType>( parameter ) ); };
		};
		
	private :
	
		typedef std::map<IECore::TypeId, Creator> CreatorMap;
		static CreatorMap &creators();
		
};

} // namespace Gaffer

#endif // GAFFER_PARAMETERHANDLER_H
