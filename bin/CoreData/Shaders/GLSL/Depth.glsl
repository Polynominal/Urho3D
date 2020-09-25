#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "AlphaMask.glsl"

varying vec3 vTexCoord;

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    vTexCoord = vec3(GetTexCoord(iTexCoord), GetDepth(gl_Position));
}

void PS()
{
    #ifdef ALPHAMASK
        vec4 diffColor = texture2D(sDiffMap, vTexCoord.xy);
        if (DiscardUsingAlphaMask(diffColor.a))
            discard;
    #endif

    gl_FragColor = vec4(EncodeDepth(vTexCoord.z), 1.0);
}
