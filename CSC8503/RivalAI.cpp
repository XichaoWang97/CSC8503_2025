#include "RivalAI.h"
#include "GameWorld.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "Debug.h"

using namespace NCL;
using namespace CSC8503;

RivalAI::RivalAI(GameWorld* world, NavigationGrid* _grid, const Vector3& position,
    Rendering::Mesh* mesh, GameTechMaterial material, Vector4 colour) : GameCharacter("RivalAI", world) {
    grid = _grid;
    gameWorld = world;
    player = nullptr;
    currentTarget = nullptr;
    rootNode = nullptr;

    moveSpeed = 10.0f;
    currentScore = 0;
    winningScore = 3; // 假设收集3个就赢
    packageSpawnPos = Vector3(0, 0, 0); // 默认，需外部Set

    BuildBehaviourTree();
    //  init like player

    float meshSize = 1.0f;
    float inverseMass = 0.5f;

    // set physics volume
    SphereVolume* volume = new SphereVolume(0.5f);
    SetBoundingVolume(volume);

    // set position
    GetTransform().SetScale(Vector3(meshSize, meshSize, meshSize)).SetPosition(position);

    // set render object
    SetRenderObject(new RenderObject(GetTransform(), mesh, material));
    GetRenderObject()->SetColour(colour);

    // set physics object
    PhysicsObject* physicsObj = new PhysicsObject(GetTransform(), GetBoundingVolume());
    physicsObj->SetInverseMass(inverseMass);
    physicsObj->InitSphereInertia();
    physicsObj->SetElasticity(0.0f); // no bounce
    SetPhysicsObject(physicsObj);
}

RivalAI::~RivalAI() {
    delete rootNode;
}

void RivalAI::Update(float dt) {
    // Jump cooldown update
    if (jumpCooldown > 0.0f) {
        jumpCooldown -= dt;
    }
	// Execute behaviour tree
    if (rootNode) {
        rootNode->Execute(dt);
    }
    GameCharacter::Update(dt);
}

