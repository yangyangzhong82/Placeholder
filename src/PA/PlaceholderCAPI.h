// src/PA/PlaceholderCAPI.h
#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(Placeholder_EXPORTS) || defined(PA_BUILD)
#define PA_C_API __declspec(dllexport)
#else
#define PA_C_API __declspec(dllimport)
#endif
#else
#define PA_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PA_C_ABI_VERSION 1u

typedef struct PA_COwner PA_COwner;
typedef struct PA_CContext PA_CContext;
typedef struct PA_CStringWriter PA_CStringWriter;

typedef struct PA_CEvaluationContext {
    size_t         size;
    uint64_t       context_type_id;
    uint64_t       placeholder_context_type_id;
    const void*    cpp_context;
    void*          raw_object;
} PA_CEvaluationContext;

typedef void (*PA_CPlaceholderEvaluateFn)(
    const PA_CEvaluationContext* context,
    const char* const*           args,
    size_t                       arg_count,
    PA_CStringWriter*            out,
    void*                        user_data
);

typedef void (*PA_CPlaceholderDestroyFn)(void* user_data);

typedef struct PA_CPlaceholderOptions {
    size_t                       size;
    PA_COwner*                   owner;
    const char*                  prefix;
    const char*                  token;
    uint64_t                     context_type_id;
    unsigned int                 cache_duration;
    PA_CPlaceholderEvaluateFn    evaluate;
    PA_CPlaceholderDestroyFn     destroy;
    void*                        user_data;
} PA_CPlaceholderOptions;

PA_C_API uint32_t PA_CApiVersion(void);

PA_C_API uint64_t PA_ContextTypeServer(void);
PA_C_API uint64_t PA_ContextTypeActor(void);
PA_C_API uint64_t PA_ContextTypeMob(void);
PA_C_API uint64_t PA_ContextTypePlayer(void);
PA_C_API uint64_t PA_ContextTypeBlock(void);
PA_C_API uint64_t PA_ContextTypeItemStackBase(void);
PA_C_API uint64_t PA_ContextTypeContainer(void);
PA_C_API uint64_t PA_ContextTypeBlockActor(void);
PA_C_API uint64_t PA_ContextTypeWorldCoordinate(void);

PA_C_API PA_COwner* PA_CreateOwner(const char* name);
PA_C_API void PA_UnregisterOwner(PA_COwner* owner);
PA_C_API void PA_DestroyOwner(PA_COwner* owner);

PA_C_API int PA_RegisterPlaceholder(const PA_CPlaceholderOptions* options);

PA_C_API PA_CContext* PA_CreateActorContext(void* actor);
PA_C_API PA_CContext* PA_CreateMobContext(void* mob);
PA_C_API PA_CContext* PA_CreatePlayerContext(void* player);
PA_C_API PA_CContext* PA_CreateBlockContext(const void* block);
PA_C_API PA_CContext* PA_CreateItemStackBaseContext(const void* item_stack_base);
PA_C_API PA_CContext* PA_CreateContainerContext(void* container);
PA_C_API PA_CContext* PA_CreateBlockActorContext(void* block_actor);
PA_C_API PA_CContext* PA_CreateWorldCoordinateContext(float x, float y, float z, int dimension_id);
PA_C_API void PA_DestroyContext(PA_CContext* context);

PA_C_API char* PA_Replace(const char* text, const PA_CContext* context);
PA_C_API char* PA_ReplaceServer(const char* text);
PA_C_API void PA_FreeCString(char* text);

PA_C_API int PA_CStringWriterAppend(PA_CStringWriter* writer, const char* text);
PA_C_API int PA_CStringWriterAppendN(PA_CStringWriter* writer, const char* text, size_t size);
PA_C_API void PA_CStringWriterClear(PA_CStringWriter* writer);

#ifdef __cplusplus
}
#endif
