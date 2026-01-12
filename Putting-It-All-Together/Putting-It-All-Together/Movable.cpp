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
    // 1. Desired velocity from input
    Vec2 desiredVelocity = Vec2(0, 0);
    if (!dir.IsZero())
        desiredVelocity = dir.Normalized() * MAXIMUM_SPEED;

    // 2. Steering = desired - current
    Vec2 steering = desiredVelocity - velocity;

    float maxAccel = acc;
    if (steering.Length() > maxAccel)
        steering = steering.Normalized() * maxAccel;

    velocity += steering * deltaTime;

    // 3. Damping when no input
    if (dir.IsZero())
    {
        float damping = 0.15f;
        velocity *= std::pow(damping, deltaTime);
    }

    // 4. Clamp speed
    if (velocity.Length() > MAXIMUM_SPEED)
        velocity = velocity.Normalized() * MAXIMUM_SPEED;

    if (velocity.Length() < 5.0f && dir.IsZero())
        velocity = Vec2(0.0f, 0.0f);

    // ---- COLLISIONS ----
    Grid& grid = GameLoop::Instance().GetGrid();

    // Agent collisions
    std::vector<Movable*> movables;
    grid.QueryEnt(position, radius * 2, movables);

    if (shouldCollideWithAgents)
    {
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
            }
        }
    }

    // Wall collisions
    std::vector<PathNode*> obstacles;
    grid.QueryNodes(position, radius * 2, obstacles);

    Vec2 combinedNormal(0, 0);
    float maxPenetration = 0.0f;

    for (const PathNode* o : obstacles)
    {
        if (!o->IsObstacle())
            continue;

        Vec2 closest = ClosestPointOnSquare(position, o->position, o->size);
        float dist = DistanceBetween(position, closest);

        if (dist < radius)
        {
            Vec2 n = (dist > 1e-5f)
                ? (position - closest).Normalized()
                : Vec2(0, -1);

            float penetration = radius - dist;

            combinedNormal += n * penetration;
            maxPenetration = std::max(maxPenetration, penetration);
        }
    }

    if (!combinedNormal.IsZero())
    {
        Vec2 normal = combinedNormal.Normalized();

        // push out once
        position += normal * maxPenetration;

        // remove inward velocity
        float vn = velocity.Dot(normal);
        if (vn < 0.0f)
            velocity -= normal * vn;
    }

    // 5. Move AFTER velocity & collisions
    position += velocity * deltaTime;

    // 6. Facing follows velocity
    if (velocity.Length() > 1e-6f)
    {
        Vec2 desiredDir = velocity.Normalized();

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
    position.x = std::clamp(position.x, radius, WORLD_WIDTH - radius);
    position.y = std::clamp(position.y, radius, WORLD_HEIGHT - radius);

    SetPos(position);
    pushforce = Vec2(0, 0);
}