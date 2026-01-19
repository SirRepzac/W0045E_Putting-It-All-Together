#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include "GameLoop.h"
#include "Constants.h"
#include "AStar.h"
#include <sstream>
#include <filesystem>
#include "AIBrainManagers.h"

static std::string LoadDataFile(const std::string& filename)
{
	std::string result;
	std::ifstream file(filename);
	if (!file) return result;

	char c;
	while (file.get(c))
	{
		if (c == ' ' || c == '\n') continue;
		result += c;
	}

	file.close();

	return result;
}

static std::string LoadMap()
{ 
	std::string dataPath = "Map";
	std::string mapPath = "map.txt";
	// Ensure directory exists (create if missing) 
	std::error_code ec; std::filesystem::create_directories(dataPath, ec); // create if needed
	std::string fileName = dataPath + "/" + mapPath;
	return LoadDataFile(fileName);
}

void GameLoop::RefreshScreen()
{
	for (int r = 0; r < grid.GetRows(); r++)
	{
		for (int c = 0; c < grid.GetCols(); c++)
		{
			renderer->MarkNodeDirty(grid.Index(c, r));
		}
	}
}

GameLoop::GameLoop() : grid(WORLD_WIDTH, WORLD_HEIGHT, 100, LoadMap())
{
	Movable::baseRadius = grid.cellSize / 5;

	pathfinder = new AStar(&grid);

	// create renderer and start window
	renderer = new Renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
	renderer->Start();
}

GameLoop::~GameLoop()
{
	if (renderer)
	{
		renderer->Stop();
		delete renderer;
	}

	DeleteAll();
}

void GameLoop::DeleteAll()
{
	if (player)
		delete player;
	player = nullptr;

	delete pathfinder;

	if (brain)
		delete brain;
	brain = nullptr;

	for (GameAI* ai : aiList)
	{
		delete ai;
	}

	aiList.clear();
	persistentEnts.clear();
	debugEnts.clear();
}

void GameLoop::InitializeGame()
{
	// walls around grid
	std::vector<float> walls = grid.GetGlobalGridPosition();
	Vec2 crnr1 = Vec2(walls.at(0), walls.at(1));
	Vec2 crnr2 = Vec2(walls.at(2), walls.at(1));
	Vec2 crnr3 = Vec2(walls.at(2), walls.at(3));
	Vec2 crnr4 = Vec2(walls.at(0), walls.at(3));

	AddPersistentLine(crnr1, crnr2, Renderer::Black);
	AddPersistentLine(crnr2, crnr3, Renderer::Black);
	AddPersistentLine(crnr3, crnr4, Renderer::Black);
	AddPersistentLine(crnr4, crnr1, Renderer::Black);

	renderer->nodeCache.resize(grid.GetCols() * grid.GetRows());
	renderer->nodeNeedsUpdate.resize(grid.GetCols() * grid.GetRows());

	for (int r = 0; r < grid.GetRows(); r++)
	{
		for (int c = 0; c < grid.GetCols(); c++)
		{
			PathNode& pathNode = grid.GetNodes()[r][c];
			float xPos = pathNode.position.x - pathNode.size;
			float yPos = pathNode.position.y - pathNode.size;
			float height = pathNode.size * 2;
			float width = pathNode.size * 2;
			Renderer::DrawNode drawNode;
			drawNode.xPos = xPos;
			drawNode.yPos = yPos;
			drawNode.height = height;
			drawNode.width = width;
			drawNode.type = pathNode.type;
			int index = grid.Index(c, r);
			renderer->nodeCache[index] = drawNode;
			renderer->nodeNeedsUpdate[index] = true;
		}
	}

	grid.SetClearance();
	resourceOverlay.position = (Vec2(WORLD_WIDTH, 0));
	renderer->AddOverlay(&resourceOverlay);
	debugOverlay.position = (Vec2(0, 0));
	renderer->AddOverlay(&debugOverlay);

	CreatePlayer(Vec2(WORLD_WIDTH / 2, WORLD_HEIGHT / 2));
	brain = new AIBrain();
}

std::vector<GameAI*> GameLoop::CreateAI(int count, Vec2 startingPosition)
{
	int aiCount = count;

	if (aiCount <= 0)
		return std::vector<GameAI*>();

	std::vector<GameAI*> newAIs;

	for (int i = 0; i < aiCount; ++i)
	{
		GameAI* ai = new GameAI(startingPosition);
		aiList.push_back(ai);
		newAIs.push_back(ai);

		Logger::Instance().Log("Created: " + ai->GetName() + "\n");
	}

	return newAIs;
}

