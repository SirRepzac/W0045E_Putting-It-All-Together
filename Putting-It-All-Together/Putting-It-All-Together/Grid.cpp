#include "Grid.h"
#include "GameLoop.h"

Grid::Grid(int width, int height, int cellSize)
{
	this->width = width;
	this->height = height;
	this->cellSize = cellSize;
	rows = height / cellSize;
	cols = width / cellSize;

	int nodeIds = 0;

	if (rows <= 0 || cols <= 0 || cellSize <= 0) return;

	nodes.assign(rows, std::vector<PathNode>(cols));
	movablesLocations.resize(cols * rows);
	specialLocations.resize(cols * rows);

	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			nodes[r][c].position = GetCellCenter(r, c);
			nodes[r][c].id = nodeIds++;
			nodes[r][c].size = cellSize / 2;
		}
	}

	SetNeighbors(rows, cols);
	SetClearence();
}

void Grid::SetClearence()
{
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			PathNode& node = nodes[r][c];
			if (node.IsObstacle())
			{
				node.clearance = 0;
				continue;
			}

			float shortest = INT_MAX;
			for (PathNode* n : specialLocations)
			{
				if (n == nullptr)
					continue;
				if (!n->IsObstacle())
					continue;

				float distBetw = DistanceBetween(n->position, node.position);
				if (distBetw < shortest)
					shortest = distBetw;
			}

			node.clearance = shortest - (DEFAULT_CELL_SIZE / 2);
		}
	}
}

bool Grid::WorldToGrid(const Vec2& pos, int& row, int& col) const
{
	float realWidth = cols * cellSize;
	float realHeight = rows * cellSize;

	Vec2 centering((width - realWidth) / 2.0f,
		(height - realHeight) / 2.0f);

	Vec2 adjusted = pos - centering;

	col = static_cast<int>(adjusted.x / cellSize);
	row = static_cast<int>(adjusted.y / cellSize);

	return row >= 0 && row < rows && col >= 0 && col < cols;
}

PathNode* Grid::GetNodeAt(Vec2 pos)
{
	int row;
	int col;

	if (!WorldToGrid(pos, row, col))
		return nullptr;

	return &nodes.at(row).at(col);
}

bool Grid::HasLineOfSight(const Vec2& from, const Vec2& to, float agentRadius) const
{
	int r0, c0, r1, c1;
	if (!WorldToGrid(from, r0, c0) || !WorldToGrid(to, r1, c1))
		return false;

	float x0 = c0 + 0.5f;
	float y0 = r0 + 0.5f;
	float x1 = c1 + 0.5f;
	float y1 = r1 + 0.5f;

	float dx = x1 - x0;
	float dy = y1 - y0;

	int stepX = (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
	int stepY = (dy > 0) ? 1 : (dy < 0 ? -1 : 0);

	constexpr float INF = std::numeric_limits<float>::infinity();

	float tDeltaX = (dx != 0.0f) ? std::abs(1.0f / dx) : INF;
	float tDeltaY = (dy != 0.0f) ? std::abs(1.0f / dy) : INF;

	float tMaxX;
	float tMaxY;

	if (dx > 0)
		tMaxX = ((std::floor(x0) + 1.0f) - x0) * tDeltaX;
	else
		tMaxX = (x0 - std::floor(x0)) * tDeltaX;

	if (dy > 0)
		tMaxY = ((std::floor(y0) + 1.0f) - y0) * tDeltaY;
	else
		tMaxY = (y0 - std::floor(y0)) * tDeltaY;

	int x = c0;
	int y = r0;

	bool first = true;

	while (true)
	{
		if (x < 0 || x >= cols || y < 0 || y >= rows)
			return false;

		if (!first)
		{
			const PathNode& node = nodes[y][x];

			// Treat insufficient clearance as blocked
			if (node.IsObstacle() || node.clearance < agentRadius)
				return false;
		}

		first = false;

		if (x == c1 && y == r1)
			break;

		if (tMaxX < tMaxY)
		{
			tMaxX += tDeltaX;
			x += stepX;
		}
		else if (tMaxY < tMaxX)
		{
			tMaxY += tDeltaY;
			y += stepY;
		}
		else // EXACT corner hit
		{
			// advance both
			tMaxX += tDeltaX;
			tMaxY += tDeltaY;
			x += stepX;
			y += stepY;
		}
	}

	return true;
}

void Grid::SetNeighbors(int rows, int cols)
{
	// Set neighbors for each node
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			PathNode& node = nodes[r][c];
			node.neighbors.clear();

			// Obstacles have no neighbors
			if (node.IsObstacle()) continue;

			// Order of the neighbors will be: 
			// top left -> top middle -> top right -> 
			// -> middle left -> skip self -> middler right ->
			// -> bottom left -> bottom middle -> bottom right
			for (int dr = -1; dr <= 1; dr++)
			{
				for (int dc = -1; dc <= 1; dc++)
				{
					if (dr == 0 && dc == 0) continue; // Skip self

					int neighborRow = r + dr;
					int neighborCol = c + dc;

					// Check bounds
					if (neighborRow < 0 || neighborRow >= rows || neighborCol < 0 || neighborCol >= cols)
						continue;

					PathNode& neighbor = nodes[neighborRow][neighborCol];

					node.neighbors.push_back(&neighbor);
				}
			}
		}
	}
}

