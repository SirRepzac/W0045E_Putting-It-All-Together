#pragma once
#include <string>
#include "Constants.h"
#include "Logger.h"
#include "Movable.h"


class Player : public Movable
{
public:
	Player(Vec2 pos = Vec2(WORLD_WIDTH / 2, WORLD_HEIGHT / 2));

	void Update(float deltaTime) override;

	std::string GetName() { return "Player"; };

	void SetDirection(Vec2 dir) { direction = dir; };


private:
	std::string name;
};