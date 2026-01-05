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
	d.targetCount = targetCount;
	d.importance = importance;
	desires.push_back(d);
}

void AIBrain::UpdateValues(float deltaTime)
{
	// Desire-driven plan for training soldiers
	for (Desire& d : desires)
	{
		if (d.added)
			continue;

		d.added = true;

		if (d.fulfillTaskType == TaskType::TrainSoldiers)
		{
			int amount = d.targetCount;
			float basePrio = d.importance;

			Task t;
			t.type = TaskType::TrainSoldiers;
			t.priority = basePrio;
			t.amount = amount;
			allocator->AddTask(t);

			Task gatherW;
			gatherW.type = TaskType::FellTrees;
			gatherW.resource = ResourceType::Wood;
			gatherW.priority = basePrio + 1;
			gatherW.amount = COST_WOOD_PER_SOLDIER * amount;
			allocator->AddTask(gatherW);

			Task gatherI;
			gatherI.type = TaskType::MineIron;
			gatherI.resource = ResourceType::Iron;
			gatherI.priority = basePrio + 1;
			gatherI.amount = COST_IRON_PER_SOLDIER * amount;
			allocator->AddTask(gatherI);

			if (!build->HasBuilding("Barrack"))
			{
				Task bTask;
				bTask.type = TaskType::Build;
				bTask.meta = "Barrack";
				bTask.priority = basePrio + 2;
				allocator->AddTask(bTask);

				Task gatherBW;
				gatherBW.type = TaskType::FellTrees;
				gatherBW.resource = ResourceType::Wood;
				gatherBW.priority = basePrio + 3;
				gatherBW.amount = BARRACK_WOOD_COST;
				allocator->AddTask(gatherBW);

				Task gatherBI;
				gatherBI.type = TaskType::MineIron;
				gatherBI.resource = ResourceType::Iron;
				gatherBI.priority = basePrio + 3;
				gatherBI.amount = BARRACK_IRON_COST;
				allocator->AddTask(gatherBI);
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

	if (!allocator->HasPending() && ownerAI->GetCurrentState() != GameAI::State::STATE_IDLE)
	{
		ownerAI->SetState(GameAI::State::STATE_IDLE);
		return;
	}

	Grid& grid = GameLoop::Instance().GetGrid();
	std::vector<PathNode*> nodes;

	Vec2 barrackLoc;

	Task* tt = allocator->GetNext();
	if (tt == nullptr)
		return;
	Task& t = *tt;
	if (t.type != prevTaskType)
		Logger::Instance().Log(ownerAI->GetName() + " assigned to " + ToString(t.type) + "(task id : " + std::to_string(t.id) + " priority = " + std::to_string(t.priority) + ")\n");
	switch (t.type)
	{
	case TaskType::FellTrees:
		grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, PathNode::Wood);
		if (!nodes.empty())
		{
			// is close
			resources->Add(ResourceType::Wood, 60 * deltaTime);
			if (resources->Get(ResourceType::Wood) > t.amount)
			{
				allocator->RemoveTask(t.id);
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
		grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, PathNode::Coal);
		if (!nodes.empty())
		{
			// is close
			resources->Add(ResourceType::Coal, 60 * deltaTime);
			if (resources->Get(ResourceType::Coal) > t.amount)
			{
				allocator->RemoveTask(t.id);
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

		grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, PathNode::Iron);
		if (!nodes.empty())
		{
			// is close
			resources->Add(ResourceType::Iron, 60 * deltaTime);

			if (resources->Get(ResourceType::Iron) > t.amount)
			{
				allocator->RemoveTask(t.id);
			}
		}
		else
		{
			// is not close, go to closest
			grid.QueryNodes(ownerAI->GetPosition(), 1000.0f, nodes, PathNode::Iron);
			ownerAI->GoToClosest(nodes);
		}
		break;
	case TaskType::Transport:
		ownerAI->SetState(GameAI::State::STATE_FOLLOW_PATH);
		break;
	case TaskType::TrainSoldiers:
		if (t.amount <= 0)
		{
			allocator->RemoveTask(t.id);
			break;
		}

		if (build->IsInQueue("Barrack"))
		{
			Logger::Instance().Log(ownerAI->GetName() + " waiting for barrack to train soldiers \n");
			break;
		}

		if (!build->HasBuilding("Barrack"))
		{
			Logger::Instance().Log(ownerAI->GetName() + " no barrack to train soldier \n");
			allocator->RemoveTask(t.id);
			break;
		}

		barrackLoc = build->GetBuildingLocation("Barrack");
		if (DistanceBetween(ownerAI->GetPosition(), barrackLoc) > ownerAI->GetRadius() + 2)
		{
			PathNode* barrackNode = grid.GetNodeAt(barrackLoc + Vec2(0, 1));
			ownerAI->GoTo(barrackNode);
		}
		else
		{
			if (resources->Request(ResourceType::Iron, COST_IRON_PER_SOLDIER) && resources->Request(ResourceType::Wood, COST_WOOD_PER_SOLDIER))
			{
				military->TrainSoldiers(1);
				t.amount--;
			}
			else
			{
				Logger::Instance().Log(ownerAI->GetName() + " not enough resources to train soldier \n");
				allocator->RemoveTask(t.id);
			}

		}
		break;
	case TaskType::Build:
		if (build->HasBuilding(t.meta))
			allocator->RemoveTask(t.id);
		if (resources->Request(ResourceType::Iron, BARRACK_IRON_COST) && resources->Request(ResourceType::Wood, BARRACK_WOOD_COST))
		{
			build->QueueBuilding(t.meta, ownerAI->GetPosition());
			allocator->RemoveTask(t.id);
			break;
		}
		else
		{
			Logger::Instance().Log(ownerAI->GetName() + " cannot afford barrack \n");
			allocator->RemoveTask(t.id);
		}
		break;
	case TaskType::Discover:
		ownerAI->SetState(GameAI::State::STATE_WANDER);
		break;
	default: break;
	}

	prevTaskType = t.type;
}

void AIBrain::CheckDeath()
{
	bool someCondition = false;
	// placeholder: no death logic yet
	if (someCondition)
	{
		Logger::Instance().Log(ownerAI->GetName() + " has died due to unmet needs.\n");
		ownerAI->SetState(GameAI::State::STATE_IDLE);
		GameLoop::Instance().ScheduleDeath(ownerAI);
	}
}