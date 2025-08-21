#include "PerConnectionIsolatedPlugin.h"

namespace WPEFramework {
namespace Plugin {

    // PUBLIC_INTERFACE
    PerConnectionIsolatedPlugin::PerConnectionIsolatedPlugin()
        : PluginHost::JSONRPC()
        , _service(nullptr) {
        // Register JSON-RPC method handlers at construction time
        RegisterMethods();
    }

    PerConnectionIsolatedPlugin::~PerConnectionIsolatedPlugin() {
        // Cleanup happens in Deinitialize, registry clears on connection Detach.
    }

    // PUBLIC_INTERFACE
    const string PerConnectionIsolatedPlugin::Initialize(PluginHost::IShell* service) {
        // Purpose: Called once when the plugin is being initialized by the framework.
        // Store the service pointer for later use.
        _service = service;

        // Return empty string on success; non-empty string signals error text in Thunder.
        return string();
    }

    // PUBLIC_INTERFACE
    void PerConnectionIsolatedPlugin::Deinitialize(PluginHost::IShell* /*service*/) {
        // Purpose: Called when the plugin is de-initialized. Cleanup if needed.
        _service = nullptr;

        // Nothing else to do; per-connection contexts are removed on Detach().
    }

    // PUBLIC_INTERFACE
    string PerConnectionIsolatedPlugin::Information() const {
        // Purpose: Provide plugin metadata if needed.
        return string("PerConnectionIsolatedPlugin - Demonstrates per-connection JSON-RPC isolation.");
    }

    void PerConnectionIsolatedPlugin::Activate(const PluginHost::IShell::ACTIVATION_REASON /*reason*/) {
        // Optionally handle activation reason
    }

    void PerConnectionIsolatedPlugin::Deactivate(const PluginHost::IShell::DEACTIVATION_REASON /*reason*/) {
        // Optionally handle deactivation reason
    }

    // Called when a new RPC channel (e.g., WebSocket) attaches.
    void PerConnectionIsolatedPlugin::Attach(PluginHost::Channel& channel) {
        const ConnectionId id = channel.Id();
        // Ensure a context exists for this connection; this is per-connection isolation
        _registry.Ensure(id);
        SYSLOG(Logging::Startup, ("PerConnectionIsolatedPlugin::Attach - Connection %u attached", id));
    }

    // Called when a RPC channel (e.g., WebSocket) detaches or closes.
    void PerConnectionIsolatedPlugin::Detach(PluginHost::Channel& channel) {
        const ConnectionId id = channel.Id();

        // Safe cleanup of per-connection state
        _registry.Erase(id);

        SYSLOG(Logging::Shutdown, ("PerConnectionIsolatedPlugin::Detach - Connection %u detached (state cleaned)", id));
    }

    void PerConnectionIsolatedPlugin::RegisterMethods() {
        // Register a simple echo method that demonstrates responding to the calling client only
        // Using lambda to capture 'this' and dispatch with channel id and jsonrpc id
        Register<LambdaType<Core::JSON::VariantContainer, Core::JSON::VariantContainer>>(
            "echo",
            [this](const Core::JSON::VariantContainer& params,
                   Core::JSON::VariantContainer& result,
                   const Core::JSONRPC::Context& ctx,
                   const Core::JSON::VariantContainer::Variant& id) -> uint32_t {
                const ConnectionId channelId = ctx.ChannelId.Value();
                return this->handler_echo(params, result, channelId, ctx, id);
            });

        // Subscribe/unsubscribe event names per connection
        Register<LambdaType<Core::JSON::VariantContainer, Core::JSON::VariantContainer>>(
            "subscribe",
            [this](const Core::JSON::VariantContainer& params,
                   Core::JSON::VariantContainer& result,
                   const Core::JSONRPC::Context& ctx,
                   const Core::JSON::VariantContainer::Variant& /*id*/) -> uint32_t {
                return this->handler_subscribe(params, result, ctx.ChannelId.Value());
            });

        Register<LambdaType<Core::JSON::VariantContainer, Core::JSON::VariantContainer>>(
            "unsubscribe",
            [this](const Core::JSON::VariantContainer& params,
                   Core::JSON::VariantContainer& result,
                   const Core::JSONRPC::Context& ctx,
                   const Core::JSON::VariantContainer::Variant& /*id*/) -> uint32_t {
                return this->handler_unsubscribe(params, result, ctx.ChannelId.Value());
            });

        // Example: an internal timer or external logic could call EmitEventToSubscribers(...)
        // to publish targeted events. This example keeps it manual from code.
    }