void Grid::DrawGrid()
{
	GameLoop::Instance().ClearDrawEntities();

	float realWidth = cols * cellSize;
	float realHeight = rows * cellSize;

	Vec2 centeringVec = Vec2((width - realWidth) / 2.0f, (height - realHeight) / 2.0f);

	if (GameLoop::Instance().DEBUG_MODE)
	{
		if (false) // show grid lines
		{

			uint32_t lineColor = Renderer::Black;
			for (int i = 0; i <= rows; i++)
			{
				Vec2 vec1 = Vec2(0.0f, static_cast<float>(i * cellSize)) + centeringVec;
				Vec2 vec2 = Vec2(realWidth, static_cast<float>(i * cellSize)) + centeringVec;

				auto e = Renderer::Entity::MakeLine(vec1.x, vec1.y, vec2.x, vec2.y, 2.0f, lineColor);
				GameLoop::Instance().AddDrawEntity(e);
			}
			for (int j = 0; j <= cols; j++)
			{
				Vec2 vec1 = Vec2(static_cast<float>(j * cellSize), 0.0f) + centeringVec;
				Vec2 vec2 = Vec2(static_cast<float>(j * cellSize), realHeight) + centeringVec;

				auto e = Renderer::Entity::MakeLine(vec1.x, vec1.y, vec2.x, vec2.y, 2.0f, lineColor);
				GameLoop::Instance().AddDrawEntity(e);
			}
		}
	}

	{
		// Draw special nodes
		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < cols; c++)
			{
				if (nodes[r][c].type != PathNode::Nothing)
				{
					auto e = nodes[r][c].Draw();
					GameLoop::Instance().AddDrawEntity(e);
				}
			}
		}
	}

}

std::vector<float> Grid::GetPosition()
{
	float realWidth = cols * cellSize;
	float realHeight = rows * cellSize;

	Vec2 centeringVec = Vec2((width - realWidth) / 2.0f, (height - realHeight) / 2.0f);

	float bottom = centeringVec.y;
	float top = realHeight + centeringVec.y;
	float left = centeringVec.x;
	float right = realWidth + centeringVec.x;

	return std::vector<float>{left, bottom, right, top};
}

Vec2 Grid::GetCellCenter(int row, int col)
{
	float realWidth = cols * cellSize;
	float realHeight = rows * cellSize;
	Vec2 centeringVec = Vec2((width - realWidth) / 2, (height - realHeight) / 2);

	return Vec2(static_cast<float>(col * cellSize + cellSize * 0.5f), static_cast<float>(row * cellSize + cellSize * 0.5f)) + centeringVec;
}

void Grid::QueryEnt(const Vec2& pos, float radius, std::vector<Movable*>& out)
{
	int minX = (int)((pos.x - radius) / cellSize);
	int maxX = (int)((pos.x + radius) / cellSize);
	int minY = (int)((pos.y - radius) / cellSize);
	int maxY = (int)((pos.y + radius) / cellSize);

	for (int cy = minY; cy <= maxY; ++cy)
		for (int cx = minX; cx <= maxX; ++cx)
		{
			if (cx < 0 || cy < 0 || cx >= cols || cy >= rows)
				continue;

			for (Movable* m : movablesLocations[Index(cx, cy)])
				out.push_back(m);
		}
}

// Query special nodes of a certain type within a radius
// given an unspecified type, it will return all special nodes
void Grid::QueryNodes(const Vec2& pos, float radius, std::vector<PathNode*>& out, PathNode::Type type)
{
	int minX = (int)((pos.x - radius) / cellSize);
	int maxX = (int)((pos.x + radius) / cellSize);
	int minY = (int)((pos.y - radius) / cellSize);
	int maxY = (int)((pos.y + radius) / cellSize);

	for (int cy = minY; cy <= maxY; ++cy)
		for (int cx = minX; cx <= maxX; ++cx)
		{
			if (cx < 0 || cy < 0 || cx >= cols || cy >= rows)
				continue;

			auto loc = specialLocations[Index(cx, cy)];

			if (loc == nullptr || (!(loc->type == type) && type != PathNode::Nothing))
				continue;

			out.push_back(loc);
		}
}

void Grid::InsertMovable(Movable* m)
{
	int cx = static_cast<int>(m->GetPosition().x / cellSize);
	int cy = static_cast<int>(m->GetPosition().y / cellSize);

	if (cx < 0 || cy < 0 || cx >= cols || cy >= rows)
		return;

	movablesLocations[Index(cx, cy)].push_back(m);
}