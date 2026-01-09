#pragma once
#include "Vec2.h"
#include "Constants.h"
#include "Renderer.h"


class PathNode
{
public:
	// Types of special nodes
enum Type
{
	Nothing = -1,
	Start = 0,
	Wall,
	Wood,
	Coal,
	Iron,
	End,
	Special
};


	// Constructor
	PathNode() : position(Vec2()) {}

	int id = -1; // Id of the node
	Vec2 position; // Position of the node

	float hitpoints = 10;

	std::vector<PathNode*> neighbors; // Adjecent nodes

	float size = 0; // radius of this node

	Type type = Nothing;

	char displayLetter = ' ';

	uint32_t color = 0xFFFFFFFF; // default white

	float clearance = 0;

	bool IsObstacle() const
	{
		if (type == Wall)
			return true;
		else return false;
	}

	bool operator==(const PathNode& other) const
	{
		return id == other.id;
	}
};

struct NodeRecord
{
	float gCost = std::numeric_limits<float>::infinity();
	float hCost = 0.0f;
	float fCost = std::numeric_limits<float>::infinity();
	PathNode* parent = nullptr;
};