    // Example method that demonstrates targeted Respond to the calling connection.
    // PUBLIC_INTERFACE
    uint32_t PerConnectionIsolatedPlugin::handler_echo(
        const Core::JSON::VariantContainer& params,
        Core::JSON::VariantContainer& /*result*/,
        const ConnectionId channelId,
        const Core::JSONRPC::Context& /*context*/,
        const Core::JSON::VariantContainer::Variant& requestId)
    {
        string message;
        if (!ExtractString(params, "message", message)) {
            // Invalid params
            Core::JSON::VariantContainer error;
            error["code"] = Core::JSON::DecUInt32(22); // ERROR_BAD_REQUEST
            error["message"] = Core::JSON::String("Missing or invalid 'message' parameter");
            // No result body set here; using Respond to return error is typically handled by framework if we return non-zero.
            return Core::ERROR_BAD_REQUEST;
        }

        // Track this request as in-flight for this connection (optional demonstration)
        if (auto* ctx = _registry.Get(channelId)) {
            // We need the request id as uint32 if possible, but JSON-RPC IDs can be string/number; here we keep a best effort
            // For demonstration, we do not convert/insert sophisticatedly; just clear inflight on completion.
            ctx->inflight.insert(0 /*dummy*/);
        }

        // Construct a result object. Note: We will Respond() directly to only the calling connection.
        Core::JSON::VariantContainer resultObj;
        resultObj["echoed"] = Core::JSON::String(message);

        // IMPORTANT: Use the JSONRPC::Respond(channelId, result, id) to route response only to the requesting client.
        // This avoids broadcasting to other connections. The 'id' passed must match the request's id.
        Respond(channelId, resultObj, requestId);

        // Mark the inflight request as complete
        if (auto* ctx = _registry.Get(channelId)) {
            ctx->inflight.erase(0 /*dummy*/);
        }

        // Since we already responded, return success
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    uint32_t PerConnectionIsolatedPlugin::handler_subscribe(
        const Core::JSON::VariantContainer& params,
        Core::JSON::VariantContainer& result,
        const ConnectionId channelId)
    {
        string eventName;
        if (!ExtractString(params, "event", eventName) || eventName.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        auto& ctx = _registry.Ensure(channelId);
        ctx.subscriptions.insert(eventName);

        result["status"] = Core::JSON::String("subscribed");
        result["event"] = Core::JSON::String(eventName);
        return Core::ERROR_NONE;
        // Note: The actual response to this method will be handled by framework if not using custom Respond().
    }

    // PUBLIC_INTERFACE
    uint32_t PerConnectionIsolatedPlugin::handler_unsubscribe(
        const Core::JSON::VariantContainer& params,
        Core::JSON::VariantContainer& result,
        const ConnectionId channelId)
    {
        string eventName;
        if (!ExtractString(params, "event", eventName) || eventName.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        if (auto* ctx = _registry.Get(channelId)) {
            ctx->subscriptions.erase(eventName);
        }

        result["status"] = Core::JSON::String("unsubscribed");
        result["event"] = Core::JSON::String(eventName);
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    void PerConnectionIsolatedPlugin::EmitEventToSubscribers(const string& eventName, const string& payload) {
        // Build the event params payload
        Core::JSON::VariantContainer params;
        params["payload"] = Core::JSON::String(payload);

        // Iterate over connections and send only to those subscribed to eventName
        _registry.ForEach([this, &eventName, &params](const ConnectionId id, const ConnectionContext& ctx) {
            if (ctx.subscriptions.count(eventName) != 0) {
                // Use JSONRPC::Notify(channelId, eventName, params) to send event to only this connection
                Notify(id, eventName, params);
            }
        });
    }

    // PUBLIC_INTERFACE
    void PerConnectionIsolatedPlugin::EmitEventToConnection(const ConnectionId channelId, const string& eventName, const string& payload) {
        Core::JSON::VariantContainer params;
        params["payload"] = Core::JSON::String(payload);
        Notify(channelId, eventName, params);
    }

    // Utility to extract a string param from VariantContainer
    bool PerConnectionIsolatedPlugin::ExtractString(const Core::JSON::VariantContainer& params, const string& name, string& out) {
        auto it = params.Elements().find(name);
        if (it == params.Elements().end()) {
            return false;
        }
        if (it->second.IsString() == true) {
            out = it->second.String();
            return true;
        }
        return false;
    }

} // namespace Plugin
} // namespace WPEFramework
