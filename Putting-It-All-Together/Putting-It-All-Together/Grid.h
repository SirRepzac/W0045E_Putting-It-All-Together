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
	Grid(int width, int height, int cellSize);

	~Grid()
	{
		//for (auto& specialNode : specialLocations)
		//{
		//	if (specialNode != nullptr)
		//	{
		//		delete specialNode;
		//	}
		//}
		specialLocations.clear();
	}

	bool WorldToGrid(const Vec2& pos, int& row, int& col) const;
	
	// Draw the outlines of the grid
	// --------------------------
	// color - the color of the lines
	void DrawGrid();

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
	std::vector<float> GetPosition();

	// Get the nodes in the grid
	// --------------------------
	// returns vector of the rows containing all the nodes in the grid
	std::vector<std::vector<PathNode>>& GetNodes() { return nodes; }

	void SetClearence();

	void QueryEnt(const Vec2& pos, float radius, std::vector<Movable*>& out);
	void QueryNodes(const Vec2& pos, float radius, std::vector<PathNode*>& out, PathNode::Type type = PathNode::Nothing);
	void InsertMovable(Movable* m);

	void ClearMovables()
	{
		for (auto& c : movablesLocations) c.clear();
	}

	inline int Index(int x, int y) const
	{
		return y * cols + x;
	}

	void SetSpecialNode(PathNode* node, PathNode::Type type, uint32_t specialColor = 0xFFFFFF)
	{
		RemoveSpecialNode(node);

		int row = static_cast<int>(node->position.y / cellSize);
		int col = static_cast<int>(node->position.x / cellSize);

		PathNode* baseNode = &nodes.at(row).at(col);
		baseNode->type = PathNode::Nothing;

		// if caller wants Nothing, we're done
		if (type == PathNode::Nothing)
			return;

		if (type == PathNode::Wood)
			baseNode->color = 0x5E3500; // brown
		else if (type == PathNode::Coal)
			baseNode->color = 0x000000; // black
		else if (type == PathNode::Iron)
			baseNode->color = 0xC0C0C0; // silver
		else if (type == PathNode::Wall)
			baseNode->color = 0xFF0000; // red
		else if (type == PathNode::Start)
			baseNode->color = 0xFFFFFF; // white
		else if (type == PathNode::End)
			baseNode->color = 0xFFFFFF; // white
		else if (type == PathNode::Special)
			baseNode->color = specialColor;

		baseNode->type = type;
		specialLocations[Index(col, row)] = baseNode;

		if (baseNode->IsObstacle())
			SetClearence();

		DrawGrid();
	}

	void RemoveSpecialNode(PathNode* node)
	{
		int row = static_cast<int>(node->position.y / cellSize);
		int col = static_cast<int>(node->position.x / cellSize);

		if (row < 0 || row >= rows || col < 0 || col >= cols)
			return;

		const int idx = Index(col, row);
		if (!specialLocations[idx])
			return;

		// destroy and clear
		//delete specialLocations[idx];
		specialLocations[idx] = nullptr;

		// ensure grid node reflects that
		PathNode* baseNode = &nodes.at(row).at(col);
		baseNode->color = 0xFFFFFF; // white
		baseNode->type = PathNode::Nothing;
	}

	PathNode* GetNodeAt(Vec2 pos);

	bool HasLineOfSight(const Vec2& from, const Vec2& to, float agentRadius) const;

private:
	int width;
	int height;
	int cellSize;
	int rows;
	int cols;
	std::vector<std::vector<PathNode>> nodes;
	std::vector<std::vector<Movable*>> movablesLocations;
	std::vector<PathNode*> specialLocations;

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