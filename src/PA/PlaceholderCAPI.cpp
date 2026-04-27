// src/PA/PlaceholderCAPI.cpp
#include "PA/PlaceholderCAPI.h"

#include "PA/PlaceholderAPI.h"
#include "PA/logger.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

struct PA_COwner {
    std::string name;
};

struct PA_CContext {
    std::unique_ptr<PA::IContext> context;
};

struct PA_CStringWriter {
    std::string* output;
};

namespace {

char* duplicateString(std::string_view value) {
    auto* result = static_cast<char*>(std::malloc(value.size() + 1));
    if (!result) {
        return nullptr;
    }
    if (!value.empty()) {
        std::memcpy(result, value.data(), value.size());
    }
    result[value.size()] = '\0';
    return result;
}

template <typename Context, typename Raw>
void* extractRawAs(const PA::IContext* ctx, Raw Context::* member) {
    if (auto* typed = dynamic_cast<const Context*>(ctx)) {
        return const_cast<void*>(static_cast<const void*>(typed->*member));
    }
    return nullptr;
}

void* extractRawObject(const PA::IContext* ctx, uint64_t requestedTypeId) {
    if (!ctx) {
        return nullptr;
    }

    if (requestedTypeId == PA::PlayerContext::kTypeId) {
        return extractRawAs(ctx, &PA::PlayerContext::player);
    }
    if (requestedTypeId == PA::MobContext::kTypeId) {
        return extractRawAs(ctx, &PA::MobContext::mob);
    }
    if (requestedTypeId == PA::ActorContext::kTypeId) {
        return extractRawAs(ctx, &PA::ActorContext::actor);
    }
    if (requestedTypeId == PA::BlockContext::kTypeId) {
        return extractRawAs(ctx, &PA::BlockContext::block);
    }
    if (requestedTypeId == PA::ItemStackBaseContext::kTypeId) {
        return extractRawAs(ctx, &PA::ItemStackBaseContext::itemStackBase);
    }
    if (requestedTypeId == PA::ContainerContext::kTypeId) {
        return extractRawAs(ctx, &PA::ContainerContext::container);
    }
    if (requestedTypeId == PA::BlockActorContext::kTypeId) {
        return extractRawAs(ctx, &PA::BlockActorContext::blockActor);
    }
    if (requestedTypeId == PA::WorldCoordinateContext::kTypeId) {
        if (auto* typed = dynamic_cast<const PA::WorldCoordinateContext*>(ctx)) {
            return typed->data.get();
        }
    }
    return nullptr;
}

class CAbiPlaceholder final : public PA::IPlaceholder {
public:
    CAbiPlaceholder(const PA_CPlaceholderOptions& options)
    : mTokenNoBraces(normalizeToken(options.token)),
      mTokenBraced("{" + mTokenNoBraces + "}"),
      mContextTypeId(options.context_type_id),
      mCacheDuration(options.cache_duration),
      mEvaluate(options.evaluate),
      mDestroy(options.destroy),
      mUserData(options.user_data) {}

    ~CAbiPlaceholder() override {
        if (mDestroy) {
            mDestroy(mUserData);
        }
    }

    std::string_view token() const noexcept override { return mTokenBraced; }
    uint64_t         contextTypeId() const noexcept override { return mContextTypeId; }
    unsigned int     getCacheDuration() const noexcept override { return mCacheDuration; }

    void evaluate(const PA::IContext* ctx, std::string& out) const override { evaluateWithArgs(ctx, {}, out); }

    void evaluateWithArgs(
        const PA::IContext*                ctx,
        const std::vector<std::string_view>& args,
        std::string&                       out
    ) const override {
        if (!mEvaluate) {
            return;
        }

        std::vector<std::string> argStorage;
        std::vector<const char*> argv;
        argStorage.reserve(args.size());
        argv.reserve(args.size());
        for (auto arg : args) {
            argStorage.emplace_back(arg);
            argv.push_back(argStorage.back().c_str());
        }

        PA_CEvaluationContext cContext{
            sizeof(PA_CEvaluationContext),
            ctx ? ctx->typeId() : PA::kServerContextId,
            mContextTypeId,
            ctx,
            extractRawObject(ctx, mContextTypeId),
        };
        PA_CStringWriter writer{&out};

        mEvaluate(&cContext, argv.data(), argv.size(), &writer, mUserData);
    }

private:
    static std::string normalizeToken(const char* token) {
        if (!token) {
            return {};
        }

        std::string_view view(token);
        if (view.size() >= 2 && view.front() == '{' && view.back() == '}') {
            view = view.substr(1, view.size() - 2);
        }
        return std::string(view);
    }

    std::string                  mTokenNoBraces;
    std::string                  mTokenBraced;
    uint64_t                     mContextTypeId{};
    unsigned int                 mCacheDuration{};
    PA_CPlaceholderEvaluateFn    mEvaluate{};
    PA_CPlaceholderDestroyFn     mDestroy{};
    void*                        mUserData{};
};

PA::IContext* getContext(const PA_CContext* handle) {
    return handle && handle->context ? handle->context.get() : nullptr;
}

} // namespace

