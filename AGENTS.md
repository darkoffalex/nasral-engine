# NasralEngine — Agent Guide

## Project Overview

NasralEngine is a C++17 game engine built around Vulkan API and an ECS architecture.

The repository is split into two main CMake sub-projects:

- **Sandbox application** — executable project used for running and testing the engine.
- **Engine object library** — core engine implementation.

The root `CMakeLists.txt` wires these sub-projects together.

## Runtime Content

The `content/` directory contains runtime assets and project data:

- **Materials / shaders**
- **Meshes / geometry**
- **Textures / images**
- **Scenes** — JSON descriptions of scenes
- **project.json** — project configuration containing resource registries, material registries, and the startup scene path

## Engine Layout

Engine code is organized by subsystem.

Public headers are located under: `include/nasral/`
Implementation files are located under: `sources/engine/`

Each subsystem usually has its own directory and namespace. The namespace generally matches the subsystem directory name.

Main subsystems:

- `ecs` — Entity Component System
- `evt` — Events
- `gfx` — Rendering
- `inp` — Input: keyboard, mouse
- `log` — Logging
- `res` — Resources: loading and unloading
- `run` — Runtime state management and execution flow
- `scn` — Scene and scene nodes

## Typical Subsystem Structure

Most subsystem directories may contain:

- `manager.h` / `manager.cpp` — subsystem manager, owns high-level subsystem lifetime
- `system.h` / `system.cpp` — ECS system performing subsystem-specific ECS operations
- `components.h` — ECS components
- `types.h` — subsystem-specific types and declarations
- `utils.h` / `utils.cpp` — subsystem-specific utilities
- `objects/` — objects created by the subsystem, often RAII wrappers

## Engine Root

The root engine class is declared in `include/nasral/engine.h` and implemented in `sources/engine/engine.cpp`.

The engine class owns and manages the lifetime of subsystem managers.

Subsystem managers own and manage:

- ECS systems
- subsystem-specific runtime objects
- objects from their `objects/` directories

## Common Types

The `common/` directory contains shared types used across subsystems.

Important examples:

- `Subsystem` base class, inherited by subsystem managers
- other cross-subsystem utility types and declarations

## ECS Usage

The engine is ECS-first.

For performance-critical hot paths, prefer direct ECS access and pure ECS systems.

For convenience-oriented code, some subsystems provide RAII-style wrappers around ECS entities. These wrappers usually:

- create an ECS entity internally
- attach required components
- expose convenient accessors to component data
- manage entity-related lifetime

Examples can be found in areas such as:

- `gfx/objects`
- `scn/objects`

Use these wrappers when API ergonomics are more important than raw iteration performance, such as UI, editor-like interfaces, and high-level scene manipulation.

## Rendering Architecture

Rendering is implemented in the `gfx` subsystem and uses Vulkan.

Material resources are represented by `res/objects/material`. A material resource encapsulates loading and owning a Vulkan graphics pipeline.

Supported material families include, but are not limited to:

- Phong
- PBR
- Textured
- Colored

Material definitions live in `content/`.

During project loading, the engine builds a global material registry. Materials are ECS-backed objects.

There is a distinction between:

- **Material resource** — shared pipeline/material definition loaded from content
- **Material instance** — per-object or per-use material data with its own parameters and textures

To support many material instances efficiently, the renderer uses a large storage/uniform buffer array where each element corresponds to one material instance.

When a material instance is created:

1. It obtains an index from a UBO index pool.
2. That index identifies its own region in the material buffer.
3. The same index is used by shaders to access the instance-specific material data and textures.

This allows multiple material instances to share the same material resource and Vulkan pipeline while keeping independent settings and textures.

## Development Notes for Agents

- Keep subsystem boundaries clear.
- Prefer placing new code into the matching subsystem directory and namespace.
- Use ECS systems for hot update/render loops.
- Use RAII ECS wrappers only when convenience and lifetime encapsulation are more important than maximum performance.
- Follow the existing manager/system/components/types/utils structure when adding subsystem functionality.
- Runtime assets and scene/project descriptions belong in `content/`.
- Avoid coupling subsystems directly when existing managers, events, resources, or ECS components can express the dependency.