#pragma once

#ifdef Placeholder_EXPORTS
#define PA_API [[maybe_unused]] __declspec(dllexport)
#else
#define PA_API [[maybe_unused]] __declspec(dllimport)
#endif

