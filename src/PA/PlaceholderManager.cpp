// PlaceholderManager.cpp
#define PA_BUILD
#include "PA/PlaceholderAPI.h"
#include "PA/PlaceholderProcessor.h"
#include "PA/PlaceholderRegistry.h"


#include <memory>

namespace PA {

class PlaceholderManager final : public IPlaceholderService {
public:
    void registerPlaceholder(std::string_view prefix, std::shared_ptr<const IPlaceholder> p, void* owner) override {
        mRegistry.registerPlaceholder(prefix, p, owner);
    }

    void registerCachedPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        void*                               owner,
        unsigned int                        cacheDuration
    ) override {
        mRegistry.registerCachedPlaceholder(prefix, p, owner, cacheDuration);
    }

    void registerRelationalPlaceholder(
        std::string_view                    prefix,
        std::shared_ptr<const IPlaceholder> p,
        void*                               owner,
        uint64_t                            mainContextTypeId,
        uint64_t                            relationalContextTypeId
    ) override {
        mRegistry.registerRelationalPlaceholder(prefix, p, owner, mainContextTypeId, relationalContextTypeId);
    }

    void unregisterByOwner(void* owner) override { mRegistry.unregisterByOwner(owner); }

    std::string replace(std::string_view text, const IContext* ctx) const override {
        return PlaceholderProcessor::process(text, ctx, mRegistry);
    }

    std::string replaceServer(std::string_view text) const override {
        return PlaceholderProcessor::processServer(text, mRegistry);
    }

    void registerContextAlias(
        std::string_view  alias,
        uint64_t          fromContextTypeId,
        uint64_t          toContextTypeId,
        ContextResolverFn resolver,
        void*             owner
    ) override {
        mRegistry.registerContextAlias(alias, fromContextTypeId, toContextTypeId, resolver, owner);
    }

private:
    PlaceholderRegistry mRegistry;
};

static PlaceholderManager gManager;

extern "C" PA_API IPlaceholderService* PA_GetPlaceholderService() { return &gManager; }

} // namespace PA