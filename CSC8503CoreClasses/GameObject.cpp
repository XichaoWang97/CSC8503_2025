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

//  New: Set renderer and physics functions
void GameObject::SetRenderProperties(Rendering::Mesh* mesh, float meshSize, const GameTechMaterial& material, const Vector4& colour) {
	// set render object
	SetRenderObject(new RenderObject(GetTransform(), mesh, material));
	GetRenderObject()->SetColour(colour);
}

void GameObject::SetSpherePhysicsProperties(Vector3 position, float radius, float inverseMass) {
	// set physics volume
	SphereVolume* volume = new SphereVolume(radius);
	SetBoundingVolume(volume);
	// set position
	GetTransform().SetScale(Vector3(radius, radius, radius)).SetPosition(position);
	// set physics object
	PhysicsObject* physicsObj = new PhysicsObject(GetTransform(), GetBoundingVolume());
	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();
	SetPhysicsObject(physicsObj);
}

void GameObject::SetAABBphysicsProperties(Vector3 position, float inverseMass, Vector3 dimensions) {
	// set physics volume
	AABBVolume* volume = new AABBVolume(dimensions);
	SetBoundingVolume(volume);
	// set position
	GetTransform().SetScale(dimensions * 2.0f).SetPosition(position);
	// set physics object
	PhysicsObject* physicsObj = new PhysicsObject(GetTransform(), GetBoundingVolume());
	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitCubeInertia();
	SetPhysicsObject(physicsObj);
}