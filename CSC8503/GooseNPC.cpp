#include "GooseNPC.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "Debug.h"

using namespace NCL;
using namespace CSC8503;

GooseNPC::GooseNPC(NavigationGrid* _grid) : StateGameObject("Goose") {
    grid = _grid;
	chaseSpeed = 3.5f; // chase speed of the goose
    rootNode = nullptr;
    timeSinceLastPathCalc = 0.0f;
    BuildBehaviourTree();
}

GooseNPC::~GooseNPC() {
    delete rootNode;
}

void GooseNPC::Update(float dt) {
    if (rootNode) {
        rootNode->Execute(dt);
    }

    GameObject::Update(dt);
}

void GooseNPC::BuildBehaviourTree() {
	// behaviour: chase player
    BehaviourSequence* chaseSequence = new BehaviourSequence("Chase Sequence");

    chaseSequence->AddChild(new BehaviourAction("Chase Player", [&](float dt, BehaviourState s){ return ChasePlayer(dt); }));

    rootNode = chaseSequence;
}

BehaviourState GooseNPC::ChasePlayer(float dt) {
    if (!playerTarget || !grid) return Failure;

    Vector3 targetPos = playerTarget->GetTransform().GetPosition();
    Vector3 myPos = GetTransform().GetPosition();

	// calculate path every second
    timeSinceLastPathCalc += dt;
    if (timeSinceLastPathCalc > 1.0f) {
        CalculatePathTo(targetPos);
        timeSinceLastPathCalc = 0.0f;
    }

    // move along the path
    if (pathPoints.empty()) {
		// if no path, move directly towards player
        Vector3 dir = (targetPos - myPos);
        dir.y = 0;
        if (Vector::Length(dir) > 2.0f) {
            GetPhysicsObject()->AddForce(Vector::Normalise(dir) * chaseSpeed);
            LookAt(targetPos, dt);
        }
        else {
            GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
        }
        return Ongoing;
    }

	// get next waypoint
    Vector3 nextWaypoint = pathPoints[0];
    nextWaypoint.y = myPos.y;
    Vector3 dir = nextWaypoint - myPos;
    dir.y = 0;
    float distToNode = Vector::Length(dir);

	// if close enough to waypoint, pop it, this paragrameter(distToNode) is very important for smooth movement
    if (distToNode < 20.0f) {
        pathPoints.erase(pathPoints.begin());
        return Ongoing;
    }

	// move
    GetPhysicsObject()->AddForce(Vector::Normalise(dir) * chaseSpeed);
    LookAt(nextWaypoint, dt);
    
	// Draw Debug Lines
	// from my position to next waypoint (red line)
    Debug::DrawLine(myPos, nextWaypoint, Vector4(1, 0, 0, 1));

	// draw the whole path (blue lines)
    for (size_t i = 0; i < pathPoints.size() - 1; ++i) {
        Vector3 a = pathPoints[i];
        Vector3 b = pathPoints[i + 1];

		// a little offset for better visibility
        a.y += 1;
        b.y += 1;

        Debug::DrawLine(a, b, Vector4(0, 0, 1, 1));
    }
	// draw target position (green line)
    Debug::DrawLine(targetPos, targetPos + Vector3(0, 10, 0), Vector4(0, 1, 0, 1));

    return Ongoing;
}

void GooseNPC::CalculatePathTo(Vector3 targetPos) {
    if (!grid) return;
    currentPath.Clear();
    pathPoints.clear();

    Vector3 startPos = GetTransform().GetPosition();

	// Find path on the navigation grid
    if (grid->FindPath(startPos, targetPos, currentPath)) {
        Vector3 pos;
        while (currentPath.PopWaypoint(pos)) {
            pathPoints.push_back(pos);
        }
    }
}

void GooseNPC::LookAt(Vector3 targetPos, float dt) {
    Vector3 dir = (targetPos - GetTransform().GetPosition());
    dir.y = 0;
    dir = Vector::Normalise(dir);

    if (Vector::LengthSquared(dir) < 0.01f) return;
    
    Matrix4 lookAtMat = Matrix::View(Vector3(0, 0, 0), dir, Vector3(0, 1, 0));
    Quaternion targetOrientation = Quaternion(Matrix::Inverse(lookAtMat));

    Quaternion currentOrientation = GetTransform().GetOrientation();

    // Slerp
    Quaternion newOrientation = Quaternion::Slerp(currentOrientation, targetOrientation, 5.0f * dt);

    GetTransform().SetOrientation(newOrientation);
	// lock rotation velocity
    GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
}