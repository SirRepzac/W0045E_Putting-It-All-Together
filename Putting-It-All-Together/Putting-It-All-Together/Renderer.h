#pragma once

#include "Constants.h"
#include "Vec2.h"
#include "PathNode.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <array>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

class AIBrain;

static uint32_t Seed(int i)
{
    uint32_t x = (uint32_t)i;
    x ^= 0x9E3779B9;
    x *= 0x85EBCA6B;
    x ^= x >> 13;
    return x;
}

class Renderer
{
public:

    enum MouseClick
    {
        Right,
        Middle,
        Left
    };

    struct Entity
    {
        enum class Type { Circle, Line, Rectangle };

        Type type = Type::Circle;

        bool filled = true;

        float x = 0.0f;
        float x2 = 0.0f;

        int radius = 8;
        int thickness = 2;

        float y = 0.0f;
        float y2 = 0.0f;

        uint32_t color = 0x0078C8;

        float dirX = 0.0f;
        float dirY = 0.0f;

        static Entity MakeCircle(float x, float y, int r, uint32_t col, bool filled = true)
        {
            Entity e;
            e.filled = filled;
            e.type = Type::Circle;
            e.x = x; e.y = y;
            e.radius = r;
            e.color = col;
            return e;
        }

        static Entity MakeLine(float x1, float y1, float x2, float y2, float thick, uint32_t col)
        {
            Entity e;
            e.type = Type::Line;
            e.x = x1; e.y = y1;
            e.x2 = x2; e.y2 = y2;
            e.thickness = thick;
            e.color = col;
            return e;
        }

        static Entity MakeRect(float left, float bottom, float right, float top, uint32_t col, bool filled = true, int thick = 2)
        {
            Entity e;
            e.filled = filled;
            e.type = Type::Rectangle;
            e.x = left; e.y = bottom;
            e.x2 = right; e.y2 = top;
            e.thickness = thick;
            e.color = col;
            return e;
        }
    };

    struct Overlay
    {
        std::mutex overlayMtx_;
        std::vector<std::string> overlayLines_;
        Vec2 position;
    };

    void AddOverlay(Overlay* o)
    {
        overlays.push_back(o);
    }

    struct DrawNode
    {
        float xPos;
        float yPos;
        float width;
        float height;

        PathNode::Type type;
		PathNode::ResourceType resource;
		float resourceAmount;
    };

    std::vector<DrawNode> nodeCache;
    std::vector<bool> nodeNeedsUpdate;

    void UpdateNode(size_t index, const DrawNode& node)
    {
        nodeCache[index] = node;
        MarkNodeDirty(index);
    }

    void MarkNodeDirty(size_t index)
    {
        nodeNeedsUpdate[index] = true;
    }

    void UpdateDirtyNodes(const AIBrain* brain);

    // helper to build 0xRRGGBB color
    static inline uint32_t Color(uint8_t r, uint8_t g, uint8_t b)
    {
        return (static_cast<uint32_t>(r) << 16) |
            (static_cast<uint32_t>(g) << 8) |
            static_cast<uint32_t>(b);
    }

    static inline uint32_t Black = 0x000000;
    static inline uint32_t White = 0xFFFFFF;
    static inline uint32_t Red = 0xFF0000;
    static inline uint32_t Lime = 0x00FF00;
    static inline uint32_t Blue = 0x0000FF;
    static inline uint32_t Olive = 0x808000;
    static inline uint32_t Purple = 0x800080;
    static inline uint32_t Maroon = 0x800000;
    static inline uint32_t Yellow = 0xFFFF00;
    static inline uint32_t DarkGray = 0x575757;

    Renderer(int width, int height);
    ~Renderer();

    // Start window thread
    void Start();
    // Stop thread and destroy window
    void Stop();

    // Provide entities to draw
    void SetEntities(const std::vector<Entity>& entities);

    // Overlay text
    void SetOverlayLines(Overlay& overlay, const std::vector<std::string>& lines);
    void ClearOverlayLines(Overlay& overlay);

    bool IsRunning() const;

    bool IsKeyDown(unsigned int vk) const;       // called from game thread

    bool IfMouseClickScreen(MouseClick click, int& x, int& y);

    //// Called from game thread to fetch the last click (returns true and clears if present)
    bool FetchRClick(Vec2& out);
    bool FetchLClick(Vec2& out);

    bool needsUpdate = true;

    void RenderRect(float xPos, float yPos, float width, float heigth, bool filled, float scale);
    void RenderCircle(float xPos, float yPos, float radius, float scale);
private:
    // non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void ThreadMain();
    void RenderFrame();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;

    int width_;
    int height_;
    std::thread thread_;

    // shared state
    std::atomic<bool> running_;
    std::mutex entityMtx_;
    std::vector<Entity> entities_;         // store entities (positions + color + name)

    std::vector<Overlay*> overlays;

    // LMB click state
    std::mutex clickLMtx_;
    bool hasLClick_ = false;
    float clickLWorldX_ = 0.0f;
    float clickLWorldY_ = 0.0f;

    // LMB click state
    std::mutex clickRMtx_;
    bool hasRClick_ = false;
    float clickRWorldX_ = 0.0f;
    float clickRWorldY_ = 0.0f;
};