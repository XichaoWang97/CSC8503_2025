#pragma once
#include "BehaviourNodeWithChildren.h"

class BehaviourSequence : public BehaviourNodeWithChildren {
public:
	BehaviourSequence(const std::string& nodeName) : BehaviourNodeWithChildren(nodeName) {}
	~BehaviourSequence() {}
	BehaviourState Execute(float dt) override {
		//std::cout << "Executing sequence " << name << "\n";
		for (auto& i : childNodes) {
			BehaviourState nodeState = i->Execute(dt);
			switch (nodeState) {
				case Success: continue;
				case Failure:
				case Ongoing:
				{
					currentState = nodeState;
					return nodeState;
				}
			}
		}
		return Success;
	}
};

/*class BehaviourSequence : public BehaviourNode {
public:
    BehaviourSequence(const std::string& nodeName) : BehaviourNode(nodeName) {}
    ~BehaviourSequence() {
        for (auto& i : childNodes) delete i;
    }
    void AddChild(BehaviourNode* n) { childNodes.emplace_back(n); }

    BehaviourState Execute(float dt) override {
        for (auto& i : childNodes) {
            BehaviourState nodeState = i->Execute(dt);
            switch (nodeState) {
                case Success: continue;
                case Failure:
                case Ongoing:
                {
                    currentState = nodeState;
                    return currentState;
                }
            }
        }
        return Success;
    }
    void Reset() override {
        currentState = Initialise;
        for (auto& i : childNodes) i->Reset();
    }
protected:
    std::vector<BehaviourNode*> childNodes;
};*/