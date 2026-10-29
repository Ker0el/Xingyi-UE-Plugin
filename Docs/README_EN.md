# Xingyi (星移) — User Guide

Xingyi is an Unreal Engine 5 editor plugin that **turns any level into an asset board**.
Open a saved level inside the panel, browse the actors it contains, and drag the ones you
want straight into the level you are editing — one at a time, or as a group.

- Plugin name: **Xingyi (星移)**
- Module: `Xingyi` — `Type: Editor`, `LoadingPhase: Default`, Win64 only
- Supported engine version: **Unreal Engine 5.8**
- License: MIT (see the repository `LICENSE`)
- Support: <https://github.com/Ker0el/Xingyi-UE-Plugin/issues>

> The panel UI is currently in Simplified Chinese. All commands referenced below are given
> in English with the original label in brackets.

---

## 1. Requirements

| Item | Requirement |
|---|---|
| Engine | Unreal Engine 5.8 |
| Platform | Windows (Win64), Windows 10 / 11 |
| Project type | C++ or Blueprint-only |
| Host project | A project that loads the plugin; it does not need to be C++ |

The plugin is an **editor-only** module. It is not compiled into packaged games and has no
effect on shipped builds.

---

## 2. Installation

### Fab / Launcher

1. Add the product to your library from Fab and install it to **UE 5.8**.
2. In the editor, open **Edit → Plugins**, search for `Xingyi`, and enable it.
3. Restart the editor.

### Manual (folder)

Copy the `Xingyi` folder into `<YourProject>/Plugins/Xingyi`, then enable the plugin and
restart. The folder must contain:

```
Xingyi/
├── Xingyi.uplugin
├── Config/FilterPlugin.ini
├── Docs/
├── Resources/
└── Source/
```

---

## 3. Quick start

1. Open **Tools (工具) → Xingyi (星移) → Xingyi Panel 1 (星移面板1)**.
   Up to four panels can be open at the same time, each showing a different level.
2. Click **Select Level (选择关卡)** in the panel toolbar and pick a level to use as the
   board — or drag a level asset from the Content Browser straight onto the panel.
3. The level's actors appear in the panel. **Left-click an actor** — it follows the cursor —
   and **release over your level** to place it.
4. To change the view, fly around inside the panel exactly as you would in a viewport;
   **Reset Camera (重置相机)** returns to the view stored in the level.

The status bar at the bottom of the panel always shows **which level is open**, **which
lighting is in use**, and **how many actors are selected**.

Toolbar, left to right: **Select Level · Reload · Favorites · Lighting · Realtime · Game View ·
Reset Camera · Multi-Select · Clear Selection**.

---

## 4. Multi-select: move a whole group at once

The **Multi-Select (多选模式)** toggle sits at the right end of the toolbar and is **off by
default** — while it is off, a click drags exactly one actor.

With it on:

| Action | Result |
|---|---|
| Left-click an **unselected** actor | Adds it to the selection (orange outline in the panel) |
| Left-click an **already selected** actor | Drags the **whole selection**, anchored on that actor |
| `Ctrl` + left-click a selected actor | Removes it from the selection |
| **Clear Selection (清空选中)** | Clears everything at once |
| Click empty space in the panel | Also clears (levels are usually full of sky spheres and volumes, so the button is more reliable) |

**A group keeps its relative position and rotation.** Offsets are measured from the actor you
drag: it lands under the cursor and the rest fall back into their original arrangement.
Each actor's **instance scale is copied from the source** as well — a plain asset drag only
carries asset defaults, so per-instance scale used to be lost.

> **During** a group drag the cursor preview shows the actors stacked on top of each other;
> they spread into their original layout when you release. This is engine behaviour — see
> [Known limitations](#7-known-limitations). The cursor shows `N objects · placed in original
> layout on release` so you can confirm how many are being dragged.

---

## 5. Preview lighting

The panel runs its own preview world. Xingyi's preview sun is pinned to
`ForwardShadingPriority = -1`, so it can never tie with a sun in the level being previewed —
the renderer warning about *"multiple directional lights competing"* cannot be triggered by
the plugin.

Lighting modes, from the **Lighting (光照 ▾)** menu:

| Mode | Behaviour |
|---|---|
| **Always On** (default) | The fallback sun stays on; preview brightness matches the level, and the status bar says `fallback light + level lighting (N lights)` |
| **Auto** | Uses the level's own lights when the level has any; the preview looks like the level |
| **Always Off** | No fallback sun |

Fallback light intensity and angle are configured in **Project Settings → Plugins → Xingyi**.

If a level has no lights at all, Xingyi lights it anyway, so an empty level is never black.

---

## 6. Settings

**Project Settings → Plugins → Xingyi**

| Setting | Description |
|---|---|
| Fallback light mode | Always On (default) / Auto / Always Off |
| Fallback light intensity / angle | Used only when the fallback light is on |
| Show floor grid | Default on |
| Show billboard sprites | Icons for particles and material spheres; default on |
| Performance mode | Disables expensive features (e.g. Lumen) for frame rate; changes preview look |
| Frame content after opening | Default **off** — when off, the level's own saved view is used |

---

## 7. Known limitations

- **World Partition (open world) levels only show the always-loaded part** (sky, atmosphere,
  fog, lighting, terrain shell). The level's actual content lives in external actor files that
  are streamed by region, and the preview world does not run that streaming. This is an engine
  limitation. To use actors from such a level, copy them into a normal level first, then open
  that level in the panel.
- **During a multi-select drag the preview is stacked at the cursor.** The engine hard-codes
  the drop preview position inside `FActorPositioning::GetSurfaceAlignedTransform`
  (`SurfaceLocation + collision offset`); only rotation and scale are taken from the dragged
  transform, and the drag thumbnail draws `AssetData[0]` only. Actors spread out on release.
- The toolbar does not wrap: in a very narrow panel the rightmost buttons are clipped.
  Widen the panel.

---

## 8. FAQ

**I dragged a level in and the panel is empty / only sky.**
Most likely a World Partition level. See Known limitations.

**I still see the "multiple directional lights competing" warning.**
The plugin's own light is pinned to the lowest priority, so this normally cannot come from
Xingyi. If it appears, the level itself probably contains two suns — select them and give one
of them a different `Forward Shading Priority`.

**Which actor does Delete remove after I click in the panel?**
Actors inside the panel belong to the preview world and are copies. Xingyi removes them from
the editor selection when a drag ends, when the panel is closed, when the level changes, when
the selection is cleared, and when multi-select is toggled — so normal use cannot delete the
wrong actor and leaves no stale outline.

**The outline does not disappear after clearing.** Fixed in v1.2.0: every clear path now
invalidates the panel itself, and the invalidate call lives inside the clear function so no
call site can forget it.

**Can I use this in a Blueprint-only project?** Yes. The Fab build is source-only and is
compiled by Epic for your engine version; no Visual Studio is required.

**"Module missing or incompatible, rebuild?" on startup.** The binary was built for a
different engine version. Install the build for your engine version.

---

## 9. Attribution and licence

The plugin is licensed under **MIT** — commercial use, modification and redistribution are
permitted. Copyright 2026 StarGame (星空 / Ker0el).

Xingyi addresses the same use case as Unreal Engine's experimental **Actor Palette** plugin.
Its source code is an independent implementation and does not contain Actor Palette source
code; see `NOTICE.md` in the repository for the full statement.

Unreal Engine and its content are not included in this product and remain the property of
Epic Games, Inc. Unreal® is a trademark of Epic Games, Inc.
