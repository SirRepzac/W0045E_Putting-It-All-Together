#include "Movable.h"
#include "GameLoop.h"
#include <cmath>

void Movable::Update(float deltaTime)
{
	GameLoop::Instance().GetGrid().UpdateMovable(this);
}

void Movable::Push(Vec2 dir, float force)
{
	float invWeight = 1 / weight;
	pushforce = dir * force * invWeight;
}

void Movable::Move(Vec2 dir, float acc, float deltaTime)
{
	Vec2 acceleration = (dir + direction) / 2 * acc;

	velocity += acceleration * deltaTime;

	position += velocity * deltaTime;

	if (!dir)
	{
		float damping = 0.05f; // per-second damping factor
		// apply damping scaled to the current timestep
		velocity = velocity * std::pow(damping, deltaTime);
	}

	if (velocity.Length() > MAXIMUM_SPEED)
	{
		velocity = velocity.Normalized() * MAXIMUM_SPEED;
	}

	if (velocity.Length() < 5.0f && !dir)
	{
		velocity = Vec2(0.0f, 0.0f);
	}

	float speed = velocity.Length();

	if (velocity.Length() > 1e-6f)
	{
		Vec2 desired = (velocity + dir * (MAXIMUM_SPEED / 4)).Normalized();

		if (direction.IsZero())
		{
			direction = desired;
		}
		else
		{
			// convert angles
			float currentAng = std::atan2(this->direction.y, this->direction.x);
			float targetAng = std::atan2(desired.y, desired.x);

			// shortest angular difference in [-PI, PI]
			float diff = targetAng - currentAng;
			while (diff > PI) diff -= 2.0f * PI;
			while (diff < -PI) diff += 2.0f * PI;

			// turn rate in radians per second (tweakable)
			const float TURN_RATE = 8.0f;
			float maxTurn = TURN_RATE * deltaTime;

			float newAng;
			if (std::fabs(diff) <= maxTurn)
				newAng = targetAng;
			else
				newAng = currentAng + (diff > 0 ? maxTurn : -maxTurn);

			this->direction = Vec2(std::cos(newAng), std::sin(newAng));
		}

		velocity = direction * speed;
	}

	Grid& grid = GameLoop::Instance().GetGrid();

	std::vector<Movable*> movables;
	grid.QueryEnt(position, radius * 2, movables);

	if (shouldCollideWithAgents)
		for (Movable* m : movables)
		{

			if (m == this)
				continue;

			float dist = DistanceBetween(position, m->position);
			float minDist = (radius + m->radius);
			if (dist < minDist)
			{
				const float eps = 1e-5f;

				// compute collision normal (from other to this)
				Vec2 normal;
				if (dist > eps)
				{
					normal = (position - m->position).Normalized();
				}

				float penetration = radius + m->radius - dist;
				// push outside
				position += normal * penetration;

				Vec2 dir = (m->position - position).Normalized();
				m->Push(dir, penetration * speed * 0.15f * weight);
			}
		}

	position += pushforce * deltaTime;

	std::vector<PathNode*> obstacles;
	grid.QueryNodes(position, radius * 2, obstacles);

	for (const PathNode* o : obstacles)
	{
		if (!o->IsObstacle())
			continue;

		Vec2 closestPos = ClosestPointOnSquare(position, o->position, o->size);

		float dist = DistanceBetween(position, closestPos);

		// If inside wall, push out
		if (dist < radius)
		{
			// compute a safe normal
			Vec2 normal;
			const float eps = 1e-5f;
			if (dist > eps)
			{
				normal = (position - closestPos).Normalized();
			}
			else
			{
				// rare case: exactly on edge/center -> pick a normal away from rect center
				Vec2 rectCenter = o->position;
				normal = (position - rectCenter).Normalized();
				if (normal.IsZero())
					normal = Vec2(0.0f, -1.0f);
			}

			float penetration = radius - dist;
			// push outside
			position += normal * penetration;
		}
	}

	if (position.x - radius < 0)
		position.x = radius;
	else if (position.x + radius > WORLD_WIDTH)
		position.x = WORLD_WIDTH - radius;
	if (position.y - radius < 0)
		position.y = radius;
	else if (position.y + radius > WORLD_HEIGHT)
		position.y = WORLD_HEIGHT - radius;

	SetPos(position);

	pushforce = Vec2(0, 0);
}