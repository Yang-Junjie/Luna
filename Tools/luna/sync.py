#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class PluginManifest:
    plugin_id: str
    name: str
    version: str
    sdk: str
    kind: str
    cmake_target: str
    entry: str
    hosts: list[str]
    dependencies: list[str]
    manifest_path: Path
    source_dir: Path


@dataclass(frozen=True)
class BundleManifest:
    bundle_id: str
    name: str
    version: str
    sdk: str
    host: str
    render_graph_provider: str | None
    enabled_plugins: list[str]
    manifest_path: Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def read_toml(path: Path) -> dict:
    try:
        with path.open("rb") as handle:
            return tomllib.load(handle)
    except FileNotFoundError as exc:
        raise RuntimeError(f"Manifest not found: {path.as_posix()}") from exc
    except tomllib.TOMLDecodeError as exc:
        raise RuntimeError(f"Failed to parse TOML file {path.as_posix()}: {exc}") from exc


def require_string(table: dict, key: str, path: Path) -> str:
    value = table.get(key)
    if not isinstance(value, str) or not value:
        fail(f"Expected non-empty string field '{key}' in {path.as_posix()}")
    return value


def read_optional_string(table: dict, key: str, path: Path) -> str | None:
    value = table.get(key)
    if value is None:
        return None
    if not isinstance(value, str) or not value:
        fail(f"Expected non-empty string field '{key}' in {path.as_posix()}")
    return value


def read_string_array(table: dict, key: str, path: Path, *, required: bool = False) -> list[str]:
    value = table.get(key)
    if value is None:
        if required:
            fail(f"Expected array field '{key}' in {path.as_posix()}")
        return []

    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        fail(f"Expected string array field '{key}' in {path.as_posix()}")

    return list(value)


def read_dependency_keys(table: dict, path: Path) -> list[str]:
    raw = table.get("dependencies")
    if raw is None:
        return []
    if not isinstance(raw, dict):
        fail(f"Expected table field 'dependencies' in {path.as_posix()}")

    dependency_ids: list[str] = []
    for key, value in raw.items():
        if not isinstance(key, str) or not key:
            fail(f"Expected string dependency id in {path.as_posix()}")
        if not isinstance(value, str):
            fail(f"Expected dependency version string for '{key}' in {path.as_posix()}")
        dependency_ids.append(key)

    dependency_ids.sort()
    return dependency_ids


def parse_plugin_manifest(path: Path) -> PluginManifest:
    data = read_toml(path)
    return PluginManifest(
        plugin_id=require_string(data, "id", path),
        name=require_string(data, "name", path),
        version=require_string(data, "version", path),
        sdk=require_string(data, "sdk", path),
        kind=require_string(data, "kind", path),
        cmake_target=require_string(data, "cmake_target", path),
        entry=require_string(data, "entry", path),
        hosts=read_string_array(data, "hosts", path),
        dependencies=read_dependency_keys(data, path),
        manifest_path=path,
        source_dir=path.parent,
    )


def parse_bundle_manifest(path: Path) -> BundleManifest:
    data = read_toml(path)
    plugins_table = data.get("plugins")
    if not isinstance(plugins_table, dict):
        fail(f"Expected table field 'plugins' in {path.as_posix()}")

    return BundleManifest(
        bundle_id=require_string(data, "id", path),
        name=require_string(data, "name", path),
        version=require_string(data, "version", path),
        sdk=require_string(data, "sdk", path),
        host=require_string(data, "host", path),
        render_graph_provider=read_optional_string(data, "render_graph_provider", path),
        enabled_plugins=read_string_array(plugins_table, "enabled", path, required=True),
        manifest_path=path,
    )


def scan_plugin_manifests(project_root: Path) -> dict[str, PluginManifest]:
    manifests: dict[str, PluginManifest] = {}
    for root in (project_root / "Plugins" / "builtin", project_root / "Plugins" / "external"):
        if not root.exists():
            continue

        for manifest_path in root.rglob("luna.plugin.toml"):
            manifest = parse_plugin_manifest(manifest_path)
            if manifest.plugin_id in manifests:
                fail(f"Duplicate plugin id '{manifest.plugin_id}' at {manifest_path.as_posix()}")
            manifests[manifest.plugin_id] = manifest

    return manifests


def resolve_plugins(bundle: BundleManifest, manifests: dict[str, PluginManifest]) -> list[PluginManifest]:
    resolved: list[PluginManifest] = []
    permanent: set[str] = set()
    temporary: set[str] = set()

    def visit(plugin_id: str) -> None:
        if plugin_id in permanent:
            return
        if plugin_id in temporary:
            fail(f"Cycle detected while resolving plugin dependencies at '{plugin_id}'")

        manifest = manifests.get(plugin_id)
        if manifest is None:
            fail(f"Bundle references unknown plugin '{plugin_id}'")

        if manifest.hosts and bundle.host not in manifest.hosts:
            fail(f"Plugin '{plugin_id}' does not support host '{bundle.host}'")

        temporary.add(plugin_id)
        for dependency in manifest.dependencies:
            visit(dependency)
        temporary.remove(plugin_id)
        permanent.add(plugin_id)
        resolved.append(manifest)

    for plugin_id in bundle.enabled_plugins:
        visit(plugin_id)

    return resolved


