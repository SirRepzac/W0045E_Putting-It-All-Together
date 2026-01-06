#include "AStar.h"
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <algorithm>
#include <iostream>
#include <cmath>

struct OpenEntry
{
	PathNode* node;
	float f;
};

struct OpenEntryCompare
{
	bool operator()(OpenEntry& a, OpenEntry& b) const
	{
		return a.f > b.f; // min-heap by f
	}
};

std::vector<PathNode*> AStar::FindPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius)
{
	std::unordered_map<PathNode*, NodeRecord> records;

	std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenEntryCompare> openQueue;
	std::unordered_set<PathNode*> closed;

	// Initialize start node costs
	NodeRecord& startRec = records[startNode];
	startRec.gCost = 0.0f;
	startRec.hCost = Heuristic(startNode, endNode);
	startRec.fCost = startRec.gCost + startRec.hCost;
	startRec.parent = nullptr;

	openQueue.push({startNode, startRec.fCost});

	while (!openQueue.empty())
	{
		OpenEntry entry = openQueue.top();
		openQueue.pop();

		PathNode* current = entry.node;


		// Ignore stale queue entries
		if (records[current].fCost != entry.f)
			continue;

		// Found goal
		if (current == endNode)
		{
			outDist = records.at(endNode).gCost;
			return ReconstructPath(records, endNode);
		}

		closed.insert(current);

		// Expand
		for (PathNode* neighbor : current->neighbors)
		{
			if (neighbor->IsObstacle() || closed.contains(neighbor) || neighbor->clearance <= agentRadius)
				continue;
			float dx = current->position.x - neighbor->position.x;
			float dy = current->position.y - neighbor->position.y;
			float edgeCost = (dx == 0 || dy == 0) ? 1.0f : 1.41421356f;
			bool diagonal = (dx == 0 || dy == 0) ? false : true;

			if (diagonal)
			{
				PathNode* sideA = grid->GetNodeAt(Vec2(current->position.x - dx, current->position.y));
				PathNode* sideB = grid->GetNodeAt(Vec2(current->position.x, current->position.y - dy));

				if ((sideA && sideA->IsObstacle()) ||
					(sideB && sideB->IsObstacle()))
				{
					continue;
				}
			}

			float tentativeG = records[current].gCost + edgeCost;

			NodeRecord& rec = records[neighbor];

			// If we've seen this node with an equal or better gCost, skip
			if (tentativeG >= rec.gCost)
				continue;

			rec.parent = current;
			rec.gCost = tentativeG;
			rec.hCost = Heuristic(neighbor, endNode);
			rec.fCost = rec.gCost + rec.hCost;

			openQueue.push({ neighbor, rec.fCost });
		}
	}

	std::cout << "Path not found!" << std::endl;
	outDist = -1;
	return std::vector<PathNode*>();
}

std::vector<PathNode*> AStar::FindClosestPath(PathNode* startNode, PathNode::Type endType, float& outDist, float agentRadius)
{
	std::unordered_map<PathNode*, NodeRecord> records;

	std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenEntryCompare> openQueue;
	std::unordered_set<PathNode*> closed;

	// Initialize start node costs
	NodeRecord& startRec = records[startNode];
	startRec.gCost = 0.0f;
	startRec.hCost = 1;
	startRec.fCost = startRec.gCost + startRec.hCost;
	startRec.parent = nullptr;

	openQueue.push({ startNode, startRec.fCost });

	while (!openQueue.empty())
	{
		OpenEntry entry = openQueue.top();
		openQueue.pop();

		PathNode* current = entry.node;


		// Ignore stale queue entries
		if (records[current].fCost != entry.f)
			continue;

		// Found goal
		if (current->type == endType)
		{
			outDist = records.at(current).gCost;
			return ReconstructPath(records, current);
		}

		closed.insert(current);

		// Expand
		for (PathNode* neighbor : current->neighbors)
		{
			if (neighbor->IsObstacle() || closed.contains(neighbor) || neighbor->clearance <= agentRadius)
				continue;
			float dx = current->position.x - neighbor->position.x;
			float dy = current->position.y - neighbor->position.y;
			float edgeCost = (dx == 0 || dy == 0) ? 1.0f : 1.41421356f;
			bool diagonal = (dx == 0 || dy == 0) ? false : true;

			if (diagonal)
			{
				PathNode* sideA = grid->GetNodeAt(Vec2(current->position.x - dx, current->position.y));
				PathNode* sideB = grid->GetNodeAt(Vec2(current->position.x, current->position.y - dy));

				if ((sideA && sideA->IsObstacle()) ||
					(sideB && sideB->IsObstacle()))
				{
					continue;
				}
			}

			float tentativeG = records[current].gCost + edgeCost;

			NodeRecord& rec = records[neighbor];

			// If we've seen this node with an equal or better gCost, skip
			if (tentativeG >= rec.gCost)
				continue;

			rec.parent = current;
			rec.gCost = tentativeG;
			rec.hCost = 1;
			rec.fCost = rec.gCost + rec.hCost;

			openQueue.push({ neighbor, rec.fCost });
		}
	}

	std::cout << "Path not found!" << std::endl;
	outDist = -1;
	return std::vector<PathNode*>();
}

float AStar::Heuristic(PathNode* a, PathNode* b)
{
	float tileSize = grid->GetCellSize();

	// Octile distance, good when allowing diagonal movement
	float dx = std::abs(a->position.x - b->position.x) / tileSize;
	float dy = std::abs(a->position.y - b->position.y) / tileSize;

	float D = 1.0f;
	float D2 = 1.41421356f;

	return D * (std::max(dx, dy)) + (D2 - D) * std::min(dx, dy);

	// Manhattan distance
	//return std::abs(a.x - b.x) + std::abs(a.y - b.y);

	// Euclidean distance
	//return DistanceBetween(a, b);
}

std::vector<PathNode*> AStar::RequestPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius)
{
	return FindPath(startNode, endNode, outDist, agentRadius);
}

std::vector<PathNode*> AStar::RequestClosestPath(PathNode* startNode, PathNode::Type endType, float& outDist, float agentRadius)
{
	return FindClosestPath(startNode, endType, outDist, agentRadius);
}