void GameLoop::RunGameLoop(double durationSeconds, unsigned int fps, std::function<void(float)> perFrame)
{
	using clock = std::chrono::steady_clock;
	using secondsd = std::chrono::duration<double>;

	const secondsd targetFrameDuration(1.0 / static_cast<double>(fps));

	auto lastFrameStart = clock::now();
	auto startTime = clock::now();

	// do initialization before entering loop
	InitializeGame();

	int frameAmount = 0;

	while (durationSeconds < 0.0 || std::chrono::duration_cast<secondsd>(clock::now() - startTime).count() < durationSeconds)
	{
		frameAmount++;
		auto frameStart = clock::now();
		secondsd delta = frameStart - lastFrameStart;
		lastFrameStart = frameStart;

		float dt = static_cast<float>(delta.count());

		currentFPS = 1 / delta.count();

		// call update with delta in seconds as float
		UpdateGameLoop(dt, std::chrono::duration_cast<secondsd>(clock::now() - startTime).count());
		UpdateRenderer();

		if (renderer && !renderer->IsRunning())
		{
			Logger::Instance().Log("Renderer closed by user. Shutting down game loop.\n");
			renderer->Stop(); // ensure renderer thread is joined / cleaned up
			break;
		}

		// allow quitting with Escape (polled each frame)
		if (renderer && renderer->IsKeyDown(SDL_SCANCODE_ESCAPE))
		{
			Logger::Instance().Log("Escape pressed. Shutting down game loop.\n");
			renderer->Stop();
			break;
		}

		// Sleep until next frame
		auto frameEnd = clock::now();
		secondsd frameTime = frameEnd - frameStart;
		secondsd sleepTime = targetFrameDuration - frameTime;
		if (sleepTime > secondsd::zero())
		{
			std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::microseconds>(sleepTime));
		}
	}

	Logger::Instance().Log("Shutdown \n");
}

void GameLoop::UpdateGameLoop(float delta, double timePassed)
{
	if (keyPressCooldown > 0)
	{
		keyPressCooldown -= delta;
		if (keyPressCooldown < 0.0f) keyPressCooldown = 0.0f;
	}

	if (delta > 0.5f)
		delta = 0.5f;

	delta *= gameSpeed;


	gameTime += delta;
	ClearDebugEntities();

	ExecuteDeathRow();

	HandlePlayerInput(delta);

	if (brain)
		brain->Think(delta);

	for (Movable* m : GetMovables())
	{
		m->Update(delta);
	}

	renderer->UpdateDirtyNodes(brain);
}

void GameLoop::UpdateRenderer()
{
	std::vector<Renderer::Entity> ents;
	ents.reserve(aiList.size() + 1 + debugEnts.size());

	if (DEBUG_MODE)
	{
		std::vector<Renderer::Entity> debugEnts = GetDebugEntities();
		for (Renderer::Entity e : debugEnts)
		{
			ents.push_back(e);
		}
	}

	for (Renderer::Entity e : persistentEnts)
	{
		ents.push_back(e);
	}

	for (GameAI* ai : aiList)
	{
		if (!ai) continue;

		Vec2 p = ai->GetPosition();
		Renderer::Entity e = Renderer::Entity::MakeCircle(p.x, p.y, ai->GetRadius(), ai->GetColor(), true);

		Vec2 dir = ai->GetDirection();
		e.dirX = dir.x;
		e.dirY = dir.y;

		ents.push_back(e);
	}

	if (player)
	{
		Vec2 p = player->GetPosition();
		Renderer::Entity e = Renderer::Entity::MakeCircle(p.x, p.y, player->GetRadius(), player->GetColor(), true);

		Vec2 dir = player->GetDirection();
		e.dirX = dir.x;
		e.dirY = dir.y;

		ents.push_back(e);
	}

	if (brain)
	{

			std::string str1 = "Wood: " + std::to_string(brain->GetResources()->Get(ItemType::Wood));
			std::string str2 = "Iron: " + std::to_string(brain->GetResources()->Get(ItemType::Iron));
			std::string str3 = "Coal: " + std::to_string(brain->GetResources()->Get(ItemType::Coal));
			std::string str4 = "Steel: " + std::to_string(brain->GetResources()->Get(ItemType::Steel));
			std::string str5 = "Swords: " + std::to_string(brain->GetResources()->Get(ItemType::Sword));
			std::string str6 = "Soldiers: " + std::to_string(brain->GetMilitary()->GetSoldierCount());
			std::string str7 = "FPS: " + std::to_string(static_cast<int>(currentFPS));

			std::vector<std::string> overlay = {
				str1,
				str2,
				str3,
				str4,
				str5,
				str6,
				str7
			};

			renderer->SetOverlayLines(resourceOverlay, overlay);
	}

	{
		std::string str1 = "Placing surface: " + ToString(currentPlacingType);
		std::string str2 = "Placing resource: " + ToString(currentPlacingResourceType);

		std::vector<std::string> overlay = {
			str1,
			str2
		};

		renderer->SetOverlayLines(debugOverlay, overlay);
	}

	if (renderer)
	{
		renderer->SetEntities(ents);
		renderer->needsUpdate = true;
	}

}

