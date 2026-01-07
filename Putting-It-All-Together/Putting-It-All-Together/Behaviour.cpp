#include <iostream> 
#include <cmath>
#include "Behaviour.h"
#include "GameAI.h"
#include "Logger.h"
#include "GameLoop.h"

Behaviour::Behaviour(GameAI* parentAI)
{
	ai = parentAI;
	
}

Behaviour::~Behaviour()
{
	ai = nullptr;
}

Behaviour::Info Behaviour::Flee(Vec2 f)
{
	Vec2 from = f;

	float acceleration = MAXIMUM_ACCELERATION;
	Vec2 direction = DirectionBetween(from, ai->GetPosition());
	return Info{ direction, acceleration };
}

Behaviour::Info Behaviour::Flee()
{
	return Flee(ai->GetTarget());
}

Behaviour::Info Behaviour::Seek(Vec2 t)
{
	Vec2 target = t;

	float acceleration = MAXIMUM_ACCELERATION;
	Vec2 direction = DirectionBetween(ai->GetPosition(), target);
	return Info{ direction, acceleration };
}
Behaviour::Info Behaviour::Seek()
{
	return Flee(ai->GetTarget());
}

Behaviour::Info Behaviour::Arrive(float deltaTime, Vec2 t)
{
	Vec2 target = t;

	Vec2 toTarget = target - ai->GetPosition();
	float dist = toTarget.Length();

	// quick outs
	if (dist <= 0.001f)
		return Info{ Vec2(0,0), 0.0f };

	// If far, behave like Seek
	const float arrivalRadius = 50.0f;
	if (dist > arrivalRadius)
	{
		return Seek(target);
	}

	// If already very close, stop
	if (dist < (arrivalRadius / 5.0f) || dist < ai->GetRadius() / 2)
	{
		return Info{ Vec2(0,0), 0.0f };
	}

	// Compute desired speed so we will come to rest at the target using
	// v = sqrt(2 * a * d), where 'a' is the maximum deceleration we can apply.
	// Cap desired speed by MAXIMUM_SPEED.
	const float maxDecel = MAXIMUM_ACCELERATION;
	float desiredSpeed = std::sqrt(2.0f * maxDecel * dist);
	if (std::isnan(desiredSpeed) || desiredSpeed < 0.0f)
		desiredSpeed = 0.0f;
	desiredSpeed = std::min(desiredSpeed, MAXIMUM_SPEED);

	float currentSpeed = ai->GetSpeed();

	// Required acceleration (signed) to move speed -> desiredSpeed this frame:
	// a_required = (desiredSpeed - currentSpeed) / deltaTime
	float aRequired = (desiredSpeed - currentSpeed) / deltaTime;

	// Clamp magnitude to allowed acceleration
	float aMag = std::min(std::fabs(aRequired), MAXIMUM_ACCELERATION);

	// Determine direction for acceleration:
	// - If we need to slow down (aRequired < 0) accelerate opposite to current velocity.
	// - Otherwise accelerate toward the target.
	Vec2 accDir;
	if (aRequired < 0.0f)
	{
		Vec2 vel = ai->GetVelocity();
		if (vel.Length() > 1e-6f)
			accDir = vel.Normalized() * -1.0f; // brake opposite current movement
		else
			accDir = toTarget.Normalized(); // fallback
	}
	else
	{
		accDir = toTarget.Normalized();
	}

	return Info{ accDir, aMag };
}

Behaviour::Info Behaviour::Arrive(float deltaTime)
{
	return Arrive(deltaTime, ai->GetTarget());
}

