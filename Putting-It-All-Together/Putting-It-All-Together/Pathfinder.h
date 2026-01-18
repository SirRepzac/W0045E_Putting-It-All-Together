#pragma once
#include "PathNode.h"
#include "Constants.h"
#include <unordered_map>
#include <functional>

using NodeFilter = std::function<bool(const PathNode*)>;

class Pathfinder
{
public:
	// Get the path from start to end
	// --------------------------
	// startNode - reference to the node where the path start
	// endNode - reference to the node where to path should end
	// outDist - output parameter to receive the distance of the path
	// --------------------------
	// returns a vector containing references to the nodes to traverse to get from startNode to endNode
	virtual std::vector<PathNode*> RequestPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius, const NodeFilter& canTraverse) = 0;
	virtual std::vector<PathNode*> RequestClosestPath(PathNode* startNode, std::vector<PathNode::ResourceType> endTypes, float& outDist, float agentRadius, const NodeFilter& canTraverse) = 0;

	// Get the name of the algorithm
	// --------------------------
	// returns a string of the name of the algorithm
	virtual std::string GetName() const = 0;

protected:
	// Get the path from the beginning to the end after calculations
	// --------------------------
	// endNode - reference to the end node
	// --------------------------
	// returns the path from start to end node
	std::vector<PathNode*> ReconstructPath(const std::unordered_map<PathNode*, NodeRecord>& records, PathNode* endNode)
	{
		std::vector<PathNode*> path;
		for (PathNode* n = endNode; n != nullptr; n = records.at(n).parent)
			path.push_back(n);

		//std::reverse(path.begin(), path.end());
		return path;
	}
};
