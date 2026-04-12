#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import sync


DEFAULT_BUNDLES = {
    "editor": Path("Bundles/EditorDefault/luna.bundle.toml"),
    "runtime": Path("Bundles/RuntimeDefault/luna.bundle.toml"),
}


@dataclass(frozen=True)
class BuildJob:
    name: str
    bundle_path: Path
    build_dir: Path
    generated_dir: Path
    lock_file: Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def sanitize_component(value: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in value)


def resolve_project_path(project_root: Path, value: str) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path.resolve()
    return (project_root / path).resolve()


def default_custom_name(bundle_path: Path) -> str:
    parent_name = bundle_path.parent.name.strip()
    if parent_name:
        return sanitize_component(parent_name)
    return sanitize_component(bundle_path.stem)


def build_jobs(args: argparse.Namespace, project_root: Path, build_root: Path) -> list[BuildJob]:
    if args.profile == "all":
        if args.bundle is not None:
            fail("The --bundle option cannot be used with the 'all' profile")
        if args.name is not None:
            fail("The --name option cannot be used with the 'all' profile")
        profiles = ["editor", "runtime"]
    else:
        profiles = [args.profile]

    jobs: list[BuildJob] = []
    for profile in profiles:
        if profile == "custom":
            if args.bundle is None:
                fail("The 'custom' profile requires --bundle")
            bundle_path = resolve_project_path(project_root, args.bundle)
            profile_name = sanitize_component(args.name or default_custom_name(bundle_path))
        else:
            if args.bundle is not None:
                fail("The --bundle option is only valid with the 'custom' profile")
            if args.name is not None:
                fail("The --name option is only valid with the 'custom' profile")
            bundle_path = resolve_project_path(project_root, str(DEFAULT_BUNDLES[profile]))
            profile_name = profile

        build_dir = (build_root / profile_name).resolve()
        jobs.append(
            BuildJob(
                name=profile_name,
                bundle_path=bundle_path,
                build_dir=build_dir,
                generated_dir=build_dir / "generated",
                lock_file=build_dir / "luna.lock",
            )
        )

    return jobs


def executable_path(job: BuildJob, target: str) -> Path:
    suffix = ".exe" if sys.platform.startswith("win") else ""
    return job.build_dir / "App" / f"{target}{suffix}"


def format_command(command: list[str]) -> str:
    return subprocess.list2cmdline(command)


def run_command(command: list[str], *, dry_run: bool) -> None:
    print(f"$ {format_command(command)}")
    if dry_run:
        return
    subprocess.run(command, check=True)


def run_sync(job: BuildJob, project_root: Path, *, dry_run: bool) -> None:
    print(f"[luna-build] sync {job.name}")
    print(f"  bundle: {sync.rel_to_project(project_root, job.bundle_path)}")
    print(f"  generated: {job.generated_dir}")
    print(f"  lock: {job.lock_file}")
    if dry_run:
        return
    sync.run_sync(project_root, job.bundle_path, job.generated_dir, job.lock_file)


def configure_command(
    project_root: Path,
    job: BuildJob,
    *,
    generator: str | None,
    defines: list[str],
) -> list[str]:
    command = [
        "cmake",
        "-S",
        str(project_root),
        "-B",
        str(job.build_dir),
        f"-DLUNA_GENERATED_DIR={job.generated_dir}",
    ]
    if generator:
        command.extend(["-G", generator])
    for define in defines:
        command.append(f"-D{define}")
    return command


def build_command(job: BuildJob, *, config: str, target: str, parallel: int | None) -> list[str]:
    command = [
        "cmake",
        "--build",
        str(job.build_dir),
        "--config",
        config,
        "--target",
        target,
    ]
    if parallel is not None:
        command.extend(["--parallel", str(parallel)])
    return command


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="build.py",
        description="Resolve a Luna bundle and build a profile-isolated LunaApp output.",
    )
    parser.add_argument(
        "profile",
        nargs="?",
        default="editor",
        choices=["editor", "runtime", "all", "custom"],
        help="Build profile to generate. Use 'custom' together with --bundle.",
    )
    parser.add_argument("--project-root", default=".", help="Project root directory. Defaults to the current working directory.")
    parser.add_argument("--build-root", default="build/profiles", help="Root directory that will contain per-profile build outputs.")
    parser.add_argument("--bundle", help="Custom bundle path. Required when profile is 'custom'.")
    parser.add_argument("--name", help="Build directory name for the 'custom' profile.")
    parser.add_argument("--config", default="Debug", help="Build configuration passed to CMake build.")
    parser.add_argument("--target", default="LunaApp", help="CMake target to build.")
    parser.add_argument("--generator", help="Optional CMake generator for the configure step.")
    parser.add_argument("--parallel", type=int, help="Optional parallel build job count passed to CMake.")
    parser.add_argument(
        "--define",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="Extra -D definitions forwarded to the CMake configure step.",
    )
    parser.add_argument("--sync-only", action="store_true", help="Run bundle resolution only.")
    parser.add_argument("--configure-only", action="store_true", help="Run bundle resolution and CMake configure, then stop.")
    parser.add_argument("--dry-run", action="store_true", help="Print the resolved steps without executing them.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    try:
        args = parse_args(argv)
        if args.sync_only and args.configure_only:
            fail("Choose at most one of --sync-only or --configure-only")

        project_root = Path(args.project_root).resolve()
        build_root = resolve_project_path(project_root, args.build_root)
        jobs = build_jobs(args, project_root, build_root)

        for job in jobs:
            print(f"[luna-build] profile {job.name}")
            print(f"  build dir: {job.build_dir}")
            run_sync(job, project_root, dry_run=args.dry_run)

            if args.sync_only:
                print("")
                continue

            print(f"[luna-build] configure {job.name}")
            run_command(
                configure_command(
                    project_root,
                    job,
                    generator=args.generator,
                    defines=args.define,
                ),
                dry_run=args.dry_run,
            )

            if args.configure_only:
                print("")
                continue

            print(f"[luna-build] build {job.name}")
            run_command(
                build_command(
                    job,
                    config=args.config,
                    target=args.target,
                    parallel=args.parallel,
                ),
                dry_run=args.dry_run,
            )
            print(f"[luna-build] artifact {executable_path(job, args.target)}")
            print("")

        return 0
    except Exception as exc:  # noqa: BLE001
        print(f"luna build: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
