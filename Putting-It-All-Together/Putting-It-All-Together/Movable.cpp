#include "Movable.h"
#include "GameLoop.h"
#include <cmath>

Movable::Movable()
{
	radius = baseRadius;
	weight = radius * radius * PI;
}

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
	Grid& grid = GameLoop::Instance().GetGrid();

	float maxSpeed = MAXIMUM_SPEED / (CELL_SIZE / grid.cellSize);
	float maxAccel = acc / (CELL_SIZE / grid.cellSize);

	// Desired velocity from input
	Vec2 desiredVelocity = Vec2(0, 0);
	if (!dir.IsZero())
		desiredVelocity = dir.Normalized() * maxSpeed;

	// Steering force
	Vec2 steering = desiredVelocity - velocity;

	if (steering.Length() > maxAccel)
		steering = steering.Normalized() * maxAccel;

	velocity += steering * deltaTime;

	// Clamp speed
	if (velocity.Length() > maxSpeed)
		velocity = velocity.Normalized() * maxSpeed;

	if (velocity.Length() < 5.0f && dir.IsZero())
		velocity = Vec2(0.0f, 0.0f);

	// Damping when idle
	if (dir.IsZero())
	{
		float damping = 6; // 1/seconds
		velocity *= std::exp(-damping * deltaTime);
	}


	// Move
	PathNode* node = grid.GetNodeAt(position);
	float surface = node ? SurfaceSpeed(node->type) : 1.0f;

	velocity += pushforce;
	position += velocity * surface * deltaTime;

	pushforce = Vec2(0, 0);

	// ---- COLLISIONS ----
	// Agent collisions
	bool collision = false;
	if (collision)
	{
		std::vector<Movable*> movables;
		grid.QueryEnt(position, grid.cellSize, movables);

		for (Movable* m : movables)
		{
			if (m == this) continue;

			float dist = DistanceBetween(position, m->position);
			float minDist = radius + m->radius;

			if (dist < minDist)
			{
				Vec2 normal = (position - m->position).Normalized();
				float penetration = minDist - dist;

				position += normal * penetration;

				float vn = velocity.Dot(normal);
				if (vn < 0.0f)
					velocity -= normal * vn;

				Vec2 tangent = Vec2(-normal.y, normal.x);
				velocity = tangent * velocity.Dot(tangent);

				float weightDiff = weight - (m->weight / 2);
				if (weightDiff < 0)
					weightDiff = 0;

				m->Push(-normal, -vn * weightDiff);
			}
		}
	}

	// Wall collisions
	std::vector<PathNode*> obstacles;
	grid.QueryNodes(position, grid.cellSize, obstacles);


	Vec2 combinedNormal(0, 0);
	float maxPenetration = 0;

	for (const PathNode* o : obstacles)
	{
		GameLoop::Instance().AddDebugEntity(o->position, 10);
		if (!o->IsObstacle())
			continue;

		Vec2 closest = ClosestPointOnSquare(position, o->position, o->size);
		float dist = DistanceBetween(position, closest);

		if (dist < radius)
		{
			Vec2 dir = (position - closest).Normalized();

			float penetration = radius - dist;

			combinedNormal += dir * penetration;
			maxPenetration = std::max(maxPenetration, penetration);
		}
	}

	if (!combinedNormal.IsZero())
	{
		Vec2 normal = combinedNormal.Normalized();

		// push out
		position += normal * maxPenetration;

		// remove inward velocity
		float vn = velocity.Dot(normal);
		if (vn < 0.0f)
			velocity -= normal * vn;

		Vec2 tangent(-normal.y, normal.x);
		velocity = tangent * velocity.Dot(tangent);
	}

	// 6. Facing follows velocity
	if (dir.Length() > 1e-6f)
	{
		Vec2 desiredDir = dir.Normalized();

		float currentAng = std::atan2(direction.y, direction.x);
		float targetAng = std::atan2(desiredDir.y, desiredDir.x);

		float diff = targetAng - currentAng;
		while (diff > PI) diff -= 2 * PI;
		while (diff < -PI) diff += 2 * PI;

		float maxTurn = 8.0f * deltaTime;
		float newAng = currentAng + std::clamp(diff, -maxTurn, maxTurn);

		direction = Vec2(std::cos(newAng), std::sin(newAng));
	}

	// World bounds
	Vec2 normal(0, 0);
	if (position.x < radius)
	{
		position.x = radius;
		normal += Vec2(1, 0);
	}
	else if (position.x > WORLD_WIDTH - radius)
	{
		position.x = WORLD_WIDTH - radius;
		normal += Vec2(-1, 0);
	}

	if (position.y < radius)
	{
		position.y = radius;
		normal += Vec2(0, 1);
	}
	else if (position.y > WORLD_HEIGHT - radius)
	{
		position.y = WORLD_HEIGHT - radius;
		normal += Vec2(0, -1);
	}

	if (!normal.IsZero())
	{
		normal = normal.Normalized();

		float vn = velocity.Dot(normal);
		if (vn < 0.0f)
			velocity -= normal * vn;
	}

	SetPos(position);
}