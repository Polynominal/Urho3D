/**
  @class ProceduralSky
  @brief Procedural Sky component for Urho3D
  @author carnalis <carnalis.j@gmail.com>
  @license MIT License
  @copyright
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "ProceduralSky.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Core/Variant.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Graphics/Material.h"
#include "../Graphics/Model.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/Skybox.h"
#include "../Graphics/Technique.h"
#include "../Graphics/Texture.h"
#include "../Graphics/TextureCube.h"
#include "../Graphics/View.h"
#include "../Graphics/Viewport.h"
#include "../Input/Input.h"
#include "../Input/InputEvents.h"
#include "../Math/Matrix4.h"
#include "../Math/Vector2.h"
#include "../Math/Vector3.h"
#include "../Math/Vector4.h"
#include "../Resource/ResourceCache.h"
#include "../IO/Log.h"
#include "../Scene/Scene.h"


#include "../Graphics/Texture2D.h"
#include "../IO/FileSystem.h"
#include "../Resource/Image.h"


namespace Urho3D{

extern const char* SCENE_CATEGORY;

ProceduralSky::ProceduralSky(Context* context):
  Component(context)
  , renderSize_(256)
  , updateAuto_(false)
  , updateInterval_(0.0f)
  , updateWait_(0)
  , cam_(NULL)
  , absorptionProfile_(Vector3(0.18867780436772762, 0.4978442963618773, 0.6616065586417131))
  , rayleighBrightness_(3.3f)
  , mieBrightness_(0.1f)
  , spotBrightness_(50.0f)
  , scatterStrength_(0.028f)
  , rayleighStrength_(0.139f)
  , mieStrength_(0.264f)
  , rayleighCollectionPower_(0.81f)
  , mieCollectionPower_(0.39f)
  , mieDistribution_(0.63f)
{
  faceRotations_[FACE_POSITIVE_X] = Matrix3(0,0,1,  0,1,0, -1,0,0);
  faceRotations_[FACE_NEGATIVE_X] = Matrix3(0,0,-1, 0,1,0,  1,0,0);
  faceRotations_[FACE_POSITIVE_Y] = Matrix3(1,0,0,  0,0,1,  0,-1,0);
  faceRotations_[FACE_NEGATIVE_Y] = Matrix3(1,0,0,  0,0,-1, 0,1,0);
  faceRotations_[FACE_POSITIVE_Z] = Matrix3(1,0,0,  0,1,0,  0,0,1);
  faceRotations_[FACE_NEGATIVE_Z] = Matrix3(-1,0,0, 0,1,0,  0,0,-1);


}
ProceduralSky::~ProceduralSky() {}

void ProceduralSky::RegisterObject(Context* context) {
  context->RegisterFactory<ProceduralSky>(SCENE_CATEGORY);
  URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);

}

void ProceduralSky::OnNodeSet(Node* node) {
  if (!node) return;
}

void ProceduralSky::ApplyAllShaderVariables()
{
    rPath_->SetShaderParameter("AbsorptionProfile", absorptionProfile_);
    rPath_->SetShaderParameter("RayleighBrightness", rayleighBrightness_);
    rPath_->SetShaderParameter("MieBrightness", mieBrightness_);
    rPath_->SetShaderParameter("SpotBrightness", spotBrightness_);
    rPath_->SetShaderParameter("ScatterStrength", scatterStrength_);
    rPath_->SetShaderParameter("RayleighStrength", rayleighStrength_);
    rPath_->SetShaderParameter("MieStrength", mieStrength_);
    rPath_->SetShaderParameter("RayleighCollectionPower", rayleighCollectionPower_);
    rPath_->SetShaderParameter("MieCollectionPower", mieCollectionPower_);
    rPath_->SetShaderParameter("MieDistribution", mieDistribution_);
}


bool ProceduralSky::Bind(RenderPath* rPath, Camera* camera)
{
    URHO3D_LOGDEBUG("Binding ProceduralSky");
    rPath_ = rPath;
    cam_ = camera;

    ResourceCache* cache(GetSubsystem<ResourceCache>());
    if (!lightNode_) {
        lightNode_ = node_->GetChild("ProceduralSkyLight");
        if (!lightNode_) {
            URHO3D_LOGDEBUG("Creating node 'ProceduralSkyLight' with directional light.");
            lightNode_ = node_->CreateChild("ProceduralSkyLight");
            Light* light(lightNode_->CreateComponent<Light>());
            light->SetLightType(LIGHT_DIRECTIONAL);
            Color lightColor;
            lightColor.FromHSV(57.0f, 9.9f, 75.3f);
            light->SetColor(lightColor);
        }
    }

    RenderPathCommand* cmd = rPath->GetCommand("ProceduralSky");

    if (cmd == nullptr){
        URHO3D_LOGDEBUG("adding ProceduralSky command to render path");

        for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i) {
            RenderPathCommand newCommand;
            newCommand.tag_ = "ProceduralSky";
            newCommand.type_ = CMD_QUAD;
            newCommand.sortMode_ = SORT_BACKTOFRONT;
            newCommand.pass_ = "base";
            newCommand.outputs_.Push(MakePair(String("DiffProceduralSky"), (CubeMapFace)i));
            newCommand.textureNames_[0] = "";
            newCommand.vertexShaderName_ = "ProceduralSky";
            newCommand.vertexShaderDefines_ = "";
            newCommand.pixelShaderName_ = "ProceduralSky";
            newCommand.pixelShaderDefines_ = "";
            newCommand.enabled_ = true;

            VariantMap atmoParams;
            atmoParams["AbsorptionProfile"] = absorptionProfile_;
            atmoParams["RayleighBrightness"] = rayleighBrightness_;
            atmoParams["MieBrightness"] = mieBrightness_;
            atmoParams["SpotBrightness"] = spotBrightness_;
            atmoParams["ScatterStrength"] = scatterStrength_;
            atmoParams["RayleighStrength"] = rayleighStrength_;
            atmoParams["MieStrength"] = mieStrength_;
            atmoParams["RayleighCollectionPower"] = rayleighCollectionPower_;
            atmoParams["MieCollectionPower"] = mieCollectionPower_;
            atmoParams["MieDistribution"] = mieDistribution_;
            atmoParams["LightDir"] = Vector3::DOWN;
            atmoParams["InvProj"] = InvProj_;
            newCommand.shaderParameters_ = atmoParams;
            newCommand.shaderParameters_["InvViewRot"] = faceRotations_[i];

            rPath_->AddCommand(newCommand);
        }
    }else{
        URHO3D_LOGDEBUG("ProceduralSky found in render path");
    }



    skybox_ = node_->GetComponent<Skybox>();
    if (!skybox_)
    {
        skybox_ = node_->CreateComponent<Skybox>();
    }

    Model* model(cache->GetResource<Model>("Models/Box.mdl"));
    skybox_->SetModel(model);
    SharedPtr<Material> skyboxMat(new Material(context_));
    skyboxMat->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffSkybox.xml"));
    skyboxMat->SetCullMode(CULL_NONE);
    skybox_->SetMaterial(skyboxMat);
    SetRenderSize(renderSize_);

    Update();
    ApplyAllShaderVariables();

    return true;
}

void ProceduralSky::HandleUpdate(StringHash eventType, VariantMap& eventData) {
  if (updateAuto_) {
    float dt(eventData[Update::P_TIMESTEP].GetFloat());
    // If using an interval, queue update when done waiting.
    if (updateInterval_ > 0) {
      updateWait_ -= dt;
      if (updateWait_ <= 0) {
        updateWait_ = updateInterval_;
        Update();
      }
    } else { // No interval; just update.
      Update();
    }
  }

}

// Update shader parameters and queue a render.
void ProceduralSky::Update() {
  if (lightNode_) {
    // In the shader code, LightDir is the direction TO the object casting light, not the direction OF the light, so invert the direction.
    Vector3 lightDir(-lightNode_->GetWorldDirection());
    rPath_->SetShaderParameter("LightDir", lightDir);
  }
}

bool ProceduralSky::SetRenderSize(unsigned size, unsigned multisample) {
  if (size >= 1) {
    // Create a TextureCube and assign to the ProceduralSky material.
    SharedPtr<TextureCube> skyboxTexCube(new TextureCube(context_));
    skyboxTexCube->SetName("DiffProceduralSky");
    skyboxTexCube->SetSize(size, Graphics::GetRGBAFormat(), TEXTURE_RENDERTARGET, multisample);
    skyboxTexCube->SetFilterMode(FILTER_ANISOTROPIC);
    skyboxTexCube->SetAddressMode(COORD_U, ADDRESS_CLAMP);
    skyboxTexCube->SetAddressMode(COORD_V, ADDRESS_CLAMP);
    skyboxTexCube->SetAddressMode(COORD_W, ADDRESS_CLAMP);
    GetSubsystem<ResourceCache>()->AddManualResource(skyboxTexCube);

    skybox_->GetMaterial()->SetTexture(TU_DIFFUSE, skyboxTexCube);
    renderSize_ = size;
    return true;
  } else {
    URHO3D_LOGWARNING("ProceduralSky::SetSize (" + String(size) + ") ignored; requires size >= 1.");
  }
  return false;
}

void ProceduralSky::SetUpdateAuto(bool updateAuto) {
  if (updateAuto_ == updateAuto) return;
  updateAuto_ = updateAuto;
  if (updateAuto)
  {
      SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(ProceduralSky, HandleUpdate));
  }
  else{
      UnsubscribeFromEvent(E_UPDATE);
  }
}


void ProceduralSky::DumpTexCubeImages(TextureCube* texCube, const String& pathName) {
  URHO3D_LOGINFO("Save TextureCube: " + pathName + "ProceduralSky_[0-5].png");
  for (unsigned i(0); i < MAX_CUBEMAP_FACES; ++i) {
    Texture2D* faceTex(static_cast<Texture2D*>(texCube->GetRenderSurface((CubeMapFace)i)->GetParentTexture()));
    SharedPtr<Image> faceImage(new Image(context_));
    faceImage->SetSize(faceTex->GetWidth(), faceTex->GetHeight(), faceTex->GetComponents());
    FileSystem* fs(GetSubsystem<FileSystem>());
    fs->CreateDir(pathName);
    String filePath(pathName + "ProceduralSky_" + String(i) + ".png");
    if (!texCube->GetData((CubeMapFace)i, 0, faceImage->GetData())) {
      URHO3D_LOGERROR("...failed GetData() for face " + filePath);
    } else {
      faceImage->SavePNG(filePath);
    }
  }
}
void ProceduralSky::DumpTexture(Texture2D* texture, const String& filePath) {
  URHO3D_LOGINFO("Save texture: " + filePath);
  SharedPtr<Image> image(new Image(context_));
  image->SetSize(texture->GetWidth(), texture->GetHeight(), texture->GetComponents());

  if (!texture->GetData(0, image->GetData())) {
    URHO3D_LOGERROR("...failed GetData().");
  } else {
    image->SavePNG(filePath);
  }
}

}
