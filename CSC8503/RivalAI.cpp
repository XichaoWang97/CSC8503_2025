#include "RivalAI.h"
#include "GameWorld.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "Debug.h"

using namespace NCL;
using namespace CSC8503;

RivalAI::RivalAI(GameWorld* world, NavigationGrid* _grid) : GameCharacter("RivalAI", world) {
    grid = _grid;
    gameWorld = world;
    allPlayers = nullptr;
    currentTarget = nullptr;
    rootNode = nullptr;

    moveSpeed = 10.0f;
    currentScore = 0;
    rivalWinningScore = 0;
    packageSpawnPos = Vector3(0, 0, 0); // Default, needs external Set
    timeSinceLastPathCalc = 0.0f;
    lastCalcTargetPos = Vector3(99999, 99999, 99999);
    BuildBehaviourTree();
    //  init like player
}

RivalAI::~RivalAI() {
    delete rootNode;
}

void RivalAI::Update(float dt) {
    actionCooldown -= dt; // General action cooldown

    if (jumpCooldown > 0.0f) { // Jump cooldown update
        jumpCooldown -= dt;
    }

    if (rootNode) { // Execute behaviour tree
        rootNode->Execute(dt);
    }
    GameCharacter::Update(dt);
    DrawGrappleLine();
}

