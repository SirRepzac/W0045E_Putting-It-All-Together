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
	End
};


	// Constructor
	PathNode() : position(Vec2(0, 0)) {}

	int id = -1; // Id of the node
	Vec2 position; // Position of the node

	std::vector<PathNode*> neighbors; // Adjecent nodes

	float size = 0; // radius of this node

	Type type = Nothing;

	char displayLetter = ' ';

	uint32_t color = 0xFFFFFFFF; // default white

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

	// Draw the cell of this node
	// --------------------------
	// c - the color in which to draw this node
	Renderer::Entity Draw()
	{
		Vec2 bottomLeft = position - Vec2(size, size);
		Vec2 topRight = position + Vec2(size, size);
		return Renderer::Entity::MakeRect(bottomLeft.x, bottomLeft.y, topRight.x, topRight.y, color, true, 2.0f, displayLetter);
	}
};

struct NodeRecord
{
	float gCost = std::numeric_limits<float>::infinity();
	float hCost = 0.0f;
	float fCost = std::numeric_limits<float>::infinity();
	PathNode* parent = nullptr;
};