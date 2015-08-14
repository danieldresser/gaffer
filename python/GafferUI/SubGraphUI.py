##########################################################################
#
#  Copyright (c) 2015, Image Engine Design Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#      * Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      * Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials provided with
#        the distribution.
#
#      * Neither the name of John Haddon nor the names of
#        any other contributors to this software may be used to endorse or
#        promote products derived from this software without specific prior
#        written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import Gaffer
import GafferUI

Gaffer.Metadata.registerNode(

	Gaffer.SubGraph,

	"description",
	"""
	Holds a nested node graph of its own.
	""",

)

def __correspondingInternalPlug( plug ) :

	node = plug.node()
	if plug.direction() == plug.Direction.In :
		for output in plug.outputs() :
			if node.isAncestorOf( output.node() ) :
				return output
	elif plug.direction() == plug.Direction.Out :
		input = plug.getInput()
		if input is not None and node.isAncestorOf( input.node() ) :
			return input

	return None

def __plugValueWidgetCreator( plug ) :

	# When a plug has been promoted, we get the widget that would
	# have been used to represent the internal plug, and then
	# call setPlug() with the external plug. This allows us to
	# transfer custom uis from inside the node to outside the node.
	#
	# But If the types don't match, we can't expect the
	# UI for the internal plug to work with the external
	# plug. Typically the types will match, because the
	# external plug was created by Box::promotePlug(), but
	# it's possible to use scripting to connect different
	# types, for instance to drive an internal IntPlug with
	# an external BoolPlug. In this case we make no attempt
	# to transfer the internal UI.
	#
	# \todo A better solution may be to mandate the use of Metadata to
	# choose a widget type for each plug, and then just make sure we
	# pass on the internal Metadata.
	internalPlug = __correspondingInternalPlug( plug )
	if type( internalPlug ) is type( plug ) :
		widget = GafferUI.PlugValueWidget.create( internalPlug )
		if widget is not None :
			widget.setPlug( plug )
		return widget

	return GafferUI.PlugValueWidget.create( plug, useTypeOnly=True )

GafferUI.PlugValueWidget.registerCreator( Gaffer.SubGraph, "*" , __plugValueWidgetCreator )
