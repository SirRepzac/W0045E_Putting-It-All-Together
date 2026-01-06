#pragma once
#include "Pathfinder.h"
#include "Grid.h"



class AStar : public Pathfinder
{
public:
	AStar(Grid* grid) : grid(grid) { }

	// Overrides base RequestPath
	std::vector<PathNode*> RequestPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius) override;
	std::vector<PathNode*> RequestClosestPath(PathNode* startNode, PathNode::Type endType, float& outDist, float agentRadius) override;

	// Buffer between RequestPath and calculations
	std::vector<PathNode*> FindPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius);
	std::vector<PathNode*> FindClosestPath(PathNode* startNode, PathNode::Type endType, float& outDist, float agentRadius);

	// Get the heuristic between a and b
	// --------------------------
	// a - reference to the first node
	// b - reference to the second node
	// --------------------------
	// returns the heuristic between a and b
	float Heuristic(PathNode* a, PathNode* b);

	// Overrides base GetName
	std::string GetName() const override { return "A-star Search"; }
private:
	Grid* grid;
};
