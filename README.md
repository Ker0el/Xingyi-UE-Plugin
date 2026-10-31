<div align="center">

<img src="Resources/Icon128.png" width="140" alt="Xingyi"/>

# Xingyi - Level as Asset Palette

**Use any saved level as an asset board: browse its actors in a panel, click one and drag it straight into the level you are editing - or move a whole group at once**

[![UE](https://img.shields.io/badge/Unreal%20Engine-5.4%20%7C%205.5%20%7C%205.6%20%7C%205.7%20%7C%205.8-0E1128?logo=unrealengine&logoColor=white)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-MIT-3DA639)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Win64-0078D4?logo=windows&logoColor=white)](../../releases/latest)
[![Release](https://img.shields.io/github/v/release/Ker0el/Xingyi-UE-Plugin?color=blue&label=Download)](../../releases/latest)

[Install](#installation) · [Quick start](#quick-start) · [Multi-select](#multi-select-moving-a-whole-group) · [Preview lighting](#preview-lighting) · [FAQ](#faq)

</div>

---

## What it does

**Xingyi** is an Unreal Engine 5 editor plugin that turns **any saved level into an asset board**.

Open an existing level - a set-dressing scene full of props, a bought environment, an old scene you made
years ago - and its actors appear in the panel. To use one, **click it and drag it straight into the level
you are editing**: no digging through the Content Browser, no opening that level just to copy and paste.

Moving actors one by one is slow. Turn on **Multi-Select** in the toolbar, click the props you want,
then drag any one of them - **the whole group lands in your level keeping its relative positions and
rotations**. Tables stay next to tables; nothing shifts.

> The plugin addresses the same use case as the engine's experimental **Actor Palette** plugin.
> It is an independent implementation and contains no Actor Palette source code.

---

## What Xingyi adds

| | Engine's experimental Actor Palette | **Xingyi** |
|:---|:---|:---|
| **Preview lighting** | The preview scene brings its own sun at the same priority as the sun in your level, so the renderer reports "multiple directional lights competing" and the message stays pinned to the viewport | The built-in sun is pinned to the **lowest priority**, so it can never tie with the level's sun; the status bar always states which lighting the preview uses |
| **Levels without lights** | A level with no lights renders **completely black** | The fallback light is lit automatically so you can see the content |
| **Crash points** | Clicking the panel while its widget is not in a window crashes in `ToSharedRef()`; reopening the same panel trips a `check()` assertion | All paths are null-checked; assertions were downgraded to notices |
| **Cross-world selection** | Selecting an actor in the panel adds it to the editor selection **and never clears it** (Delete removes the panel copy, not the level one) | Cleared automatically when a drag ends, the level changes, the panel closes, the selection is cleared or the mode changes - and the panel is repainted, so no orange outline is left behind |
| **Multi-select / group drag** | Not available; actors can only be dragged one at a time | A **Multi-Select** toggle in the toolbar; drag any selected actor and **the whole group moves with its relative positions and rotations preserved** |
| **Instance scale** | Only the asset is passed, so **scale changed on the instance is lost** | The source actor's scale is carried over |
| **Realtime toggle** | Has no effect | Actually ticks the preview world (particles, animation, texture streaming) |
| **Billboard sprites** | Off by default, so particles and material spheres are invisible | Shown by default; the ground grid is shown by default too |
| **Opening a level** | Only through a list in the menu | Menu **and** dragging a level asset straight onto the panel |
| **Clear selection** | Click empty space and hope | A dedicated **Clear Selection** button - levels are full of sky spheres, atmosphere and volume boxes, so clicking empty space rarely works |
| **Automation tests** | None | 4 (lighting decision, recent/favorites list, viewport construction smoke test, World Partition diagnostic) |

---

## Installation

### Option 1: precompiled package (recommended - no Visual Studio required)

Download the zip for your engine version from the [**Releases**](../../releases/latest) page:

| Engine version | Package |
|:---|:---|
| UE 5.4 | `Xingyi_UE5.4.zip` |
| UE 5.5 | `Xingyi_UE5.5.zip` |
| UE 5.6 | `Xingyi_UE5.6.zip` |
| UE 5.7 | `Xingyi_UE5.7.zip` |
| UE 5.8 | `Xingyi_UE5.8.zip` |

Unzip it into your project's `Plugins` folder. The result should look like this:

```
YourProject/
├── Content/
├── Plugins/
│   └── Xingyi/              <- the folder you unzipped
│       ├── Xingyi.uplugin
│       ├── Binaries/Win64/UnrealEditor-Xingyi.dll
│       ├── Resources/
│       └── Source/
└── YourProject.uproject
```

> Create the `Plugins` folder if it does not exist. **Blueprint-only projects work too** - the precompiled
> binary needs no C++ compilation.

When the editor asks whether to rebuild the module, answer **No** - the binary in the package is built
for that engine version.

### Option 2: build from source

```bash
cd YourProject/Plugins
git clone https://github.com/Ker0el/Xingyi-UE-Plugin.git Xingyi
```

Then **right-click the `.uproject` → Generate Visual Studio project files**, open the solution and build
the `Development Editor` configuration, or use the command line:

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" YourProjectEditor Win64 Development -Project="YourProject.uproject" -WaitMutex
```

---

## Quick start

1. Open the editor and choose **Tools → Xingyi → Xingyi Panel 1** (up to 4 panels can be open at once,
   each showing its own level)
2. Click **Select Level** at the top-left of the panel and pick a level to use as an asset board - or
   **drag a level asset from the Content Browser onto the panel**
3. The level content appears in the panel. **Click any actor** and it follows the mouse; **release to place
   it in your level**
4. To change the view, fly around as in any viewport; **Reset Camera** returns to the view stored in the level

The status bar at the bottom shows **which level is open**, **which lighting the preview uses** and
**how many actors are selected**.

Toolbar, left to right: **Select Level · Reload · Favorite · Lighting · Realtime · Game View · Reset Camera ·
Multi-Select · Clear Selection**.

---

## Multi-select: moving a whole group

The **Multi-Select** toggle sits at the right end of the toolbar and is **off** by default - with it off,
behaviour is exactly as before: click once and drag that one actor.

Turn it on:

| Action | Result |
|:---|:---|
| Left-click an **unselected** actor | Adds it to the selection (drawn with an orange outline) |
| Left-click an **already selected** actor | **Drags the whole group**, anchored on that actor |
| `Ctrl` + left-click a selected actor | Removes it from the selection |
| **Clear Selection** | Clears everything at once |
| Click empty space | Also clears the selection (but levels are usually full of sky spheres and volumes, so the button is more reliable) |

**When a group is dropped, the relative positions and rotations are preserved.** Positions are computed
relative to the actor you dragged: that actor lands under the mouse and the others return to their
original places around it. **Each actor's scale is also carried over** - a drag passes assets only, and
assets store the default scale, so manual instance scaling used to be lost.

With nothing selected, clicking any actor adds it to the selection; once you have a selection, clicking
one of the selected actors starts the drag.

> **While dragging, the preview under the cursor is a stack of overlapping actors**; the group spreads
> into its original layout when you release. This is how the engine's drag preview works - see
> "Known limitations" below. A line next to the cursor tells you how many objects are being dragged.

---

## Preview lighting

The preview scene comes with its own directional light, and the level you open usually has a sun as well.
When two lights share the same priority, the renderer reports:

> Multiple directional lights are competing to be the single one used for forward shading, translucency,
> water or volumetric fog...

Xingyi's approach:

- The built-in light is set to the **lowest priority** (`ForwardShadingPriority = -1`), so it can never tie
  with the level's sun and that message cannot be triggered by the plugin;
- The default mode is **Always On**: the fallback light stays lit, so the preview brightness matches the
  engine's own palette; because it has the lowest priority the message still does not appear;
- Other modes are available from the **Lighting** menu: **Auto** (use the level's own lighting whenever it
  has lights, so the preview matches the level) and **Always Off**;
- The status bar always states whether the preview uses "Fallback light + level lighting (N lights)" or
  "Using the level's own lighting (N lights)".

Sun intensity and angle are configured in **Project Settings → Plugins → Xingyi**.

---

## Settings

**Project Settings → Plugins → Xingyi**:

| Setting | Description |
|:---|:---|
| **Fallback Light Mode** | Always On (default) / Auto / Always Off |
| **Fallback Light Intensity / Rotation** | Only used when the fallback light is on |
| **Show Ground Grid** | On by default |
| **Show Billboard Sprites** | Icons for particles and material spheres, on by default |
| **Performance Mode** | Disables advanced features such as Lumen for a higher frame rate; changes how the preview looks |
| **Frame Content On Level Load** | Off by default. When off, the level's stored view is used |

---

## Compatibility

| Engine version | Status | Binary compatibility |
|:---|:---:|:---|
| UE 5.4 | ✅ | All 5.4.x |
| UE 5.5 | ✅ | All 5.5.x |
| UE 5.6 | ✅ | All 5.6.x |
| UE 5.7 | ✅ | All 5.7.x |
| UE 5.8 | ✅ | All 5.8.x |

> **This release (v1.2.4) ships the UE 5.8 package only.** Precompiled packages for 5.4-5.7 follow in
> later releases.

- **Platform**: Win64 (Windows 10 / 11)
- **Module type**: Editor module (editor only; it is never compiled into a packaged game)
- Each package is built against that engine's `CompatibleChangelist`, so minor upgrades within the same
  major version do not require a rebuild

---

## Known limitations

- **World Partition levels only show the "always loaded" part** (sky, atmosphere, fog, lighting, terrain
  shell and so on). The bulk of such a level lives in spatial loading cells that the streaming system
  loads by region, and the preview world does not run that streaming - an engine-level limitation that
  applies to the engine's own Actor Palette as well.
  To use content from a World Partition level, **copy the actors you need into a regular level** and open
  that level in Xingyi.
- **While dragging a group, the preview under the cursor overlaps**; the group spreads out when dropped.
  The engine pins the drag preview to the cursor position (`FActorPositioning::GetSurfaceAlignedTransform`
  returns `SurfaceLocation + collision offset`; the starting transform only affects rotation and scale),
  and the drag thumbnail draws `AssetData[0]` only. As a compensation, a line next to the cursor reads
  "N object(s) - placed in their original layout when dropped".

---

## FAQ

<details>
<summary><b>A level I dragged in shows only sky / nothing at all</b></summary>

It is most likely a **World Partition level** - its content lives in external actor files loaded by region
by the streaming system, which the panel cannot reach. See "Known limitations" above.

Use a **regular level** as the asset board, or copy the actors you need into a regular level first.

</details>

<details>
<summary><b>The panel reports "multiple directional lights competing..."</b></summary>

That is the renderer's message about **two directional lights with the same priority in the scene**.

Xingyi already pins its own light to the lowest priority, so the plugin cannot trigger it. If you still
see it, **your level probably contains two suns** - select them and give one of them a different
`Forward Shading Priority`.

</details>

<details>
<summary><b>After clicking an actor in the panel, what does Delete remove?</b></summary>

Actors in the panel belong to the **preview world**; they are copies of the level's actors. Xingyi removes
them from the editor selection and repaints the panel when a drag ends, the level changes, the panel
closes, the selection is cleared or the mode changes, so normal use will not delete the wrong actor and
will not leave an orange outline behind.

</details>

<details>
<summary><b>When dragging several actors, the cursor shows a stack of overlapping objects</b></summary>

That is expected - see "Known limitations": the drag preview position is fixed by the engine. **The group
spreads into its original relative layout when you release.**

A line next to the cursor reads "N object(s) - placed in their original layout when dropped" so you can
confirm how many objects are being dragged.

</details>

<details>
<summary><b>Can I use it in a Blueprint-only project?</b></summary>

Yes. The precompiled package is made for Blueprint-only projects - no Visual Studio and no C++ build
required.

</details>

<details>
<summary><b>On startup the editor says the module is missing or incompatible</b></summary>

Precompiled binaries are built for a **specific engine version**; installing the wrong one causes this.

Check that `Xingyi/Binaries/Win64/UnrealEditor.modules` matches your engine's
`Engine/Build/Build.version`:

```json
{
    "BuildId": "55116800",
    "Modules": { "Xingyi": "UnrealEditor-Xingyi.dll" }
}
```

Install the zip that matches your engine version.

</details>

<details>
<summary><b>Toolbar buttons are cut off in a narrow panel</b></summary>

The toolbar does not wrap, so the rightmost buttons are clipped when the panel is narrow. Widen the panel.

</details>

---

## Credits and license

This plugin is released under the **[MIT](LICENSE)** license - commercial use, modification and
redistribution are unrestricted.

| | |
|:---|:---|
| **Author** | **StarGame** ([Bilibili](https://space.bilibili.com/177308205)) |
| **License** | MIT, full text in [LICENSE](LICENSE) |

It addresses the same use case as the engine's experimental **Actor Palette** plugin; the code is an
independent implementation. See **[NOTICE.md](NOTICE.md)** for details.

> This plugin is provided "as is", without any express or implied warranty.

---

<div align="center">

**If this plugin helps you, please give it a ⭐**

By [StarGame](https://space.bilibili.com/177308205) · a UE editor plugin that turns any level into an asset board

</div>
