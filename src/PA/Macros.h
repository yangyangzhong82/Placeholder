#pragma once

#if defined(Placeholder_EXPORTS) || defined(PA_BUILD)
#define PA_API [[maybe_unused]] __declspec(dllexport)
#else
#define PA_API [[maybe_unused]] __declspec(dllimport)
#endif

