#pragma once
#include <iostream>
#include <string>
#include "Constants.h"

class Vec2
{
public:
	union
	{
		float vect[2];
		struct
		{
			float x;
			float y;
		};
	};

	Vec2() : x(0.0f), y(0.0f) {}
	Vec2(float x, float y) : x(x), y(y) {}
	Vec2(float vec[2]) : x(vec[0]), y(vec[1]) {}

	bool operator==(const Vec2& other) const {  return (x == other.x && y == other.y); }

	bool operator!=(const Vec2& other) const { return !(*this == other); }

	Vec2 operator* (float const scalar) const { return Vec2(x * scalar, y * scalar); }

	Vec2 operator+ (const Vec2& other) const { return Vec2(x + other.x, y + other.y); }

	Vec2 operator- (const Vec2& other) const { return Vec2(x - other.x, y - other.y); }

	Vec2 operator- () const { return Vec2(-x, -y); };

	Vec2& operator+= (const Vec2& other)
	{
		x += other.x;
		y += other.y;
		return *this;
	}

	Vec2& operator-= (const Vec2& other)
	{
		x -= other.x;
		y -= other.y;
		return *this;
	}

	Vec2& operator*= (const float& scalar)
	{
		x *= scalar;
		y *= scalar;
		return *this;
	}

	static float DistanceBetween(Vec2 pos1, Vec2 pos2);
	static Vec2 DirectionBetween(Vec2 from, Vec2 to);


	std::string ToString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }

	float Length() const;

	float Dot(const Vec2& other) const
	{
		return x * other.x + y * other.y;
	}

	static float Dot(const Vec2& a, const Vec2& b)
	{
		return a.x * b.x + a.y * b.y;
	}

	bool IsZero() { return (this->x == 0 && this->y == 0); }

	Vec2 Normalized() const;

	explicit constexpr operator bool() const noexcept
	{
		return x != 0.0f || y != 0.0f;
	}

	Vec2 Rotated(float amount, bool usingRad = true)
	{
		float radians = amount;

		if (!usingRad)
			radians = DegToRad(amount);

		float c = cosf(radians);
		float s = sinf(radians);
		return Vec2(
			x * c - y * s,
			x * s + y * c
		);
	}
private:

};

static Vec2 DirectionBetween(Vec2 from, Vec2 to)
{
	// Compute wrapped deltas
	float dx = to.x - from.x;
	float dy = to.y - from.y;

	Vec2 dir(dx, dy);
	return dir.Normalized();
}

static Vec2 ClosestPointOnSquare(Vec2 point, Vec2 squarePos, float squareRadius)
{
	float distx = point.x - squarePos.x;
	float disty = point.y - squarePos.y;
	float c_distx = std::max<float>(-squareRadius, std::min<float>(squareRadius, distx));
	float c_disty = std::max<float>(-squareRadius, std::min<float>(squareRadius, disty));
	return Vec2(squarePos.x + c_distx, squarePos.y + c_disty);
}

static float DistanceBetween(Vec2 from, Vec2 to)
{
	float dx = to.x - from.x;
	float dy = to.y - from.y;

	if (dx == 0 && dy == 0)
		return 0.0f;

	return std::sqrt(dx * dx + dy * dy);
}

// fov in rad
static float IsDirWithinFOV(Vec2 forward, float fov, Vec2 dir)
{
	float valFromFOV = cos(fov / 2);

	if (forward.Dot(dir) > valFromFOV)
		return true;
	return false;
}

static bool SegIntersect(const Vec2& p, const Vec2& p2, const Vec2& q, const Vec2& q2, Vec2& out)
{
	Vec2 segment1 = p2 - p;
	Vec2 segment2 = q2 - q;

	// cross products
	float rxs = segment1.x * segment2.y - segment1.y * segment2.x;
	Vec2 qp = q - p;
	float qpxr = qp.x * segment1.y - qp.y * segment1.x;

	const float eps = 1e-6f;
	if (std::fabs(rxs) < eps)
		return false; // parallel

	float t = (qp.x * segment2.y - qp.y * segment2.x) / rxs;
	float u = qpxr / rxs;

	if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f)
	{
		out = p + segment1 * t;
		return true;
	}
	return false;
}