#pragma once
#include "Pathfinder.h"
#include "Grid.h"
#include <functional>

using NodeFilter = std::function<bool(const PathNode*)>;

class AStar : public Pathfinder
{
public:
	AStar(Grid* grid) : grid(grid) { }

	// Overrides base RequestPath
	std::vector<PathNode*> RequestPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius, const NodeFilter& canTraverse) override;
	std::vector<PathNode*> RequestClosestPath(PathNode* startNode, const std::vector<PathNode*>& possibleEndNodes, float& outDist, float agentRadius, const NodeFilter& canTraverse) override;

	// Buffer between RequestPath and calculations
	std::vector<PathNode*> FindPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius, const NodeFilter& canTraverse);
	std::vector<PathNode*> FindClosestPath(PathNode* startNode, const std::vector<PathNode*>& possibleEndNodes, float& outDist, float agentRadius, const NodeFilter& canTraverse);

	float BestHeuristic(PathNode* a, std::vector<PathNode*> possibleb);

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
