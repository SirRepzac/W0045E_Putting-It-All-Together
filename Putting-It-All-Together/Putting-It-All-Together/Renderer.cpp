#include "Renderer.h"
#include <cmath>
#include <algorithm>

#include "GameLoop.h"
#include "AIBrain.h"

static SDL_Color ToSDLColor(uint32_t c)
{
	return {
		(Uint8)((c >> 16) & 0xFF),
		(Uint8)((c >> 8) & 0xFF),
		(Uint8)(c & 0xFF),
		255
	};
}

Renderer::Renderer(int width, int height)
	: width_(width), height_(height), running_(false)
{
}

Renderer::~Renderer()
{
	Stop();
}

void Renderer::Start()
{
	if (running_) return;
	running_ = true;
	thread_ = std::thread(&Renderer::ThreadMain, this);
}

void Renderer::Stop()
{
	running_ = false;
	if (thread_.joinable())
		thread_.join();
}

bool Renderer::IsRunning() const
{
	return running_;
}

void Renderer::SetEntities(const std::vector<Entity>& ents)
{
	std::lock_guard<std::mutex> lk(entityMtx_);
	entities_ = ents;
}

void Renderer::SetOverlayLines(Overlay& overlay, const std::vector<std::string>& lines)
{
	std::lock_guard<std::mutex> lk(overlay.overlayMtx_);
	overlay.overlayLines_ = lines;
}


void Renderer::ClearOverlayLines(Overlay& overlay)
{
	std::lock_guard<std::mutex> lk(overlay.overlayMtx_);
	overlay.overlayLines_.clear();
}

void Renderer::ThreadMain()
{
	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_WindowFlags flags = SDL_WINDOW_MOUSE_CAPTURE;


	if (!SDL_CreateWindowAndRenderer("Putting it all together", width_, height_, flags, &window, &renderer))
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
		return;
	}

	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	window_ = window;
	renderer_ = renderer;

	font_ = TTF_OpenFont("font.ttf", 24);

	if (!font_)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create font: %s", SDL_GetError());

	GameLoop& game = GameLoop::Instance();

	while (running_)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
				game.MouseClickAction();
			if (e.type == SDL_EVENT_KEY_DOWN)
				game.KeyPressed();
			if (e.type == SDL_EVENT_QUIT)
				running_ = false;
		}
		if (needsUpdate)
		{
			RenderFrame(window, renderer);
			needsUpdate = false;
		}
		SDL_Delay(1);
	}

	TTF_CloseFont(font_);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();
}

bool Renderer::IsKeyDown(unsigned int vk) const
{
	int arraySize = 0;
	const bool* keyBoardState = SDL_GetKeyboardState(&arraySize);

	return keyBoardState[vk];
}

bool Renderer::IfMouseClickScreen(MouseClick click, int& x, int& y)
{
	float sx;
	float sy;

	Uint32 flags = SDL_GetMouseState(&sx, &sy);

	if (!(flags & SDL_BUTTON_LEFT) && click == MouseClick::Left)
		return false;
	if (!(flags & SDL_BUTTON_RIGHT) && click == MouseClick::Right)
		return false;

	// Convert client px -> world coords using same scale as OnPaint
	int clientW;
	int clientH;

	SDL_GetWindowSize(window_, &clientW, &clientH);

	const float scaleX = (float)width_ / clientW;
	const float scaleY = (float)height_ / clientH;
	const float scale = std::min(scaleX, scaleY);

	// screen -> world
	float wx = sx / scale;
	float wy = sy / scale;

	// clamp to world bounds (safer than wrapping here)
	if (wx < 0.0f) wx = 0.0f;
	if (wx >= width_) wx = width_ - 1.0f;
	if (wy < 0.0f) wy = 0.0f;
	if (wy >= height_) wy = height_ - 1.0f;

	x = wx;
	y = wy;

	return true;
}

bool Renderer::FetchLClick(Vec2& out)
{
    std::lock_guard<std::mutex> lk(clickLMtx_);
    if (!hasLClick_) return false;
    out.x = clickLWorldX_;
    out.y = clickLWorldY_;
    hasLClick_ = false;
    return true;
}

bool Renderer::FetchRClick(Vec2& out)
{
    std::lock_guard<std::mutex> lk(clickRMtx_);
    if (!hasRClick_) return false;
    out.x = clickRWorldX_;
    out.y = clickRWorldY_;
    hasRClick_ = false;
    return true;
}

