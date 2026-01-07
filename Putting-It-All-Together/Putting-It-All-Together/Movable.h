#pragma once
#include <iostream>
#include <string>
#include "Constants.h"
#include "Vec2.h"

class Movable
{
public:
	virtual void Update(float deltaTime);
	Vec2 GetPosition() { return position; };
	Vec2 GetVelocity() { return velocity; };
	Vec2 GetDirection() { return direction; };
	float GetRadius() { return radius; };
	virtual float GetSpeed() { return velocity.Length(); };
	void SetPos(Vec2 pos) { position = pos; };

	void SetVelocity(Vec2 vel) { velocity = vel; }

	std::string GetName() { return name; }

	void Push(Vec2 dir, float force);

	uint32_t GetColor() { return color; }

	int cellX = -1;
	int cellY = -1;

protected:

	void Move(Vec2 dir, float acc, float deltaTime);
	uint32_t color;
	Vec2 velocity;
	Vec2 position;
	Vec2 direction;
	float radius = DEFAULT_CELL_SIZE;
	float weight = radius * radius * PI;
	std::string name;

	Vec2 pushforce;

	bool shouldCollideWithAgents = false;

};