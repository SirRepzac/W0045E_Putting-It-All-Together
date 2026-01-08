#pragma once
#include "Constants.h"
#include "Vec2.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <utility>
#include <atomic>
#include <condition_variable>
#include <array>
#include <limits>
#include <algorithm>

class AIBrain;

class Renderer
{
public:
    // Simple entity description used for drawing
    struct Entity
    {
        enum class Type { Circle, Line, Rectangle };

        Type type = Type::Circle;

        bool filled = true;

        // circle fields
        float x = 0.0f;
        float y = 0.0f;
        int radius = 8;

        // line fields
        float x2 = 0.0f;
        float y2 = 0.0f;
        float thickness = 2.0f;

        uint32_t color = 0x0078C8;
        std::string name;

        float dirX = 0.0f;
        float dirY = 0.0f;

        static Entity MakeCircle(float x, float y, int r, uint32_t col, bool filled = true, std::string name = "")
        {
            Entity e;
            e.filled = filled;
            e.type = Type::Circle;
            e.x = x; e.y = y;
            e.radius = r;
            e.color = col;
            e.name = name;
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

        static Entity MakeRect(float left, float bottom, float right, float top, uint32_t col, bool filled = true, float thick = 2.0f, char displayedLetter = ' ')
        {
            Entity e;
            e.filled = filled;
            e.type = Type::Rectangle;
            e.x = left; e.y = bottom;
            e.x2 = right; e.y2 = top;
            e.thickness = thick;
            e.color = col;
			e.name = (displayedLetter != ' ') ? std::string(1, displayedLetter) : "";
            return e;
        }
    };

    //struct Overlay
    //{
    //    std::string overlayName;
    //    std::vector<std::string> texts;
    //    Vec2 position;
    //};

    //std::vector<Overlay> overlays;

    //void AddOverlay(Overlay o)
    //{
    //    for (auto& ol : overlays)
    //    {
    //        if (ol.overlayName == o.overlayName)
    //        {
    //            ol = o;
    //            return;
    //        }
    //    }
    //    overlays.push_back(o);
    //}



    struct DrawNode
    {
        float left;
        float right;
        float top;
        float bottom;

        uint32_t color;

        //DrawNode(float left, float right, float top, float bottom, uint32_t col) : left(left), right(right), top(top), bottom(bottom), color(col) {}
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

    // Provide entities to draw (preferred)
    void SetEntities(const std::vector<Entity>& entities);

    // Overlay text (top-right). Thread-safe: call from game thread.
    void SetOverlayLines(const std::vector<std::string>& lines);
    void ClearOverlayLines();

    void OnPaint(void* hdc);

    bool IsRunning() const;

    void SetKeyState(unsigned int vk, bool down); // called from window thread
    bool IsKeyDown(unsigned int vk) const;       // called from game thread

    // Called from window thread when mouse is clicked (screen/client coords)
    void OnRMouseClickScreen(int sx, int sy);

    void OnLMouseClickScreen(int sx, int sy);

    // Called from game thread to fetch the last click (returns true and clears if present)
    bool FetchLClick(Vec2& out);

    bool FetchRClick(Vec2& out);

private:
    // non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void ThreadMain();
    void RegisterClassAndCreateWindow();
    void DestroyWindowInternal();

    int width_;
    int height_;
    std::thread thread_;

    // shared state
    std::atomic<bool> running_;
    std::mutex mtx_;
    std::vector<Entity> entities_;         // store entities (positions + color + name)

    // overlay text
    std::mutex overlayMtx_;
    std::vector<std::string> overlayLines_;

    // input state
    mutable std::mutex inputMtx_;
    std::array<char, 256> keyState_; // virtual-key states (0/1)

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

    // synchronization to let Start() wait until the window is created
    std::condition_variable cv_;
    std::mutex cv_mtx_;

    // Win32 handles stored in the thread owning them
    void* hwnd_; // HWND, but keep as void* to avoid windows.h in header

	bool shouldDrawNames_ = false;
};