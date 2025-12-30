#include "AIBrain.h"
#include "AIBrainManagers.h"
#include "GameLoop.h"
#include "Logger.h"
#include <algorithm>

AIBrain::AIBrain(GameAI* owner) : ownerAI(owner)
{
	discovery = std::make_unique<WorldDiscoveryManager>(this);
	resources = std::make_unique<ResourceManager>(this);
	transport = std::make_unique<TransportManager>(this);
	build = std::make_unique<BuildManager>(this);
	manufacturing = std::make_unique<ManufacturingManager>(this);
	military = std::make_unique<MilitaryManager>(this);
	allocator = std::make_unique<TaskAllocator>(this);

	// initialize some inventory
	resources->Add(ResourceType::Wood, 0);
	resources->Add(ResourceType::Coal, 0);
	resources->Add(ResourceType::Iron, 0);

	// Example desire:20 soldiers
	AddDesire("HaveSoldiers", TaskType::TrainSoldiers, ResourceType::None, 20, 1.0f);
}

AIBrain::~AIBrain()
{
	ownerAI = nullptr;
}

void AIBrain::Think(float deltaTime)
{
	if (!ownerAI)
		return;

	Decay(deltaTime);
	UpdateValues(deltaTime);

	// update managers
	discovery->Update(deltaTime);
	resources->Update(deltaTime);
	transport->Update(deltaTime);
	build->Update(deltaTime);
	manufacturing->Update(deltaTime);
	military->Update(deltaTime);
	allocator->Update(deltaTime);

	FSM(deltaTime);
	CheckDeath();
}

void AIBrain::SetMaterialPriority(float p) { materialPriority = p; }
void AIBrain::SetLaborPriority(float p) { laborPriority = p; }
void AIBrain::SetConstructionPriority(float p) { constructionPriority = p; }

void AIBrain::AddDesire(const std::string& name, TaskType taskType, ResourceType primaryResource, int targetCount, float importance)
{
	Desire d;
	d.name = name;
	d.fulfillTaskType = taskType;
	d.primaryResource = primaryResource;
	d.targetCount = targetCount;
	d.currentCount = 0;
	d.importance = importance;
	desires.push_back(d);
}

void AIBrain::UpdateValues(float deltaTime)
{
	// Delegate to a planner function implemented in the managers layer
	// For now call a helper that composes manager calls (kept in this file for clarity)

	// Desire-driven plan for training soldiers
	for (Desire& d : desires)
	{
		if (d.fulfillTaskType == TaskType::TrainSoldiers)
		{
			// current progress includes queued training
			d.currentCount = military->GetSoldierCount() + military->GetTrainingQueue();
			int remaining = std::max(0, d.targetCount - d.currentCount);
			if (remaining <= 0)
				continue;

			// If we don't have a barrack, queue building it first
			if (!build->HasBuilding("Barrack"))
			{
				Task bTask;
				bTask.type = TaskType::Build;
				bTask.meta = "Barrack";
				bTask.priority = 3;
				allocator->AddTask(bTask);
				continue;
			}

			// Barrack exists. Attempt to allocate resources for one soldier at a time.
			int haveWood = resources->Get(ResourceType::Wood);
			int haveIron = resources->Get(ResourceType::Iron);

			if (haveWood >= COST_WOOD_PER_SOLDIER && haveIron >= COST_IRON_PER_SOLDIER)
			{
				bool okWood = resources->Request(ResourceType::Wood, COST_WOOD_PER_SOLDIER);
				bool okIron = resources->Request(ResourceType::Iron, COST_IRON_PER_SOLDIER);
				if (okWood && okIron)
				{
					Task t; 
					t.type = TaskType::TrainSoldiers; 
					t.priority = 10;
					allocator->AddTask(t);
				}
				else
				{
					if (okWood && !okIron)
					{
						Logger::Instance().Log(ownerAI->GetName() + " failed to get iron from inventory when creating soldier \n");

						//resources->Add(ResourceType::Wood, COST_WOOD_PER_SOLDIER);
					}
					if (!okWood && okIron)
					{
						Logger::Instance().Log(ownerAI->GetName() + " failed to get wood from inventory when creating soldier \n");

						//resources->Add(ResourceType::Iron, COST_IRON_PER_SOLDIER);
					}
				}
			}
			else
			{
				if (haveWood < COST_WOOD_PER_SOLDIER)
				{
					//if (!discovery->HasUnexplored()) 
					//{ 
					//	Task disc; 
					//	disc.type = TaskType::Discover; 
					//	disc.priority = 1.0f;
					//	allocator->AddTask(disc); 
					//}

					Task gather; 
					gather.type = TaskType::FellTrees; 
					gather.resource = ResourceType::Wood; 
					gather.priority = 2;
					gather.amount = COST_WOOD_PER_SOLDIER * remaining;
					allocator->AddTask(gather);
				}
				if (haveIron < COST_IRON_PER_SOLDIER)
				{
					Task gather; 
					gather.type = TaskType::MineIron; 
					gather.resource = ResourceType::Iron; 
					gather.priority = 2;
					gather.amount = COST_IRON_PER_SOLDIER * remaining;
					allocator->AddTask(gather);
				}
			}
		}
	}
}