void RivalAI::BuildBehaviourTree() {
    // --- 1. 胜利逃跑分支 (Survival) ---
    BehaviourSequence* seqRunAway = new BehaviourSequence("Survival Mode");
    seqRunAway->AddChild(new BehaviourAction("Check Score", [&](float dt, BehaviourState s) { return HasHighScore(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Check Package", [&](float dt, BehaviourState s) { return IsHoldingPackage(dt); }));
    seqRunAway->AddChild(new BehaviourAction("Flee Player", [&](float dt, BehaviourState s) { return RunAwayFromPlayer(dt); }));

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

    // 简单的 LookAt 矩阵转四元数
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

// ================= 行为逻辑 (Conditions) =================

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
    // 假设石头名字是 "Stone" 或 "Rock"
    if (item && item->GetName() == "Stone") return Success;
    return Failure;
}

BehaviourState RivalAI::DoesPlayerHavePackage(float dt) {
    if (!player) return Failure;
    // 假设 Player 类有 GetHeldItem 方法 (继承自 GameCharacter)
    GameObject* playerItem = player->GetHeldItem();
    if (playerItem && playerItem->GetName() == "FragilePackage") return Success;
    return Failure;
}

// ================= 行为逻辑 (Actions) =================

BehaviourState RivalAI::RunAwayFromPlayer(float dt) {
    if (!player) return Failure;

    Vector3 playerPos = player->GetTransform().GetPosition();
    Vector3 myPos = GetTransform().GetPosition();
    Vector3 dirAway = (myPos - playerPos);
    dirAway.y = 0; // 忽略高度
    if (Vector::Length(dirAway) < 0.1f) dirAway = Vector3(1, 0, 0); // 防止重叠

    Vector3 targetPos = myPos + Vector::Normalise(dirAway) * 15.0f; // 往反方向跑15米

    // 简单的物理移动，不寻路了，因为是逃跑
    CalculatePath(targetPos); // 尝试寻路去那个反方向的点

    // 如果寻路失败(比如是墙)，直接物理移动
    if (pathPoints.empty()) {
        GetPhysicsObject()->AddForce(Vector::Normalise(dirAway) * 20.0f);
        LookAt(targetPos);
    }
    else {
        // 使用 MoveToTarget 的逻辑片段
        Vector3 next = pathPoints[0];
        Vector3 moveDir = Vector::Normalise(next - myPos);
        moveDir.y = 0;
        GetPhysicsObject()->AddForce(moveDir * 20.0f);
        LookAt(next);
        if (Vector::Length(next - myPos) < 2.0f) pathPoints.erase(pathPoints.begin());
    }

    Debug::DrawLine(myPos, targetPos, Vector4(0, 1, 1, 1)); // 画出逃跑线
    return Ongoing; // 只要一直满足条件就一直跑
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
    // 如果没有金币了，可能意味着要赢了或者去哪？
    return Failure;
}

BehaviourState RivalAI::GetPackageOrCamp(float dt) {
    GameObject* pkg = FindPackage();
    if (pkg && pkg->IsActive()) {
        currentTarget = pkg; // 找到了，去抓
    }
    else {
        // 没找到或者不活跃，去生成点蹲着
        // 为了让 MoveToTarget 工作，我们需要一个临时的目标或者把 targetPos 存起来
        // 既然 MoveToTarget 依赖 currentTarget，我们这里可以设置 currentTarget 为 nullptr
        // 但为了统一，我们可以利用 MoveToTarget 的特殊逻辑，或者创建一个虚拟物体？
        // 更好的方法：MoveToTarget 检查 currentTarget，如果为空，检查是否我们要去 SpawnPos

        currentTarget = nullptr; // 标记为空，意味着去蹲点
    }
    return Success;
}

BehaviourState RivalAI::MoveToTarget(float dt) {
    Vector3 targetPos;

    if (currentTarget) {
        if (!currentTarget->IsActive()) return Failure; // 目标消失
        targetPos = currentTarget->GetTransform().GetPosition();
    }
    else {
        targetPos = packageSpawnPos; // 没有具体物体目标，就去生成点
    }
    //Vector3 myPos = GetTransform().GetPosition();
    float dist = Vector::Length(targetPos - GetTransform().GetPosition());
    // 新增逻辑1
    // 1. 到达判断
    if (dist < 3.0f) {
        GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0)); // 刹车
        return Success; // 到达
    }

    // 2. 寻路刷新 (简单处理：每隔一段距离或路径空了重新算)
    if (pathPoints.empty()) {
        CalculatePath(targetPos);
    }

    // 3. 移动
    if (!pathPoints.empty()) {
        Vector3 nextWaypoint = pathPoints[0];
        Vector3 dir = nextWaypoint - GetTransform().GetPosition();
        dir.y = 0;

        float distToNode = Vector::Length(dir);
        if (distToNode < 3.0f) {
            pathPoints.erase(pathPoints.begin()); // 到了这个点，删掉
            return Ongoing;
        }

        // 物理移动
        GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 20.0f);
        LookAt(nextWaypoint);
    }
    else {
        // 寻路失败（可能在直线上），直接走
        Vector3 dir = (targetPos - GetTransform().GetPosition());
        dir.y = 0;
        GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 20.0f);
        LookAt(targetPos);
    }

    return Ongoing;
}
/*BehaviourState RivalAI::MoveToTarget(float dt) {
    bool usingPath = false;

    // --- 新增逻辑 1: 处理高处的金币 (Vertical Jump) ---
    // 如果水平距离很近，但是目标在头顶，就跳
    float yDiff = targetPos.y - myPos.y;
    float hDist = Vector::Length(Vector3(targetPos.x, 0, targetPos.z) - Vector3(myPos.x, 0, myPos.z));

    // 如果目标比我高 2.0米以上，且水平距离小于 2.0米
    if (yDiff > 2.0f && hDist < 2.0f) {
        LookAt(targetPos); // 抬头看（虽然LookAt只处理水平，但物理上保持朝向）
        GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0)); // 停下来准备起跳
        Jump();
        return Ongoing; // 还在努力中
    }

    // --- 逻辑 2: 到达检测 ---
    if (dist < 3.0f) {
        // 如果高度差不大，才算真的到了 (防止站在金币正下方也算到了)
        if (abs(yDiff) < 2.0f) {
            GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
            return Success;
        }
    }

    // --- 逻辑 3: 寻路 (绕过高墙) ---
    // 如果没有路径或者离当前路径点太远，重新计算
    if (pathPoints.empty()) {
        CalculatePath(targetPos);
    }

    Vector3 moveDir;
    Vector3 nextDest = targetPos; // 默认直奔目标

    if (!pathPoints.empty()) {
        nextDest = pathPoints[0];
        float distToNode = Vector::Length(nextDest - myPos);
        if (distToNode < 3.0f) {
            pathPoints.erase(pathPoints.begin());
            return Ongoing;
        }
        usingPath = true; // 标记我们在使用导航网格
    }

    // --- 逻辑 4: 移动执行 ---
    moveDir = (nextDest - myPos);
    moveDir.y = 0;
    moveDir = Vector::Normalise(moveDir);

    // 施加力移动
    GetPhysicsObject()->AddForce(moveDir * 20.0f);
    LookAt(nextDest);

    // --- 新增逻辑 5: 动态避障 (跳过矮障碍) ---
    // 只有在移动时才检测前方
    CheckObstaclesAndJump(dt);

    return Ongoing;
}*/
BehaviourState RivalAI::AttemptGrab(float dt) {
    if (!currentTarget) return Failure; // 蹲点模式下不需要抓东西

    float dist = Vector::Length(currentTarget->GetTransform().GetPosition() - GetTransform().GetPosition());

    if (dist < 5.0f) {
        LookAt(currentTarget->GetTransform().GetPosition()); // 必须看着它才能Raycast抓到

        // 确保CD过了
        if (actionCooldown <= 0.0f) {
            Vector3 aimDir = GetTransform().GetOrientation() * Vector3(0, 0, 1);
            TryGrab(aimDir); // 父类方法
            actionCooldown = 1.0f; // 重置CD

            if (GetHeldItem() == currentTarget) return Success; // 抓到了！
        }
    }
    return Failure;
}

