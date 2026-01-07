#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <Windows.h>
#include "GameLoop.h"
#include "Constants.h"
#include "AStar.h"
#include <direct.h>
#include <filesystem>
#include "AIBrainManagers.h"


struct DataPoint
{
	DataPoint(float x, float y, PathNode::Type type) : x(x), y(y), type(type) {}
	float x;
	float y;
	PathNode::Type type;
};

static std::vector<DataPoint> LoadDataFile(const std::string& filename)
{
	std::vector<DataPoint> result; 
	std::ifstream file(filename); 
	if (!file) return result;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty()) continue;
		std::istringstream iss(line);
		int typeInt;
		float x, y;
		if (iss >> typeInt >> x >> y)
		{
			result.push_back({ x, y, static_cast<PathNode::Type>(typeInt) });
		}
	}

	return result;
}

GameLoop::GameLoop() : grid(WORLD_WIDTH, WORLD_HEIGHT, DEFAULT_CELL_SIZE)
{
}

GameLoop::~GameLoop()
{
	if (renderer)
	{
		renderer->Stop();
	}
	delete renderer;

	DeleteAll();
}

void GameLoop::DeleteAll()
{
	if (player)
		delete player;
	player = nullptr;

	delete pathfinder;

	for (GameAI* ai : aiList)
	{
		delete ai;
	}

	aiList.clear();
	persistentEnts.clear();
	debugEnts.clear();
	grid.ClearMovables();
}

void GameLoop::InitializeGame()
{
	pathfinder = new AStar(&grid);

	CreateAI(1);

	//CreatePlayer(Vec2(WORLD_WIDTH / 2, WORLD_HEIGHT / 2));

	LoadGameData();

	grid.SetClearence();

	grid.DrawGrid();
}

void GameLoop::SaveData()
{ 
	// Ensure directory exists
	std::error_code ec;
	std::filesystem::create_directories(dataPath, ec);

	std::string runName = dataPath + "/" + positionPath;
	std::ofstream s(runName, std::ios::out | std::ios::trunc);
	if (!s) return;

	s.setf(std::ios::fixed);
	s << std::setprecision(6);

	auto& nodes = grid.GetNodes();
	int rows = grid.GetRows();
	int cols = grid.GetCols();

	// Write one node per line: "<type> <x> <y>\n"
	for (int r = 0; r < rows; ++r)
	{
		for (int c = 0; c < cols; ++c)
		{
			PathNode& n = nodes[r][c];
			if (n.type <= PathNode::Start || n.type >= PathNode::End) continue;
			s << static_cast<int>(n.type) << " " << n.position.x << " " << n.position.y << "\n";
		}
	}

	s.flush();
	s.close();
}

void GameLoop::LoadGameData()
{ // Ensure directory exists (create if missing) 
	std::error_code ec; std::filesystem::create_directories(dataPath, ec); // create if needed
	std::string fileName = dataPath + "/" + positionPath;
	std::vector<DataPoint> dataPoints = LoadDataFile(fileName);
	for (const DataPoint& p : dataPoints)
	{
		grid.SetNode(grid.GetNodeAt({ p.x, p.y }), p.type);
	}
}

void GameLoop::CreateAI(int count)
{
	int aiCount = count;

	if (aiCount <= 0)
		return;

	std::vector<Vec2> startingPositions = {
		Vec2(110.000000, 90.000000),
		Vec2(310.000000, 90.000000),
		Vec2(510.000000, 90.000000),
		Vec2(710.000000, 90.000000)
	};

	for (int i = 0; i < aiCount; ++i)
	{
		Vec2 aiPos;
		if (i >= startingPositions.size())
			aiPos = Vec2(static_cast<float>(rand() % (WORLD_WIDTH - 100) + 50), static_cast<float>(rand() % (WORLD_HEIGHT - 100) + 50));
		else
			aiPos = Vec2(startingPositions[i].x, startingPositions[i].y);

		GameAI* ai = new GameAI(aiPos);
		aiList.push_back(ai);

		Logger::Instance().Log("Created: " + ai->GetName() + "\n");
	}
}

