#include "CollisionDetection.h"
#include "CollisionVolume.h"
#include "AABBVolume.h"
#include "OBBVolume.h"
#include "SphereVolume.h"
#include "Window.h"
#include "Maths.h"
#include "Debug.h"
using namespace NCL;

bool CollisionDetection::RayPlaneIntersection(const Ray&r, const Plane&p, RayCollision& collisions) {
	float ln = Vector::Dot(p.GetNormal(), r.GetDirection());

	if (ln == 0.0f) {
		return false; //direction vectors are perpendicular!
	}
	
	Vector3 planePoint = p.GetPointOnPlane();

	Vector3 pointDir = planePoint - r.GetPosition();

	float d = Vector::Dot(pointDir, p.GetNormal()) / ln;

	collisions.collidedAt = r.GetPosition() + (r.GetDirection() * d);

	return true;
}

bool CollisionDetection::RayIntersection(const Ray& r,GameObject& object, RayCollision& collision) {
	bool hasCollided = false;

	const Transform& worldTransform = object.GetTransform();
	const CollisionVolume* volume	= object.GetBoundingVolume();

	if (!volume) {
		return false;
	}

	switch (volume->type) {
		case VolumeType::AABB:		hasCollided = RayAABBIntersection(r, worldTransform, (const AABBVolume&)*volume	, collision); break;
		case VolumeType::OBB:		hasCollided = RayOBBIntersection(r, worldTransform, (const OBBVolume&)*volume	, collision); break;
		case VolumeType::Sphere:	hasCollided = RaySphereIntersection(r, worldTransform, (const SphereVolume&)*volume	, collision); break;

		case VolumeType::Capsule:	hasCollided = RayCapsuleIntersection(r, worldTransform, (const CapsuleVolume&)*volume, collision); break;
	}

	return hasCollided;
}

bool CollisionDetection::RayBoxIntersection(const Ray&r, const Vector3& boxPos, const Vector3& boxSize, RayCollision& collision) {
	Vector3 boxMin = boxPos - boxSize;
	Vector3 boxMax = boxPos + boxSize;
	
	Vector3 rayPos = r.GetPosition();
	Vector3 rayDir = r.GetDirection();
	
	Vector3 tVals(-1, -1, -1);
	
	for (int i = 0; i < 3; ++i) { // get best 3 intersections
		if (rayDir[i] > 0) {
			tVals[i] = (boxMin[i] - rayPos[i]) / rayDir[i];
		}
		else if (rayDir[i] < 0) {
			tVals[i] = (boxMax[i] - rayPos[i]) / rayDir[i];
		}
	}
	float bestT = Vector::GetMaxElement(tVals);
	if (bestT < 0.0f) {
		return false; // no backwards rays !
	}
	Vector3 intersection = rayPos + (rayDir * bestT);
	const float epsilon = 0.0001f; // an amount of leeway in our calcs
	for (int i = 0; i < 3; ++i) {
		if (intersection[i] + epsilon < boxMin[i] ||
			intersection[i] - epsilon > boxMax[i]) {
			return false; // best intersection doesn 抰 touch the box !
		}
	}
	
	collision.collidedAt = intersection;
	collision.rayDistance = bestT;
	return true;
}

bool CollisionDetection::RayAABBIntersection(const Ray&r, const Transform& worldTransform, const AABBVolume& volume, RayCollision& collision) {
	Vector3 boxPos = worldTransform.GetPosition();
	Vector3 boxSize = volume.GetHalfDimensions();
	return RayBoxIntersection(r, boxPos, boxSize, collision);
}

bool CollisionDetection::RayOBBIntersection(const Ray&r, const Transform& worldTransform, const OBBVolume& volume, RayCollision& collision) {
	Quaternion orientation = worldTransform.GetOrientation();
	Vector3 position = worldTransform.GetPosition();
	
	Matrix3 transform = Quaternion::RotationMatrix < Matrix3 >(orientation);
	Matrix3 invTransform = Quaternion::RotationMatrix < Matrix3 >(orientation.Conjugate());
	Vector3 localRayPos = r.GetPosition() - position;
	Ray tempRay(invTransform * localRayPos, invTransform * r.GetDirection());
	bool collided = RayBoxIntersection(tempRay, Vector3(),
	volume.GetHalfDimensions(), collision);
	
	if (collided) {
		collision.collidedAt = transform * collision.collidedAt + position;
	}
	return collided;
}