BehaviourState RivalAI::ThrowAtPlayer(float dt) {
    if (!player) return Failure;

    Vector3 playerPos = player->GetTransform().GetPosition();
    Vector3 myPos = GetTransform().GetPosition();
    float dist = Vector::Length(playerPos - myPos);

    // 1. 接近
    if (dist > 15.0f) {
        Vector3 dir = (playerPos - myPos);
        dir.y = 0;
        GetPhysicsObject()->AddForce(Vector::Normalise(dir) * 20.0f);
        LookAt(playerPos);
        return Ongoing;
    }

    // 2. 攻击
    LookAt(playerPos);
    if (actionCooldown <= 0.0f) {
        Vector3 aimDir = Vector::Normalise((playerPos - myPos));
		ThrowHeldItem(aimDir); // parent method
        actionCooldown = 1.0f;
        return Success;
    }

    return Ongoing;
}

void RivalAI::Jump() {
    if (jumpCooldown <= 0.0f && IsOnGround()) {
        // 施加向上的冲量，数值参考 Player (15.0f)
        GetPhysicsObject()->ApplyLinearImpulse(Vector3(0, 15.0f, 0));
        jumpCooldown = 1.0f; // 1秒冷却
        // 稍微加一点向前的力，防止原地起跳被卡住
        Vector3 fwd = GetTransform().GetOrientation() * Vector3(0, 0, -1);
        GetPhysicsObject()->ApplyLinearImpulse(fwd * 2.0f);
    }
}

bool RivalAI::IsOnGround() {
    // 向下发射射线检测地面
    Vector3 pos = GetTransform().GetPosition();
    Ray ray(pos, Vector3(0, -1, 0));
    RayCollision collision;
    // 1.1f 是腿长+一点缓冲
    if (gameWorld->Raycast(ray, collision, true, this)) {
        if (collision.rayDistance < 1.1f) return true;
    }
    return false;
}

void RivalAI::CheckObstaclesAndJump(float dt) {
    if (jumpCooldown > 0) return;

    Vector3 pos = GetTransform().GetPosition();
    Vector3 fwd = GetTransform().GetOrientation() * Vector3(0, 0, -1); // 假设模型朝向是 -Z，如果是 Z 则用 (0,0,1)

    // 1. 膝盖高度射线 (检测有没有障碍物)
    Vector3 lowOrigin = pos + Vector3(0, 0.5f, 0);
    Ray lowRay(lowOrigin, fwd);
    RayCollision lowCol;

    // 2. 头部高度射线 (检测障碍物是否够矮)
    Vector3 highOrigin = pos + Vector3(0, 2.5f, 0); // 比如障碍物高2米，这里射向2.5米
    Ray highRay(highOrigin, fwd);
    RayCollision highCol;

    bool hitLow = gameWorld->Raycast(lowRay, lowCol, true, this);
    bool hitHigh = gameWorld->Raycast(highRay, highCol, true, this);

    // 逻辑：如果下面撞到了东西(距离近)，但上面没撞到 -> 说明是矮墙 -> 跳！
    if (hitLow && lowCol.rayDistance < 3.0f) {
        if (!hitHigh || highCol.rayDistance > 3.5f) { // 上面是空的，或者很远才有墙
            Jump();
            return;
        }
    }

    // 补充：如果上下都撞到了，那是高墙，导航网格应该处理，但如果卡住了...
    // 这里通常不需要处理，因为 NavigationPath 会规划绕路。
}