Behaviour::Info Behaviour::Wander()
{

	Vec2 target = ai->GetTarget();

	if (previousState != GameAI::State::STATE_WANDER)
		target = ai->GetPosition();

	float wanderRadius = ai->GetRadius() * 1.75f;
	float distFromAI = wanderRadius + ai->GetRadius() * 3;

	if (AtTarget() || !target)
	{
		Vec2 direction = PickRandomDirection();
		Vec2 newTarget = ai->GetPosition() + ai->GetDirection() * distFromAI + direction * wanderRadius;

		bool shouldRecalculate = false;
		Vec2 adjDir;


		adjDir = ai->GetDirection();
		if (newTarget.x <= ai->GetRadius())
		{
			adjDir.x = -adjDir.x;
			shouldRecalculate = true;
		}
		if (newTarget.x >= WORLD_WIDTH - ai->GetRadius())
		{
			adjDir.x = -adjDir.x;
			shouldRecalculate = true;

		}
		if (newTarget.y <= ai->GetRadius())
		{
			adjDir.y = -adjDir.y;
			shouldRecalculate = true;

		}
		if (newTarget.y >= WORLD_HEIGHT - ai->GetRadius())
		{
			adjDir.y = -adjDir.y;
			shouldRecalculate = true;
		}

		Grid& grid = GameLoop::Instance().GetGrid();

		if (!shouldRecalculate && !grid.HasLineOfSight(ai->GetPosition(), newTarget, ai->GetRadius()))
		{
			adjDir *= cos(PI);
			shouldRecalculate = true;
		}


		if (shouldRecalculate)
			newTarget = ai->GetPosition() + adjDir * distFromAI + direction * wanderRadius;

		ai->SetTarget(newTarget);
	}

	// draw circle where wander target can spawn
	if (GameLoop::Instance().DEBUG_MODE)
	{
		GameLoop::Instance().AddDebugEntity(ai->GetPosition() + ai->GetDirection() * distFromAI, Renderer::Color(0, 0, 200)/*blue*/, wanderRadius, false);
	}

	return Seek(target);
}

Behaviour::Info Behaviour::Evade(float deltaTime, Movable* from, float predictionTime)
{
	return Flee(PredictFuturePosition(from, predictionTime));
}
Behaviour::Info Behaviour::Evade(float deltaTime, float predictionTime)
{
	return Evade(deltaTime, ai->GetMovingTarget(), predictionTime);
}

Behaviour::Info Behaviour::Persue(float deltaTime, Movable* target, float predictionTime)
{
	return Seek(PredictFuturePosition(target, predictionTime));
}
Behaviour::Info Behaviour::Persue(float deltaTime, float predictionTime)
{
	return Persue(deltaTime, ai->GetMovingTarget(), predictionTime);
}

Behaviour::Info Behaviour::FollowPath(float deltaTime)
{
	GameAI* ai = dynamic_cast<GameAI*>(this->ai);

	if (path.empty())
	{
		ai->SetState(GameAI::State::STATE_IDLE);
		return Info{ Vec2(0,0), 0.0f };
	}

	GameLoop& game = GameLoop::Instance();
	if (game.DEBUG_MODE)
	{
		game.AddDebugEntity(path.front()->position, Renderer::Red, path.front()->size * 0.5f);
		for (int i = 0; i <= path.size(); i++)
		{
			int j = i;
			Vec2 prev;
			if (i == path.size())
			{
				prev = ai->GetPosition();
				j -= 1;
			}
			else if (i != 0)
				prev = path[i - 1]->position;
			if (prev)
				game.AddDebugLine(path[j]->position, prev, Renderer::Blue);
		}
	}

	if (DistanceBetween(ai->GetPosition(), path.back()->position) < ai->GetRadius())
	{
		path.pop_back();
	}

	if (path.empty())
	{
		return Info{ Vec2(0,0), 0.0f };
	}

	Grid& grid = game.GetGrid();

	// If the AI has lost the path
	if (!grid.HasLineOfSight(ai->GetPosition(), path.back()->position, ai->GetRadius()))
	{
		PathNode* dest = path.front();
		path.clear();
		bool valid = true;
		ai->GoTo(dest, valid);
	}

	// LOS smoothing
	bool foundObstructed = false;
	while (!foundObstructed && path.size() > 1)
	{
		if (grid.HasLineOfSight(ai->GetPosition(), path.at(path.size() - 2)->position, ai->GetRadius()))
		{
			path.pop_back();
		}
		else
		{
			foundObstructed = true;
		}
	}

	if (path.size() == 1)
		return Arrive(deltaTime, path.back()->position);
	if (!path.empty())
		return Seek(path.back()->position);
	return Info();
}

