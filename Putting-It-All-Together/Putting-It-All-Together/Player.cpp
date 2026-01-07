#include "Player.h"

Player::Player(Vec2 pos)
{
	position = pos;
	name = "Player";
	direction = Vec2(0.0f, 1.0f);
	velocity = Vec2(0.0f, 0.0f);
	color = 0x0078C8;
}

void Player::Update(float deltaTime)
{
	Move(direction, MAXIMUM_ACCELERATION, deltaTime);

	Movable::Update(deltaTime);
}


