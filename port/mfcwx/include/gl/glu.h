#pragma once
#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif
