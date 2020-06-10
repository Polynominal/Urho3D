
uniform float cAlphaDiscardRange;


bool DiscardUsingAlphaMask(float alpha)
{
    float alphaDiscard = cAlphaDiscardRange;
    if (alphaDiscard == 0.0f){
        //default value. glsl 1.10 and less doesnt support defaults and 0 discard range is useless so... may aswell
        alphaDiscard = 0.5f;
    }
    //diffColor.a) < cAlphaDiscardRange
    if (alpha < alphaDiscard)
    {
        return true;
    }
    return false;
}
