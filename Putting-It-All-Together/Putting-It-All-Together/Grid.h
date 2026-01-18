#pragma once
#include <vector>
#include <string>
#include "PathNode.h"
#include "Vec2.h"
#include "Movable.h"

class Grid
{
public:
	// Constructor for Grid that creates a fully open map
	// --------------------------
	// width - how wide the grid is
	// height - how high the grid is
	// cellsize - the diameter of each cell
	Grid(int width, int height, int cellSize, Vec2 gridSize = {0, 0});

	~Grid()
	{
		//for (auto& specialNode : nodeLocations)
		//{
		//	if (specialNode != nullptr)
		//	{
		//		delete specialNode;
		//	}
		//}
		nodeLocations.clear();
	}

	bool WorldToGrid(const Vec2& pos, int& row, int& col) const;
	
	// Draw the outlines of the grid
	// --------------------------
	// color - the color of the lines
	void DrawGridLines();

	// Get the amount of rows in the grid
	// --------------------------
	// returns the amount of rows in the grid
	int GetRows() const { return rows; }

	// Get the amount of columns in the grid
	// --------------------------
	// returns the amount of columns in the grid
	int GetCols() const { return cols; }

	// Get the size the cells in the grid
	// --------------------------
	// returns diameter of the cells
	int GetCellSize() const { return cellSize; }

	// Get the position of the walls of the grid
	// --------------------------
	// returns the position of the grid walls in the order: left, bottom, right, top
	std::vector<float> GetGlobalGridPosition();

	// Get the nodes in the grid
	// --------------------------
	// returns vector of the rows containing all the nodes in the grid
	std::vector<std::vector<PathNode>>& GetNodes() { return nodes; }

	void SetClearance();

	void QueryEnt(const Vec2& pos, float radius, std::vector<Movable*>& out);
	void QueryNodes(const Vec2& pos, float radius, std::vector<PathNode*>& out, std::vector<PathNode::ResourceType> types = std::vector<PathNode::ResourceType>());
	void UpdateMovable(Movable* m);

	inline int Index(int x, int y) const
	{
		return y * cols + x;
	}

	std::pair<int, int> TwoDIndex(int index)
	{
		int rowSize = cols;

		int row = index / rowSize;
		int col = index % rowSize;

		return std::pair<int, int>(row, col);
	}

	void SetNode(PathNode* node, PathNode::Type type);

	void SetNode(PathNode* node, PathNode::ResourceType type, float resourceAmount = 0);

	PathNode* GetNodeAt(Vec2 pos);

	bool HasLineOfSight(const Vec2& from, const Vec2& to, float agentRadius) const;

	float cellSize = 20;

private:
	int width;
	int height;
	int rows;
	int cols;
	std::vector<std::vector<PathNode>> nodes;
	std::vector<std::vector<Movable*>> movableLocations;
	std::vector<PathNode*> nodeLocations;

	// Set all neighbors for all nodes
	// --------------------------
	// rows - the amount of rows in the grid
	// cols - the amount of columns in the grid
	void SetNeighbors(int rows, int cols);

	// Get the center of the node supposed to be located at row, column
	// --------------------------
	// row - the row of the node
	// col - the column of the node
	// --------------------------
	// returns the center position of the node
	Vec2 GetCellCenter(int row, int col);
};