def rel_to_project(project_root: Path, path: Path) -> str:
    try:
        return path.relative_to(project_root).as_posix()
    except ValueError:
        return path.as_posix()


def sanitize_component(value: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in value)


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def generate_plugin_list(project_root: Path, plugins: list[PluginManifest]) -> str:
    lines = [
        "# This file is generated by `python Tools/luna/sync.py`. Do not edit manually.",
        'set(LUNA_PLUGIN_TARGETS "")',
    ]
    for plugin in plugins:
        lines.append(
            f"if(NOT TARGET {plugin.cmake_target})"
        )
        lines.append(
            f'    add_subdirectory("${{PROJECT_SOURCE_DIR}}/{rel_to_project(project_root, plugin.source_dir)}" '
            f'"${{PROJECT_BINARY_DIR}}/plugins/{sanitize_component(plugin.plugin_id)}")'
        )
        lines.append("endif()")
        lines.append(f"list(APPEND LUNA_PLUGIN_TARGETS {plugin.cmake_target})")

    return "\n".join(lines) + "\n"


def generate_resolved_header() -> str:
    return """#pragma once

namespace luna {

class PluginRegistry;

void registerResolvedPlugins(PluginRegistry& registry);
const char* getResolvedRenderGraphProviderId();

} // namespace luna
"""


def generate_resolved_cpp(bundle: BundleManifest, plugins: list[PluginManifest]) -> str:
    lines = [
        '#include "Plugin/PluginBootstrap.h"',
        '#include "Plugin/PluginRegistry.h"',
        "",
    ]
    for plugin in plugins:
        lines.append(f'extern "C" void {plugin.entry}(luna::PluginRegistry& registry);')

    lines.extend(
        [
            "",
            "namespace luna {",
            "",
            "void registerResolvedPlugins(PluginRegistry& registry)",
            "{",
        ]
    )
    for plugin in plugins:
        lines.append(f"    {plugin.entry}(registry);")
    lines.extend(["}", "", "const char* getResolvedRenderGraphProviderId()", "{"])
    if bundle.render_graph_provider is None:
        lines.append("    return nullptr;")
    else:
        lines.append(f'    return "{bundle.render_graph_provider}";')
    lines.extend(["}", "", "} // namespace luna", ""])
    return "\n".join(lines)


def generate_lock(project_root: Path, bundle: BundleManifest, plugins: list[PluginManifest]) -> str:
    lines = [
        "# This file is generated by `python Tools/luna/sync.py`.",
        f'bundle = "{rel_to_project(project_root, bundle.manifest_path)}"',
        f'host = "{bundle.host}"',
        f'sdk = "{bundle.sdk}"',
    ]

    if bundle.render_graph_provider is not None:
        lines.append(f'render_graph_provider = "{bundle.render_graph_provider}"')

    for plugin in plugins:
        lines.extend(
            [
                "",
                "[[plugin]]",
                f'id = "{plugin.plugin_id}"',
                f'version = "{plugin.version}"',
                f'kind = "{plugin.kind}"',
                f'cmake_target = "{plugin.cmake_target}"',
                f'entry = "{plugin.entry}"',
                f'source = "{rel_to_project(project_root, plugin.source_dir)}"',
            ]
        )

    return "\n".join(lines) + "\n"


def run_sync(project_root: Path, bundle_path: Path, generated_dir: Path, lock_file: Path) -> None:
    bundle = parse_bundle_manifest(bundle_path)
    manifests = scan_plugin_manifests(project_root)
    plugins = resolve_plugins(bundle, manifests)

    write_text(generated_dir / "PluginList.cmake", generate_plugin_list(project_root, plugins))
    write_text(generated_dir / "ResolvedPlugins.h", generate_resolved_header())
    write_text(generated_dir / "ResolvedPlugins.cpp", generate_resolved_cpp(bundle, plugins))
    write_text(lock_file, generate_lock(project_root, bundle, plugins))

    print(
        f"Resolved {len(plugins)} plugin(s) from bundle {rel_to_project(project_root, bundle_path)} "
        f"into {rel_to_project(project_root, generated_dir)}"
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="sync.py", description="Generate Luna plugin build files from bundle manifests.")
    parser.add_argument("--project-root", default=".", help="Project root directory. Defaults to current working directory.")
    parser.add_argument(
        "--bundle",
        default="Bundles/EditorDefault/luna.bundle.toml",
        help="Bundle manifest path relative to the project root unless absolute.",
    )
    parser.add_argument(
        "--generated-dir",
        default="Plugins/Generated",
        help="Generated output directory relative to the project root unless absolute.",
    )
    parser.add_argument(
        "--lock-file",
        default="luna.lock",
        help="Lock file path relative to the project root unless absolute.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    try:
        args = parse_args(argv)
        project_root = Path(args.project_root).resolve()
        bundle_path = Path(args.bundle)
        if not bundle_path.is_absolute():
            bundle_path = (project_root / bundle_path).resolve()

        generated_dir = Path(args.generated_dir)
        if not generated_dir.is_absolute():
            generated_dir = (project_root / generated_dir).resolve()

        lock_file = Path(args.lock_file)
        if not lock_file.is_absolute():
            lock_file = (project_root / lock_file).resolve()

        run_sync(project_root, bundle_path, generated_dir, lock_file)
        return 0
    except Exception as exc:  # noqa: BLE001
        print(f"luna sync: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
