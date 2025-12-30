#include "AIBrain.h"
#include "GameLoop.h"
#include "Logger.h"
#include <algorithm>

// Costs and constants
static const int COST_WOOD_PER_SOLDIER = 2;
static const int COST_IRON_PER_SOLDIER = 1;
static const int COST_COAL_PER_SOLDIER = 0; // unused for now

static const int BARRACK_WOOD_COST = 50;
static const int BARRACK_IRON_COST = 20;

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
	resources->inventory[ResourceType::Wood] = 0;
	resources->inventory[ResourceType::Coal] = 0;
	resources->inventory[ResourceType::Iron] = 0;

	// Example desire:20 soldiers
	AddDesire("HaveSoldiers", TaskType::TrainSoldiers, ResourceType::None, 20, 1.0f);
}

AIBrain::~AIBrain()
{
	// unique_ptr will clean up
	ownerAI = nullptr;
}

void AIBrain::Think(float deltaTime)
{
	if (!ownerAI)
		return;

	// Simple priority decay over time
	Decay(deltaTime);

	UpdateValues(deltaTime);

	// Update managers
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

void AIBrain::UpdateValues(float deltaTime)
{
	// Desire-driven plan for training soldiers
	for (Desire& d : desires)
	{
		if (d.fulfillTaskType != TaskType::TrainSoldiers)
			continue;

		// current progress includes queued training
		d.currentCount = military->soldiers + military->trainingQueue;
		int remaining = std::max(0, d.targetCount - d.currentCount);
		if (remaining <= 0)
			continue;

		// If we don't have a barrack, queue building it first
		if (!build->HasBuilding("Barrack"))
		{
			// add a build task for barrack if not already queued
			Task bTask;
			bTask.type = TaskType::Build;
			bTask.meta = "Barrack";
			bTask.priority = static_cast<float>(remaining) * d.importance * constructionPriority;
			allocator->AddTask(bTask);
			// also ensure we have discovery info (very simple)
			if (!discovery->HasUnexplored())
			{
				Task disc;
				disc.type = TaskType::Discover;
				disc.priority = 1.0f;
				allocator->AddTask(disc);
			}
			// move to next desire — building is prerequisite
			continue;
		}

		// Barrack exists. Attempt to allocate resources for one soldier at a time.
		// Check available resources and, when available, reserve them and create a train task.
		int haveWood = resources->Get(ResourceType::Wood);
		int haveIron = resources->Get(ResourceType::Iron);

		if (haveWood >= COST_WOOD_PER_SOLDIER && haveIron >= COST_IRON_PER_SOLDIER)
		{
			// Reserve resources immediately so we don't create duplicate train tasks
			bool okWood = resources->Request(ResourceType::Wood, COST_WOOD_PER_SOLDIER);
			bool okIron = resources->Request(ResourceType::Iron, COST_IRON_PER_SOLDIER);
			if (okWood && okIron)
			{
				// create a single train task
				Task t;
				t.type = TaskType::TrainSoldiers;
				t.priority = static_cast<float>(remaining) * d.importance * laborPriority;
				allocator->AddTask(t);
			}
			else
			{
				// If reservation failed, put back any partial consumption
				if (okWood && !okIron)
					resources->Add(ResourceType::Wood, COST_WOOD_PER_SOLDIER);
				if (!okWood && okIron)
					resources->Add(ResourceType::Iron, COST_IRON_PER_SOLDIER);
			}
		}
		else
		{
			// Not enough resources: create tasks to obtain them
			if (haveWood < COST_WOOD_PER_SOLDIER)
			{
				// ensure discovery is running to find wood
				if (!discovery->HasUnexplored())
				{
					Task disc;
					disc.type = TaskType::Discover;
					disc.priority = 1.0f;
					allocator->AddTask(disc);
				}
				Task gather;
				gather.type = TaskType::FellTrees;
				gather.resource = ResourceType::Wood;
				gather.priority = static_cast<float>(remaining) * d.importance * materialPriority;
				allocator->AddTask(gather);
			}
			if (haveIron < COST_IRON_PER_SOLDIER)
			{
				// add transport / gather for iron
				Task gather;
				gather.type = TaskType::Transport;
				gather.resource = ResourceType::Iron;
				gather.priority = static_cast<float>(remaining) * d.importance * materialPriority;
				allocator->AddTask(gather);
			}
		}
	}
}

// Decay of the AI's priorities over time
void AIBrain::Decay(float deltaTime)
{
	// Gradually lower priorities to simulate attention shifting
	materialPriority = std::max(0.1f, materialPriority - deltaTime * 0.01f);
	laborPriority = std::max(0.1f, laborPriority - deltaTime * 0.005f);
	constructionPriority = std::max(0.1f, constructionPriority - deltaTime * 0.007f);
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

void AIBrain::FSM(float deltaTime)
{
	if (!ownerAI)
		return;

	// Very simple finite-state handling: pull next task and log it
	if (allocator->HasPending())
	{
		Task t = allocator->GetNext();
		switch (t.type)
		{
			case TaskType::FellTrees:
				Logger::Instance().Log(ownerAI->GetName() + " assigned to fell trees (task id: " + std::to_string(t.id) + ")\n");
				ownerAI->SetState(GameAI::State::STATE_SEEK);
				break;
			case TaskType::Transport:
				Logger::Instance().Log(ownerAI->GetName() + " assigned to transport resource (task id: " + std::to_string(t.id) + ")\n");
				ownerAI->SetState(GameAI::State::STATE_FOLLOW_PATH);
				break;
			case TaskType::TrainSoldiers:
				Logger::Instance().Log(ownerAI->GetName() + " assigned to train soldiers (task id: " + std::to_string(t.id) + ")\n");
				// queue training; resources already reserved when task was created
				military->TrainSoldiers(1);
				break;
			case TaskType::Build:
				Logger::Instance().Log(ownerAI->GetName() + " assigned to build (task id: " + std::to_string(t.id) + ") meta=" + t.meta + "\n");
				// queue building in build manager (actual resource check in manager update)
				build->QueueBuilding(t.meta, ownerAI->GetPosition());
				break;
			case TaskType::Discover:
				Logger::Instance().Log(ownerAI->GetName() + " assigned to discover (task id: " + std::to_string(t.id) + ")\n");
				ownerAI->SetState(GameAI::State::STATE_WANDER);
				break;
			default:
				break;
		}
	}
	else
	{
		// No tasks: wander or discover
		if (discovery->HasUnexplored())
		{
			ownerAI->SetState(GameAI::State::STATE_WANDER);
		}
		else
		{
			ownerAI->SetState(GameAI::State::STATE_IDLE);
		}
	}
}

// Manager implementations

void AIBrain::WorldDiscoveryManager::Update(float dt)
{
	// placeholder: nothing heavy, could scan map and add unexplored positions
}

void AIBrain::WorldDiscoveryManager::DiscoverAround(const Vec2& pos, float range)
{
	// placeholder: add a dummy unexplored point
	unexplored.push_back(pos + Vec2(range, 0.0f));
}

void AIBrain::ResourceManager::Update(float dt)
{
	// placeholder: resources might slowly be consumed or produced
}

int AIBrain::ResourceManager::Get(ResourceType r) const
{
	auto it = inventory.find(r);
	if (it == inventory.end())
		return 0;
	return it->second;
}

void AIBrain::ResourceManager::Add(ResourceType r, int amount)
{
	inventory[r] += amount;
}

bool AIBrain::ResourceManager::Request(ResourceType r, int amount)
{
	auto it = inventory.find(r);
	if (it == inventory.end()) return false;
	if (it->second < amount) return false;
	it->second -= amount;
	return true;
}

void AIBrain::TransportManager::Update(float dt)
{
	// process pending transports
	if (pendingTransports > 0)
	{
		// pretend one transport completed per update
		pendingTransports = std::max(0, pendingTransports - 1);
	}
}

void AIBrain::TransportManager::ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to)
{
	(void)r; (void)amount; (void)from; (void)to;
	pendingTransports++;
}

void AIBrain::BuildManager::Update(float dt)
{
	// attempt to construct first queued building if resources available
	if (queue.empty())
		return;

	std::string name = queue.front();
	if (name == "Barrack")
	{
		// check resource availability and consume
		if (owner->resources->Request(ResourceType::Wood, BARRACK_WOOD_COST) && owner->resources->Request(ResourceType::Iron, BARRACK_IRON_COST))
		{
			builtBuildings.push_back(name);
			queue.erase(queue.begin());
			Logger::Instance().Log(std::string("Built: ") + name + "\n");
		}
		else
		{
			// if not enough resources, leave in queue and maybe the UpdateValues logic will spawn gather tasks
		}
	}
	else
	{
		// generic immediate build (no resource checks)
		builtBuildings.push_back(name);
		queue.erase(queue.begin());
	}
}

bool AIBrain::BuildManager::HasBuilding(const std::string& name) const
{
	return std::find(builtBuildings.begin(), builtBuildings.end(), name) != builtBuildings.end();
}

void AIBrain::BuildManager::QueueBuilding(const std::string& name, const Vec2& pos)
{
	(void)pos;
	queue.push_back(name);
}

void AIBrain::ManufacturingManager::Update(float dt)
{
	// process orders (very simplified)
	for (auto it = orders.begin(); it != orders.end(); )
	{
		// pretend one item produced per update
		// decrement or remove
		it->second = std::max(0, it->second - 1);
		if (it->second == 0)
			it = orders.erase(it);
		else
			++it;
	}
}

void AIBrain::ManufacturingManager::QueueManufacture(const std::string& item, int amount)
{
	orders[item] += amount;
}

void AIBrain::MilitaryManager::Update(float dt)
{
	// simple: complete one training per update tick
	if (trainingQueue > 0)
	{
		trainingQueue = std::max(0, trainingQueue - 1);
		soldiers += 1;
		Logger::Instance().Log(std::string("Trained soldier. Total now: ") + std::to_string(soldiers) + "\n");
	}
}

void AIBrain::MilitaryManager::TrainSoldiers(int count)
{
	// add to training queue; actual production happens in Update
	trainingQueue += count;
}

// TaskAllocator implementations
int AIBrain::TaskAllocator::AddTask(const Task& t)
{
	// avoid duplicates: if an unassigned task of same type/resource/meta exists, boost priority and return its id
	for (Task& existing : tasks)
	{
		if (!existing.assigned && existing.type == t.type && existing.resource == t.resource && existing.meta == t.meta)
		{
			existing.priority = std::max(existing.priority, t.priority);
			return existing.id;
		}
	}

	Task copy = t;
	copy.id = nextId++;
	tasks.push_back(copy);
	return copy.id;
}

void AIBrain::TaskAllocator::Update(float dt)
{
	(void)dt;
	// Reprioritize each update so tasks respond to shifting priorities
	Reprioritize();
}

void AIBrain::TaskAllocator::Reprioritize()
{
	// Simple example: boost tasks that relate to material using brain priority weights
	for (Task& t : tasks)
	{
		float base = t.priority;
		if (t.resource != ResourceType::None)
		{
			base *= owner->materialPriority;
		}
		// construction related
		if (t.type == TaskType::Build)
			base *= owner->constructionPriority;
		// labor-related (training)
		if (t.type == TaskType::TrainSoldiers)
			base *= owner->laborPriority;
		// clamp
		t.priority = std::max(0.0f, base);
	}
}

AIBrain::Task AIBrain::TaskAllocator::GetNext()
{
	Reprioritize();
	// return highest-priority unassigned task
	auto it = std::max_element(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) { return a.priority < b.priority; });
	if (it == tasks.end()) return Task();
	Task t = *it;
	// mark assigned
	for (Task& tt : tasks)
	{
		if (tt.id == t.id) { tt.assigned = true; break; }
	}
	return t;
}

// New: AddDesire implementation
void AIBrain::AddDesire(const std::string& name, AIBrain::TaskType taskType, AIBrain::ResourceType primaryResource, int targetCount, float importance)
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