extern "C" {

PA_C_API uint32_t PA_CApiVersion(void) { return PA_C_ABI_VERSION; }

PA_C_API uint64_t PA_ContextTypeServer(void) { return PA::kServerContextId; }
PA_C_API uint64_t PA_ContextTypeActor(void) { return PA::ActorContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypeMob(void) { return PA::MobContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypePlayer(void) { return PA::PlayerContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypeBlock(void) { return PA::BlockContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypeItemStackBase(void) { return PA::ItemStackBaseContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypeContainer(void) { return PA::ContainerContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypeBlockActor(void) { return PA::BlockActorContext::kTypeId; }
PA_C_API uint64_t PA_ContextTypeWorldCoordinate(void) { return PA::WorldCoordinateContext::kTypeId; }

PA_C_API PA_COwner* PA_CreateOwner(const char* name) {
    auto* owner = new (std::nothrow) PA_COwner;
    if (!owner) {
        return nullptr;
    }
    if (name) {
        owner->name = name;
    }
    return owner;
}

PA_C_API void PA_UnregisterOwner(PA_COwner* owner) {
    if (!owner) {
        return;
    }
    if (auto* service = PA::PA_GetPlaceholderService()) {
        service->unregisterByOwner(owner);
    }
}

PA_C_API void PA_DestroyOwner(PA_COwner* owner) {
    if (!owner) {
        return;
    }
    PA_UnregisterOwner(owner);
    delete owner;
}

PA_C_API int PA_RegisterPlaceholder(const PA_CPlaceholderOptions* options) {
    if (!options || options->size < sizeof(PA_CPlaceholderOptions) || !options->owner || !options->token
        || !options->evaluate) {
        return 0;
    }
    std::string_view tokenView(options->token);
    if (tokenView.empty() || tokenView == "{}") {
        return 0;
    }

    auto* service = PA::PA_GetPlaceholderService();
    if (!service) {
        return 0;
    }

    auto placeholder = std::make_shared<CAbiPlaceholder>(*options);
    service->registerPlaceholder(options->prefix ? options->prefix : "", std::move(placeholder), options->owner);
    return 1;
}

PA_C_API PA_CContext* PA_CreateActorContext(void* actor) {
    if (!actor) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{std::make_unique<PA::ActorContext>(PA::ActorContext::from(static_cast<Actor*>(actor)))};
}

PA_C_API PA_CContext* PA_CreateMobContext(void* mob) {
    if (!mob) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{std::make_unique<PA::MobContext>(PA::MobContext::from(static_cast<Mob*>(mob)))};
}

PA_C_API PA_CContext* PA_CreatePlayerContext(void* player) {
    if (!player) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{
        std::make_unique<PA::PlayerContext>(PA::PlayerContext::from(static_cast<Player*>(player)))
    };
}

PA_C_API PA_CContext* PA_CreateBlockContext(const void* block) {
    if (!block) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{
        std::make_unique<PA::BlockContext>(PA::BlockContext::from(static_cast<const Block*>(block)))
    };
}

PA_C_API PA_CContext* PA_CreateItemStackBaseContext(const void* item_stack_base) {
    if (!item_stack_base) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{
        std::make_unique<PA::ItemStackBaseContext>(
            PA::ItemStackBaseContext::from(static_cast<const ItemStackBase*>(item_stack_base))
        )
    };
}

PA_C_API PA_CContext* PA_CreateContainerContext(void* container) {
    if (!container) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{
        std::make_unique<PA::ContainerContext>(PA::ContainerContext::from(static_cast<Container*>(container)))
    };
}

PA_C_API PA_CContext* PA_CreateBlockActorContext(void* block_actor) {
    if (!block_actor) {
        return nullptr;
    }
    return new (std::nothrow) PA_CContext{
        std::make_unique<PA::BlockActorContext>(PA::BlockActorContext::from(static_cast<BlockActor*>(block_actor)))
    };
}

PA_C_API PA_CContext* PA_CreateWorldCoordinateContext(float x, float y, float z, int dimension_id) {
    auto context  = std::make_unique<PA::WorldCoordinateContext>();
    context->data = std::make_shared<PA::WorldCoordinateData>(
        PA::WorldCoordinateData{Vec3{x, y, z}, static_cast<DimensionType>(dimension_id)}
    );
    return new (std::nothrow) PA_CContext{std::move(context)};
}

PA_C_API void PA_DestroyContext(PA_CContext* context) { delete context; }

PA_C_API char* PA_Replace(const char* text, const PA_CContext* context) {
    auto* service = PA::PA_GetPlaceholderService();
    if (!service || !text) {
        return nullptr;
    }
    return duplicateString(service->replace(text, getContext(context)));
}

PA_C_API char* PA_ReplaceServer(const char* text) {
    auto* service = PA::PA_GetPlaceholderService();
    if (!service || !text) {
        return nullptr;
    }
    return duplicateString(service->replaceServer(text));
}

PA_C_API void PA_FreeCString(char* text) { std::free(text); }

PA_C_API int PA_CStringWriterAppend(PA_CStringWriter* writer, const char* text) {
    if (!writer || !writer->output || !text) {
        return 0;
    }
    writer->output->append(text);
    return 1;
}

PA_C_API int PA_CStringWriterAppendN(PA_CStringWriter* writer, const char* text, size_t size) {
    if (!writer || !writer->output || (!text && size != 0)) {
        return 0;
    }
    writer->output->append(text, size);
    return 1;
}

PA_C_API void PA_CStringWriterClear(PA_CStringWriter* writer) {
    if (writer && writer->output) {
        writer->output->clear();
    }
}

} // extern "C"
