# Luna Authoring Protocol

`luna.authoring` is the shared scene-authoring protocol used by CLI, future MCP tools, and editor bridges. The current protocol version is `1`.

## Execution Model

Commands are executed left-to-right against `AuthoringSession` through `AuthoringExecutor`.

Errors are terminal:

- Parse error: `ok=false`, no commands execute.
- Execution error: `ok=false`, execution stops at the failing command.
- Verification failure: `ok=false`, execution stops at the failing verification.
- Inspect commands do not mutate the scene.
- `summary` is text-only and has no JSON-side effect.

## CLI Commands

Authoring commands:

- `new`
- `open <scene-path>`
- `save <scene-path>`
- `entity <alias> <name>`
- `camera <alias>`
- `directional-light <alias>`
- `point-light <alias>`
- `spot-light <alias>`
- `primitive <alias> <Cube|Sphere|Plane|Cylinder|Cone>`
- `parent <child-ref> <parent-ref>`
- `unparent <child-ref>`
- `name <entity-ref> <name>`
- `transform <entity-ref> tx ty tz rxDeg ryDeg rzDeg sx sy sz`
- `light-intensity <entity-ref> <value>`
- `light-color <entity-ref> r g b`
- `camera-perspective <entity-ref> fovDeg near far`
- `camera-orthographic <entity-ref> size near far`

Query commands:

- `inspect scene`
- `inspect entity <entity-ref>`
- `inspect hierarchy`

Verification commands:

- `verify saved`
- `verify entity <entity-ref>`
- `verify component <entity-ref> <component>`
- `verify entity-count-at-least <count>`

Entity refs are aliases created earlier in the same execution or numeric UUIDs.

## Plan JSON

Plan files are JSON objects:

```json
{
  "protocol": { "name": "luna.authoring", "version": 1 },
  "project": "optional/path/to/project.lunaproj",
  "commands": [
    { "op": "new" },
    { "op": "primitive", "alias": "Box", "mesh": "Cube" },
    { "op": "inspect", "target": "entity", "entity": "Box" },
    { "op": "verify", "check": "hasComponent", "entity": "Box", "component": "Mesh" }
  ]
}
```

The `protocol` field is optional for v1 compatibility. If present, `name` must be `luna.authoring` and `version` must be `1`.

## Report JSON

`LunaCLI --json ...` emits one report object:

```json
{
  "protocol": { "name": "luna.authoring", "version": 1 },
  "ok": true,
  "scene": {
    "name": "SceneName",
    "path": "SceneName.lunascene",
    "entityCount": 3,
    "dirty": false
  },
  "entities": [],
  "savedScenes": [],
  "inspections": [],
  "verifications": [],
  "diagnostics": [],
  "errors": []
}
```

Exit code is `0` when `ok=true`, otherwise `1`.

`errors` is retained as the legacy human-readable error list. New automation, AI, and editor integrations should prefer `diagnostics`.

Diagnostics are structured objects:

```json
{
  "severity": "error",
  "phase": "execute",
  "code": "UnknownEntity",
  "message": "Unknown entity reference 'Box'.",
  "commandIndex": 2,
  "command": "transform",
  "field": null,
  "entityRef": "Box",
  "component": null,
  "path": null
}
```

Stable diagnostic codes currently include:

- `InvalidPlan`
- `ProtocolMismatch`
- `UnsupportedCommand`
- `UnsupportedVerifyCheck`
- `MissingArgument`
- `InvalidArgument`
- `InvalidNumber`
- `NoBoundScene`
- `UnknownEntity`
- `UnknownBuiltinAsset`
- `MissingComponent`
- `OpenSceneFailed`
- `SaveSceneFailed`
- `ProjectLoadFailed`
- `ExecutionFailed`
- `VerificationFailed`

## Versioning

Protocol version `1` is additive by default. New commands, optional fields, inspections, and verification kinds may be added without changing the version. Renaming fields, changing command semantics, or changing error/exit behavior requires a version bump.
