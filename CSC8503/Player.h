#pragma once
#include "GameCharacter.h"
#include "Window.h"
#include "RenderObject.h"
using namespace NCL;

namespace NCL::CSC8503 {
    class Rendering::Mesh;

    // define a struct about player input
    struct PlayerInputs {
        bool isMoving = false;
        bool jump = false;
        bool attack = false; // Grab/Throw
        Vector3 axis = Vector3(0, 0, 0);
        float cameraYaw = 0.0f;
    };

    class Player : public GameCharacter {
    public:
        Player(GameWorld* world);
        ~Player();

        void Update(float dt) override;
        void SetIgnoreInput(bool ignore) { ignoreInput = ignore; } // We should ignore input in NetworkedGame Mode
        bool GetIgnoreInput() { return ignoreInput; }
        void SetPlayerInput(const PlayerInputs& inputs); // for networked game
        bool IsDead() const { return isDead; }
        void SetDead(bool dead) { isDead = dead; }

    private:
        // Jump Synchronization For Networked Game
        float jumpBufferTime = 0.0f;           // Jump input buffer
        float jumpCooldown = 0.0f;             // Jump cooldown timer
        bool wasOnGround = false;              // Was on ground last frame
        const float JUMP_BUFFER_DURATION = 0.15f;   // 150ms buffer window
        const float JUMP_COOLDOWN_DURATION = 0.3f;  // 300ms cooldown duration

        virtual void PlayerControl(float dt);
        bool IsPlayerOnGround();
        bool ignoreInput = false;
        bool isDead = false;
        PlayerInputs currentInputs;
    };
}