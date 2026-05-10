# Luna TypeScript Editor

This directory is reserved for the TypeScript editor frontend. It will consume the shared editor protocols under `Editor/Protocol` instead of binding directly to the C++ ImGui implementation.

Shared TypeScript authoring code starts in `core/`, editor protocol mirrors live in `protocol/`, the View Protocol bridge clients live in `bridge/`, and the first editor shell state/session lives in `shell/`. `Editor/Ts/index.ts` is the public entry point. The CLI keeps compatibility re-export files under `CLI/client`, but the reusable protocol, host client, controller, turn, planner, chat, provider, eval harness, environment loader, editor view protocol, render data plane, bridge clients, and shell session now live here so the future Electron editor can use the same implementation.
