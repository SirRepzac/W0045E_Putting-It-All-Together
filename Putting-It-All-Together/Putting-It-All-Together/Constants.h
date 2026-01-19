#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>

static float const MAXIMUM_SPEED = 1; // m/s
static float const MAXIMUM_ACCELERATION = 1; // m/s^2

static float const CELL_SIZE = 10; // SIZE x SIZE meters

static int const WINDOW_WIDTH = 1920;
static int const WINDOW_HEIGHT = 1080;

static int const WORLD_WIDTH = 1920;
static int const WORLD_HEIGHT = 1080;

static double const PI = 3.14159265358979323846;

static float DegToRad(float deg)
{
	return deg * PI / 180;
}