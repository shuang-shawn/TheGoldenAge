# PROJECT_CONTEXT.md

**TheGoldenAge** — shared knowledge base for human developers and AI agents working on this Unreal Engine project.

**Last verified against codebase:** 2026-11-08  
**Document maintainer:** Any agent or developer who makes meaningful project changes (see [Instructions for AI Agents](#instructions-for-ai-agents)).

---

## Instructions for AI Agents

This file is the **authoritative shared reference** for project state. It is not a substitute for reading source and assets. **Update this document after completing your work**, not only when starting a task.

### Before making any changes

1. Read `PROJECT_CONTEXT.md` in its entirety before starting development work.
2. Use it to understand architecture, conventions, existing implementations, and technical decisions.
3. Inspect relevant source files, configs, and content to confirm documented information is still accurate.
4. Consider how proposed changes affect other systems, content, or ongoing work.
5. Follow established project conventions unless there is a clear technical reason to deviate (document the deviation and rationale in this file).

### While making changes

1. Avoid unnecessarily modifying existing architecture or established implementations.
2. Reuse existing systems, classes, functions, and utilities when appropriate.
3. Maintain compatibility with existing systems and team members' work.
4. Do not introduce new dependencies, plugins, or architectural patterns without a valid reason (record the decision if you do).
5. If a change affects another system, verify integration (compile, smoke test in editor when possible).
6. Do not overwrite or remove another developer's work without understanding its purpose.

### After making changes

1. Update `PROJECT_CONTEXT.md` to reflect **meaningful** changes.
2. Update architecture, technical implementation, and development status sections as needed.
3. Add an entry to the [Development Change Log](#h-development-change-log).
4. Document new classes, systems, dependencies, interfaces, and important implementation details.
5. Record known limitations, unresolved issues, or follow-up tasks.
6. Ensure the document reflects the **actual** codebase, not intended or assumed state.

### Shared document rules

- Treat this as a **team knowledge base**, not personal session memory.
- Do not rewrite the entire document for small updates; edit targeted sections.
- **Preserve** historical change log entries and [Important Technical Decisions](#g-important-technical-decisions).
- Avoid duplicating information across sections; cross-link or reference instead.
- Distinguish clearly: **implemented** | **in progress** | **planned** | **proposed** | **deprecated**.
- Never mark a feature **completed** unless verified in the repo or editor.
- If the document conflicts with the codebase, **fix the document** after investigation (or document the conflict explicitly).
- Do not store secrets (tokens, API keys, passwords) in this file.
- Prefer **project-relative paths** (e.g. `Content/_Main/`, `Source/TheGoldenAge/`).
- If detailed subsystem docs outgrow this file, add `Docs/<Topic>.md` and link from here; keep this file as the entry point.

---

## A. Project Overview

| Item | Status |
|------|--------|
| **Project name** | TheGoldenAge |
| **Game concept & genre** | **Unknown / unverified.** No design doc, README, or gameplay code. Title alone does not imply a verified theme. |
| **Core gameplay experience** | **Not implemented.** Default map is an empty template-style level (lighting, floor, `PlayerStart`). |
| **Target platforms** | **Desktop** per `Config/DefaultEngine.ini` (`HardwareTargetingSettings`). Renderer configs also reference mobile/Linux/Mac shader paths (template defaults). **Shipping targets not formally defined.** |
| **Unreal Engine version** | **5.8** (`EngineAssociation` in `TheGoldenAge.uproject`). Editor log indicates **5.8.3** install at `K:/UE_5.8` (environment-specific; not required for all developers). |
| **Development stage** | **Early prototype / post-template setup** — C++ module with no game classes, minimal content, single git commit, no gameplay systems. |
| **Primary languages** | C++ (game module shell), C# (UnrealBuildTool targets/rules), Blueprints (**none in repo on disk**), ini configuration. |

### Main technologies and dependencies

| Technology | Role | Verified |
|------------|------|----------|
| Unreal Engine 5.8 | Runtime & editor | Yes |
| **TheGoldenAge** game module | Primary runtime module | Yes |
| **Enhanced Input** | Default input component classes in config; module dependency in `TheGoldenAge.Build.cs` | Yes — **no Input Actions / Mapping Context assets in repo** |
| Lumen, Nanite, ray tracing, virtual shadow maps | Project renderer defaults | Yes (`DefaultEngine.ini`) |
| **Model Context Protocol (engine plugins)** | Editor tooling: `MCPClientToolset`, `ModelContextProtocol`, `EditorToolset`, `Terminal` | Yes (`TheGoldenAge.uproject`) |
| **ModelingToolsEditorMode** | Editor-only modeling | Yes |
| **unreal-mcp** (external HTTP server) | IDE MCP integration (`http://127.0.0.1:8000/mcp`) | Config in local untracked files (`.cursor/mcp.json`, etc.) — **not part of game runtime** |
| OnlineSubsystem / Steam | Commented in `Build.cs`; not enabled in `.uproject` | Not in use |
| CommonUI | Console variables in `DefaultGame.ini` | **Plugin not listed in `.uproject`** — likely template residue; **intent unverified** |
| Git | Version control | Yes — one commit on `main`, **no remote configured** |

---

## B. Architecture Overview

### High-level architecture (current)

The project has **no custom gameplay architecture yet**. Structure matches a **blank C++ game template** renamed from `TP_Blank`:

```
TheGoldenAge.uproject
    └── Runtime module: TheGoldenAge (FDefaultGameModuleImpl only)
            └── No AActor / APawn / AGameMode / UGameInstance subclasses in Source/
Content/_Main/
    └── Lvl_Main (game & editor default map)
    └── Lvl_M (small map; purpose unverified)
Config/
    └── Engine, game, input defaults
```

### Major gameplay systems

| System | Responsibility | Status |
|--------|----------------|--------|
| Game framework (GameMode, Pawn, PlayerController, etc.) | Core play loop | **Not implemented** (engine defaults only) |
| Input | Player controls | **Enhanced Input configured**; no project assets |
| UI | Menus/HUD | **Not implemented** |
| AI / NPCs | — | **Not implemented** |
| Audio gameplay | — | **Not implemented** |
| Save/load | — | **Not implemented** |
| Multiplayer / replication | — | **Not configured** |
| World partition / streaming | Commandlet class referenced in config | **No evidence of partitioned main level content** |

### C++ classes and Blueprints

**C++ (verified in `Source/`):**

- Module: `TheGoldenAge` — header is empty aside from includes; cpp uses `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ...)`.
- Build: `TheGoldenAge.Build.cs`, `TheGoldenAge.Target.cs`, `TheGoldenAgeEditor.Target.cs`.

**No `UCLASS` / `AActor` hierarchy exists yet.**

**Blueprints:**

- **No `.uasset` files** under `Content/` in the repository. `Lvl_Main.umap` contains engine actors only (lighting, `PlayerStart`, template floor, sky sphere), per static analysis of the map asset.

### Communication between components

**N/A** for custom systems. At runtime, Unreal's default `GameMode` / `DefaultPawn` (if any engine default applies without custom content) would apply — **not verified in project config** (`GlobalDefaultGameMode` not set in tracked `Config/`).

### Multiplayer architecture

- **Not applicable / not configured.**
- `OnlineSubsystem` dependency is commented out in `TheGoldenAge.Build.cs`.
- No `DefaultNetwork.ini` or custom net driver settings in tracked config.
- PIE in editor logs indicates normal single-player PIE, not a dedicated multiplayer setup.

### Backend / external integrations

- **None in game code.**
- Local **unreal-mcp** server for editor automation via MCP (developer machine setup; optional).
- Android File Server plugin settings exist in `DefaultEngine.ini` (editor/deploy aid); treat credentials in ini as **environment secrets** — do not copy tokens into this document.

---

## C. Project Structure

### Root layout (important paths)

| Path | Purpose |
|------|---------|
| `TheGoldenAge.uproject` | Project definition, modules, enabled plugins |
| `Source/` | C++ game module and UBT target files |
| `Config/` | Default engine, game, input, editor settings |
| `Content/` | Game assets (maps only in repo) |
| `Plugins/` | **Absent** — no project-local plugins |
| `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` | Generated / local (largely gitignored) |
| `TheGoldenAge.sln` / `.slnx`, `Automation_TheGoldenAge.sln(x)` | Generated IDE solutions |
| `.idea/` | JetBrains Rider metadata (partially tracked) |

### Content organization

| Path | Notes |
|------|--------|
| `Content/_Main/Lvl_Main.umap` | **Game default map** and **editor startup map** |
| `Content/_Main/Lvl_M.umap` | Very small map; references `Lvl_Main` in asset strings — **role unverified** (sublevel, test, or WIP) |
| `Content/Collections/` | Present; no game assets verified on disk |
| `Content/Developers/` | Per-developer editor folders (standard UE) |

### Source layout

```
Source/
  TheGoldenAge.Target.cs
  TheGoldenAgeEditor.Target.cs
  TheGoldenAge/
    TheGoldenAge.Build.cs
    TheGoldenAge.h
    TheGoldenAge.cpp
```

### Files to read before significant changes

1. `TheGoldenAge.uproject` — modules and plugins  
2. `Source/TheGoldenAge/TheGoldenAge.Build.cs` — module dependencies  
3. `Config/DefaultEngine.ini` — maps, rendering, redirects from `TP_Blank`  
4. `Config/DefaultInput.ini` — Enhanced Input defaults  
5. `Config/DefaultGame.ini` — project ID, any game-specific CVars  
6. `PROJECT_CONTEXT.md` (this file)

### Project rename lineage

`Config/DefaultEngine.ini` contains active game name redirects:

- `TP_Blank` → `/Script/TheGoldenAge`
- `/Script/TP_Blank` → `/Script/TheGoldenAge`

Indicates origin from the **TP_Blank** template, not necessarily the full Third Person gameplay template.

---

## D. Technical Implementation

### Game module (`TheGoldenAge`)

**Implementation:** Standard primary game module with no custom startup logic.

- **Build settings:** `BuildSettingsVersion.V7`, `IncludeOrderVersion.Unreal5_8` (in target `.cs` files).
- **Public dependencies:** `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`.
- **Private dependencies:** none.
- **Pattern for future work:** Add gameplay classes under `Source/TheGoldenAge/`, update `Build.cs` when adding modules (e.g. Slate, OnlineSubsystem), regenerate project files.

### Maps and default play

| Setting | Value |
|---------|--------|
| `GameDefaultMap` | `/Game/_Main/Lvl_Main` |
| `EditorStartupMap` | `/Game/_Main/Lvl_Main` |

**Lvl_Main (verified at high level):** Template lighting stack, `PlayerStart`, floor mesh, sky — suitable as a greybox empty stage.

**Lvl_M:** **Unverified** — inspect in editor before relying on it in builds or streaming.

### Input stack

- `DefaultPlayerInputClass` → `EnhancedPlayerInput`
- `DefaultInputComponentClass` → `EnhancedInputComponent`
- **No** `ActionMappings` / `AxisMappings` (legacy) or references to `InputAction` / `InputMappingContext` assets in config.

**Expected pattern when implementing controls:** Create Enhanced Input assets under a consistent content folder (e.g. `Content/_Main/Input/` — **convention not yet established**), bind in PlayerController or Pawn C++/Blueprint.

### Rendering (project defaults)

Notable settings in `DefaultEngine.ini` (affect performance and look globally):

- Dynamic GI: Lumen (`r.DynamicGlobalIlluminationMethod=1`)
- Reflections: Lumen (`r.ReflectionMethod=1`)
- Nanite enabled for project
- Ray tracing and path tracing enabled at project level
- Static lighting disabled (`r.AllowStaticLighting=False`)
- Windows default RHI: DX12, shader model 6

**Constraint:** These are **aggressive defaults** for an empty project; shipping targets may need tuning later.

### Editor / AI tooling plugins

Enabled in `.uproject` for editor and MCP workflows:

- `ModelingToolsEditorMode` (Editor-only allow list)
- `MCPClientToolset`, `ModelContextProtocol`, `Terminal`, `EditorToolset`

These support **editor automation**, not packaged game features, unless explicitly used in content pipelines.

### Data flow

**No custom data flow.** Future systems should document:

- Authoritative state (server vs client) if multiplayer is added  
- Save game objects / subsystems  
- Asset manager rules if using streamed content  

---

## E. Development Conventions and Rules

### Established (observed in repo)

| Area | Convention |
|------|------------|
| **Project / module name** | PascalCase: `TheGoldenAge` |
| **Content root for maps** | `Content/_Main/` with prefix `Lvl_` for levels (`Lvl_Main`, `Lvl_M`) |
| **C++ module** | Single primary module matching project name |
| **Copyright header** | Epic template style in `Source/` files |
| **Build.cs comments** | Epic template pattern for optional Slate / OnlineSubsystem |
| **Git** | Standard UE `.gitignore` (binaries, intermediate, saved, etc.) |

### Not yet established (recommendations — not adopted until team agrees)

Record here when the team **adopts** them; until then they are suggestions only:

- **C++ vs Blueprint:** e.g. core types and performance-critical logic in C++; designer-facing behavior in Blueprint — **no team rule yet**.
- **Content folder taxonomy:** e.g. `Content/_Main/Characters`, `.../UI` — **not present**.
- **Input asset naming:** `IA_*`, `IMC_*` — **not present**.
- **Coding standard:** Follow Epic UE C++ conventions unless `.editorconfig` / clang-format is added.

### Plugin and dependency management

- Add engine plugins via `TheGoldenAge.uproject` `Plugins` array with explicit `Enabled` and `TargetAllowList` when editor-only.
- Add module dependencies in `TheGoldenAge.Build.cs`; avoid enabling Online/Steam until multiplayer design exists.

### Multiplayer considerations (future)

None active. If added: enable appropriate `OnlineSubsystem`, define server travel defaults, replication on game state classes, and update this section before implementation.

### Build and testing

| Step | Notes |
|------|--------|
| **Generate project files** | Right-click `.uproject` or use UE batch files |
| **Build** | `TheGoldenAgeEditor` Win64 target (primary dev platform per artifacts) |
| **Run** | Editor PIE on `Lvl_Main` |
| **Automated tests** | **None** in repo |
| **CI** | **None** configured |

**IDEs observed:** Visual Studio (`.vsconfig`), JetBrains Rider (`.idea/`, Rider project files under `Intermediate/ProjectFiles/.Rider/`).

---

## F. Current Development Status

### Completed (verified)

- [x] UE 5.8 C++ project created and renamed to TheGoldenAge (`TP_Blank` redirects)
- [x] Primary game module compiles (`TheGoldenAge`)
- [x] Default map `Lvl_Main` set for game and editor
- [x] Enhanced Input set as default input classes
- [x] High-end rendering defaults configured (Lumen/Nanite/RT)
- [x] MCP-related engine plugins enabled for editor tooling
- [x] Initial git commit (`3fc9962`)

### In progress

- **None documented in codebase** (no TODO/FIXME in `Source/`).

### Planned

- **Unknown** — no roadmap in repo.

### Known issues / technical debt

| Item | Severity | Notes |
|------|----------|--------|
| No gameplay framework classes | Expected for stage | Blocks all game features |
| No content beyond two maps | Medium | No blueprints, meshes, or input assets in VCS |
| CommonUI settings without plugin enable | Low | May confuse; verify before UI work |
| Aggressive RT/Lumen defaults | Low | May hurt perf on low-end hardware during prototyping |
| `DefaultEditor.ini` empty | Low | Harmless |
| MCP config dirs untracked | Info | `.cursor/`, `.vscode/`, etc. — per-machine tooling |
| No git remote | Info | Collaboration/backup not configured |
| `Lvl_M` purpose unclear | Low | Confirm in editor before use |

### Placeholder / incomplete

- Entire gameplay layer is **placeholder** (engine defaults only).

### Outstanding decisions

| Decision | Notes |
|----------|--------|
| Game genre, camera, core loop | **Undecided** (no design doc) |
| C++ vs Blueprint split | **Undecided** |
| Multiplayer scope | **Undecided** (no net config) |
| Meaning and role of `Lvl_M` | **Undecided** |
| Whether to adopt CommonUI | **Undecided** |
| Target shipping platforms beyond Desktop | **Undecided** |

---

## G. Important Technical Decisions

| Date | Decision | Reasoning | Alternatives | Affected systems |
|------|----------|-----------|--------------|------------------|
| Unknown (template) | Use **C++ primary game module** `TheGoldenAge` | Standard blank C++ template | Blueprint-only project | Build, all future C++ code |
| Unknown (template) | **Enhanced Input** as default | UE modern input path; reflected in Build.cs + DefaultInput.ini | Legacy input mappings | Player input |
| Unknown (config) | **Lvl_Main** as default game/editor map | Central main level under `Content/_Main/` | Other maps | Maps, PIE, packaging |
| Unknown (config) | **Lumen + Nanite + RT** project renderer settings | UE 5.8 template-style defaults | Scalable/mobile-first renderer profile | Performance, art pipeline |
| 2026-11-08 | Maintain **`PROJECT_CONTEXT.md`** as shared agent/developer knowledge base | Multi-agent continuity and onboarding | Wiki-only or no doc | Process / all systems |
| Recent (dev env) | Enable **MCP / Editor toolset** plugins in `.uproject` | AI-assisted editor workflows | Manual editor-only workflow | Editor, not packaged game |

**Reconsideration policy:** If reversing a decision (e.g. disabling Lumen for shipping), add a new row here with rationale; do not delete historical rows.

---

## H. Development Change Log

Entries are **newest first**. Preserve all history.

### 2026-11-08 — Create shared project context document

| Field | Detail |
|-------|--------|
| **Summary** | Added `PROJECT_CONTEXT.md` at repository root after full codebase inspection. |
| **Files / systems** | `PROJECT_CONTEXT.md` (new); references `Source/`, `Config/`, `Content/_Main/`, `TheGoldenAge.uproject`. |
| **Reason** | Enable consistent multi-agent and team development with a single source of truth. |
| **Implementation notes** | Document reflects verified empty-template state: no gameplay C++, two maps, UE 5.8, Enhanced Input defaults, MCP editor plugins. Unknowns explicitly marked. |
| **Dependencies / architecture** | None code-related; process dependency on agents updating this file after each meaningful change. |
| **Follow-up** | When gameplay begins: add GameMode/Pawn, content folders, and update Sections B, D, F; resolve `Lvl_M` and CommonUI; add git remote if collaborating. |

---

## Appendix: Environment setup (optional)

| Item | Notes |
|------|--------|
| **Engine** | Install UE **5.8** and associate with `TheGoldenAge.uproject` |
| **unreal-mcp** | Optional; run local server on port 8000 if using Cursor/VS MCP configs (files may live in untracked `.cursor/`, `.vscode/`) |
| **Android File Server** | If used, tokens live in `Config/DefaultEngine.ini` — **rotate/regenerate if leaked**; do not commit secrets to shared docs |

---

## Quick reference

| Question | Answer |
|----------|--------|
| Default map? | `Content/_Main/Lvl_Main` |
| Game module? | `Source/TheGoldenAge/` |
| Any custom C++ gameplay classes? | **No** |
| Multiplayer? | **Not configured** |
| Where to log agent work? | [Development Change Log](#h-development-change-log) |

---

*End of PROJECT_CONTEXT.md*