bool CollisionDetection::RaySphereIntersection(const Ray&r, const Transform& worldTransform, const SphereVolume& volume, RayCollision& collision) {
			Vector3 spherePos = worldTransform.GetPosition();
			float sphereRadius = volume.GetRadius();
			// Get the direction between the ray origin and the sphere origin
			Vector3 dir = (spherePos - r.GetPosition());
			// Then project the sphere 抯 origin onto our ray direction vector
			float sphereProj = Vector::Dot(dir, r.GetDirection());
			if (sphereProj < 0.0f) {
				return false; // point is behind the ray !
			}
			// Get closest point on ray line to sphere
			Vector3 point = r.GetPosition() + (r.GetDirection() * sphereProj);
			
			float sphereDist = Vector::Length(point - spherePos);
			
			if (sphereDist > sphereRadius) {
				return false;
			}
			
			float offset = sqrt((sphereRadius * sphereRadius) - (sphereDist * sphereDist));
			
			collision.rayDistance = sphereProj - (offset);
			collision.collidedAt = r.GetPosition() + (r.GetDirection() * collision.rayDistance);
			return true;
}

bool CollisionDetection::RayCapsuleIntersection(const Ray& r, const Transform& worldTransform, const CapsuleVolume& volume, RayCollision& collision) {
	return false;
}

