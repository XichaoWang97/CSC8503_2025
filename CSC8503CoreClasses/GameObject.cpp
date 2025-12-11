#include "GameObject.h"
#include "CollisionDetection.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "NetworkObject.h"

using namespace NCL::CSC8503;

GameObject::GameObject(const std::string& objectName)	
{
	name			= objectName;
	worldID			= -1;
	isActive		= true;
	boundingVolume	= nullptr;
	physicsObject	= nullptr;
	renderObject	= nullptr;
	networkObject	= nullptr;
}

GameObject::~GameObject()	
{
	delete boundingVolume;
	delete physicsObject;
	delete renderObject;
	delete networkObject;
}

bool GameObject::GetBroadphaseAABB(Vector3&outSize) const 
{
	if (!boundingVolume) {
		return false;
	}
	outSize = broadphaseAABB;
	return true;
}

void GameObject::UpdateBroadphaseAABB() 
{
	if (!boundingVolume) {
		return;
	}
	switch (boundingVolume->type)
	{
		case VolumeType::AABB : {
			broadphaseAABB = ((AABBVolume&)*boundingVolume).GetHalfDimensions();
		}break;
		case VolumeType::Sphere: {
			float r = ((SphereVolume&)*boundingVolume).GetRadius();
			broadphaseAABB = Vector3(r, r, r);
		}break;
		case VolumeType::OBB: {
			Matrix3 mat = Quaternion::RotationMatrix<Matrix3>(transform.GetOrientation());
			mat = Matrix::Absolute(mat);
			Vector3 halfSizes = ((OBBVolume&)*boundingVolume).GetHalfDimensions();
			broadphaseAABB = mat * halfSizes;
		}break;
		default: {
			std::cout << "Object " << this->name << " has unsupported bounding volume type for GameObject::UpdateBroadphaseAABB()\n";
		}
	}
}

void GameObject::ResetAfterFall() {
	// check y position
	if (this->GetTransform().GetPosition().y <= -100.0f && this->GetPhysicsObject()->GetInverseMass() != 0.0f) {

		// reset position
		this->GetTransform().SetPosition(this->GetInitPosition());

		// reset physics
		if (this->GetPhysicsObject()) {
			this->GetPhysicsObject()->ClearForces();
			this->GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
			this->GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
		}
	}
}