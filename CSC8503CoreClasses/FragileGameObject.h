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

			void SetAttached(bool attached) { isAttached = attached; }
            bool GetAttached() const { return isAttached; }
			int GetCollectionCount() const { return collectionCount; }
			void IncreaseCollectionCount() { collectionCount++; }

        protected:
            float health;
            float maxHealth;
            bool isBroken;
			bool isAttached;
            float timer = 0.0f;
			int collectionCount;
			Vector3 initialPosition;
        };
    }
}