void AIBrain::Decay(float deltaTime)
{
	materialPriority = std::max(0.1f, materialPriority - deltaTime * 0.01f);
	laborPriority = std::max(0.1f, laborPriority - deltaTime * 0.005f);
	constructionPriority = std::max(0.1f, constructionPriority - deltaTime * 0.007f);
}

void AIBrain::FSM(float deltaTime)
{
	if (!ownerAI) 
		return;

	if (!allocator->HasPending())
	{
		if (discovery->HasUnexplored()) 
			ownerAI->SetState(GameAI::State::STATE_WANDER);
		else 
			ownerAI->SetState(GameAI::State::STATE_IDLE);
		return;
	}
	Grid grid = GameLoop::Instance().GetGrid();
	std::vector<PathNode*> nodes;

	Task t = allocator->GetNext();
	switch (t.type)
	{
	case TaskType::FellTrees:
		if (t.id != prevTaskID)
		Logger::Instance().Log(ownerAI->GetName() + " assigned to fell trees (task id: " + std::to_string(t.id) + " priority=" + std::to_string(t.priority) + ")\n");

		grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, PathNode::Wood);
		if (!nodes.empty())
		{
			// is close
			resources->Add(ResourceType::Wood, 60 * deltaTime);
			if (resources->Get(ResourceType::Wood) > t.amount)
			{
				allocator->RemoveTaskByType(t.type);
			}
		}
		else
		{
			// is not close, go to closest
			grid.QueryNodes(ownerAI->GetPosition(), 1000.0f, nodes, PathNode::Wood);
			ownerAI->GoToClosest(nodes);
		}
		break;
	case TaskType::MineCoal:
		if (t.id != prevTaskID)
		Logger::Instance().Log(ownerAI->GetName() + " assigned to mine coal (task id: " + std::to_string(t.id) + " priority=" + std::to_string(t.priority) + ")\n");

		grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, PathNode::Coal);
		if (!nodes.empty())
		{
			// is close
			resources->Add(ResourceType::Coal, 60 * deltaTime);
			if (resources->Get(ResourceType::Coal) > t.amount)
			{
				allocator->RemoveTaskByType(t.type);
			}
		}
		else
		{
			// is not close, go to closest
			grid.QueryNodes(ownerAI->GetPosition(), 1000.0f, nodes, PathNode::Coal);
			ownerAI->GoToClosest(nodes);
		}
		break;
	case TaskType::MineIron:
		if (t.id != prevTaskID)
		Logger::Instance().Log(ownerAI->GetName() + " assigned mine iron (task id: " + std::to_string(t.id) + " priority=" + std::to_string(t.priority) + ")\n");

		grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, PathNode::Iron);
		if (!nodes.empty())
		{
			// is close
			resources->Add(ResourceType::Iron, 60 * deltaTime);

			if (resources->Get(ResourceType::Iron) > t.amount)
			{
				allocator->RemoveTaskByType(t.type);
			}
		}
		else
		{
			// is not close, go to closest
			grid.QueryNodes(ownerAI->GetPosition(), 1000.0f, nodes, PathNode::Iron);
			ownerAI->GoToClosest(nodes);
		}
		break;
	//case TaskType::Transport:
	//	Logger::Instance().Log(ownerAI->GetName() + " assigned to transport resource (task id: " + std::to_string(t.id) + ")\n");
	//	ownerAI->SetState(GameAI::State::STATE_FOLLOW_PATH);
	//	break;
	case TaskType::TrainSoldiers:
		Logger::Instance().Log(ownerAI->GetName() + " assigned to train soldiers (task id: " + std::to_string(t.id) + " priority=" + std::to_string(t.priority) + ")\n");
		military->TrainSoldiers(1);
		allocator->RemoveTask(t.id);
		break;
	case TaskType::Build:
		Logger::Instance().Log(ownerAI->GetName() + " assigned to build (task id: " + std::to_string(t.id) + ") meta=" + t.meta + " priority=" + std::to_string(t.priority) + "\n");
		build->QueueBuilding(t.meta, ownerAI->GetPosition());
		break;
	case TaskType::Discover:
		Logger::Instance().Log(ownerAI->GetName() + " assigned to discover (task id: " + std::to_string(t.id) + " priority=" + std::to_string(t.priority) + ")\n");
		ownerAI->SetState(GameAI::State::STATE_WANDER);
		break;
	default: break;
	}
	prevTaskID = t.id;

	if (military->GetSoldierCount() >= 20)
	{
		ownerAI->SetState(GameAI::State::STATE_IDLE);
	}
}

void AIBrain::CheckDeath()
{
	// placeholder
}