Behaviour::Info Behaviour::AgentAvoidance(float deltaTime, GameAI::State state)
{
	if (state == GameAI::State::STATE_IDLE)
		return Info();

	Vec2 pos = ai->GetPosition();
	Vec2 forward = ai->GetDirection();

	// in degrees
	float fov = 90.0f;

	Vec2 steering(0, 0);

	GameLoop& game = GameLoop::Instance();
	float detectionRadius = 75;

	if (game.DEBUG_MODE)
	{
		float halfFov = DegToRad(fov) * 0.5f;

		Vec2 leftDir = forward.Rotated(halfFov);
		Vec2 rightDir = forward.Rotated(-halfFov);

		game.AddDebugLine(pos, pos + leftDir * detectionRadius, Renderer::Color(200, 0, 0));
		game.AddDebugLine(pos, pos + rightDir * detectionRadius, Renderer::Color(200, 0, 0));
	}

	std::vector<Movable*> agents;
	game.GetGrid().QueryEnt(pos, detectionRadius + ai->GetRadius() * 0.5f, agents);

	for (Movable* n : agents)
	{
		if (n == ai) continue;

		Vec2 dir = DirectionBetween(pos, n->GetPosition());

		if (!IsDirWithinFOV(forward, DegToRad(fov), dir))
			continue;

		float dist = DistanceBetween(pos, n->GetPosition());

		if (dist < detectionRadius)
		{
			Vec2 diff = pos - n->GetPosition();

			float dist = diff.Length() - ai->GetRadius() - n->GetRadius();
			if (dist == 0)
				continue;

			// strength scales with proximity (closer -> stronger)
			float strength = (detectionRadius - dist) / detectionRadius; // 0..1
			
			steering += diff.Normalized() * strength;
		}
	}

	if (steering.IsZero())
		return Info();


	return Info{ steering, MAXIMUM_ACCELERATION };
}

Behaviour::Info Behaviour::WallAvoidance(float deltaTime, GameAI::State state)
{

	if (state == GameAI::State::STATE_IDLE)
		return Info();

	Vec2 pos = ai->GetPosition();
	GameLoop& game = GameLoop::Instance();

	Vec2 steering(0.0f, 0.0f);

	const float detectionRadius = ai->GetRadius() * 0.5f;
	Vec2 forward = ai->GetDirection();

	std::vector<PathNode*> obstacles;
	game.GetGrid().QueryNodes(pos, detectionRadius + 10, obstacles);

	for (const PathNode* so : obstacles)
	{

		if (!so->IsObstacle())
			continue;

		Vec2 diff = pos - so->position;

		float dist = diff.Length() - ai->GetRadius() - so->size * 0.5f;

		if (dist == 0)
			continue;
		// strength scales with proximity (closer -> stronger)
		float strength = (detectionRadius - dist) / detectionRadius; // 0..1

		steering += diff.Normalized() * strength;

		if (game.DEBUG_MODE)
		{
			Vec2 closest = ClosestPointOnSquare(pos, so->position, so->size);
			// show closest point and line to it
			game.AddDebugEntity(closest, Renderer::Color(0, 200, 0), 3, true);
			game.AddDebugLine(pos, closest, Renderer::Color(0, 200, 0));
		}
	}

	if (steering.IsZero())
		return Info();

	return Info{ steering, MAXIMUM_ACCELERATION };
}

Behaviour::Info Behaviour::Separation(float deltaTime, GameAI::State state)
{
	if (state == GameAI::State::STATE_IDLE)
		return Info();

	Vec2 pos = ai->GetPosition();
	Vec2 forward = ai->GetDirection();

	GameLoop& game = GameLoop::Instance();
	float detectionRadius = 40.0f;

	std::vector<Movable*> neighbors;
	game.GetGrid().QueryEnt(pos, detectionRadius + 10, neighbors);

	Vec2 steering(0, 0);

	for (Movable* n : neighbors)
	{
		if (n == ai) continue;

		float dist = DistanceBetween(pos, n->GetPosition());
		if (dist < detectionRadius)
		{
			Vec2 dir = Evade(deltaTime, n, dist * 0.002f).direction;

			// Avoid going backwards
			if (forward.Dot(dir) < 0)
			{
				dir = dir * 0.75;
			}

			// strength scales with proximity (closer -> stronger)
			float strength = (detectionRadius - dist) / detectionRadius; // 0..1

			steering += dir * strength;
		}
	}

	if (steering.IsZero())
		return Info();

	return Info{ steering, MAXIMUM_ACCELERATION };
}