void RivalAI::BuildBehaviourTree() {
    // --- 1. Win & Run Branch (Survival) ---
    BehaviourSequence* seqRunAway = new BehaviourSequence("Survival Mode");
    seqRunAway->AddChild(new BehaviourAction("Check Score", [&](float dt, BehaviourState s) { return HasHighScore(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Check Package", [&](float dt, BehaviourState s) { return IsHoldingPackage(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Set Win Target", [&](float dt, BehaviourState s) { return FindWinZone(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Move To Exit", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));

    // --- 2. Intercept Player Branch (Intercept) ---
    // 2.1 Attack Sub-branch
    BehaviourSequence* seqAttack = new BehaviourSequence("Attack Player");
    seqAttack->AddChild(new BehaviourAction("Check Stone", [&](float dt, BehaviourState s) { return IsHoldingStone(dt); }));
    // Here we borrow ThrowAtPlayer which contains move-close logic internally, or you can split it into MoveToPlayer -> Throw
    seqAttack->AddChild(new BehaviourAction("Throw Logic", [&](float dt, BehaviourState s) { return ThrowAtPlayer(dt); }));

    // 2.2 Find Stone Sub-branch
    BehaviourSequence* seqGetWeapon = new BehaviourSequence("Find Weapon");
    seqGetWeapon->AddChild(new BehaviourAction("Locate Stone", [&](float dt, BehaviourState s) { return GetClosestStone(dt); }));
    seqGetWeapon->AddChild(new BehaviourAction("Move Stone", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));
    seqGetWeapon->AddChild(new BehaviourAction("Grab Stone", [&](float dt, BehaviourState s) { return AttemptGrab(dt); }));

    BehaviourSelector* selWeapon = new BehaviourSelector("Weapon Choice");
    selWeapon->AddChild(seqAttack);
    selWeapon->AddChild(seqGetWeapon);

    BehaviourSequence* seqIntercept = new BehaviourSequence("Intercept Mode");
    seqIntercept->AddChild(new BehaviourAction("Check Enemy", [&](float dt, BehaviourState s) { return DoesPlayerHavePackage(dt); }));
    seqIntercept->AddChild(selWeapon);

    // --- 3. Greedy Collection Branch (Greedy) ---
    BehaviourSequence* seqGreedy = new BehaviourSequence("Greedy Mode");
    seqGreedy->AddChild(new BehaviourAction("Check Pkg", [&](float dt, BehaviourState s) { return IsHoldingPackage(dt); }));
    seqGreedy->AddChild(new BehaviourAction("Locate Coin", [&](float dt, BehaviourState s) { return GetClosestCoin(dt); }));
    seqGreedy->AddChild(new BehaviourAction("Move Coin", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));
    // MyGame usually handles collection when walking to the coin, so Grab is not needed

    // --- 4. Scavenge/Camp Branch (Scavenge) ---
    BehaviourSequence* seqScavenge = new BehaviourSequence("Scavenge Mode");
    seqScavenge->AddChild(new BehaviourAction("Find Pkg/Spot", [&](float dt, BehaviourState s) { return GetPackageOrCamp(dt); }));
    seqScavenge->AddChild(new BehaviourAction("Move Pkg", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));
    seqScavenge->AddChild(new BehaviourAction("Grab Pkg", [&](float dt, BehaviourState s) { return AttemptGrab(dt); }));

    // --- Root Node ---
    BehaviourSelector* root = new BehaviourSelector("Rival Root");
    root->AddChild(seqRunAway);  // Priority 1: Won, run away quickly
    root->AddChild(seqIntercept); // Priority 2: Not won but player has it, attack him
    root->AddChild(seqGreedy);    // Priority 3: I have it, go pick up money
    root->AddChild(seqScavenge);  // Priority 4: No one has it, go grab it

    rootNode = root;
}


void RivalAI::LookAt(Vector3 targetPos) {
    Vector3 dir = (targetPos - GetTransform().GetPosition());
    dir.y = 0;
    dir = Vector::Normalise(dir);

    // set orientation
    if (Vector::Length(dir) > 0) {
        Matrix4 lookAt = Matrix::View(Vector3(0, 0, 0), -dir, Vector3(0, 1, 0));
        GetTransform().SetOrientation(Quaternion(Matrix::Inverse(lookAt)));
    }
}

GameObject* RivalAI::FindClosestObject(std::string name) {
    GameObject* closest = nullptr;
    float minDist = 9999.0f;
    Vector3 myPos = GetTransform().GetPosition();

    gameWorld->OperateOnContents([&](GameObject* o) {
        if (!o->IsActive()) return;
        if (o->GetName() == name) {
            float d = Vector::Length(o->GetTransform().GetPosition() - myPos);
            if (d < minDist) {
                minDist = d;
                closest = o;
            }
        }
        });
    return closest;
}

GameObject* RivalAI::FindPackage() {
    return FindClosestObject("FragilePackage");
}

void RivalAI::CalculatePath(Vector3 targetPos) {
    if (!grid) return;
    NavigationPath outPath;
    bool found = grid->FindPath(GetTransform().GetPosition(), targetPos, outPath);
    pathPoints.clear();
    Vector3 pos;
    while (outPath.PopWaypoint(pos)) {
        pathPoints.push_back(pos);
    }
}

// Conditions: score and holding states

BehaviourState RivalAI::HasHighScore(float dt) {
    return (currentScore >= rivalWinningScore) ? Success : Failure;
}

BehaviourState RivalAI::IsHoldingPackage(float dt) {
    GameObject* item = GetHeldItem();
    if (item && item->GetName() == "FragilePackage") return Success;
    return Failure;
}

BehaviourState RivalAI::IsHoldingStone(float dt) {
    GameObject* item = GetHeldItem();
    if (item && item->GetName() == "Stone") return Success;
    return Failure;
}

Player* RivalAI::FindPlayerHoldingPackage() {
    if (!allPlayers) return nullptr;

    for (Player* p : *allPlayers) { // check all players
        if (!p) continue;
        GameObject* heldItem = p->GetHeldItem();
        if (heldItem && heldItem->GetName() == "FragilePackage") {
            return p; // find
        }
    }
    return nullptr;
}

BehaviourState RivalAI::DoesPlayerHavePackage(float dt) {
    Player* targetP = FindPlayerHoldingPackage();
    if (targetP != nullptr) {
        // set the player who has package as target
        currentTarget = targetP;
        return Success;
    }
    return Failure;
}

// Actions

BehaviourState RivalAI::FindWinZone(float dt) {
    GameObject* winZone = FindClosestObject("winZone");
    if (winZone) {
        currentTarget = winZone;
        return Success;
    }
    return Failure;
}

// find closest stone
BehaviourState RivalAI::GetClosestStone(float dt) {
    GameObject* bestStone = FindBestStoneWithNav(GetTransform().GetPosition());

    if (bestStone) {
        currentTarget = bestStone;
        return Success;
    }
    return Failure;
}

// find closest coin
BehaviourState RivalAI::GetClosestCoin(float dt) {
    GameObject* coin = FindClosestObject("Coin");
    if (coin) {
        currentTarget = coin;
        return Success;
    }

    return Failure;
}

BehaviourState RivalAI::GetPackageOrCamp(float dt) {
    GameObject* pkg = FindPackage();
    if (pkg && pkg->IsActive()) {
        currentTarget = pkg; // catch the package
    }
    else {
        currentTarget = nullptr;
    }
    return Success;
}

BehaviourState RivalAI::MoveToTarget(float dt) {
    Vector3 targetPos;
    Vector3 myPos = GetTransform().GetPosition();

    if (currentTarget) {
        if (!currentTarget->IsActive()) return Failure;
        targetPos = currentTarget->GetTransform().GetPosition();
        if (currentTarget->GetName() == "WinZone") {
            targetPos.y += 1.5f; // we should set it a little higher to make sure rival is standing on it
        }
    }
    else {
        targetPos = packageSpawnPos; // if no target, go to camp point(start position of package)
    }

    // Retargeting logic: Periodically check for a better target
    // Only check when searching for stones
    if (currentTarget && (currentTarget->GetName() == "Stone")) {
        retargetTimer -= dt;
        if (retargetTimer <= 0.0f) {
            retargetTimer = 1.0f; // Reset timer

            GameObject* bestNewStone = FindBestStoneWithNav(myPos);

            if (bestNewStone && bestNewStone != currentTarget) {
                // Recalculate both path distances for comparison
                float newDist = CalculatePathDistance(myPos, bestNewStone->GetTransform().GetPosition());
                float oldDist = CalculatePathDistance(myPos, currentTarget->GetTransform().GetPosition());

                // Threshold check
                if (newDist < oldDist - 5.0f) {
                    currentTarget = bestNewStone;
                    CalculatePath(currentTarget->GetTransform().GetPosition());
                }
            }
        }
    }

    // Target position changed significantly
    float distToLastTarget = Vector::LengthSquared(targetPos - lastCalcTargetPos);
    bool targetMoved = distToLastTarget > 25.0f;

    // Call FindPath
    if (targetMoved) {
        CalculatePath(targetPos);
        lastCalcTargetPos = targetPos; // Update "last recorded position"
    }

    // if no path, go to the target directly
    if (pathPoints.empty()) {
        Vector3 dir = (targetPos - myPos);
        dir.y = 0;
        if (Vector::Length(dir) > 1.0f) { // only care about horizontal distance
            GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 15.0f); // set it slower than player
            LookAt(targetPos);
        }
        // Jump logic
        float yDiff = targetPos.y - myPos.y;
        dir.y = 0;
        float xzDist = Vector::Length(dir);

        if (yDiff >= 1.0f && xzDist <= 1.0f) { // jump conditions
            Jump();
        }
        return Success; // catch
    }

    // get next waypoint
    Vector3 nextWaypoint = pathPoints[0];
    nextWaypoint.y = myPos.y;
    Vector3 dir = nextWaypoint - myPos;
    dir.y = 0;
    float distToNode = Vector::Length(dir);

    // get to the point
    if (distToNode < 5.0f) {
        pathPoints.erase(pathPoints.begin());
        return Ongoing;
    }

    // physical move
    GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 15.0f);
    LookAt(nextWaypoint);

    // Draw Debug Lines
    Debug::DrawLine(GetTransform().GetPosition(), nextWaypoint, Vector4(1, 0, 0, 1));
    // complete path(blue)
    for (size_t i = 0; i < pathPoints.size() - 1; ++i) {
        Vector3 a = pathPoints[i];
        Vector3 b = pathPoints[i + 1];
        // a little up
        a.y += 1;
        b.y += 1;
        Debug::DrawLine(a, b, Vector4(0, 0, 1, 1));
    }
    // draw target(green)
    Debug::DrawLine(targetPos, targetPos + Vector3(0, 10, 0), Vector4(0, 1, 0, 1));

    return Ongoing;
}

BehaviourState RivalAI::AttemptGrab(float dt) {
    if (!currentTarget) return Failure;

    float dist = Vector::Length(currentTarget->GetTransform().GetPosition() - GetTransform().GetPosition());

    if (dist < 10.0f) {
        LookAt(currentTarget->GetTransform().GetPosition());

        // throw current held item first
        GameObject* item = GetHeldItem();
        if (item) {
            ThrowHeldItem(Vector3(0, 0, 0));
        }
        // make sure cooldown is ready
        if (actionCooldown <= 0.0f) {
            Vector3 aimDir = GetTransform().GetOrientation() * Vector3(0, 0, 1);
            TryGrab(aimDir);
            actionCooldown = 1.0f;

            if (GetHeldItem() == currentTarget) return Success; // catch!
        }
    }
    return Failure;
}

BehaviourState RivalAI::ThrowAtPlayer(float dt) {
    Player* targetP = FindPlayerHoldingPackage(); // confirm target player
    // Actually, this action aims to the package that player is holding
    if (!targetP) return Failure;
    std::cout << "RivalAI: ThrowAtPlayer action executing.\n";
    Vector3 packagePos = fragilePackage->GetTransform().GetPosition(); // get package position
    Vector3 myPos = GetTransform().GetPosition();
    float dist = Vector::Length(packagePos - myPos);

    // check distance
    if (dist > 50.0f) {
        Vector3 dir = (packagePos - myPos);
        dir.y = 0;
        GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 20.0f);
        LookAt(packagePos);
        return Ongoing;
    }
    std::cout << actionCooldown << std::endl;
    // Attack when in range
    LookAt(packagePos);
    if (actionCooldown <= 0.0f) {
        Vector3 aimDir = Vector::Normalise((packagePos - myPos));
        ThrowHeldItem(aimDir); // parent method
        actionCooldown = 1.0f;
        return Success;
    }

    return Ongoing;
}

void RivalAI::Jump() {
    if (jumpCooldown <= 0.0f && IsOnGround()) {
        GetPhysicsObject()->ApplyLinearImpulse(Vector3(0, 15, 0));
        jumpCooldown = 1.0f;
        // add forward impulse to avoid getting stuck
        Vector3 fwd = GetTransform().GetOrientation() * Vector3(0, 0, 1);
        GetPhysicsObject()->ApplyLinearImpulse(fwd * 1.5f);
    }
}

bool RivalAI::IsOnGround() {
    // raycast down to check ground
    Vector3 pos = GetTransform().GetPosition();
    Ray ray(pos, Vector3(0, -1.0, 0));
    RayCollision collision;
    if (gameWorld->Raycast(ray, collision, true, this)) {
        if (collision.rayDistance < 1.1f) return true;
    }
    return false;
}

// Calculate distance using navigation method
float RivalAI::CalculatePathDistance(Vector3 startPos, Vector3 endPos) {
    if (!grid) return 99999.0f;

    NavigationPath outPath;
    // Use NCL's FindPath
    bool found = grid->FindPath(startPos, endPos, outPath);

    // If unreachable (e.g., stone is blocked), return infinity
    if (!found) return 99999.0f;

    float totalDist = 0.0f;
    Vector3 currentPos = startPos;
    Vector3 nextPos;

    // Accumulate distance between path points
    while (outPath.PopWaypoint(nextPos)) {
        totalDist += Vector::Length(nextPos - currentPos);
        currentPos = nextPos;
    }

    // Add distance from last point to target (depends on Path implementation, sometimes last point is target)
    return totalDist;
}

GameObject* RivalAI::FindBestStoneWithNav(Vector3 searchPos) {
    struct Candidate {
        GameObject* obj;
        float distSq;
    };
    std::vector<Candidate> candidates;

    // collect stones
    gameWorld->OperateOnContents([&](GameObject* o) {
        if (o->IsActive() && o->GetName() == "Stone" && o != GetHeldItem()) {
            Vector3 diff = o->GetTransform().GetPosition() - searchPos;
            candidates.push_back({ o, Vector::LengthSquared(diff) });
        }
        });

    if (candidates.empty()) return nullptr;

    // Sort by straight-line distance
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.distSq < b.distSq;
        });

    // Check the top 3 candidates and find the one with the shortest navigation distance
    GameObject* bestStone = nullptr;
    float minNavDist = 99999.0f;
    int checkCount = std::min((int)candidates.size(), 3);

    for (int i = 0; i < checkCount; ++i) {
        float pathDist = CalculatePathDistance(searchPos, candidates[i].obj->GetTransform().GetPosition());
        if (pathDist < minNavDist) {
            minNavDist = pathDist;
            bestStone = candidates[i].obj;
        }
    }

    // Only return if the path is valid
    if (minNavDist < 90000.0f) {
        return bestStone;
    }
    return nullptr;
}