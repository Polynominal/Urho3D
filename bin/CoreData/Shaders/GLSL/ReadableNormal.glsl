#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "ScreenPos.glsl"
#include "Fog.glsl"
#include "AlphaMask.glsl"

varying vec2 vTexCoord;
varying vec4 vWorldPos;
#ifdef VERTEXCOLOR
    varying vec4 vColor;
#endif
varying vec3 vNormal;

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    vNormal = GetWorldNormal(modelMatrix);

    gl_Position = GetClipPos(worldPos);
    vTexCoord = GetTexCoord(iTexCoord);
    vWorldPos = vec4(worldPos, GetDepth(gl_Position));

    #ifdef VERTEXCOLOR
        vColor = iColor;
    #endif

}

void PS()
{
    // Get material diffuse albedo
    #ifdef DIFFMAP
        vec4 diffColor = cMatDiffColor * texture2D(sDiffMap, vTexCoord);
        #ifdef ALPHAMAP
            vec4 stencil = texture2D(sEnvMap, vTexCoord);
            diffColor.a = cMatDiffColor.a*stencil.a;
        #endif
        #ifdef ALPHAMASK
            if (DiscardUsingAlphaMask(diffColor.a))
                discard;
        #endif
        gl_FragColor = vec4(0,0,0,diffColor.a);
    #else
        gl_FragColor = vec4(vNormal * 0.5 + 0.5, 1);
    #endif



}
