#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>

static float const MAXIMUM_SPEED = 300.0f;
static float const MAXIMUM_ACCELERATION = 260.0f;

static int const WINDOW_WIDTH = 1920;
static int const WINDOW_HEIGHT = 1080;

static int const WORLD_WIDTH = 1920;
static int const WORLD_HEIGHT = 1080;

static double const PI = 3.14159265358979323846;

static float DegToRad(float deg)
{
	return deg * PI / 180;
}