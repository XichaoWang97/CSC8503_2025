#pragma once
#include "GameObject.h"
#include "RenderObject.h"

namespace NCL {
    namespace CSC8503 {
        class FragileGameObject : public GameObject {
        public:
            FragileGameObject(const std::string& name, const Vector3& position,
                Rendering::Mesh* mesh, GameTechMaterial material, Vector4 colour);

            ~FragileGameObject();

			// collision event
            virtual void OnCollisionBegin(GameObject* otherObject) override;

            bool IsBroken() const { return isBroken; }
            float GetHealth() const { return health; }

            void Reset();
            virtual void Update(float dt) override;

        protected:
            float health;
            float maxHealth;
            bool isBroken;
            float timer = 0.0f;
			Vector3 initialPosition;
        };
    }
}