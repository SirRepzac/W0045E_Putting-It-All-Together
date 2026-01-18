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
	Grass,
	Rock,
	Water,
	Swamp,
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
		if (type == Rock || type == Water)
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

static uint32_t NodeColor(PathNode::Type type)
{
	if (type == PathNode::Nothing)
		return 0xFFFFFF; // white
	if (type == PathNode::Grass)
		return 0x00BF00; // green
	if (type == PathNode::Water)
		return 0x0000FF; // blue
	else if (type == PathNode::Swamp)
		return 0x003917; // dark green
	else if (type == PathNode::Wood)
		return 0x5E3500; // brown
	else if (type == PathNode::Coal)
		return 0x000000; // black
	else if (type == PathNode::Iron)
		return 0xC0C0C0; // silver
	else if (type == PathNode::Rock)
		return 0x575757; // dark gray
	else 
		return 0xFFFFFF; // white
}

static float SurfaceSpeed(PathNode::Type type)
{
	if (type == PathNode::Swamp)
		return 0.5f;
	else
		return 1.0f;
}