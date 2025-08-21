# PerConnectionIsolatedPlugin (Thunder 4.4 JSONRPC example)

This plugin skeleton demonstrates:
- Thread-safe per-connection isolation via a registry keyed by ConnectionId
- Tracking of per-connection subscriptions and in-flight requests
- JSON-RPC method handler that replies only to the calling client via `Respond(channelId, result, id)`
- Safe cleanup on connection close (Detach)
- Event emission only to subscribed/targeted connections using `Notify(channelId, event, params)`

Files:
- PerConnectionIsolatedPlugin.h / .cpp: Plugin implementation (inherits from `PluginHost::JSONRPC` as requested).
- CMakeLists.txt: Minimal build config.

Key calls:
- Attach/Detach: Creates and cleans per-connection state.
- echo (method): Uses `Respond(channelId, result, id)` to ensure response goes only to the requester.
- subscribe/unsubscribe (methods): Manage per-connection subscriptions.
- EmitEventToSubscribers / EmitEventToConnection: Demonstrate targeted event emission.

Example JSON-RPC usage:
- Subscribe to example event:
  Request: `{ "jsonrpc":"2.0", "id":1, "method":"PerConnectionIsolatedPlugin.subscribe", "params":{"event":"exampleEvent"} }`
- Emit an event (from within the plugin code): `EmitEventToSubscribers("exampleEvent", "hello")`
- Only subscribed connections will receive: `{"jsonrpc":"2.0","method":"PerConnectionIsolatedPlugin.onexampleEvent","params":{"payload":"hello"}}`
- Echo (responds only to caller):
  Request: `{ "jsonrpc":"2.0","id":2,"method":"PerConnectionIsolatedPlugin.echo","params":{"message":"hi"}}`
  Response (to caller only): `{ "jsonrpc":"2.0","id":2,"result":{"echoed":"hi"} }`

Notes:
- No external dependencies are used beyond Thunder 4.4.
- This is a template; adjust method names, event names, and manifest integration to your environment.
