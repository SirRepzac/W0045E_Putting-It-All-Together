#pragma once
#include <iostream> 
#include <string>
#include "Constants.h"
#include "Vec2.h"
#include "Renderer.h"
#include "Movable.h"
#include "GameAI.h"
#include "PathNode.h"

class GameAI;

class Behaviour
{
public:

    struct Info
    {
        Vec2 direction = Vec2(0, 0);
        float acceleration = 0;
    };

    Behaviour(GameAI* parentAI);
    ~Behaviour();

    Info Flee(Vec2 from);
    Info Flee();
    Info Seek(Vec2 target);
    Info Seek();
    Info Arrive(float deltaTime, Vec2 target);
    Info Arrive(float deltaTime);
    Info Wander();
    Info Evade(float deltaTime, Movable* from = nullptr, float predictionTime = 0.75f);
    Info Evade(float deltaTime, float predictionTime);
    Info Persue(float deltaTime, Movable* target = nullptr, float predictionTime = 0.75f);
    Info Persue(float deltaTime, float predictionTime);
    Info FollowPath(float deltaTime);

    Info AgentAvoidance(float deltaTime, GameAI::State state);
    Info WallAvoidance(float deltaTime, GameAI::State state);
    Info Separation(float deltaTime, GameAI::State state);

    Info Update(float deltaTime, GameAI::State state);

    Vec2 PredictFuturePosition(Movable* target, float inHowLong = 1.0f);
    Vec2 PickRandomDirection();
    void DrawDebugTarget();

    bool AtTarget();
    bool GetDebugTarget(Vec2& out) { (void)out; return false; }

    void SetPath(std::vector<PathNode*> path) { this->path = path; }

    PathNode* GetDestinationNode()
    {
        if (path.empty())
            return nullptr;
        return path.back();
	}

private:
    void UpdateLoggerWithDiscrepancies(GameAI::State state);

    std::vector<PathNode*> path;

    GameAI* ai = nullptr;
    GameAI::State previousState = GameAI::State::STATE_IDLE;
};