std::vector<Movable*> GameLoop::GetMovables()
{
	std::vector<Movable*> allMovables;
	for (GameAI* ai : aiList)
	{
		allMovables.push_back(ai);
	}
	if (player)
		allMovables.push_back(player);
	return allMovables;
}

void GameLoop::MouseClickAction()
{
	int x;
	int y;

	if (renderer->IfMouseClickScreen(Renderer::MouseClick::Right, x, y))
	{
		Vec2 clickPos = Vec2(x, y);
		PathNode* clicked = grid.GetNodeAt(clickPos);
		if (clicked)
			Logger::Instance().Log(std::string("RMB click at: ") + clickPos.ToString() + " (Node: " + clicked->position.ToString() + ")\n");

		RMBMouseClickAction(Vec2(x, y));
	}
	if (renderer->IfMouseClickScreen(Renderer::MouseClick::Left, x, y))
	{
		Vec2 clickPos = Vec2(x, y);
		PathNode* clicked = grid.GetNodeAt(clickPos);
		if (clicked)
			Logger::Instance().Log(std::string("LMB click at: ") + clickPos.ToString() + " (Node: " + clicked->position.ToString() + ")\n");
	
		LMBMouseClickAction(Vec2(x, y));
	}
}

void GameLoop::KeyPressed()
{
	if (!renderer)
		return;

	if (keyPressCooldown > 0)
		return;

	if (renderer->IsKeyDown(SDL_SCANCODE_G))
	{
		DEBUG_MODE = !DEBUG_MODE;
		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Debug mode: ") + (DEBUG_MODE ? "ON\n" : "OFF\n"));
	}

	if (renderer->IsKeyDown(SDL_SCANCODE_H))
	{
		USE_FOG_OF_WAR = !USE_FOG_OF_WAR;
		keyPressCooldown = 0.2f;
		RefreshScreen();

		Logger::Instance().Log(std::string("Fog of war mode: ") + (DEBUG_MODE ? "ON\n" : "OFF\n"));
	}

	if (renderer->IsKeyDown(SDL_SCANCODE_SPACE))
	{
		gameSpeed = (gameSpeed == 0.0f ? 1.0f : 0.0f);
		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(gameSpeed) + "\n");
	}

	if (renderer->IsKeyDown(SDL_SCANCODE_TAB))
	{
		placingResource = !placingResource;
		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(placingResource) + "\n");
	}

	if (renderer->IsKeyDown(SDL_SCANCODE_UP))
	{
		if (gameSpeed < 75) // max 75x speed or game starts breaking
		{
			gameSpeed += 5;
			keyPressCooldown = 0.2f;
			Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(gameSpeed) + "\n");
		}
	}
	if (renderer->IsKeyDown(SDL_SCANCODE_DOWN))
	{
		if (gameSpeed > 0)
		{
			gameSpeed -= 5;
			if (gameSpeed < 0)
				gameSpeed = 0;

			keyPressCooldown = 0.2f;
			Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(gameSpeed) + "\n");
		}
	}

	if (renderer->IsKeyDown(SDL_SCANCODE_1))
	{
		if (currentPlacingType + 1 >= PathNode::TypeEnd)
			currentPlacingType = PathNode::Type(PathNode::TypeStart);

		currentPlacingType = PathNode::Type((int)currentPlacingType + 1);

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Upped placing type to " + std::to_string((int)currentPlacingType) + "\n"));
	}

	if (renderer->IsKeyDown(SDL_SCANCODE_2))
	{
		if (currentPlacingType - 1 <= PathNode::TypeStart)
			currentPlacingType = PathNode::Type(PathNode::TypeEnd);

		currentPlacingType = PathNode::Type((int)currentPlacingType - 1);

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Lowered placing type to " + std::to_string((int)currentPlacingType) + "\n"));
	}
	if (renderer->IsKeyDown(SDL_SCANCODE_3))
	{
		if (currentPlacingResourceType + 1 >= PathNode::ResourceEnd)
			currentPlacingResourceType = PathNode::ResourceType(PathNode::ResourceStart);

		currentPlacingResourceType = PathNode::ResourceType((int)currentPlacingResourceType + 1);

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Upped placing resource type to " + std::to_string((int)currentPlacingResourceType) + "\n"));
	}
	if (renderer->IsKeyDown(SDL_SCANCODE_4))
	{
		if (currentPlacingResourceType + 1 <= PathNode::ResourceStart)
			currentPlacingResourceType = PathNode::ResourceType(PathNode::ResourceEnd);

		currentPlacingResourceType = PathNode::ResourceType((int)currentPlacingResourceType - 1);

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Lowered placing resource type to " + std::to_string((int)currentPlacingResourceType) + "\n"));
	}

}