Behaviour::Info Behaviour::Update(float deltaTime, GameAI::State state)
{	
	//UpdateLoggerWithDiscrepancies(state);
	previousState = state;

	GameAI::State currentState = state;
	switch (currentState)
	{
		case GameAI::State::STATE_IDLE:
			return Info{ Vec2(0,0), 0.0f };
		case GameAI::State::STATE_SEEK:
			DrawDebugTarget();
			return Seek();
		case GameAI::State::STATE_FLEE:
			DrawDebugTarget();
			return Flee();
		case GameAI::State::STATE_ARRIVE:
			DrawDebugTarget();
			return Arrive(deltaTime);
		case GameAI::State::STATE_WANDER:
			DrawDebugTarget();
			return Wander();
		case GameAI::State::STATE_EVADE:
			DrawDebugTarget();
			return Evade(deltaTime);
		case GameAI::State::STATE_PURSUE:
			DrawDebugTarget();
			return Persue(deltaTime);
		case GameAI::State::STATE_FOLLOW_PATH:
			return FollowPath(deltaTime);
		default:
			return Info{ Vec2(0,0), 0.0f };
	}
}

Vec2 Behaviour::PickRandomDirection()
{
	// random angle in [0, 2pi)
	float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	float angle = r * PI * 2;
	Vec2 wanderDirection = Vec2(std::cos(angle), std::sin(angle));
	return wanderDirection;
}

Vec2 Behaviour::PredictFuturePosition(Movable* target, float inHowLong)
{
	Vec2 targetPos = target->GetPosition();
	Vec2 targetVel = target->GetVelocity();
	float targetSpeed = targetVel.Length();

	Vec2 predictedTargetPosition = Vec2(targetPos.x + targetVel.x * inHowLong,
		targetPos.y + targetVel.y * inHowLong);

	return predictedTargetPosition;
}

void Behaviour::DrawDebugTarget()
{
	if (GameLoop::Instance().DEBUG_MODE)
		GameLoop::Instance().AddDebugEntity(ai->GetTarget(), Renderer::Color(0, 200, 0)/*green*/, 4);
}

void Behaviour::UpdateLoggerWithDiscrepancies(GameAI::State state)
{
	if (state == previousState)
		return;

	std::string stateStr = ToString(state);
	std::string endStr = "";
	bool shouldLog = false;
	std::vector<GameAI::State> movingTargetDependant = { GameAI::State::STATE_EVADE, GameAI::State::STATE_PURSUE };
	std::vector<GameAI::State> pathDependant = { GameAI::State::STATE_FOLLOW_PATH };
	std::vector<GameAI::State> staticTargetDependant = { GameAI::State::STATE_SEEK, GameAI::State::STATE_FLEE, GameAI::State::STATE_ARRIVE };

	for (GameAI::State s : movingTargetDependant)
	{
		if (s == state && !ai->GetMovingTarget())
		{
			shouldLog = true;
			endStr += "moving ";
			break;
		}
	}
	for (GameAI::State s : pathDependant)
	{
		if (s == state && !GameLoop::Instance().pathfinder)
		{
			shouldLog = true;
			endStr += "pathfinder ";
			break;
		}
	}
	for (GameAI::State s : staticTargetDependant)
	{
		if (s == state && ai->GetTarget().IsZero() || !ai->GetTarget())
		{
			shouldLog = true;
			endStr += "static ";
			break;
		}
	}

	Logger::Instance().Log(ai->GetName() + " performing " + stateStr + " without having a valid " + endStr + "target. \n");
}

bool Behaviour::AtTarget()
{
	 return (DistanceBetween(ai->GetPosition(), ai->GetTarget()) < 20.0f);
}