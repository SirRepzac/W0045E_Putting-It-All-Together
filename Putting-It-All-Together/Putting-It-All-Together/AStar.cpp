#include "AStar.h"
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <algorithm>
#include <iostream>
#include <cmath>

using NodeFilter = std::function<bool(const PathNode*)>;

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

PathNode* ResolveGoalNode(PathNode* desired, float agentRadius)
{
	if (!desired)
		return nullptr;
	// If the goal itself is valid, use it
	if (desired->clearance > agentRadius)
		return desired;

	PathNode* best = nullptr;
	float bestDist = std::numeric_limits<float>::max();

	for (PathNode* n : desired->neighbors)
	{
		if (n->IsObstacle())
			continue;

		if (n->clearance < agentRadius)
			continue;

		float d = DistanceBetween(n->position, desired->position);
		if (d < bestDist)
		{
			bestDist = d;
			best = n;
		}
	}

	return best; // may be nullptr
}

#include "GameLoop.h"
std::vector<PathNode*> AStar::FindPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius, const NodeFilter& canTraverse)
{

	PathNode* goalNode = ResolveGoalNode(endNode, agentRadius);
	if (goalNode == nullptr)
	{
		std::cout << "Path not found!" << std::endl;
		GameLoop::Instance().AddDebugEntity(endNode->position, Renderer::Lime, 10);
		outDist = -1;
		return std::vector<PathNode*>();
	}

	std::unordered_map<PathNode*, NodeRecord> records;

	std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenEntryCompare> openQueue;
	std::unordered_set<PathNode*> closed;


	// Initialize start node costs
	NodeRecord& startRec = records[startNode];
	startRec.gCost = 0.0f;
	startRec.hCost = Heuristic(startNode, goalNode);
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
		if (current == goalNode)
		{
			outDist = records.at(goalNode).gCost;
			return ReconstructPath(records, goalNode);
		}

		closed.insert(current);

		// Expand
		for (PathNode* neighbor : current->neighbors)
		{
			if (closed.find(neighbor) != closed.end())
				continue;

			if (!canTraverse(neighbor) && neighbor != goalNode)
				continue;

			if (neighbor->clearance < agentRadius)
				continue;

			float dx = current->position.x - neighbor->position.x;
			float dy = current->position.y - neighbor->position.y;
			float edgeCost = (dx == 0 || dy == 0) ? 1.0f : 1.41421356f;
			bool diagonal = (dx == 0 || dy == 0) ? false : true;

			float terrainPenalty = 1 / SurfaceSpeed(neighbor->type);

			if (diagonal)
			{
				PathNode* sideA = grid->GetNodeAt(Vec2(current->position.x - dx, current->position.y));
				PathNode* sideB = grid->GetNodeAt(Vec2(current->position.x, current->position.y - dy));

				if ((sideA && !canTraverse(sideA)) ||
					(sideB && !canTraverse(sideB)))
				{
					continue;
				}
			}

			float tentativeG = records[current].gCost + edgeCost * terrainPenalty;

			NodeRecord& rec = records[neighbor];

			// If we've seen this node with an equal or better gCost, skip
			if (tentativeG >= rec.gCost)
				continue;

			rec.parent = current;
			rec.gCost = tentativeG;
			rec.hCost = Heuristic(neighbor, goalNode);
			rec.fCost = rec.gCost + rec.hCost;

			openQueue.push({ neighbor, rec.fCost });
		}
	}

	GameLoop::Instance().AddDebugEntity(goalNode->position, Renderer::Lime, 10);

	outDist = -1;
	return std::vector<PathNode*>();
}

std::vector<PathNode*> AStar::FindClosestPath(PathNode* startNode, const std::vector<PathNode*>& possibleEndNodes, float& outDist, float agentRadius, const NodeFilter& canTraverse)
{
	std::unordered_map<PathNode*, NodeRecord> records;

	std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenEntryCompare> openQueue;
	std::unordered_set<PathNode*> closed;

	// Initialize start node costs
	NodeRecord& startRec = records[startNode];
	startRec.gCost = 0.0f;
	startRec.hCost = BestHeuristic(startNode, possibleEndNodes);
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

		auto isDestination = [&](const PathNode* n)
			{
				for (PathNode* t : possibleEndNodes)
					if (n == t)
						return true;
				return false;
			};

		// Found goal
		if (isDestination(current))
		{
			outDist = records.at(current).gCost;
			return ReconstructPath(records, current);
		}

		closed.insert(current);

		// Expand
		for (PathNode* neighbor : current->neighbors)
		{
			if (closed.find(neighbor) != closed.end())
				continue;

			if (!canTraverse(neighbor))
				continue;

			if (neighbor->clearance < agentRadius)
				continue;

			float dx = current->position.x - neighbor->position.x;
			float dy = current->position.y - neighbor->position.y;
			float edgeCost = (dx == 0 || dy == 0) ? 1.0f : 1.41421356f;
			bool diagonal = (dx == 0 || dy == 0) ? false : true;

			float terrainPenalty = 1 / SurfaceSpeed(neighbor->type);

			if (diagonal)
			{
				PathNode* sideA = grid->GetNodeAt(Vec2(current->position.x - dx, current->position.y));
				PathNode* sideB = grid->GetNodeAt(Vec2(current->position.x, current->position.y - dy));

				if ((sideA && !canTraverse(sideA)) ||
					(sideB && !canTraverse(sideB)))
				{
					continue;
				}
			}

			float tentativeG = records[current].gCost + edgeCost * terrainPenalty;

			NodeRecord& rec = records[neighbor];

			// If we've seen this node with an equal or better gCost, skip
			if (tentativeG >= rec.gCost)
				continue;

			rec.parent = current;
			rec.gCost = tentativeG;
			rec.hCost = BestHeuristic(neighbor, possibleEndNodes);
			rec.fCost = rec.gCost + rec.hCost;

			openQueue.push({ neighbor, rec.fCost });
		}
	}

	outDist = -1;
	return std::vector<PathNode*>();
}

float AStar::BestHeuristic(PathNode* a, std::vector<PathNode*> possibleb)
{
	float smallest = std::numeric_limits<float>::max();
	for (PathNode* b : possibleb)
	{
		float h = Heuristic(a, b);
		if (h < smallest)
			smallest = h;
	}
	return smallest;
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
}

std::vector<PathNode*> AStar::RequestPath(PathNode* startNode, PathNode* endNode, float& outDist, float agentRadius, const NodeFilter& canTraverse)
{
	return FindPath(startNode, endNode, outDist, agentRadius, canTraverse);
}

std::vector<PathNode*> AStar::RequestClosestPath(PathNode* startNode, const std::vector<PathNode*>& possibleEndNodes, float& outDist, float agentRadius, const NodeFilter& canTraverse)
{
	return FindClosestPath(startNode, possibleEndNodes, outDist, agentRadius, canTraverse);
}
