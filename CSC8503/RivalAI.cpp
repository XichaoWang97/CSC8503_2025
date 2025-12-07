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
    winningScore = 3; // 假设收集3个就赢
    packageSpawnPos = Vector3(0, 0, 0); // 默认，需外部Set
    timeSinceLastPathCalc = 0.0f;
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
    // --- 1. 胜利逃跑分支 (Survival) ---
    BehaviourSequence* seqRunAway = new BehaviourSequence("Survival Mode");
    seqRunAway->AddChild(new BehaviourAction("Check Score", [&](float dt, BehaviourState s) { return HasHighScore(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Check Package", [&](float dt, BehaviourState s) { return IsHoldingPackage(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Set Win Target", [&](float dt, BehaviourState s) { return FindWinZone(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Move To Exit", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));

    // --- 2. 拦截玩家分支 (Intercept) ---
    // 2.1 攻击子分支
    BehaviourSequence* seqAttack = new BehaviourSequence("Attack Player");
    seqAttack->AddChild(new BehaviourAction("Check Stone", [&](float dt, BehaviourState s) { return IsHoldingStone(dt); }));
    // 这里借用 ThrowAtPlayer 内部包含移动接近逻辑，或者你可以拆分开 MoveToPlayer -> Throw
    seqAttack->AddChild(new BehaviourAction("Throw Logic", [&](float dt, BehaviourState s) { return ThrowAtPlayer(dt); }));

    // 2.2 找石头子分支
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

    // --- 3. 贪婪收集分支 (Greedy) ---
    BehaviourSequence* seqGreedy = new BehaviourSequence("Greedy Mode");
    seqGreedy->AddChild(new BehaviourAction("Check Pkg", [&](float dt, BehaviourState s) { return IsHoldingPackage(dt); }));
    seqGreedy->AddChild(new BehaviourAction("Locate Coin", [&](float dt, BehaviourState s) { return GetClosestCoin(dt); }));
    seqGreedy->AddChild(new BehaviourAction("Move Coin", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));
    // 走到金币处MyGame通常会处理收集，所以不需要Grab

    // --- 4. 争夺/守点分支 (Scavenge) ---
    BehaviourSequence* seqScavenge = new BehaviourSequence("Scavenge Mode");
    seqScavenge->AddChild(new BehaviourAction("Find Pkg/Spot", [&](float dt, BehaviourState s) { return GetPackageOrCamp(dt); }));
    seqScavenge->AddChild(new BehaviourAction("Move Pkg", [&](float dt, BehaviourState s) { return MoveToTarget(dt); }));
    seqScavenge->AddChild(new BehaviourAction("Grab Pkg", [&](float dt, BehaviourState s) { return AttemptGrab(dt); }));

    // --- 根节点 ---
    BehaviourSelector* root = new BehaviourSelector("Rival Root");
    root->AddChild(seqRunAway);  // 优先级1：赢了赶紧跑
    root->AddChild(seqIntercept); // 优先级2：没赢但玩家拿着，打他
    root->AddChild(seqGreedy);    // 优先级3：我拿着，去捡钱
    root->AddChild(seqScavenge);  // 优先级4：没人拿着，去抢

    rootNode = root;
}

// ================= 辅助函数 =================

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
    return (currentScore >= winningScore) ? Success : Failure;
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

    for (Player* p : *allPlayers) { // 遍历所有玩家
        if (!p) continue;
        GameObject* heldItem = p->GetHeldItem();
        if (heldItem && heldItem->GetName() == "FragilePackage") {
            return p; // 找到了！就是这个家伙
        }
    }
    return nullptr;
}

BehaviourState RivalAI::DoesPlayerHavePackage(float dt) {
    Player* targetP = FindPlayerHoldingPackage();
    if (targetP != nullptr) {
        // 找到了拿着包裹的玩家，把他设为当前攻击目标
        currentTarget = targetP;
        return Success;
    }
    return Failure;
}

// Actions

BehaviourState RivalAI::FindWinZone(float dt) {
    GameObject* winZone = FindClosestObject("WinZone");
    if (exitPoint) {
        currentTarget = exitPoint;
        return Success;
    }
    return Failure;
}

// find closest stone
BehaviourState RivalAI::GetClosestStone(float dt) {
    GameObject* stone = FindClosestObject("Stone");
    if (stone) {
        currentTarget = stone;
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
    }
    else {
		targetPos = packageSpawnPos; // if no target, go to camp point(start position of package)
    }

	// calculate path periodically 1.0s per time
    timeSinceLastPathCalc += dt;
    if (timeSinceLastPathCalc > 1.0f) {
        CalculatePath(targetPos);
        timeSinceLastPathCalc = 0.0f;
    }
    
	// if no path, go to the target directly
    if (pathPoints.empty()) {
        Vector3 dir = (targetPos - myPos);
        dir.y = 0;
		if (Vector::Length(dir) > 1.0f) { // only care about horizontal distance
            GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 15.0f); // set it slower than player
            LookAt(targetPos);
        }
        /*else {
            GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
        }*/

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
    if (distToNode < 10.0f) {
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
        if(item){
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
