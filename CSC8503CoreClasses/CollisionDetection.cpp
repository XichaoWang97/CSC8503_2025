#include "CollisionDetection.h"
#include "CollisionVolume.h"
#include "AABBVolume.h"
#include "OBBVolume.h"
#include "SphereVolume.h"
#include "Window.h"
#include "Maths.h"
#include "Debug.h"
#include <algorithm>
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
			return false; // best intersection doesn ’t touch the box !
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
			// Then project the sphere ’s origin onto our ray direction vector
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
	// New: AABB vs OBB pairs
	// treat AABB as a special case of OBB with no rotation
	if (volA->type == VolumeType::AABB && volB->type == VolumeType::OBB) {
		OBBVolume tempOBB(((AABBVolume*)volA)->GetHalfDimensions());
		return OBBIntersection(tempOBB, transformA, (OBBVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::OBB && volB->type == VolumeType::AABB) {
		OBBVolume tempOBB(((AABBVolume*)volB)->GetHalfDimensions());
		collisionInfo.a = b;
		collisionInfo.b = a;
		return OBBIntersection(tempOBB, transformB, (OBBVolume&)*volA, transformA, collisionInfo);
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
			{(maxB.x - minA.x),
			(maxA.x - minB.x),
			(maxB.y - minA.y),
			(maxA.y - minB.y),
			(maxB.z - minA.z),
			(maxA.z - minB.z)
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

// Sphere / Sphere Collision
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
		return true;// we're colliding
	}
	return false;
}

// AABB - Sphere Collision
bool CollisionDetection::AABBSphereIntersection(const AABBVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	Vector3 boxSize = volumeA.GetHalfDimensions();
	
	Vector3 delta = worldTransformB.GetPosition() -
	worldTransformA.GetPosition();
	
	Vector3 closestPointOnBox = Vector::Clamp(delta, -boxSize, boxSize);
	Vector3 localPoint = delta - closestPointOnBox;
	float distance = Vector::Length(localPoint);
	
	if (distance < volumeB.GetRadius()) {
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

	const float EPSILON = 1e-6f;
	auto Clamp = [&](float v, float a, float b)->float { if (v < a) return a; if (v > b) return b; return v; };

	// Centers
	Vector3 cA = worldTransformA.GetPosition();
	Vector3 cB = worldTransformB.GetPosition();

	// Half-extents
	Vector3 a = volumeA.GetHalfDimensions();
	Vector3 b = volumeB.GetHalfDimensions();

	// Orientation -> world axes
	Quaternion qa = worldTransformA.GetOrientation();
	Quaternion qb = worldTransformB.GetOrientation();
	Matrix3 RA = Quaternion::RotationMatrix<Matrix3>(qa);
	Matrix3 RB = Quaternion::RotationMatrix<Matrix3>(qb);

	Vector3 Aaxes[3] = { RA * Vector3(1,0,0), RA * Vector3(0,1,0), RA * Vector3(0,0,1) };
	Vector3 Baxes[3] = { RB * Vector3(1,0,0), RB * Vector3(0,1,0), RB * Vector3(0,0,1) };

	// R matrix and absolute
	float R[3][3], absR[3][3];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			R[i][j] = Vector::Dot(Aaxes[i], Baxes[j]);
			absR[i][j] = fabs(R[i][j]) + EPSILON;
		}
	}

	// translation vector t expressed in A's frame
	Vector3 tWorld = cB - cA;
	float t[3] = { Vector::Dot(tWorld, Aaxes[0]), Vector::Dot(tWorld, Aaxes[1]), Vector::Dot(tWorld, Aaxes[2]) };

	// SAT tests, track minimum overlap and best axis
	float minOverlap = FLT_MAX;
	Vector3 bestAxis(0, 0, 0);
	bool bestIsAxisA = false;
	bool bestIsAxisB = false;
	int bestIndexA = -1, bestIndexB = -1;

	auto consider = [&](float overlap, const Vector3& axis, bool isA, bool isB, int ia, int ib) {
		if (overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis = axis;
			bestIsAxisA = isA;
			bestIsAxisB = isB;
			bestIndexA = ia;
			bestIndexB = ib;
		}
		};

	// A axes
	for (int i = 0; i < 3; i++) {
		float ra = a[i];
		float rb = b.x * absR[i][0] + b.y * absR[i][1] + b.z * absR[i][2];
		float dist = fabs(t[i]);
		float overlap = (ra + rb) - dist;
		if (overlap <= 0.0f) return false;
		consider(overlap, Aaxes[i], true, false, i, -1);
	}

	// B axes
	for (int j = 0; j < 3; j++) {
		float ra = a.x * absR[0][j] + a.y * absR[1][j] + a.z * absR[2][j];
		float dist = fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]);
		float rb = b[j];
		float overlap = (ra + rb) - dist;
		if (overlap <= 0.0f) return false;
		consider(overlap, Baxes[j], false, true, -1, j);
	}

	// cross axes
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			int ip1 = (i + 1) % 3, ip2 = (i + 2) % 3;
			int jp1 = (j + 1) % 3, jp2 = (j + 2) % 3;
			float ra = a[ip1] * absR[ip2][j] + a[ip2] * absR[ip1][j];
			float rb = b[jp1] * absR[i][jp2] + b[jp2] * absR[i][jp1];
			float dist = fabs(t[ip2] * R[ip1][j] - t[ip1] * R[ip2][j]);
			float overlap = (ra + rb) - dist;
			if (overlap <= 0.0f) return false;
			Vector3 axis = Vector::Cross(Aaxes[i], Baxes[j]);
			float len = Vector::Length(axis);
			if (len > EPSILON) {
				axis = axis * (1.0f / len);
				consider(overlap, axis, false, false, i, j);
			}
		}
	}

	// No separating axis -> collision
	Vector3 normal = bestAxis;
	if (Vector::Dot(cB - cA, normal) < 0.0f) normal = normal * -1.0f;
	float penetrationFallback = minOverlap;

	// Helpers for face polygon generation and clipping
	auto buildFacePolygon = [&](const Vector3 axes[3], const Vector3& center, const Vector3& half, int faceIndex, bool positiveNormal,
		std::vector<Vector3>& outVerts, Vector3& faceCenter, Vector3& faceNormal) {
			faceNormal = axes[faceIndex] * (positiveNormal ? 1.0f : -1.0f);
			faceCenter = center + faceNormal * half[faceIndex];
			int iu = (faceIndex + 1) % 3;
			int iv = (faceIndex + 2) % 3;
			Vector3 u = axes[iu] * half[iu];
			Vector3 v = axes[iv] * half[iv];
			outVerts.clear();
			outVerts.push_back(faceCenter + u + v);
			outVerts.push_back(faceCenter - u + v);
			outVerts.push_back(faceCenter - u - v);
			outVerts.push_back(faceCenter + u - v);
		};

	auto clipPolygonAgainstPlane = [&](const std::vector<Vector3>& inPoly, const Vector3& planePoint, const Vector3& planeNormal) {
		std::vector<Vector3> outPoly;
		if (inPoly.empty()) return outPoly;
		auto isInside = [&](const Vector3& p)->bool {
			return Vector::Dot(planeNormal, p - planePoint) <= 0.00001f;
			};
		auto intersect = [&](const Vector3& aP, const Vector3& bP)->Vector3 {
			Vector3 ab = bP - aP;
			float denom = Vector::Dot(planeNormal, ab);
			if (fabs(denom) < 1e-8f) return aP;
			float t = Vector::Dot(planeNormal, planePoint - aP) / denom;
			t = Clamp(t, 0.0f, 1.0f);
			return aP + ab * t;
			};

		for (size_t i = 0; i < inPoly.size(); i++) {
			Vector3 curr = inPoly[i];
			Vector3 prev = inPoly[(i + inPoly.size() - 1) % inPoly.size()];
			bool currIn = isInside(curr);
			bool prevIn = isInside(prev);

			if (currIn) {
				if (!prevIn) {
					Vector3 ip = intersect(prev, curr);
					outPoly.push_back(ip);
				}
				outPoly.push_back(curr);
			}
			else if (prevIn) {
				Vector3 ip = intersect(prev, curr);
				outPoly.push_back(ip);
			}
		}
		return outPoly;
		};

	std::vector<Vector3> contactWorldPoints;

	// If best axis is face of A or B -> face-face clipping path
	if (bestIsAxisA || bestIsAxisB) {
		bool refIsA = bestIsAxisA;
		int refIndex = refIsA ? bestIndexA : bestIndexB;
		const Vector3* refAxes = refIsA ? Aaxes : Baxes;
		const Vector3* incAxes = refIsA ? Baxes : Aaxes;
		Vector3 refCenter = refIsA ? cA : cB;
		Vector3 incCenter = refIsA ? cB : cA;
		Vector3 refHalf = refIsA ? a : b;
		Vector3 incHalf = refIsA ? b : a;

		// Determine ref face normal pointing outward of reference box,
		// orient it so it faces incident box
		Vector3 refFaceNormal = refAxes[refIndex];
		if (Vector::Dot(refFaceNormal, normal) < 0.0f) refFaceNormal = refFaceNormal * -1.0f;

		// Build reference face polygon
		std::vector<Vector3> refVerts;
		Vector3 refFaceCenter, refFaceNormalOut;
		Vector3 refAxesLocal[3] = { refAxes[0], refAxes[1], refAxes[2] };
		buildFacePolygon(refAxesLocal, refCenter, refHalf, refIndex, Vector::Dot(refAxesLocal[refIndex], refFaceNormal) > 0.0f,
			refVerts, refFaceCenter, refFaceNormalOut);

		// Build incident face (choose face most anti-parallel to refFaceNormal)
		int incBest = 0;
		float bestDot = fabs(Vector::Dot(incAxes[0], refFaceNormal));
		for (int i = 1; i < 3; i++) {
			float d = fabs(Vector::Dot(incAxes[i], refFaceNormal));
			if (d > bestDot) { bestDot = d; incBest = i; }
		}
		float dPos = Vector::Dot(incAxes[incBest], refFaceNormal);
		bool incPositive = (dPos < 0.0f); // choose sign so incident face normal faces reference face

		std::vector<Vector3> incVerts;
		Vector3 incFaceCenter, incFaceNormalOut;
		Vector3 incAxesLocal[3] = { incAxes[0], incAxes[1], incAxes[2] };
		buildFacePolygon(incAxesLocal, incCenter, incHalf, incBest, incPositive,
			incVerts, incFaceCenter, incFaceNormalOut);

		// Clip incident polygon by reference face side planes
		std::vector<Vector3> poly = incVerts;
		for (int i = 0; i < 4 && !poly.empty(); i++) {
			Vector3 v0 = refVerts[i];
			Vector3 v1 = refVerts[(i + 1) % 4];
			Vector3 edge = v1 - v0;
			Vector3 planeNormal = Vector::Cross(edge, refFaceNormalOut);
			float len = Vector::Length(planeNormal);
			if (len > EPSILON) planeNormal = planeNormal * (1.0f / len);
			else continue;
			Vector3 planePoint = v0;
			poly = clipPolygonAgainstPlane(poly, planePoint, planeNormal);
		}

		// If clipped polygon empty -> fallback to single contact
		if (poly.empty()) {
			float ra = a.x * fabs(Vector::Dot(Aaxes[0], normal)) +
				a.y * fabs(Vector::Dot(Aaxes[1], normal)) +
				a.z * fabs(Vector::Dot(Aaxes[2], normal));
			float rb = b.x * fabs(Vector::Dot(Baxes[0], normal)) +
				b.y * fabs(Vector::Dot(Baxes[1], normal)) +
				b.z * fabs(Vector::Dot(Baxes[2], normal));
			Vector3 contactA = cA + normal * ra;
			Vector3 contactB = cB - normal * rb;
			contactWorldPoints.push_back((contactA + contactB) * 0.5f);
		}
		else {
			// For each vertex in poly compute penetration depth relative to reference face plane
			std::vector<std::pair<float, Vector3>> depthPoints;
			for (auto& p : poly) {
				// depth = distance along refFaceNormalOut from point to plane (positive means penetrating)
				float depth = -Vector::Dot(refFaceNormalOut, p - refFaceCenter);
				if (depth > 0.00001f) depthPoints.emplace_back(depth, p);
			}
			// If none positive (numerical), use geometric average with fallback penetration
			if (depthPoints.empty()) {
				Vector3 avg(0, 0, 0);
				for (auto& p : poly) avg += p;
				avg = avg * (1.0f / (float)poly.size());
				depthPoints.emplace_back(penetrationFallback, avg);
			}

			// sort by depth desc and pick up to 4 deepest points
			std::sort(depthPoints.begin(), depthPoints.end(), [](const std::pair<float, Vector3>& A, const std::pair<float, Vector3>& B) {
				return A.first > B.first;
				});
			size_t take = std::min((size_t)4, depthPoints.size());
			for (size_t i = 0; i < take; i++) contactWorldPoints.push_back(depthPoints[i].second);
		}
	}
	else {
		// edge-edge or other case: single contact estimate
		float ra = a.x * fabs(Vector::Dot(Aaxes[0], normal)) +
			a.y * fabs(Vector::Dot(Aaxes[1], normal)) +
			a.z * fabs(Vector::Dot(Aaxes[2], normal));
		float rb = b.x * fabs(Vector::Dot(Baxes[0], normal)) +
			b.y * fabs(Vector::Dot(Baxes[1], normal)) +
			b.z * fabs(Vector::Dot(Baxes[2], normal));
		Vector3 contactA = cA + normal * ra;
		Vector3 contactB = cB - normal * rb;
		contactWorldPoints.push_back((contactA + contactB) * 0.5f);
	}

	// Add contacts to collisionInfo with per-point penetration (compute per-point depth when possible)
	if (contactWorldPoints.empty()) {
		// fallback single mid-point
		Vector3 mid = (cA + cB) * 0.5f;
		Vector3 localA = mid - cA;
		Vector3 localB = mid - cB;
		collisionInfo.AddContactPoint(localA, localB, normal, penetrationFallback);
	}
	else {
		for (auto& pw : contactWorldPoints) {
			// try to compute per-point penetration more accurately by projecting pw onto normal from both boxes' supports
			// Compute signed distance from pw to reference along normal: use projection of pw onto normal from each box support
			float projA = a.x * fabs(Vector::Dot(Aaxes[0], normal)) +
				a.y * fabs(Vector::Dot(Aaxes[1], normal)) +
				a.z * fabs(Vector::Dot(Aaxes[2], normal));
			float projB = b.x * fabs(Vector::Dot(Baxes[0], normal)) +
				b.y * fabs(Vector::Dot(Baxes[1], normal)) +
				b.z * fabs(Vector::Dot(Baxes[2], normal));
			float centerDist = Vector::Dot((cB - cA), normal);
			float pointDepth = (projA + projB) - fabs(centerDist);
			// As a safer per-point depth, we can recompute using the reference face plane if available:
			// But for stability ensure positive:
			float pen = pointDepth;
			if (pen <= 0.0f) pen = penetrationFallback;

			Vector3 localA = pw - cA;
			Vector3 localB = pw - cB;
			collisionInfo.AddContactPoint(localA, localB, normal, pen);
		}
	}

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

