#
# Copyright (c) 2008-2019 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Set project name
project (Urho3D-Tools)

setup_lint ()

# Find Urho3D library
find_package (Urho3D REQUIRED)
include_directories (${URHO3D_INCLUDE_DIRS})

# Urho3DPlayer target is rather special, although it is here, it is not a tool because it is built into target platform binary as opposed to host platform
# In order to test the player on Web platform built using Emscripten, the request parameters need to be parsed into Module['argument'] (see https://github.com/kripken/emscripten/blob/master/src/emrun_prejs.js for inspiration)
# The CTest could "run" the Urho3DPlayer.html with script name as parameter, thanks to emrun performing this parsing automatically
# In a production environment, however, the request parameters parsing has to be done by the application itself one way or another
if (URHO3D_ANGELSCRIPT OR URHO3D_LUA)
    # Add Urho3DPlayer as target only when one of the supported scripting subsystems is enabled
    add_subdirectory (Urho3DPlayer)
endif ()


if (URHO3D_TOOLS)
    # Urho3D tools
    add_subdirectory (AssetImporter)
    add_subdirectory (OgreImporter)
    add_subdirectory (RampGenerator)
    add_subdirectory (SpritePacker)
elseif (NOT CMAKE_CROSSCOMPILING AND URHO3D_PACKAGING)
endif ()