void GameLoop::LMBMouseClickAction(Vec2 clickPos)
{
	PathNode* node = grid.GetNodeAt(clickPos);

	if (node == nullptr)
		return;

	PathNode::Type placingType = currentPlacingType;
	PathNode::ResourceType placingResourceType = currentPlacingResourceType;
	// if the same node is pressed twice, remove the type node instead
	if (currentPlacingType == node->type)
		placingType = PathNode::Nothing;

	float resourceAmount = 100.0f;
	if (currentPlacingResourceType == node->resource)
	{
		resourceAmount = 0;
		placingResourceType = PathNode::None;
	}

	if (placingResource)
		grid.SetNode(node, placingResourceType, resourceAmount);
	else
		grid.SetNode(node, placingType);
}

void GameLoop::RMBMouseClickAction(Vec2 clickPos)
{
	PathNode* node = grid.GetNodeAt(clickPos);

	for (auto ai : aiList)
	{
		bool valid = true;
		ai->GoTo(node, valid);
	}
}

void GameLoop::CreatePlayer(Vec2 pos)
{
	if (player)
		return;

	Vec2 playerPos = pos;
	player = new Player(playerPos);
}

void GameLoop::HandlePlayerInput(float delta)
{
	if (!player || !renderer)
		return;
	Vec2 moveDir(0.0f, 0.0f);
	if (renderer->IsKeyDown(SDL_SCANCODE_W))
	{
		moveDir.y -= 1.0f;
	}
	if (renderer->IsKeyDown(SDL_SCANCODE_S))
	{
		moveDir.y += 1.0f;
	}
	if (renderer->IsKeyDown(SDL_SCANCODE_A))
	{
		moveDir.x -= 1.0f;
	}
	if (renderer->IsKeyDown(SDL_SCANCODE_D))
	{
		moveDir.x += 1.0f;
	}

	player->SetDirection(moveDir);
}

void GameLoop::ExecuteDeathRow()
{
	for (GameAI* ai : deathRow)
	{
		auto it = std::find(aiList.begin(), aiList.end(), ai);
		if (it != aiList.end())
		{

			Renderer::Entity te = Renderer::Entity::MakeCircle(
				ai->GetPosition().x, ai->GetPosition().y, ai->GetRadius(), ai->GetColor(), true);

			persistentEnts.push_back(te);

			delete ai;
			aiList.erase(it);
		}
	}
	deathRow.clear();
}

void GameLoop::AddDebugEntity(Vec2 pos, uint32_t color, int radius, bool filled)
{
	Renderer::Entity te = Renderer::Entity::MakeCircle(pos.x, pos.y, radius, color, filled);
	debugEnts.push_back(te);
}

void GameLoop::AddDebugEntity(Renderer::Entity e)
{
	debugEnts.push_back(e);
}

void GameLoop::AddDebugLine(Vec2 a, Vec2 b, uint32_t color, float thickness)
{
	Renderer::Entity e = Renderer::Entity::MakeLine(a.x, a.y, b.x, b.y, thickness, color);
	debugEnts.push_back(e);
}

void GameLoop::AddPersistentLine(Vec2 a, Vec2 b, uint32_t color, float thickness)
{
	Renderer::Entity e = Renderer::Entity::MakeLine(a.x, a.y, b.x, b.y, thickness, color);
	persistentEnts.push_back(e);
}
