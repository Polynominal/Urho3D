#pragma once

#include "../Scene/Component.h"
#include "../Scene/LogicComponent.h"
#include "../Graphics/Camera.h"
#include "../Scene/Node.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/Viewport.h"
#include "../Container/Vector.h"
#include "../Math/Matrix3.h"
#include "../Math/Vector3.h"

using namespace Urho3D;

namespace Urho3D{
class URHO3D_API DepthOfFieldFocus: public LogicComponent {
	URHO3D_OBJECT(DepthOfFieldFocus, LogicComponent);

public:
	DepthOfFieldFocus(Context* context);
    virtual ~DepthOfFieldFocus(){};

	static void RegisterObject(Context* context);
	virtual void Start();
	void PostUpdate(float timeStep);
	void SetRenderPath(RenderPath* renderpath);

    void SetSmoothValue(float v);
    void SetSmoothFocusTime(float time);
    void SetSmoothFocusEnabled(bool enabled);

    float GetSmoothValue(){return smoothValue_;};
    float GetSmoothFocusTime(){return smoothFocusTime_;};
    float GetSmoothFocusEnabled(){return smoothFocusEnabled_;};

	float smoothValue_;
	float smoothFocusTime_;
    bool smoothFocusEnabled_;

private:
	SharedPtr<Camera> camera;
	SharedPtr<RenderPath> rp;
	SharedPtr<Octree> octree;

	// Get closer hit with scene drawables(from screen center), else return far value
	float GetNearestFocus(float zCameraFarClip);

};

}