bool CollisionDetection::ObjectIntersection(GameObject* a, GameObject* b, CollisionInfo& collisionInfo) {
	const CollisionVolume* volA = a->GetBoundingVolume();
	const CollisionVolume* volB = b->GetBoundingVolume();

	if (!volA || !volB) {
		return false;
	}

	collisionInfo.a = a;
	collisionInfo.b = b;

	Transform& transformA = a->GetTransform();
	Transform& transformB = b->GetTransform();

	VolumeType pairType = (VolumeType)((int)volA->type | (int)volB->type);

	//Two AABBs
	if (pairType == VolumeType::AABB) {
		return AABBIntersection((AABBVolume&)*volA, transformA, (AABBVolume&)*volB, transformB, collisionInfo);
	}
	//Two Spheres
	if (pairType == VolumeType::Sphere) {
		return SphereIntersection((SphereVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}
	//Two OBBs
	if (pairType == VolumeType::OBB) {
		return OBBIntersection((OBBVolume&)*volA, transformA, (OBBVolume&)*volB, transformB, collisionInfo);
	}
	//Two Capsules

	//AABB vs Sphere pairs
	if (volA->type == VolumeType::AABB && volB->type == VolumeType::Sphere) {
		return AABBSphereIntersection((AABBVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::Sphere && volB->type == VolumeType::AABB) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return AABBSphereIntersection((AABBVolume&)*volB, transformB, (SphereVolume&)*volA, transformA, collisionInfo);
	}

	//OBB vs sphere pairs
	if (volA->type == VolumeType::OBB && volB->type == VolumeType::Sphere) {
		return OBBSphereIntersection((OBBVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::Sphere && volB->type == VolumeType::OBB) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return OBBSphereIntersection((OBBVolume&)*volB, transformB, (SphereVolume&)*volA, transformA, collisionInfo);
	}

	//Capsule vs other interactions
	if (volA->type == VolumeType::Capsule && volB->type == VolumeType::Sphere) {
		return SphereCapsuleIntersection((CapsuleVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::Sphere && volB->type == VolumeType::Capsule) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return SphereCapsuleIntersection((CapsuleVolume&)*volB, transformB, (SphereVolume&)*volA, transformA, collisionInfo);
	}

	if (volA->type == VolumeType::Capsule && volB->type == VolumeType::AABB) {
		return AABBCapsuleIntersection((CapsuleVolume&)*volA, transformA, (AABBVolume&)*volB, transformB, collisionInfo);
	}
	if (volB->type == VolumeType::Capsule && volA->type == VolumeType::AABB) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return AABBCapsuleIntersection((CapsuleVolume&)*volB, transformB, (AABBVolume&)*volA, transformA, collisionInfo);
	}

	return false;
}

bool CollisionDetection::AABBTest(const Vector3& posA, const Vector3& posB, const Vector3& halfSizeA, const Vector3& halfSizeB) {
	Vector3 delta = posB - posA;
	Vector3 totalSize = halfSizeA + halfSizeB;

	if (abs(delta.x) < totalSize.x &&
		abs(delta.y) < totalSize.y &&
		abs(delta.z) < totalSize.z) {
		return true;
	}
	return false;
}

//AABB/AABB Collisions
bool CollisionDetection::AABBIntersection(const AABBVolume& volumeA, const Transform& worldTransformA,
	const AABBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	Vector3 boxAPos = worldTransformA.GetPosition();
	Vector3 boxBPos = worldTransformB.GetPosition();
	
	Vector3 boxASize = volumeA.GetHalfDimensions();
	Vector3 boxBSize = volumeB.GetHalfDimensions();
	
	bool overlap = AABBTest(boxAPos, boxBPos, boxASize, boxBSize);
	if (overlap) {
		static const Vector3 faces[6] = {Vector3(-1, 0, 0), Vector3(1, 0, 0), Vector3(0, -1, 0), 
			Vector3(0, 1, 0), Vector3(0, 0, -1), Vector3(0, 0, 1),};
		
		Vector3 maxA = boxAPos + boxASize;
		Vector3 minA = boxAPos - boxASize;
		
		Vector3 maxB = boxBPos + boxBSize;
		Vector3 minB = boxBPos - boxBSize;
		
		float distances[6] =
			{(maxB.x - minA.x),// distance of box ’b’ to ’left ’ of ’a ’.
			(maxA.x - minB.x),// distance of box ’b’ to ’right ’ of ’a ’.
			(maxB.y - minA.y),// distance of box ’b’ to ’bottom ’ of ’a ’.
			(maxA.y - minB.y),// distance of box ’b’ to ’top ’ of ’a ’.
			(maxB.z - minA.z),// distance of box ’b’ to ’far ’ of ’a ’.
			(maxA.z - minB.z) // distance of box ’b’ to ’near ’ of ’a ’.
			};
		float penetration = FLT_MAX;
		Vector3 bestAxis;
		
		for (int i = 0; i < 6; i++){
			if (distances[i] < penetration) {
				penetration = distances[i];
				bestAxis = faces[i];
			}
		}
		collisionInfo.AddContactPoint(Vector3(), Vector3(), bestAxis, penetration);
		return true;
	}
	return false;
}

//Sphere / Sphere Collision
bool CollisionDetection::SphereIntersection(const SphereVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	float radii = volumeA.GetRadius() + volumeB.GetRadius();
	Vector3 delta = worldTransformB.GetPosition() - worldTransformA.GetPosition();
	
	float deltaLength = Vector::Length(delta);
	
	if (deltaLength < radii) {
		float penetration = (radii - deltaLength);
		Vector3 normal = Vector::Normalise(delta);
		Vector3 localA = normal * volumeA.GetRadius();
		Vector3 localB = -normal * volumeB.GetRadius();
		
		collisionInfo.AddContactPoint(localA, localB, normal, penetration);
		return true;// we ’re colliding !
	}
	return false;
}

//AABB - Sphere Collision
bool CollisionDetection::AABBSphereIntersection(const AABBVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	Vector3 boxSize = volumeA.GetHalfDimensions();
	
	Vector3 delta = worldTransformB.GetPosition() -
	worldTransformA.GetPosition();
	
	Vector3 closestPointOnBox = Vector::Clamp(delta, -boxSize, boxSize);
	Vector3 localPoint = delta - closestPointOnBox;
	float distance = Vector::Length(localPoint);
	
	if (distance < volumeB.GetRadius()) {// yes , we ’re colliding !
		Vector3 collisionNormal = Vector::Normalise(localPoint);
		float penetration = (volumeB.GetRadius() - distance);
		
		Vector3 localA = Vector3();
		Vector3 localB = -collisionNormal * volumeB.GetRadius();
		
		collisionInfo.AddContactPoint(localA, localB, collisionNormal, penetration);
		return true;
	}
	return false;
}

bool CollisionDetection::OBBSphereIntersection(const OBBVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	// Get OBB information about world transform and orientation
	Quaternion orientation = worldTransformA.GetOrientation();
	Vector3 position = worldTransformA.GetPosition();

	Matrix3 transform = Quaternion::RotationMatrix<Matrix3>(orientation);
	Matrix3 invTransform = Quaternion::RotationMatrix<Matrix3>(orientation.Conjugate());

	// Transform the sphere center into the OBB's local space
	Vector3 localSpherePos = invTransform * (worldTransformB.GetPosition() - position);

	// Perform AABB vs Sphere detection in local space (similar to AABBSphereIntersection)
	Vector3 boxSize = volumeA.GetHalfDimensions();
	// Find the point on the AABB that is closest to the sphere center (Clamp)
	Vector3 closestPointInLocal = Vector::Clamp(localSpherePos, -boxSize, boxSize);

	// calculate the distance from the sphere center to this closest point
	Vector3 localDist = localSpherePos - closestPointInLocal;
	float distance = Vector::Length(localDist);

	// collision check
	if (distance < volumeB.GetRadius()) {
		
		Vector3 collisionNormal = transform * Vector::Normalise(localDist);
		float penetration = (volumeB.GetRadius() - distance);

		Vector3 worldClosestPoint = (transform * closestPointInLocal) + position;

		Vector3 contactPointVector = worldClosestPoint - position; // OBB center to contact point in world space
		Vector3 localA = contactPointVector;
		Vector3 localB = -collisionNormal * volumeB.GetRadius(); // To sphere surface in world space

		collisionInfo.AddContactPoint(localA, localB, collisionNormal, penetration);
		return true;
	}
	return false;
}

bool CollisionDetection::AABBCapsuleIntersection(
	const CapsuleVolume& volumeA, const Transform& worldTransformA,
	const AABBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	return false;
}

bool CollisionDetection::SphereCapsuleIntersection(
	const CapsuleVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	return false;
}

bool CollisionDetection::OBBIntersection(const OBBVolume& volumeA, const Transform& worldTransformA,
	const OBBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	Vector3 centreA = worldTransformA.GetPosition();
	Vector3 centreB = worldTransformB.GetPosition();
	Vector3 halfSizeA = volumeA.GetHalfDimensions();
	Vector3 halfSizeB = volumeB.GetHalfDimensions();

	Quaternion qA = worldTransformA.GetOrientation();
	Quaternion qB = worldTransformB.GetOrientation();

	Matrix3 rotA = Quaternion::RotationMatrix<Matrix3>(qA);
	Matrix3 rotB = Quaternion::RotationMatrix<Matrix3>(qB);

	// OBB 的局部坐标轴（世界空间）
	Vector3 A[3] = {
		rotA * Vector3(1,0,0),
		rotA * Vector3(0,1,0),
		rotA * Vector3(0,0,1)
	};

	Vector3 B[3] = {
		rotB * Vector3(1,0,0),
		rotB * Vector3(0,1,0),
		rotB * Vector3(0,0,1)
	};

	// === 2. 构造旋转矩阵 R = dot(Ai,Bj)，以及 t = B 在 A 局部空间中的中心偏移 ===
	float R[3][3];
	float absR[3][3];

	const float EPSILON = 1e-6f;

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			R[i][j] = Vector::Dot(A[i], B[j]);
			absR[i][j] = std::fabs(R[i][j]) + EPSILON; // 加一点偏移避免数值误差
		}
	}

	Vector3 tWorld = centreB - centreA;
	// t 用 A 的坐标系表示
	Vector3 t(
		Vector::Dot(tWorld, A[0]),
		Vector::Dot(tWorld, A[1]),
		Vector::Dot(tWorld, A[2])
	);

	float minPenetration = FLT_MAX;
	Vector3 bestAxis;     // 碰撞法线（世界空间）

	auto updateBestAxis = [&](const Vector3& axis, float penetration) {
		if (penetration < minPenetration) {
			minPenetration = penetration;
			bestAxis = axis;
		}
		};

	// === 3. 15 个分离轴测试（3 + 3 + 9） ===
	// --- 3.1 A 的三个轴 ---
	for (int i = 0; i < 3; ++i) {
		float ra = halfSizeA[i];
		float rb =
			halfSizeB.x * absR[i][0] +
			halfSizeB.y * absR[i][1] +
			halfSizeB.z * absR[i][2];

		float dist = std::fabs(t[i]);
		if (dist > ra + rb) {
			return false; // 找到分离轴，没碰到
		}

		float penetration = (ra + rb) - dist;

		// 法线要从 A 指向 B：根据 t 在该轴上的符号调整方向
		Vector3 axis = A[i];
		if (Vector::Dot(tWorld, axis) < 0.0f) {
			axis = -axis;
		}
		updateBestAxis(axis, penetration);
	}

	// --- 3.2 B 的三个轴 ---
	for (int j = 0; j < 3; ++j) {
		float rb = halfSizeB[j];
		float ra =
			halfSizeA.x * absR[0][j] +
			halfSizeA.y * absR[1][j] +
			halfSizeA.z * absR[2][j];

		float dist =
			std::fabs(t.x * R[0][j] +
				t.y * R[1][j] +
				t.z * R[2][j]);

		if (dist > ra + rb) {
			return false; // 分离轴
		}

		float penetration = (ra + rb) - dist;

		Vector3 axis = B[j];
		if (Vector::Dot(tWorld, axis) < 0.0f) {
			axis = -axis;
		}
		updateBestAxis(axis, penetration);
	}

	// --- 3.3 交叉轴 Ai x Bj（9 个） ---
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {

			// 交叉轴如果非常小，就说明两轴接近平行，可以跳过
			Vector3 axis = Vector::Cross(A[i], B[j]);
			float axisLenSq = Vector::Dot(axis, axis);
			if (axisLenSq < EPSILON * EPSILON) {
				continue;
			}
			axis = Vector::Normalise(axis);

			float ra =
				halfSizeA[(i + 1) % 3] * absR[(i + 2) % 3][j] +
				halfSizeA[(i + 2) % 3] * absR[(i + 1) % 3][j];

			float rb =
				halfSizeB[(j + 1) % 3] * absR[i][(j + 2) % 3] +
				halfSizeB[(j + 2) % 3] * absR[i][(j + 1) % 3];

			// t 在该交叉轴上的投影长度
			float dist =
				std::fabs(
					t[(i + 2) % 3] * R[(i + 1) % 3][j] -
					t[(i + 1) % 3] * R[(i + 2) % 3][j]);

			if (dist > ra + rb) {
				return false; // 分离轴
			}

			float penetration = (ra + rb) - dist;

			if (Vector::Dot(tWorld, axis) < 0.0f) {
				axis = -axis;
			}
			updateBestAxis(axis, penetration);
		}
	}

	// === 4. 没有分离轴 -> 有碰撞，用最小穿透轴作为法线 ===
	Vector3 collisionNormal = Vector::Normalise(bestAxis);
	float penetration = minPenetration;

	// === 5. 近似计算接触点（用支持函数求两 OBB 在法线方向的最外点） ===
	auto SupportPointOBB = [](const Vector3& centre,
		const Vector3 axes[3],
		const Vector3& halfSize,
		const Vector3& dir) {
			Vector3 result = centre;
			for (int i = 0; i < 3; ++i) {
				float sign = Vector::Dot(dir, axes[i]) > 0.0f ? 1.0f : -1.0f;
				result += axes[i] * (sign * halfSize[i]);
			}
			return result;
	};

	Vector3 pointOnA = SupportPointOBB(centreA, A, halfSizeA, collisionNormal);
	Vector3 pointOnB = SupportPointOBB(centreB, B, halfSizeB, -collisionNormal);

	// 取两个面上点的中点当作接触点（体验上会更稳定一点）
	Vector3 contactWorld = (pointOnA + pointOnB) * 0.5f;

	// localA / localB 是相对各自中心的偏移
	Matrix3 invRotA = NCL::Maths::Matrix::Transpose(rotA);
	Matrix3 invRotB = NCL::Maths::Matrix::Transpose(rotB);

	Vector3 localA = invRotA * (contactWorld - centreA);
	Vector3 localB = invRotB * (contactWorld - centreB);

	collisionInfo.AddContactPoint(localA, localB, collisionNormal, penetration);

	return true;
}

Matrix4 GenerateInverseView(const Camera &c) {
	float pitch = c.GetPitch();
	float yaw	= c.GetYaw();
	Vector3 position = c.GetPosition();

	Matrix4 iview =
		Matrix::Translation(position) *
		Matrix::Rotation(-yaw, Vector3(0, -1, 0)) *
		Matrix::Rotation(-pitch, Vector3(-1, 0, 0));

	return iview;
}

Matrix4 GenerateInverseProjection(float aspect, float fov, float nearPlane, float farPlane) {
	float negDepth = nearPlane - farPlane;

	float invNegDepth = negDepth / (2 * (farPlane * nearPlane));

	Matrix4 m;

	float h = 1.0f / tan(fov*PI_OVER_360);

	m.array[0][0] = aspect / h;
	m.array[1][1] = tan(fov * PI_OVER_360);
	m.array[2][2] = 0.0f;

	m.array[2][3] = invNegDepth;//// +PI_OVER_360;
	m.array[3][2] = -1.0f;
	m.array[3][3] = (0.5f / nearPlane) + (0.5f / farPlane);

	return m;
}

Vector3 CollisionDetection::Unproject(const Vector3& screenPos, const PerspectiveCamera& cam) {
	Vector2i screenSize = Window::GetWindow()->GetScreenSize();

	float aspect = Window::GetWindow()->GetScreenAspect();
	float fov		= cam.GetFieldOfVision();
	float nearPlane = cam.GetNearPlane();
	float farPlane  = cam.GetFarPlane();

	//Create our inverted matrix! Note how that to get a correct inverse matrix,
	//the order of matrices used to form it are inverted, too.
	Matrix4 invVP = GenerateInverseView(cam) * GenerateInverseProjection(aspect, fov, nearPlane, farPlane);

	Matrix4 proj  = cam.BuildProjectionMatrix(aspect);

	//Our mouse position x and y values are in 0 to screen dimensions range,
	//so we need to turn them into the -1 to 1 axis range of clip space.
	//We can do that by dividing the mouse values by the width and height of the
	//screen (giving us a range of 0.0 to 1.0), multiplying by 2 (0.0 to 2.0)
	//and then subtracting 1 (-1.0 to 1.0).
	Vector4 clipSpace = Vector4(
		(screenPos.x / (float)screenSize.x) * 2.0f - 1.0f,
		(screenPos.y / (float)screenSize.y) * 2.0f - 1.0f,
		(screenPos.z),
		1.0f
	);

	//Then, we multiply our clipspace coordinate by our inverted matrix
	Vector4 transformed = invVP * clipSpace;

	//our transformed w coordinate is now the 'inverse' perspective divide, so
	//we can reconstruct the final world space by dividing x,y,and z by w.
	return Vector3(transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w);
}

Ray CollisionDetection::BuildRayFromMouse(const PerspectiveCamera& cam) {
	Vector2 screenMouse = Window::GetMouse()->GetAbsolutePosition();
	Vector2i screenSize	= Window::GetWindow()->GetScreenSize();

	//We remove the y axis mouse position from height as OpenGL is 'upside down',
	//and thinks the bottom left is the origin, instead of the top left!
	Vector3 nearPos = Vector3(screenMouse.x,
		screenSize.y - screenMouse.y,
		-0.99999f
	);

	//We also don't use exactly 1.0 (the normalised 'end' of the far plane) as this
	//causes the unproject function to go a bit weird. 
	Vector3 farPos = Vector3(screenMouse.x,
		screenSize.y - screenMouse.y,
		0.99999f
	);

	Vector3 a = Unproject(nearPos, cam);
	Vector3 b = Unproject(farPos, cam);
	Vector3 c = b - a;

	c = Vector::Normalise(c);

	return Ray(cam.GetPosition(), c);
}

//http://bookofhook.com/mousepick.pdf
Matrix4 CollisionDetection::GenerateInverseProjection(float aspect, float fov, float nearPlane, float farPlane) {
	Matrix4 m;

	float t = tan(fov*PI_OVER_360);

	float neg_depth = nearPlane - farPlane;

	const float h = 1.0f / t;

	float c = (farPlane + nearPlane) / neg_depth;
	float e = -1.0f;
	float d = 2.0f*(nearPlane*farPlane) / neg_depth;

	m.array[0][0] = aspect / h;
	m.array[1][1] = tan(fov * PI_OVER_360);
	m.array[2][2] = 0.0f;

	m.array[2][3] = 1.0f / d;

	m.array[3][2] = 1.0f / e;
	m.array[3][3] = -c / (d * e);

	return m;
}

/*
And here's how we generate an inverse view matrix. It's pretty much
an exact inversion of the BuildViewMatrix function of the Camera class!
*/
Matrix4 CollisionDetection::GenerateInverseView(const Camera &c) {
	float pitch = c.GetPitch();
	float yaw	= c.GetYaw();
	Vector3 position = c.GetPosition();

	Matrix4 iview =
		Matrix::Translation(position) *
		Matrix::Rotation(yaw, Vector3(0, 1, 0)) *
		Matrix::Rotation(pitch, Vector3(1, 0, 0));

	return iview;
}

/*
If you've read through the Deferred Rendering tutorial you should have a pretty
good idea what this function does. It takes a 2D position, such as the mouse
position, and 'unprojects' it, to generate a 3D world space position for it.

Just as we turn a world space position into a clip space position by multiplying
it by the model, view, and projection matrices, we can turn a clip space
position back to a 3D position by multiply it by the INVERSE of the
view projection matrix (the model matrix has already been assumed to have
'transformed' the 2D point). As has been mentioned a few times, inverting a
matrix is not a nice operation, either to understand or code. But! We can cheat
the inversion process again, just like we do when we create a view matrix using
the camera.

So, to form the inverted matrix, we need the aspect and fov used to create the
projection matrix of our scene, and the camera used to form the view matrix.

*/
Vector3	CollisionDetection::UnprojectScreenPosition(Vector3 position, float aspect, float fov, const PerspectiveCamera& c) {
	//Create our inverted matrix! Note how that to get a correct inverse matrix,
	//the order of matrices used to form it are inverted, too.
	Matrix4 invVP = GenerateInverseView(c) * GenerateInverseProjection(aspect, fov, c.GetNearPlane(), c.GetFarPlane());


	Vector2i screenSize = Window::GetWindow()->GetScreenSize();

	//Our mouse position x and y values are in 0 to screen dimensions range,
	//so we need to turn them into the -1 to 1 axis range of clip space.
	//We can do that by dividing the mouse values by the width and height of the
	//screen (giving us a range of 0.0 to 1.0), multiplying by 2 (0.0 to 2.0)
	//and then subtracting 1 (-1.0 to 1.0).
	Vector4 clipSpace = Vector4(
		(position.x / (float)screenSize.x) * 2.0f - 1.0f,
		(position.y / (float)screenSize.y) * 2.0f - 1.0f,
		(position.z) - 1.0f,
		1.0f
	);

	//Then, we multiply our clipspace coordinate by our inverted matrix
	Vector4 transformed = invVP * clipSpace;

	//our transformed w coordinate is now the 'inverse' perspective divide, so
	//we can reconstruct the final world space by dividing x,y,and z by w.
	return Vector3(transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w);
}

/*bool CollisionDetection::OBBAABBIntersection(const OBBVolume& volumeA, const Transform& worldTransformA,
	const AABBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	// Get OBB information about world transform and orientation
	Quaternion orientation = worldTransformA.GetOrientation();
	Vector3 position = worldTransformA.GetPosition();

	Matrix3 transform = Quaternion::RotationMatrix<Matrix3>(orientation);
	Matrix3 invTransform = Quaternion::RotationMatrix<Matrix3>(orientation.Conjugate());

	// Transform the sphere center into the OBB's local space
	Vector3 localSpherePos = invTransform * (worldTransformB.GetPosition() - position);

	// Perform AABB vs Sphere detection in local space (similar to AABBSphereIntersection)
	Vector3 boxSize = volumeA.GetHalfDimensions();
	// Find the point on the AABB that is closest to the sphere center (Clamp)
	Vector3 closestPointInLocal = Vector::Clamp(localSpherePos, -boxSize, boxSize);

	// calculate the distance from the sphere center to this closest point
	Vector3 localDist = localSpherePos - closestPointInLocal;
	float distance = Vector::Length(localDist);

	
	return false;
}*/
