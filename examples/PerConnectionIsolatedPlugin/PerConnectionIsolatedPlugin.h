#pragma once

// Thunder 4.4 (WPEFramework) JSON-RPC plugin skeleton that demonstrates:
// - Per-connection isolation using a thread-safe registry keyed by ConnectionId
// - Tracking per-connection subscriptions and in-flight requests
// - JSON-RPC method handler that Responds only to the calling client using Respond(channelId, result, id)
// - Safe cleanup on WebSocket connection close
// - Example event emission to only subscribed or targeted connections
//
// This header declares a plugin that inherits from PluginHost::JSONRPC as requested.
// It is designed to be integration-ready for Thunder 4.4 builds, but remains a standalone example.

#include <core/core.h>
#include <tracing/tracing.h>
#include <plugins/Plugin.h>
#include <plugins/JSONRPC.h>

namespace WPEFramework {
namespace Plugin {

    // Helper alias for readability (Thunder uses uint32_t for connection IDs)
    using ConnectionId = uint32_t;

    // A small per-connection context container for example purposes.
    // This tracks:
    //  - subscriptions: the event names this connection subscribed to
    //  - inflight: JSON-RPC request ids initiated by this connection (or being processed)
    struct ConnectionContext {
        Core::hsm::StateType stateDummy; // placeholder to show you can keep state if needed
        std::set<string> subscriptions;
        std::set<uint32_t> inflight;

        ConnectionContext()
            : stateDummy(0) {
        }
    };

    // Thread-safe registry keyed by ConnectionId.
    // This is intentionally simple for example clarity. A read/write lock is used to protect access.
    class ConnectionRegistry {
    public:
        ConnectionRegistry() = default;
        ~ConnectionRegistry() = default;

        // Create or get the context for a given connection id.
        ConnectionContext& Ensure(const ConnectionId id) {
            Core::SafeSyncType<Core::CriticalSection> guard(_adminLock);
            return _contexts[id]; // operator[] inserts if not found
        }

        // Returns a pointer to the context or nullptr if not found.
        ConnectionContext* Get(const ConnectionId id) {
            Core::SafeSyncType<Core::CriticalSection> guard(_adminLock);
            auto it = _contexts.find(id);
            return (it != _contexts.end()) ? &it->second : nullptr;
        }

        // Remove context for a given id (cleanup on close).
        void Erase(const ConnectionId id) {
            Core::SafeSyncType<Core::CriticalSection> guard(_adminLock);
            _contexts.erase(id);
        }

        // Enumerate over connections with shared lock behavior.
        template <typename F>
        void ForEach(F&& f) {
            Core::SafeSyncType<Core::CriticalSection> guard(_adminLock);
            for (auto& kv : _contexts) {
                f(kv.first, kv.second);
            }
        }

    private:
        Core::CriticalSection _adminLock;
        std::map<ConnectionId, ConnectionContext> _contexts;
    };

    // The actual JSON-RPC plugin. It inherits from PluginHost::JSONRPC (as requested).
    class PerConnectionIsolatedPlugin : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        PerConnectionIsolatedPlugin(const PerConnectionIsolatedPlugin&) = delete;
        PerConnectionIsolatedPlugin& operator=(const PerConnectionIsolatedPlugin&) = delete;

        PerConnectionIsolatedPlugin();
        ~PerConnectionIsolatedPlugin() override;

        // IPlugin methods
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

        // JSONRPC connection handlers
        // Called when a new channel is opened (e.g. WebSocket connection established)
        void Activate(const PluginHost::IShell::ACTIVATION_REASON reason) override;
        void Deactivate(const PluginHost::IShell::DEACTIVATION_REASON reason) override;

        // These JSONRPC overrides inform us about channel lifecycle
        void Attach(PluginHost::Channel& channel) override;
        void Detach(PluginHost::Channel& channel) override;

    private:
        // PRIVATE HELPERS

        // Register JSON-RPC method handlers
        void RegisterMethods();

        // A simple method handler for "echo" that demonstrates responding only to the calling client.
        // Request: { "jsonrpc": "2.0", "id": 1, "method": "echo", "params": { "message": "..." } }
        // Response is sent using Respond(channelId, result, id)
        uint32_t handler_echo(const Core::JSON::VariantContainer& params,
                              Core::JSON::VariantContainer& result,
                              const ConnectionId channelId,
                              const Core::JSONRPC::Context& context,
                              const Core::JSON::VariantContainer::Variant& requestId);

        // Methods to manage subscriptions
        uint32_t handler_subscribe(const Core::JSON::VariantContainer& params,
                                   Core::JSON::VariantContainer& result,
                                   const ConnectionId channelId);
        uint32_t handler_unsubscribe(const Core::JSON::VariantContainer& params,
                                     Core::JSON::VariantContainer& result,
                                     const ConnectionId channelId);

        // Example: emit an event to only subscribed or targeted connections
        void EmitEventToSubscribers(const string& eventName, const string& payload);
        void EmitEventToConnection(const ConnectionId channelId, const string& eventName, const string& payload);

        // Utility to extract a field from params (string)
        static bool ExtractString(const Core::JSON::VariantContainer& params, const string& name, string& out);

        // Track plugin state
        PluginHost::IShell* _service;

        // Registry maintaining per-connection isolation
        ConnectionRegistry _registry;
    };

} // namespace Plugin
} // namespace WPEFramework
