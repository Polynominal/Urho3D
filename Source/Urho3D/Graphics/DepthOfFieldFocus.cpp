#include "../Scene/Component.h"
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Core/Variant.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Scene/LogicComponent.h"
#include "../Graphics/Camera.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"

#include "../Graphics/RenderPath.h"
#include "../Graphics/Viewport.h"
#include "../Container/Vector.h"
#include "../Math/Matrix3.h"
#include "../Math/Vector3.h"

#include "../Graphics/Octree.h"
#include "../Math/Ray.h"

#include "DepthOfFieldFocus.h"

namespace Urho3D{

extern const char* SCENE_CATEGORY;

DepthOfFieldFocus::DepthOfFieldFocus(Context* context) : LogicComponent(context)
{
	SetUpdateEventMask(USE_POSTUPDATE);
}

void DepthOfFieldFocus::RegisterObject(Context* context)
{
	context->RegisterFactory<DepthOfFieldFocus>(SCENE_CATEGORY);
    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
}

void DepthOfFieldFocus::Start()
{
	camera = GetNode()->GetComponent<Camera>();
	octree = GetScene()->GetComponent<Octree>();
	smoothFocusTime_ = 0.5f;
}

void DepthOfFieldFocus::PostUpdate(float timeStep)
{
	if (!rp | !smoothFocusEnabled_) return;

	float targetFocus = GetNearestFocus(camera->GetFarClip());

	smoothValue_ = Lerp(smoothValue_, targetFocus, timeStep * 10.0f / smoothFocusTime_);
	rp->SetShaderParameter("SmoothFocus", Variant(smoothValue_));
}
void DepthOfFieldFocus::SetRenderPath(RenderPath* renderpath)
{
	rp = renderpath;
	rp->SetShaderParameter("SmoothFocusEnabled", Variant(true));

}

float DepthOfFieldFocus::GetNearestFocus(float zCameraFarClip)
{
	if (!octree | !camera) return zCameraFarClip;

	PODVector<RayQueryResult> results;

	Ray ray = camera->GetScreenRay(0.5f, 0.5f);

	RayOctreeQuery query(results, ray, RAY_TRIANGLE, zCameraFarClip, DRAWABLE_GEOMETRY, -1);
	octree->RaycastSingle(query);

	if (results.Size())
	{
		for (unsigned int i = 0; i < results.Size(); i++)
		{
			RayQueryResult& result = results[i];
			Vector3 hitPoint = result.position_;

			float distance = (camera->GetNode()->GetWorldPosition() - hitPoint).Length();

			return distance;
		}
	}

	return zCameraFarClip;
}




// bool SmoothFocus::GetSmoothFocus()
// {
// 	// Variant v = rp->GetShaderParameter("SmoothFocusEnabled");
// 	// return v.GetBool();
//     return smoothFocusEnabled_;
// }

void DepthOfFieldFocus::SetSmoothValue(float v){
    smoothValue_ = v;
}
void DepthOfFieldFocus::SetSmoothFocusTime(float time){
    smoothFocusTime_ = time;
}
void DepthOfFieldFocus::SetSmoothFocusEnabled(bool enabled){
    smoothFocusEnabled_ = enabled;
    rp->SetShaderParameter("SmoothFocusEnabled", Variant(smoothFocusEnabled_));
}
}
