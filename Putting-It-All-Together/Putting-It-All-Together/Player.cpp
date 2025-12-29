#include "Player.h"

Player::Player(Vec2 pos)
{
	position = pos;
	name = "Player";
	direction = Vec2(0.0f, 1.0f);
	velocity = Vec2(0.0f, 0.0f);
	color = 0x0078C8;
}

void Player::Update(Vec2 dir, float acc, float deltaTime)
{
	Move(dir, acc, deltaTime);
}


