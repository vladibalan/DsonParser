// pch.h: This is a precompiled header file.
#ifndef PCH_H
#define PCH_H

// Prevent Windows.h from defining min/max macros that conflict with std::min/max
#define NOMINMAX

// add headers that you want to pre-compile here
#include "framework.h"

// RapidJSON headers
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"

#endif //PCH_H