void Renderer::RenderFrame(SDL_Window* window, SDL_Renderer* renderer)
{
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);

	const float scaleX = (float)width_ / WORLD_WIDTH;
	const float scaleY = (float)height_ / WORLD_HEIGHT;
	const float scale = std::min(scaleX, scaleY);

	// Draw nodes
	for (const auto& node : nodeCache)
	{
		SDL_Color c = ToSDLColor(node.color);
		SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);

		SDL_FRect r;
		r.x = node.left * scale;
		r.y = node.bottom * scale;
		r.w = (node.right - node.left) * scale;
		r.h = (node.top - node.bottom) * scale;

		SDL_RenderFillRect(renderer, &r);
	}

	std::vector<Entity> ents;
	{
		std::lock_guard<std::mutex> lk(entityMtx_);
		ents = entities_;
	}

	for (const Entity& e : ents)
	{
		SDL_Color c = ToSDLColor(e.color);
		SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);

		if (e.type == Entity::Type::Line)
		{
			SDL_RenderLine(
				renderer,
				e.x * scale,
				e.y * scale,
				e.x2 * scale,
				e.y2 * scale
			);
		}
		else if (e.type == Entity::Type::Rectangle)
		{
			SDL_FRect r;
			r.x = e.x * scale;
			r.y = e.y * scale;
			r.w = (e.x2 - e.x) * scale;
			r.h = (e.y2 - e.y) * scale;

			if (e.filled)
				SDL_RenderFillRect(renderer, &r);
			else
				SDL_RenderRect(renderer, &r);
		}
		else if (e.type == Entity::Type::Circle)
		{
			float cx = e.x * scale;
			float cy = e.y * scale;
			float r = e.radius * scale;

			for (int w = -r; w <= r; w++)
			{
				for (int h = -r; h <= r; h++)
				{
					if (w * w + h * h <= r * r)
						SDL_RenderPoint(renderer, cx + w, cy + h);
				}
			}

			{
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);

				int x1 = e.x;
				int y1 = e.y;
				int x2 = x1 + sinf(e.dirX) * e.radius * 1.5f * scale;
				int y2 = y1 + sinf(e.dirY) * e.radius * 1.5f * scale;

				SDL_RenderLine(
					renderer,
					x1 * scale,
					y1 * scale,
					x2 * scale,
					y2 * scale
				);
			}
		}
		for (Overlay* overlayPtr : overlays)
		{
			if (!font_)
				continue;

			Overlay& overlay = *overlayPtr;

			if (overlay.overlayLines_.empty())
				continue;


			int lineHeight = TTF_GetFontHeight(font_);
			const int padding = 8;
			const int lineSpacing = 2;

			int lineCount = (int)overlay.overlayLines_.size();
			int blockHeight =
				padding * 2 +
				lineCount * lineHeight +
				(lineCount - 1) * lineSpacing;

			int blockWidth = -1;

			for (const std::string& s : overlay.overlayLines_)
			{
				int w;
				int h;
				const char* c = s.c_str();
				if (s.empty()) continue;
				TTF_GetStringSize(font_, c, s.size(), &w, &h);
				if (w > blockWidth)
					blockWidth = w;
			}

			blockWidth += padding * 2;

			int blockX = overlay.position.x - padding;
			int blockY = overlay.position.y - padding;

			int clientW;
			int clientH;
			SDL_GetWindowSize(window, &clientW, &clientH);

			if (blockX < 0)
				blockX = 0;
			else if (blockX + blockWidth > clientW)
				blockX = clientW - blockWidth;

			if (blockY < 0)
				blockY = 0;
			else if (blockY + blockHeight > clientH)
				blockY = clientH - blockHeight;

			// --- Render each line ---
			for (int i = 0; i < overlay.overlayLines_.size(); ++i)
			{
				const std::string& s = overlay.overlayLines_[i];

				if (s.empty())
					continue;

				int w;
				int h;
				int blockWidth = TTF_GetStringSize(font_, s.c_str(), s.size(), &w, &h);


				int textX = blockX + padding;
				int textY = blockY + (int)i * (lineHeight + lineSpacing);

				SDL_FRect rect;
				rect.x = textX;
				rect.y = textY;
				rect.h = h;
				rect.w = w;

				SDL_Color c = ToSDLColor(Black);
				SDL_Surface* text = TTF_RenderText_Solid(font_, s.c_str(), s.length(), c);
				SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, text);

				SDL_RenderTexture(renderer, texture, NULL, &rect);

				SDL_DestroySurface(text);
				SDL_DestroyTexture(texture);
			}
		}
	}

	SDL_RenderPresent(renderer);
}


void Renderer::UpdateDirtyNodes(const AIBrain* brain)
{
	GameLoop& game = GameLoop::Instance();
	Grid& grid = game.GetGrid();

	for (size_t i = 0; i < nodeCache.size(); ++i)
	{
		if (!nodeNeedsUpdate[i]) continue;

		DrawNode& node = nodeCache[i];
		auto indexPair = grid.TwoDIndex(i);
		PathNode& gridNode = grid.GetNodes()[indexPair.first][indexPair.second];

		if (game.USE_FOG_OF_WAR && brain != nullptr)
		{
			node.color = brain->IsDiscovered(i)
				? gridNode.color
				: Fog;
		}
		else
			node.color = gridNode.color;

		nodeNeedsUpdate[i] = false;
	}
}