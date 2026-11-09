# PROJECT_CONTEXT.md

**TheGoldenAge** — shared knowledge base for human developers and AI agents working on this Unreal Engine project.

**Last verified against codebase:** 2026-11-08 (agents do not compile; the user runs Live Coding manually after each task)  
**Document maintainer:** Any agent or developer who makes meaningful project changes (see [Instructions for AI Agents](#instructions-for-ai-agents)).

---

## Instructions for AI Agents

This file is the **authoritative shared reference** for project state. It is not a substitute for reading source and assets. **Update this document after completing your work**, not only when starting a task.

### Before making any changes

Cursor always applies `.cursor/rules/project-context.mdc`: read this file before each task, and update it after each task.

1. Read `PROJECT_CONTEXT.md` in its entirety before starting development work.
2. Use it to understand architecture, conventions, existing implementations, and technical decisions.
3. Inspect relevant source files, configs, and content to confirm documented information is still accurate.
4. Consider how proposed changes affect other systems, content, or ongoing work.
5. Follow established project conventions unless there is a clear technical reason to deviate (document the deviation and rationale in this file).
6. Prefer extending existing C++ classes over duplicating systems.
7. Use **Enhanced Input** for all player controls (see [Input architecture (design)](#input-architecture-design)).
8. Keep core gameplay logic in C++; expose designer-facing customization to Blueprint where appropriate (see [C++ and Blueprint development philosophy](#c-and-blueprint-development-philosophy)).
9. Follow existing naming conventions and folder organization; consider impact on other gameplay systems before changing architecture.
10. Clearly distinguish **implemented** behaviour from **planned** design in code and in this document.

### While making changes

1. Avoid unnecessarily modifying existing architecture or established implementations.
2. Reuse existing systems, classes, functions, and utilities when appropriate.
3. Maintain compatibility with existing systems and team members' work.
4. Do not introduce new dependencies, plugins, or architectural patterns without a valid reason (record the decision if you do).
5. If a change affects another system, name the integration points for the user to check after they compile and smoke-test. Do not compile for them.
6. Do not overwrite or remove another developer's work without understanding its purpose.
7. **Do not configure the game from runtime startup code** (e.g. `BeginPlay`, `PostInitializeComponents`, constructors that patch content) when the same result can be achieved by editing assets. Example: attaching the weapon and shield to the left/right hand sockets must be set up in the Blueprint/asset, not via code on startup. Use MCP to edit the asset/Blueprint/level directly; **if MCP cannot do it, tell the user the exact manual steps to perform in the editor** instead of falling back to startup code. Runtime code should implement gameplay behaviour, not assemble or repair content configuration.

### After making changes

1. **Do not compile.** Do not trigger Live Coding, hot reload, or a command-line build. The user compiles manually with Live Coding in the open editor after the task. Do not close or restart the editor, and do not start a full rebuild. In the report, tell them to compile. Never claim a successful compile unless they already compiled and shared the result.
2. Update `PROJECT_CONTEXT.md` to reflect **meaningful** changes.
3. Update architecture, technical implementation, and development status sections as needed.
4. Add an entry to the [Development Change Log](#h-development-change-log).
5. Document new classes, systems, dependencies, interfaces, and important implementation details.
6. Record known limitations, unresolved issues, or follow-up tasks.
7. Ensure the document reflects the **actual** codebase, not intended or assumed state.

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
| **Game concept & genre** | **[Design]** **3D top-down action RPG** (*Hades*-style camera; MOBA-inspired targeting **planned**). See [Gameplay design requirements](#gameplay-design-requirements). **Locomotion implemented.** |
| **Core gameplay experience** | **[Design]** Mouse combat targeting, abilities — **planned**. **[Impl]** Top-down follow camera, camera-relative WASD movement, movement-facing on `BP_TinyHeroPolyart`; battle in-place locomotion on `Lvl_Main` via `ATheGoldenAgeGameMode`. |
| **Target platforms** | **Desktop** per `Config/DefaultEngine.ini` (`HardwareTargetingSettings`). Renderer configs also reference mobile/Linux/Mac shader paths (template defaults). **Shipping targets not formally defined.** |
| **Unreal Engine version** | **5.8** (`EngineAssociation` in `TheGoldenAge.uproject`). Editor log indicates **5.8.3** install at `K:/UE_5.8` (environment-specific; not required for all developers). |
| **Development stage** | **Early prototype** — basic player movement and battle locomotion animations implemented; combat/targeting not started. |
| **Primary languages** | C++ (gameplay base classes), Blueprints (`BP_TinyHeroPolyart`, input assets), C# (UBT), ini configuration. |

### Main technologies and dependencies

| Technology | Role | Verified |
|------------|------|----------|
| Unreal Engine 5.8 | Runtime & editor | Yes |
| **TheGoldenAge** game module | Primary runtime module | Yes |
| **Enhanced Input** | `IA_Move` (Axis2D), `IMC_Default`; bindings in C++ | Yes — assets under `Content/_Main/Input/` |
| Lumen, Nanite, ray tracing, virtual shadow maps | Project renderer defaults | Yes (`DefaultEngine.ini`) |
| **Model Context Protocol (engine plugins)** | Editor tooling: `MCPClientToolset`, `ModelContextProtocol`, `EditorToolset`, `Terminal` | Yes (`TheGoldenAge.uproject`) |
| **ModelingToolsEditorMode** | Editor-only modeling | Yes |
| **unreal-mcp** (external HTTP server) | IDE MCP integration (`http://127.0.0.1:8000/mcp`) | Config in local untracked files (`.cursor/mcp.json`, etc.) — **not part of game runtime** |
| OnlineSubsystem / Steam | Commented in `Build.cs`; not enabled in `.uproject` | Not in use |
| CommonUI | Console variables in `DefaultGame.ini` | **Plugin not listed in `.uproject`** — likely template residue; **intent unverified** |
| Git + **Git LFS** | Version control; binary assets via LFS (`git-lfs` 3.3.0 verified) | Remote: `origin` → `https://github.com/shuang-shawn/TheGoldenAge.git` |

---

## Gameplay design requirements

**Legend:** **[Design]** = intended product requirement (may be unimplemented). **[Impl]** = verified in repository or config. When both apply, each is called out separately.

### Perspective and camera

| Requirement | Status |
|-------------|--------|
| Fully 3D world and characters | **[Design]** |
| Top-down camera, *Hades*-style perspective | **[Design]** |
| Camera continuously follows the player character | **[Design]** |
| Player character remains primary camera focus | **[Design]** |
| Smooth camera follow (no abrupt snapping) | **[Design]** |
| Consistent top-down angle and distance unless intentionally changed | **[Design]** |

**[Impl]** `ATheGoldenAgePlayerCharacter`: `USpringArmComponent` + `UCameraComponent`; world-fixed tilt (~−55° pitch, ~45° yaw), position lag; view target set on possess. **Tune** arm length/angles in C++ defaults or BP.

### Player character controls

#### Movement [Design]

- **WASD** drives movement; **8-directional** (cardinal + diagonals).
- Diagonal input must be **normalized** so diagonal speed matches cardinal speed.
- Movement must feel responsive for real-time action combat.

**[Impl]** `ATheGoldenAgePlayerCharacter::Move` — Enhanced Input `IA_Move`, camera-yaw-relative `AddMovementInput`, implicit diagonal normalization. Max walk speed 300 (C++ default). Movement is normal walking (no plane constraint); a Z-up plane constraint left the capsule stuck against `Floor_0` while walk anims still played.

#### Character facing and rotation [Design]

**Default (locomotion):**

- Facing follows **movement direction** while moving.
- Mouse cursor does **not** continuously rotate the character.
- When idle, keep the **last valid** facing direction.
- Rotation should be smooth and responsive.

**Combat and skill casting:**

- Mouse cursor defines **targeting direction** for attacks and abilities.
- On attack, ability, or other directional combat action, character turns toward cursor **world position** on the playable ground plane (screen cursor → ground projection).
- Character may **move independently** of aim direction where mechanics allow.
- Do **not** continuously rotate toward the cursor while idle or moving.

**Examples (design intent):**

1. Move north (W) → face north.  
2. Stop → retain last facing.  
3. Move north while aiming east → face east when the ability starts.  
4. Directional projectile → travels toward cursor target on ground plane.

#### Future combat and skill system [Design]

Architecture should support (without implementing all systems upfront):

- Directional melee, ranged/projectiles, targeted and directional abilities, AOE, mouse-based casting, per-skill targeting rules.
- **Reusable targeting** shared across abilities; **no tight coupling** between basic movement and cursor targeting.

### Input architecture (design)

**Policy:** All player controls use Unreal **Enhanced Input** — `UInputAction` and `UInputMappingContext` assets; bind via Player Controller or Character input setup; **no** direct keyboard/mouse polling in gameplay logic.

| Input | Action | Behaviour |
|-------|--------|-----------|
| WASD | Movement | 8-directional movement |
| Mouse cursor | Targeting position | World-space ground target for combat/abilities |
| Future mouse buttons | Attack / skills | Fire actions using cursor target |

**Separation of responsibilities [Design]:**

1. **Player Controller** — cursor screen position; project to ground plane; expose targeting to gameplay; **no** direct rotation/combat logic.  
2. **Combat / Ability system** — consume targeting; per-skill behaviour; request rotation when needed.  
3. **Player Character** — execute movement, movement-facing, and externally requested combat rotation; **no** cursor polling or skill targeting logic.

Movement input must **not** auto-rotate toward the mouse. Modules (cursor targeting, rotation, movement, skill execution) must evolve independently.

**[Impl]** Assets: `Content/_Main/Input/IA_Move`, `IMC_Default`. WASD mappings (with axis modifiers) are authored on `IMC_Default`. `ATheGoldenAgePlayerController` only adds that context at runtime. `ATheGoldenAgePlayerCharacter` binds `IA_Move`. **No** mouse targeting actions yet.

**Recommended content paths (when implementing):** e.g. `Content/_Main/Input/` with naming `IA_*`, `IMC_*` — adopt consistently when first assets are created.

### C++ and Blueprint development philosophy

**[Design — team direction]**

1. Choose an appropriate Unreal parent class (`ACharacter`, `APlayerController`, `AActor`, `UActorComponent`, `UUserWidget`, `AGameModeBase`, etc.).
2. Implement **core, reusable logic in C++**.
3. Expose properties, functions, and events to Blueprint where designers need them.
4. Create **Blueprint child classes** for tuning, assembly, and level-specific setup — avoid duplicating core logic in Blueprint.

| Layer | Responsibility |
|-------|----------------|
| **C++** | Core gameplay, systems/components, movement, input integration, networking, performance-critical logic |
| **Blueprint** | Configuring exposed properties, child class variants, presentation/event-driven behaviour, animation/UI hooks, designer customization |

Do not add redundant custom C++ when an existing project or engine class already suffices.

### Code quality and engineering practices [Design]

Follow Epic C++ conventions, reflection, and memory rules; modular, maintainable types; clear names; focused classes; minimal Blueprint surface; document non-obvious logic; handle edge cases; consider multiplayer when designing gameplay systems; avoid unnecessary plugins/dependencies. Match scope to current needs without blocking future expansion.

### Content and source folder structure [Design]

- Logical C++ modules/directories under `Source/TheGoldenAge/`.
- Blueprint assets grouped by system (player, enemies, UI, input, etc.).
- Consistent naming across C++, Blueprints, and assets.
- Inspect existing layout before adding folders; **do not** reorganize the whole project without cause.

**[Impl]** Maps under `Content/_Main/`; imported squad content under `Content/RPGHeroSquad/` (Fab-style tree). No `Content/_Main/Characters` or `Input/` yet.

### Intended gameplay architecture (summary)

| System | Intended responsibility | Implementation status |
|--------|-------------------------|------------------------|
| **Player Controller** | Enhanced Input; mapping context; future cursor → ground target | **Partial** — input only |
| **Player Character** | Movement, movement-facing; `RequestFaceDirection` hook for future combat | **Partial** — locomotion only |
| **Camera system** | Top-down follow, smooth tracking | **Impl** — spring arm on character |
| **Input system** | `IA_*` / `IMC_*` assets, mappings | **Impl** — move only |
| **Combat / abilities** | Targeting consumption, per-skill rules | **Planned** |
| **Gameplay components** | Reusable behaviour where appropriate | **Planned** |

---

## B. Architecture Overview

### High-level architecture (current)

The project has a **minimal top-down player stack** in C++ with `BP_TinyHeroPolyart` as the visual/animation Blueprint child. **[Design]** full architecture in [Gameplay design requirements](#gameplay-design-requirements). High-level runtime flow:

```
TheGoldenAge.uproject
    └── Module: TheGoldenAge
            ├── ATheGoldenAgeGameMode → Default pawn: BP_TinyHeroPolyart
            ├── ATheGoldenAgePlayerController → IMC_Default + IA_Move
            └── ATheGoldenAgePlayerCharacter (BP parent) → movement, camera, battle anims
Content/_Main/
    ├── Lvl_Main
    └── Input/ (IA_Move, IMC_Default)
Content/RPGHeroSquad/
    ├── CharacterBP/BP_TinyHeroPolyart (playable; reparented from SkeletalMeshActor)
    └── Animation/TinyHero/… (battle in-place locomotion)
Config/
    └── GlobalDefaultGameMode → TheGoldenAgeGameMode
```

### Major gameplay systems

| System | Responsibility | Status |
|--------|----------------|--------|
| Game framework (GameMode, Pawn, PlayerController, etc.) | Core play loop | **Partial** — `ATheGoldenAgeGameMode`, `BP_TinyHeroPolyart`, `ATheGoldenAgePlayerController`. |
| Camera | Top-down follow camera | **Implemented** — spring arm + lag on `ATheGoldenAgePlayerCharacter`. |
| Player movement & facing | WASD, movement-facing, combat rotation | **Partial** — movement + facing; `RequestFaceDirection` stub for future combat. |
| Input | Enhanced Input actions/mappings; cursor targeting | **Partial** — `IA_Move` / `IMC_Default`; **no** cursor targeting. |
| Combat / abilities | MOBA-style ground targeting, reusable targeting API | **Planned** — architecture only. |
| UI | Menus/HUD | **Not implemented** |
| AI / NPCs | — | **Not implemented** |
| Audio gameplay | — | **Not implemented** |
| Save/load | — | **Not implemented** |
| Multiplayer / replication | — | **Not configured** (design asks to consider net compatibility in new systems). |
| World partition / streaming | Commandlet class referenced in config | **No evidence of partitioned main level content** |

### C++ classes and Blueprints

**C++ (`Source/TheGoldenAge/`):**

| Class | Role |
|-------|------|
| `ATheGoldenAgeGameMode` | Default pawn class `BP_TinyHeroPolyart`; `GetDefaultPawnClassForController` loads BP if needed |
| `ATheGoldenAgePlayerController` | Adds `IMC_Default` (mappings authored on the asset); sets view target |
| `ATheGoldenAgePlayerCharacter` | Camera-relative movement, movement-facing, single-node battle locomotion |

**Blueprints / content:**

- **`BP_TinyHeroPolyart`** — parent `ATheGoldenAgePlayerCharacter`; mesh `SK_TinyHeroPolyart`; sword/shield attached to `CharacterMesh0`; `AutoPossessPlayer` = Player0.
- **`Content/_Main/Input/`** — `IA_Move` (Axis2D), `IMC_Default`.
- **`Lvl_Main`** — greybox play map.
- **`Content/RPGHeroSquad/`** — hero assets (commit via LFS when ready).
- **Must use LFS** when adding: all `.uasset` / `.umap` per `.gitattributes`.

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
| `Content/` | Game assets (maps tracked; `RPGHeroSquad/` pending first LFS commit) |
| `.gitattributes` | Git LFS pointer rules for UE and media binaries |
| `.gitignore` | UE generated dirs + local IDE/MCP exclusions. `.cursor/*` is ignored; `.cursor/rules/` is tracked. |
| `Plugins/` | **Absent** — no project-local plugins |
| `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` | Generated / local (largely gitignored) |
| `TheGoldenAge.sln` / `.slnx`, `Automation_TheGoldenAge.sln(x)` | Generated IDE solutions |
| `.idea/` | JetBrains Rider metadata (partially tracked) |

### Content organization

| Path | Notes |
|------|--------|
| `Content/_Main/Lvl_Main.umap` | **Game default map** and **editor startup map** (only level in `_Main`) |
| `Content/RPGHeroSquad/` | Imported character/animation content (Fab-style tree). **Use LFS on commit.** Verify licenses/attribution before shipping. |
| `Content/Collections/` | Present; no game assets verified on disk |
| `Content/Developers/` | Per-developer editor folders (standard UE) |

### Source layout

```
Source/
  TheGoldenAge.Target.cs
  TheGoldenAgeEditor.Target.cs
  TheGoldenAge/
    TheGoldenAge.Build.cs
    TheGoldenAge.h / TheGoldenAge.cpp
    TheGoldenAgeGameMode.h / .cpp
    TheGoldenAgePlayerController.h / .cpp
    TheGoldenAgePlayerCharacter.h / .cpp
```

### Files to read before significant changes

1. `TheGoldenAge.uproject` — modules and plugins  
2. `Source/TheGoldenAge/TheGoldenAge.Build.cs` — module dependencies  
3. `Config/DefaultEngine.ini` — maps, rendering, redirects from `TP_Blank`  
4. `Config/DefaultInput.ini` — Enhanced Input defaults  
5. `Config/DefaultGame.ini` — project ID, any game-specific CVars  
6. `PROJECT_CONTEXT.md` (this file) — including [Gameplay design requirements](#gameplay-design-requirements) before gameplay work  

### Project rename lineage

`Config/DefaultEngine.ini` contains active game name redirects:

- `TP_Blank` → `/Script/TheGoldenAge`
- `/Script/TP_Blank` → `/Script/TheGoldenAge`

Indicates origin from the **TP_Blank** template, not necessarily the full Third Person gameplay template.

---

## D. Technical Implementation

### Game module (`TheGoldenAge`)

**Implementation:** Primary game module plus gameplay classes listed above.

- **Build settings:** `BuildSettingsVersion.V7`, `IncludeOrderVersion.Unreal5_8` (in target `.cs` files).
- **Public dependencies:** `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`.
- **Private dependencies:** none.
- **Pattern for future work:** Add gameplay classes under `Source/TheGoldenAge/`, update `Build.cs` when adding modules (e.g. Slate, OnlineSubsystem), regenerate project files.

### Maps and default play

| Setting | Value |
|---------|--------|
| `GameDefaultMap` | `/Game/_Main/Lvl_Main` |
| `EditorStartupMap` | `/Game/_Main/Lvl_Main` |
| `GlobalDefaultGameMode` | `/Script/TheGoldenAge.TheGoldenAgeGameMode` |

**Lvl_Main (verified at high level):** Template lighting stack, `PlayerStart`, floor mesh, sky — suitable as a greybox empty stage.

### Input stack

- `DefaultPlayerInputClass` → `EnhancedPlayerInput`
- `DefaultInputComponentClass` → `EnhancedInputComponent`
| Asset / setting | Path / behaviour |
|-----------------|------------------|
| `IA_Move` | `/Game/_Main/Input/IA_Move` — `Axis2D` |
| `IMC_Default` | `/Game/_Main/Input/IMC_Default` — WASD authored on the asset (`IA_Move`: W swizzle YXZ, S swizzle YXZ then negate, A negate, D none) |
| Bind | `IA_Move` → `ATheGoldenAgePlayerCharacter::Move` |
| Context | Added in `ATheGoldenAgePlayerController` on possess |

**Planned:** mouse ground targeting on Player Controller; combat actions — not implemented.

### Player character and battle locomotion animations

**Playable pawn:** `Content/RPGHeroSquad/CharacterBP/BP_TinyHeroPolyart` (child of `ATheGoldenAgePlayerCharacter`).

**Locomotion logic (C++):** `AnimationSingleNode` on skeletal mesh; switches sequences from velocity (no root-motion drive). Idle when speed is below `MinMovingSpeed` (default 10). The local pawn uses `CharacterMovement` velocity. Other online pawns are transform-snapped by `UTheGoldenAgePlayerSync` with movement simulation disabled, so their actor tick stays on and locomotion uses a velocity derived from position deltas (`RemoteIdleGraceSeconds` holds the last move clip between snaps; jumps over `RemoteTeleportDistance` do not count as steps). Direction and play rate use the same four in-place clips as the local pawn. Camera and movement-facing still run only on the locally controlled pawn.

| Local velocity (character space) | Animation asset |
|----------------------------------|-----------------|
| Dominant +Y | `Anim_MoveFWD_Battle_InPlace_TinyHero` |
| Dominant −Y | `Anim_MoveBWD_Battle_InPlace_TinyHero` |
| Dominant −X | `Anim_MoveLFT_Battle_InPlace_TinyHero` |
| Dominant +X | `Anim_MoveRGT_Battle_InPlace_TinyHero` |
| Not moving | `Anim_Idle_Battle_TinyHero` |

Diagonals: **dominant axis** picks one of the four move clips. Play rate scaled by speed vs `AnimPlayRateReferenceSpeed` (default 300).

**Equipment (sword/shield):** attached to skeleton sockets (`Weapon_R` sword, `Weapon_L` shield) manually in `BP_TinyHeroPolyart` (editor). No C++ attachment code (the earlier `AttachEquipmentToSockets` startup code was removed, per the no-startup-configuration rule).

**Input mapping:** WASD rows live on `IMC_Default` (saved asset). `IA_Move` is Axis2D (X = right, Y = forward): W swizzle `YXZ`, S swizzle `YXZ` then negate, A negate, D no modifier. The controller does not rewrite mappings at runtime. `IA_Move` is also resolved in `SetupPlayerInputComponent`, since possess can run before `BeginPlay`.

**Facing:** interpolates yaw toward velocity while moving; idle keeps last rotation; **no** mouse-driven facing.

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
| **Content root for maps** | `Content/_Main/` with prefix `Lvl_` for levels (e.g. `Lvl_Main`) |
| **C++ module** | Single primary module matching project name |
| **Copyright header** | Epic template style in `Source/` files |
| **Build.cs comments** | Epic template pattern for optional Slate / OnlineSubsystem |
| **Git** | Standard UE `.gitignore` (binaries, intermediate, saved, etc.) |

### Adopted design conventions (2026-11-08)

Documented in [Gameplay design requirements](#gameplay-design-requirements); apply to new gameplay work:

| Area | Convention |
|------|------------|
| **C++ vs Blueprint** | Core gameplay and reusable systems in C++; Blueprint children for configuration and designer-facing behaviour |
| **Input** | Enhanced Input only; modular targeting (Controller) vs rotation/movement (Character) vs abilities (combat system) |
| **Code quality** | Epic naming/macros; focused classes; minimal unnecessary Blueprint exposure; consider multiplayer |
| **Content folders** | Group by system; prefer `Content/_Main/Input/` (`IA_*`, `IMC_*`) when input assets are added |
| **Configuration vs runtime code** | Configure content (socket attachments, mappings, defaults) in assets via MCP or manual editor steps — **not** in startup code. If MCP can't do it, instruct the user to configure it manually. See [While making changes](#while-making-changes) #7. |

### Not yet established

- **Content folder taxonomy** beyond `_Main` and `RPGHeroSquad/` (e.g. `Characters`, `UI`) — **not present in repo**.
- **Coding standard automation:** `.editorconfig` / clang-format — **not added** (still follow Epic C++ style manually).
- **Camera class naming/location** — decide when first camera C++ or Blueprint is added.

### Plugin and dependency management

- Add engine plugins via `TheGoldenAge.uproject` `Plugins` array with explicit `Enabled` and `TargetAllowList` when editor-only.
- Add module dependencies in `TheGoldenAge.Build.cs`; avoid enabling Online/Steam until multiplayer design exists.

### Multiplayer considerations (future)

None active. If added: enable appropriate `OnlineSubsystem`, define server travel defaults, replication on game state classes, and update this section before implementation.

### Build and testing

| Step | Notes |
|------|--------|
| **Generate project files** | Right-click `.uproject` or use UE batch files |
| **Build** | `TheGoldenAgeEditor` Win64 target (primary dev platform per artifacts). **The user runs Live Coding manually** after each implementation task. Agents do not compile (see [After making changes](#after-making-changes)). |
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
- [x] **Player movement** — `BP_TinyHeroPolyart`, camera-relative WASD, top-down camera, battle in-place locomotion (2026-11-08)

### In progress

- **None documented in codebase** (no TODO/FIXME in `Source/`).

### Planned gameplay (design documented, not coded)

See [Gameplay design requirements](#gameplay-design-requirements). Next steps (flexible):

- [ ] Player Controller — cursor → ground-plane targeting API
- [ ] Combat rotation via `RequestFaceDirection` / ability system
- [ ] Attack/skill `InputAction`s and mouse bindings
- [ ] Animation Blueprint (optional) if single-node limits grow

### Partially implemented

| Item | Done | Outstanding |
|------|------|-------------|
| GameMode / pawn / input / locomotion | See [Completed](#completed-verified) | Combat, targeting, additional anims |
| Enhanced Input stack | `IA_Move`, `IMC_Default` (WASD saved on the asset), C++ bindings | Targeting actions |
| Third-party hero content | Wired to `BP_TinyHeroPolyart` | LFS commit; tune capsule vs mesh offset if needed |

### Known issues / technical debt

| Item | Severity | Notes |
|------|----------|--------|
| Basic locomotion only | Expected for stage | Combat, AI, UI still blocked |
| No content beyond two maps | Medium | No blueprints, meshes, or input assets in VCS |
| CommonUI settings without plugin enable | Low | May confuse; verify before UI work |
| Aggressive RT/Lumen defaults | Low | May hurt perf on low-end hardware during prototyping |
| `DefaultEditor.ini` empty | Low | Harmless |
| MCP / IDE local config | Info | `.cursor/`, `.vscode/`, `.codex/`, etc. — **gitignored** (was untracked) |
| `.idea/` still in Git history | Medium | Added to `.gitignore`; remove from index with team agreement (see [Git, Git LFS, and Asset Management Guidelines](#git-git-lfs-and-asset-management-guidelines)) |
| Maps in Git as non-LFS blobs | Low | `Lvl_Main` committed before LFS rules — **history migration optional** (~48 KiB) |
| `Content/RPGHeroSquad/` not committed | In progress | ~64 MiB local; must be first major LFS content drop |

### Placeholder / incomplete

- Entire gameplay layer is **placeholder** (engine defaults only).

### Outstanding decisions

| Decision | Notes |
|----------|--------|
| Game genre, camera, core loop | **Decided** — 3D top-down action RPG. **Locomotion implemented**; combat loop not started. |
| C++ vs Blueprint split | **Decided** — C++ foundation, Blueprint customization (see [C++ and Blueprint development philosophy](#c-and-blueprint-development-philosophy)) |
| Multiplayer scope | **Undecided** (no net config); new gameplay code should avoid unnecessary single-player assumptions |
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
| 2026-11-08 | **3D top-down action RPG** direction (*Hades*-style camera; MOBA-inspired targeting) | Establishes consistent vision for camera, movement, facing, and future combat | Third-person action, twin-stick without ground targeting, etc. | Camera, Character, Input, combat (future) |
| 2026-11-08 | **Enhanced Input only** for player controls; modular targeting architecture | UE modern input; separates movement, cursor target, and abilities | Legacy mappings; cursor-driven locomotion rotation | Player Controller, Character, Input assets |
| 2026-11-08 | **C++ core + Blueprint children** for customized gameplay | Performance and reuse in C++; designer iteration in BP | Blueprint-only or duplicated logic in both layers | All future gameplay classes |
| 2026-11-08 | Track UE/media binaries with **Git LFS** via `.gitattributes` | Prevent repo bloat; consistent clones for team/agents | Vendor Perforce / plain Git blobs | Git, `Content/`, CI |
| Recent (dev env) | Enable **MCP / Editor toolset** plugins in `.uproject` | AI-assisted editor workflows | Manual editor-only workflow | Editor, not packaged game |

**Reconsideration policy:** If reversing a decision (e.g. disabling Lumen for shipping), add a new row here with rationale; do not delete historical rows.

---

## H. Development Change Log

Entries are **newest first**. Preserve all history.

### 2026-11-08 — User compiles manually; agents do not trigger Live Coding

| Field | Detail |
|-------|--------|
| **Summary** | Agents no longer run Live Coding or any other compile after a task. The user compiles manually in the open editor when the work is done. |
| **Files / systems** | `.cursor/rules/unreal-implementation-workflow.mdc`; `PROJECT_CONTEXT.md` — [After making changes](#after-making-changes), [Build and testing](#build-and-testing), [AI agent responsibilities](#ai-agent-responsibilities). |
| **Reason** | Compilation stays under the user's control. |
| **Follow-up** | None. |

### 2026-11-08 — Always-on rule to maintain PROJECT_CONTEXT.md

| Field | Detail |
|-------|--------|
| **Summary** | Added `.cursor/rules/project-context.mdc` (`alwaysApply: true`). Before each task, read `PROJECT_CONTEXT.md`. After each task, update it with the relevant result. |
| **Files / systems** | `.cursor/rules/project-context.mdc`, `.gitignore` (track `.cursor/rules/` only), `PROJECT_CONTEXT.md`. |
| **Reason** | Make the existing context workflow automatic for every agent task, and keep the rule in the repo. |
| **Follow-up** | None. |

### 2026-11-08 — Remote player movement animations

| Field | Detail |
|-------|--------|
| **Summary** | Other online pawns now play the same battle move clips as the local pawn while their snapped transform is changing, and return to idle after they stop. |
| **Files / systems** | `ATheGoldenAgePlayerCharacter` presentation velocity; `UTheGoldenAgePlayerSync::ConfigurePawnPresence` keeps remote actor tick enabled. |
| **Reason** | Remote pawns were teleported with movement disabled and actor tick off, so `GetVelocity()` stayed zero and the idle single-node clip never switched. |
| **Follow-up** | In a two-client session, move one character and confirm the other window shows forward/back/left/right clips, then idle when movement stops. |

### 2026-11-08 — Require Live Coding compile after each task

| Field | Detail |
|-------|--------|
| **Summary** | Agents must run Unreal Live Coding after each implementation task and confirm successful compilation before treating the task as done. |
| **Files / systems** | `PROJECT_CONTEXT.md` — [After making changes](#after-making-changes), [Build and testing](#build-and-testing), [AI agent responsibilities](#ai-agent-responsibilities). |
| **Reason** | Catch C++ compile failures in the open editor before a task is reported complete. |
| **Follow-up** | None. |

### 2026-11-08 — Character stuck on floor while walk anim played

| Field | Detail |
|-------|--------|
| **Summary** | Removed the Z-up plane constraint on `ATheGoldenAgePlayerCharacter`. The capsule sat ~0.5 cm into `Floor_0` and could not be lifted, so movement sweeps failed while velocity still drove the walk animation. |
| **Files / systems** | `TheGoldenAgePlayerCharacter.cpp`; `BP_TinyHeroPolyart` CharacterMovement `bConstrainToPlane` cleared and saved. |
| **Reason** | Editor log: `is stuck and failed to move` against `Floor_0`, normal +Z, location Z=95.50. |
| **Follow-up** | PIE again after Live Coding. |

### 2026-11-08 — Player movement and battle locomotion (`BP_TinyHeroPolyart`)

| Field | Detail |
|-------|--------|
| **Summary** | Added `ATheGoldenAgeGameMode`, `ATheGoldenAgePlayerController`, `ATheGoldenAgePlayerCharacter`; reparented `BP_TinyHeroPolyart`; Enhanced Input assets; four battle in-place move clips + idle via `AnimationSingleNode`. |
| **Files / systems** | `Source/TheGoldenAge/*Player*`, `Config/DefaultEngine.ini`, `Content/_Main/Input/`, `Content/RPGHeroSquad/CharacterBP/BP_TinyHeroPolyart` |
| **Reason** | First playable top-down movement using existing Tiny Hero assets. |
| **Follow-up** | Commit new/changed assets with LFS; verify feel in PIE; add mouse targeting when implementing combat. |

### 2026-11-08 — Document gameplay design and development standards

| Field | Detail |
|-------|--------|
| **Summary** | Expanded `PROJECT_CONTEXT.md` with intended 3D top-down action RPG perspective, *Hades*-inspired camera, *Hades*/MOBA hybrid controls (WASD movement-facing, mouse ground targeting), Enhanced Input policy, C++/Blueprint split, code quality rules, folder guidance, and system responsibilities. Distinguished **[Design]** from **[Impl]** throughout. |
| **Files / systems** | `PROJECT_CONTEXT.md` only (no gameplay code changes). |
| **Reason** | Align agents and developers on long-term input architecture and implementation conventions before first gameplay classes land. |
| **Implementation notes** | Verified: `Source/` still has no `UCLASS` gameplay types; Enhanced Input defaults in config/Build.cs only; no project Input Action assets in Git. |
| **Follow-up** | Implement Player Controller, Character, camera, and first `IA_*` / `IMC_*` assets per [Planned gameplay](#planned-gameplay-design-documented-not-coded). |

### 2026-11-08 — Remove unused `Lvl_M` map

| Field | Detail |
|-------|--------|
| **Summary** | Deleted `Content/_Main/Lvl_M.umap` (not used). Default maps unchanged (`Lvl_Main` only). |
| **Files / systems** | `Content/_Main/Lvl_M.umap` (removed); `PROJECT_CONTEXT.md` |
| **Reason** | Team decision — map will not be used. |
| **Follow-up** | Commit deletion with `git rm` so remote/history no longer ship the asset. |

### 2026-11-08 — Configure Git LFS and UE git exclusions

| Field | Detail |
|-------|--------|
| **Summary** | Added Git LFS patterns for UE and media assets; extended `.gitignore` for IDE/MCP/OS junk; documented team Git/LFS rules in this file. Ran `git lfs install` (hooks updated). **No commit or history rewrite performed.** |
| **Files / systems** | `.gitattributes`, `.gitignore`, `PROJECT_CONTEXT.md` |
| **Reason** | Support collaborative UE development and large binaries (`Content/RPGHeroSquad/`, future Fab imports). |
| **Implementation notes** | LFS: `.uasset`, `.umap`, `fbx`, textures, audio, video, fonts; `Content/**/*.obj` only (avoids C++ `*.obj` in `.gitignore`). Ignore: `.idea/`, `.vscode/`, `.cursor/`, `.codex/`, `.gemini/`, `.mcp.json`, `.DS_Store`, `Thumbs.db`. |
| **Follow-up** | Commit `.gitattributes` + `.gitignore` first; then add content with LFS; optional `git lfs migrate` for old maps; stop tracking `.idea/` if team agrees. Ensure GitHub LFS storage/billing for ~64 MiB+ squad content. |

### 2026-11-08 — Create shared project context document

| Field | Detail |
|-------|--------|
| **Summary** | Added `PROJECT_CONTEXT.md` at repository root after full codebase inspection. |
| **Files / systems** | `PROJECT_CONTEXT.md` (new); references `Source/`, `Config/`, `Content/_Main/`, `TheGoldenAge.uproject`. |
| **Reason** | Enable consistent multi-agent and team development with a single source of truth. |
| **Implementation notes** | Document reflects verified empty-template state: no gameplay C++, two maps, UE 5.8, Enhanced Input defaults, MCP editor plugins. Unknowns explicitly marked. |
| **Dependencies / architecture** | None code-related; process dependency on agents updating this file after each meaningful change. |
| **Follow-up** | When gameplay begins: add GameMode/Pawn, content folders, and update Sections B, D, F; resolve CommonUI. |

---

## Git, Git LFS, and Asset Management Guidelines

Shared rules for **all developers and AI agents**. Remote: `https://github.com/shuang-shawn/TheGoldenAge.git` (Git LFS endpoint enabled on GitHub).

### Prerequisites (each machine)

1. Install [Git LFS](https://git-lfs.com/) (project verified with **3.3.0** on Windows).
2. Clone, then run: `git lfs install` (once per user account).
3. After pull: `git lfs pull` if assets look like small text pointer files instead of binaries.

### Asset management

- All Unreal binary assets **must** be tracked through Git LFS where configured in `.gitattributes` (`.uasset`, `.umap`, textures, audio, etc.).
- **Imported Fab / marketplace content** (e.g. `Content/RPGHeroSquad/`): commit **all** dependencies (meshes, materials, textures, skeletons, animations). Open referencers in editor before push to catch missing pieces.
- **Do not** commit generated UE folders: `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, build artifacts (see `.gitignore`).
- Before adding large binaries, confirm extension is in `.gitattributes` (`git check-attr filter -- <path>` should report `filter: lfs`).
- **`*.obj`:** C++ object files are excluded by `*.obj` in `.gitignore`. 3D Wavefront models under `Content/**/*.obj` are LFS-tracked only if force-added or placed under `Content/` with team awareness — prefer **FBX/GLB** for source art.
- Preserve third-party **licenses and attribution** (Fab/Epic/marketplace) in `Content/` or `Docs/` as required by the license.
- Do not commit **secrets** (Android File Server tokens, API keys, MCP credentials).

### Blueprint and map collaboration

- Avoid editing the **same** `.uasset` or `.umap` at the same time across machines/agents — Git cannot merge UE binaries meaningfully.
- Coordinate **ownership** of major Blueprints, maps (e.g. `Lvl_Main`), and gameplay systems.
- Do not overwrite another developer's work without understanding its purpose.
- Commit often; keep commits **focused** (one map, one feature, or one content pack).
- If a binary conflict occurs, **do not** pick a side blindly — coordinate with the last editor of that asset.

### Git workflow

1. `git pull` (and `git lfs pull`) before starting work.
2. Review `git status` and existing uncommitted changes (especially another agent's WIP).
3. After adding binaries, verify: `git lfs ls-files` lists new paths; files are not 130-byte `version https://git-lfs.github.com/spec/v1` stubs on disk when opened in UE.
4. Commit `.gitattributes` and `.gitignore` when tracking or exclusion rules change.
5. **Never** `git push --force` or rewrite **shared** history without explicit team agreement.
6. Push order for new LFS setup: commit **`.gitattributes` first**, then LFS content (GitHub must know patterns before objects upload).

### AI agent responsibilities

**Before modifying the project**

1. Read `PROJECT_CONTEXT.md` (including this section).
2. Check `git status` and `git diff`.
3. Understand uncommitted work; do not clobber another developer or agent's changes.

**After modifying the project**

1. **Do not compile.** Leave Live Coding to the user (see [After making changes](#after-making-changes)).
2. Review `git diff` and `git lfs status` (if available).
3. Confirm new binaries are LFS-tracked, not plain blobs.
4. Update `PROJECT_CONTEXT.md` if Git conventions, content layout, or repo state changed materially.
5. Add a [Development Change Log](#h-development-change-log) entry for meaningful repo/config changes.

### Current repository state (LFS migration)

| Item | Status |
|------|--------|
| **LFS rules in `.gitattributes`** | Configured locally (**uncommitted** until team commits) |
| **Files already in Git as plain blobs** | `Content/_Main/Lvl_Main.umap` (~48 KiB) — committed **before** LFS rules (`Lvl_M` removed 2026-11-08) |
| **`git lfs ls-files` on HEAD** | **Empty** — no LFS objects in history yet |
| **Impact** | Very small; new adds use LFS once rules are on `main` |
| **Optional history rewrite** | Not required for size; recommended for **consistency** after team agrees |

**Optional migration plan (do not run without team + remote coordination)**

1. Ensure everyone has pushed or backed up work; announce maintenance window.
2. Commit and push current `.gitattributes` on `main`.
3. On a clean clone, migrate history (example — adjust includes as needed):

   ```bash
   git lfs migrate import --include="*.umap,*.uasset" --everything
   ```

4. All collaborators: **re-clone** or hard-reset to the new history per Git LFS migrate docs; **never** mix old and new history.
5. Force-push only if **all** team members agree and GitHub is the single source of truth.

**Stop tracking IDE metadata (recommended follow-up)**

`.idea/` is still in the initial commit but now **gitignored**. To remove from the index without deleting local files:

```bash
git rm -r --cached .idea/
git commit -m "Stop tracking Rider .idea metadata"
```

### Adding `Content/RPGHeroSquad/` (recommended next content commit)

1. Commit `.gitattributes` + `.gitignore` + `PROJECT_CONTEXT.md` updates.
2. `git add Content/RPGHeroSquad/`
3. Verify: `git lfs ls-files | head` shows `.uasset` entries.
4. Commit and push; confirm GitHub LFS objects on the repo’s Insights → LFS.

---

## Appendix: Environment setup (optional)

| Item | Notes |
|------|--------|
| **Engine** | Install UE **5.8** and associate with `TheGoldenAge.uproject` |
| **Git LFS** | Required for content work; `git lfs install` after clone |
| **unreal-mcp** | Optional; run local server on port 8000 if using Cursor/VS MCP configs (gitignored under `.cursor/`, `.vscode/`) |
| **Android File Server** | If used, tokens live in `Config/DefaultEngine.ini` — **rotate/regenerate if leaked**; do not commit secrets to shared docs |

---

## Quick reference

| Question | Answer |
|----------|--------|
| Default map? | `Content/_Main/Lvl_Main` |
| Game module? | `Source/TheGoldenAge/` |
| Intended genre / camera? | **3D top-down action RPG** (*Hades*-style); [Gameplay design requirements](#gameplay-design-requirements) |
| Player controls (design)? | WASD movement; mouse ground targeting for combat; no idle cursor-facing |
| Input system? | **Enhanced Input** (project assets **not** created yet) |
| C++ vs Blueprint? | C++ core; Blueprint children for tuning |
| Default pawn? | `BP_TinyHeroPolyart` via `ATheGoldenAgeGameMode` |
| Any custom C++ gameplay classes? | **Yes** — GameMode, PlayerController, PlayerCharacter |
| Multiplayer? | **Not configured** |
| Where to log agent work? | [Development Change Log](#h-development-change-log) |
| Git LFS / assets? | [Git, Git LFS, and Asset Management Guidelines](#git-git-lfs-and-asset-management-guidelines) |

---

*End of PROJECT_CONTEXT.md*
