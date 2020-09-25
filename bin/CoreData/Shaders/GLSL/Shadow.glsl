#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "AlphaMask.glsl"

#ifdef VSM_SHADOW
    varying vec4 vTexCoord;
#else
    varying vec2 vTexCoord;
#endif

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    #ifdef VSM_SHADOW
        vTexCoord = vec4(GetTexCoord(iTexCoord), gl_Position.z, gl_Position.w);
    #else
        vTexCoord = GetTexCoord(iTexCoord);
    #endif
}

void PS()
{
    #ifdef ALPHAMASK
        vec4 diffColor = texture2D(sDiffMap, vTexCoord.xy);
        if (DiscardUsingAlphaMask(diffColor.a))
            discard;
    #endif

    #ifdef VSM_SHADOW
        float depth = vTexCoord.z / vTexCoord.w * 0.5 + 0.5;
        gl_FragColor = vec4(depth, depth * depth, 1.0, 1.0);
    #else
        gl_FragColor = vec4(1.0);
    #endif
}
