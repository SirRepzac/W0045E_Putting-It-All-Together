#include "Vec2.h"
#include <iostream> 
#include <cmath>

float Vec2::DistanceBetween(Vec2 pos1, Vec2 pos2)
{
	float dx = pos2.x - pos1.x;
	float dy = pos2.y - pos1.y;
	return sqrtf(dx * dx + dy * dy);
}

Vec2 Vec2::DirectionBetween(Vec2 from, Vec2 to)
{
	return (from - to).Normalized();
}

float Vec2::Length() const
{
	if (x == 0 && y == 0)
		return 0;

	return sqrtf(x * x + y * y);
}

Vec2 Vec2::Normalized() const
{

	float length = Length();

	if (length != 0.0f)
	{
		return Vec2(x / length, y / length);
	}
	return Vec2();
}