void GameLoop::RunGameLoop(double durationSeconds, unsigned int fps, std::function<void(float)> perFrame)
{
	using clock = std::chrono::steady_clock;
	using secondsd = std::chrono::duration<double>;

	const secondsd targetFrameDuration(1.0 / static_cast<double>(fps));

	auto lastFrameStart = clock::now();
	auto startTime = clock::now();

	// create renderer and start window
	renderer = new Renderer(WINDOW_WIDTH, WINDOW_HEIGHT);
	renderer->Start();

	// do initialization before entering loop
	InitializeGame();

	int frameAmount = 0;

	while (durationSeconds < 0.0 || std::chrono::duration_cast<secondsd>(clock::now() - startTime).count() < durationSeconds)
	{
		frameAmount++;
		auto frameStart = clock::now();
		secondsd delta = frameStart - lastFrameStart;
		lastFrameStart = frameStart;

		if (frameAmount % 300 == 299)
			SaveData();

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
		if (renderer && renderer->IsKeyDown(VK_ESCAPE))
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

	SaveData();
	Logger::Instance().Log("Shutdown \n");
}

void GameLoop::UpdateGameLoop(float delta, double timePassed)
{
	HandleInput(delta);

	delta *= gameSpeed;
	gameTime += delta;
	grid.ClearMovables();
	ClearDebugEntities();

	ExecuteDeathRow();

	for (Movable* m : GetMovables())
		grid.InsertMovable(m);

	for (GameAI* ai : aiList)
	{
		ai->Update(delta);
	}
	HandlePlayerInput(delta);
}

void GameLoop::UpdateRenderer()
{
	renderer->ClearOverlayLines();
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

	for (Renderer::Entity e : environmentEnts)
	{
		ents.push_back(e);
	}

	for (GameAI* ai : aiList)
	{
		if (!ai) continue;

		Vec2 p = ai->GetPosition();
		Renderer::Entity e = Renderer::Entity::MakeCircle(p.x, p.y, ai->GetRadius(), ai->GetColor(), true, ai->GetName());

		Vec2 dir = ai->GetDirection();
		e.dirX = dir.x;
		e.dirY = dir.y;

		ents.push_back(e);
	}

	if (player)
	{
		Vec2 p = player->GetPosition();
		Renderer::Entity e = Renderer::Entity::MakeCircle(p.x, p.y, player->GetRadius(), player->GetColor(), true, player->GetName());

		Vec2 dir = player->GetDirection();
		e.dirX = dir.x;
		e.dirY = dir.y;

		ents.push_back(e);
	}

	if (!aiList.empty())
	{

		std::string str1 = "Wood: " + std::to_string(aiList[0]->GetBrain()->GetResources()->Get(ResourceType::Wood));
		std::string str2 = "Iron: " + std::to_string(aiList[0]->GetBrain()->GetResources()->Get(ResourceType::Iron));
		std::string str3 = "Coal: " + std::to_string(aiList[0]->GetBrain()->GetResources()->Get(ResourceType::Coal));
		std::string str4 = "Soldiers: " + std::to_string(aiList[0]->GetBrain()->GetMilitary()->GetSoldierCount());
		std::string str5 = "FPS: " + std::to_string(static_cast<int>(currentFPS));

		std::vector<std::string> overlay = {
			str1,
			str2,
			str3,
			str4,
			str5
		};

		renderer->SetOverlayLines(overlay);
	}

	if (renderer)
	{
		renderer->SetEntities(ents);
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

void GameLoop::HandleInput(float delta)
{
	if (!renderer)
		return;

	Vec2 clickLPos;
	if (renderer->FetchLClick(clickLPos))
	{
		Logger::Instance().Log(std::string("LMB click at: ") + clickLPos.ToString() + " (Node: " + grid.GetNodeAt(clickLPos)->position.ToString() + ")\n");
		LMBMouseClickAction(clickLPos);
	}
	Vec2 clickRPos;
	if (renderer->FetchRClick(clickRPos))
	{
		Logger::Instance().Log(std::string("RMB click at: ") + clickRPos.ToString() + " (Node: " + grid.GetNodeAt(clickRPos)->position.ToString() + ")\n");
		RMBMouseClickAction(clickRPos);
	}

	if (keyPressCooldown > 0)
	{
		keyPressCooldown -= delta;
		if (keyPressCooldown < 0.0f) keyPressCooldown = 0.0f;
		return;
	}

	if (renderer->IsKeyDown('G'))
	{
		DEBUG_MODE = !DEBUG_MODE;
		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Debug mode: ") + (DEBUG_MODE ? "ON\n" : "OFF\n"));
	}

	if (renderer->IsKeyDown(VK_SPACE))
	{
		gameSpeed = (gameSpeed == 0.0f ? 1.0f : 0.0f);
		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(gameSpeed) + "\n");
	}

	if (renderer->IsKeyDown(VK_UP))
	{
		gameSpeed += 1;
		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(gameSpeed) + "\n");
	}
	if (renderer->IsKeyDown(VK_DOWN))
	{
		gameSpeed -= 1;
		if (gameSpeed < 0)
			gameSpeed = 0;

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Game speed set to: ") + std::to_string(gameSpeed) + "\n");
	}

	if (renderer->IsKeyDown('1'))
	{
		if (currentPlacingType + 1 >= PathNode::End)
			currentPlacingType = PathNode::Type(PathNode::Start);

		currentPlacingType = PathNode::Type((int)currentPlacingType + 1);

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Upped placing type to " + std::to_string((int)currentPlacingType) + "\n"));
	}

	if (renderer->IsKeyDown('2'))
	{
		if (currentPlacingType - 1 <= PathNode::Start)
			currentPlacingType = PathNode::Type(PathNode::End);

		currentPlacingType = PathNode::Type((int)currentPlacingType - 1);

		keyPressCooldown = 0.2f;
		Logger::Instance().Log(std::string("Lowered placing type to " + std::to_string((int)currentPlacingType) + "\n"));
	}

}

void GameLoop::CreatePlayer(Vec2 pos)
{
	if (player)
		return;

	Vec2 playerPos = pos;
	player = new Player(playerPos);
}

void GameLoop::LMBMouseClickAction(Vec2 clickPos)
{
	PathNode* node = grid.GetNodeAt(clickPos);

	PathNode::Type placingType = currentPlacingType;
	// if the same node is pressed twice, remove the type node instead
	if (currentPlacingType == node->type)
		placingType = PathNode::Nothing;

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

void GameLoop::HandlePlayerInput(float delta)
{
	if (!player || !renderer)
		return;
	Vec2 moveDir(0.0f, 0.0f);
	if (renderer->IsKeyDown('W'))
	{
		moveDir.y -= 1.0f;
	}
	if (renderer->IsKeyDown('S'))
	{
		moveDir.y += 1.0f;
	}
	if (renderer->IsKeyDown('A'))
	{
		moveDir.x -= 1.0f;
	}
	if (renderer->IsKeyDown('D'))
	{
		moveDir.x += 1.0f;
	}


	player->Update(moveDir, MAXIMUM_ACCELERATION, delta);
}

void GameLoop::ExecuteDeathRow()
{
	for (GameAI* ai : deathRow)
	{
		auto it = std::find(aiList.begin(), aiList.end(), ai);
		if (it != aiList.end())
		{

			Renderer::Entity te = Renderer::Entity::MakeCircle(
				ai->GetPosition().x, ai->GetPosition().y, ai->GetRadius(), ai->GetColor(), true, "DEAD " + ai->GetName()
			);

			persistentEnts.push_back(te);

			delete ai;
			aiList.erase(it);
		}
	}
	deathRow.clear();
}

void GameLoop::AddDebugEntity(Vec2 pos, uint32_t color, int radius, bool filled, std::string name)
{
	Renderer::Entity te = Renderer::Entity::MakeCircle(pos.x, pos.y, radius, color, filled, name);
	debugEnts.push_back(te);
}

void GameLoop::AddDebugLine(Vec2 a, Vec2 b, uint32_t color, float thickness)
{
	Renderer::Entity e = Renderer::Entity::MakeLine(a.x, a.y, b.x, b.y, thickness, color);
	debugEnts.push_back(e);
}
