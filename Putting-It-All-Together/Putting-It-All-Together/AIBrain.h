#pragma once
#include "GameAI.h"
#include <memory>
#include <vector>
#include <string>
#include <queue>
#include <unordered_set>

#include "AIBrainManagers.h"

struct KnownNode
{
	bool discovered = false;
	bool walkable = false;   // belief
	float lastSeenTime = 0;
};

struct Agent
{
	Agent(GameAI* ai) : ai(ai), busy(false), type(PopulationType::Worker) {}
	GameAI* ai;
	PopulationType type;
	bool busy;
	ItemType holding = ItemType::None;
	Task* currentTask = nullptr;
	float workTimer = 0.0f;

	void Update(float dt)
	{
		if (currentTask == nullptr)
			return;

		if (type == PopulationType::Worker)
		{
			if (currentTask->type == TaskType::GatherWood)
			{
				Grid& grid = GameLoop::Instance().GetGrid();
				std::vector<PathNode*> nodes;
				grid.QueryNodes(ai->GetPosition(), ai->GetRadius() * 2, nodes, PathNode::ResourceType::Wood);

				for (std::vector<PathNode*>::iterator it = nodes.begin(); it != nodes.end();)
				{
					if ((*it)->resourceAmount <= 0)
						it = nodes.erase(it);
					else
						it++;
				}

				// is close to resource
				if (!nodes.empty())
				{
					if (workTimer >= 30.0f)
					{
						PathNode* node = nodes[0];
						holding = ItemType::Wood;

						node->resourceAmount--;
						if (node->resourceAmount <= 0)
						{
							node->resource = PathNode::ResourceType::None;
							int r;
							int c;
							grid.WorldToGrid(node->position, r, c);
							GameLoop::Instance().renderer->MarkNodeDirty(grid.Index(c, r));

						}
						bool valid = true;
						ai->GoTo(currentTask->building->targetNode, valid);
						currentTask->type = TaskType::Transport;
						//busy = false;
					}
					else
					{
						workTimer += dt;
					}
				}
				else
				{
					bool valid = true;
					// is not close, go to closest
					ai->GoToClosest(PathNode::ResourceType::Wood, valid);
				}
			}
			else if (currentTask->type == TaskType::Transport)
			{
				if (DistanceBetween(ai->GetPosition(), currentTask->building->targetNode->position) < ai->GetRadius() * 2)
				{
					if (currentTask->building->AddResource(holding))
					{
						holding = ItemType::None;
					}

					currentTask->completed = true;
					busy = false;
					currentTask = nullptr;
				}
				else
				{
					bool valid = true;
					ai->GoTo(currentTask->building->targetNode, valid);
				}
			}
		}
	}
};

class AIBrain
{
public:
	AIBrain();
	~AIBrain();

	void Think(float deltaTime);

	// Desires
	void AddDesire(const std::string& name, TaskType taskType, ItemType primaryResource, int targetCount, float importance = 1.0f);

	ResourceManager* GetResources() { return resources.get(); }
	BuildManager* GetBuild() { return build.get(); }
	PopulationManager* GetMilitary() { return population.get(); }
	TaskAllocator* GetAllocator() { return taskAllocator.get(); }

	void UpdateDiscovered();

	void ExploreNode(PathNode* node, Grid& grid, double& gameTime);

	std::vector<std::vector<KnownNode>> knownNodes;
	bool IsDiscovered(int index) const;
	bool IsDiscovered(int row, int col) const;

private:
	void UpdateValues(float deltaTime);
	Agent* GetBestAgent(PopulationType type, PathNode* node);
	void UpdateWorkers(float dt);
	void TrainUnit(PopulationType type);
	void PickupNewTrained();
	void GatherResources(Task& t, float deltaTime);
	void ManufactureProducts(Task& t, float deltaTime);
	void AddAcquisitionTask(std::vector<std::pair<ItemType, float>> lackingResources, float priority);
	void LogCurrentTaskList();
	void FSM(float deltaTime);
	void CheckDeath();

	bool IsFrontierNode(const PathNode* node);

	PathNode* FindClosestFrontier();

	PathNode* FindClosestOpenArea(Vec2 areaSize);

	std::vector<Agent> agents;

	std::map<PopulationType, std::vector<Agent*>> populationMap;

	float materialPriority = 1.0f;
	float laborPriority = 1.0f;
	float constructionPriority = 1.0f;

	std::vector<Desire> desires;

	std::unique_ptr<ResourceManager> resources;
	std::unique_ptr<BuildManager> build;
	std::unique_ptr<ManufacturingManager> manufacturing;
	std::unique_ptr<PopulationManager> population;
	std::unique_ptr<TaskAllocator> taskAllocator;

	std::map<ItemType, std::vector<PathNode*>> knownResources;

	int prevTaskId = -1;

	PathNode* GetBuildingLocation(BuildingType type);

	std::map<BuildingType, PathNode*> buildingLoc;

	int frames = 0;
};