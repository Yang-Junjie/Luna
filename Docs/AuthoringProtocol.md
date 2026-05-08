# Luna Authoring Protocol

`luna.authoring` is the shared scene-authoring protocol used by CLI, future MCP tools, and editor bridges. The current protocol version is `1`.

## Execution Model

Commands are executed left-to-right against `AuthoringSession` through `AuthoringExecutor`.

`AuthoringExecutor` validates each plan before execution. Validation does not mutate the scene or write files; it checks protocol compatibility, command references, builtin asset names, component requirements, verification conditions that can be known ahead of time, and filesystem boundaries for `open` and `save`.

`AuthoringExecutor` runs each valid plan inside one authoring transaction. A successful plan commits as one undo step. A failed plan rolls back scene mutations before returning diagnostics.

Command effects are explicit in the protocol layer:

- Scene mutation commands include `new`, entity creation, parenting, renaming, transforms, lights, cameras, `open`, and `save`.
- Filesystem read commands currently include `open`.
- Filesystem write commands currently include `save`.
- Read-only commands include `inspect`, `verify`, and `summary`.

Filesystem writes must be registered with the executor's side-effect boundary before the write happens. Today `save` snapshots the target `.lunascene` file first: a failed plan removes a newly-created scene file or restores the original contents of an overwritten file. Future file-writing authoring commands, such as asset import, project save, or script generation, should declare `WritesFileSystem` and add a matching rollback boundary before implementation.

Dry-run mode uses the same validation path and exits before execution:

```powershell
build\CLI\LunaCLI.exe --dry-run --json new primitive Box Cube save Generated/BoxScene
```

Errors are terminal:

- Parse error: `ok=false`, no commands execute.
- Validation error: `ok=false`, no commands execute.
- Execution error: `ok=false`, execution stops at the failing command.
- Verification failure: `ok=false`, execution stops at the failing verification.
- Inspect commands do not mutate the scene.
- `summary` is text-only and has no JSON-side effect.

## History And Transactions

Editor actions, AI plans, and future MCP tools share the same `AuthoringSession` history layer.

- `beginTransaction(name)` captures the scene state before a batch.
- `commitTransaction()` captures the scene state after the batch and pushes one undo step.
- `rollbackTransaction()` restores the pre-transaction scene state.
- `undo()` and `redo()` restore committed transaction snapshots.

The current implementation uses scene-level snapshots for correctness and broad coverage. High-frequency editor interactions, such as viewport gizmo transforms, should use an explicit transaction so a drag gesture becomes one undo step.

## CLI Commands

Global options:

- `--json`
- `--dry-run`
- `--project <path>`

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

Diagnostics may also carry machine-readable hint fields:

- `recoverable`
- `expected`
- `actual`
- `suggestedCommand`

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
  "path": null,
  "recoverable": true,
  "expected": "resolvable entity reference",
  "actual": "Box",
  "suggestedCommand": null
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
- `FileOverwrite`
- `ProjectLoadFailed`
- `ExecutionFailed`
- `VerificationFailed`

## Versioning

Protocol version `1` is additive by default. New commands, optional fields, inspections, and verification kinds may be added without changing the version. Renaming fields, changing command semantics, or changing error/exit behavior requires a version bump.
