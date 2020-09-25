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

#pragma once


#include "../Scene/Component.h"
#include "../Graphics/Camera.h"
#include "../Scene/Node.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/Viewport.h"
#include "../Container/Vector.h"
#include "../Math/Matrix3.h"
#include "../Math/Vector3.h"

using namespace Urho3D;

namespace Urho3D {
class Skybox;
class StringHash;
class Texture2D;
class TextureCube;


class URHO3D_API ProceduralSky: public Component {
  URHO3D_OBJECT(ProceduralSky, Component);

public:
  ProceduralSky(Context* context);
  virtual ~ProceduralSky();
  static void RegisterObject(Context* context);
  void OnNodeSet(Node* node);

  /// Automatic update renders according to update interval. If Manual, user calls Update() to render.
  void SetUpdateAuto(bool updateAuto);
  /// Set the rendering interval (default 0).
  void SetUpdateInterval(float interval) { updateInterval_ = interval; }
  /// Set size of Skybox TextureCube.
  bool SetRenderSize(unsigned size);
  /// Queue render of next frame.
  void Update();

  bool GetUpdateAuto() const { return updateAuto_; }
  float GetUpdateInterval() const { return updateInterval_; }
  float GetUpdateWait() const { return updateWait_; }
  unsigned GetRenderSize() const { return renderSize_; }
  bool Bind(RenderPath* rPath, Camera* camera);



  void SetAbsorptionProfile(Vector3 absorptionProfile)           {absorptionProfile_        = absorptionProfile;       if(rPath_){rPath_->SetShaderParameter("AbsorptionProfile", absorptionProfile_);}             };
  void SetRayleighBrightness(float rayleighBrightness)           {rayleighBrightness_       = rayleighBrightness;      if(rPath_){rPath_->SetShaderParameter("RayleighBrightness", rayleighBrightness_);}           };
  void SetMieBrightness(float mieBrightness)                     {mieBrightness_            = mieBrightness;           if(rPath_){rPath_->SetShaderParameter("MieBrightness", mieBrightness_);}                     };
  void SetSpotBrightness(float spotBrightness)                   {spotBrightness_           = spotBrightness;          if(rPath_){rPath_->SetShaderParameter("SpotBrightness", spotBrightness_);}                   };
  void SetScatterStrength(float scatterStrength)                 {scatterStrength_          = scatterStrength;         if(rPath_){rPath_->SetShaderParameter("ScatterStrength", scatterStrength_);}                 };
  void SetRayleighStrength(float rayleighStrength)               {rayleighStrength_         = rayleighStrength;        if(rPath_){rPath_->SetShaderParameter("RayleighStrength", rayleighStrength_);}               };
  void SetMieStrength(float mieStrength)                         {mieStrength_              = mieStrength;             if(rPath_){rPath_->SetShaderParameter("MieStrength", mieStrength_);}                         };
  void SetRayleighCollectionPower(float rayleighCollectionPower) {rayleighCollectionPower_  = rayleighCollectionPower; if(rPath_){rPath_->SetShaderParameter("RayleighCollectionPower", rayleighCollectionPower_);} };
  void SetMieCollectionPower(float mieCollectionPower)           {mieCollectionPower_       = mieCollectionPower;      if(rPath_){rPath_->SetShaderParameter("MieCollectionPower", mieCollectionPower_);}           };
  void SetMieDistribution(float mieDistribution)                 {mieDistribution_          = mieDistribution;         if(rPath_){rPath_->SetShaderParameter("MieDistribution", mieDistribution_);}                 };
  void ApplyAllShaderVariables();

  Vector3 GetAbsorptionProfile()             {return absorptionProfile_;                           }; // Absorption profile of air.
  float GetRayleighBrightness()              {return rayleighBrightness_;                          };
  float GetMieBrightness()                   {return mieBrightness_;                               };
  float GetSpotBrightness()                  {return spotBrightness_;                              };
  float GetScatterStrength()                 {return scatterStrength_;                             };
  float GetRayleighStrength()                {return rayleighStrength_;                            };
  float GetMieStrength()                     {return mieStrength_;                                 };
  float GetRayleighCollectionPower()         {return rayleighCollectionPower_;                     };
  float GetMieCollectionPower()              {return mieCollectionPower_;                          };
  float GetMieDistribution()                 {return mieDistribution_;                             };


protected:
  void HandleUpdate(StringHash eventType, VariantMap& eventData);
  /// Set rendering of next frame active/inactive.
  void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData);

  void DumpTexCubeImages(TextureCube* texCube, const String& filePathPrefix);
  void DumpTexture(Texture2D* texture, const String& filePath);

protected:

  /// Camera used for face projections.
  Camera* cam_;
  /// Urho3D Skybox with geometry and main TextureCube.
  SharedPtr<Skybox> skybox_;
  /// Node used for light direction.
  WeakPtr<Node> lightNode_;
  SharedPtr<RenderPath> rPath_;
  /// Render size of each face.
  unsigned renderSize_;
  /// Fixed rotations for each cube face.
  Matrix3 faceRotations_[MAX_CUBEMAP_FACES];

  bool updateAuto_;
  float updateInterval_;
  float updateWait_;
  bool renderQueued_;

public:
  /// Atmospheric parameters.
  Vector3 absorptionProfile_; // Absorption profile of air.
  Urho3D::Matrix4 InvProj_{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f};
  float rayleighBrightness_;
  float mieBrightness_;
  float spotBrightness_;
  float scatterStrength_;
  float rayleighStrength_;
  float mieStrength_;
  float rayleighCollectionPower_;
  float mieCollectionPower_;
  float mieDistribution_;
};

}
