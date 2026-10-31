# Credits and license · NOTICE

Xingyi is an Unreal Engine editor plugin released under the **MIT** license (full text in
[LICENSE](LICENSE)) - commercial use, modification and redistribution are unrestricted.

Copyright 2026 StarGame. All Rights Reserved.

A Chinese translation of the user guide is available on GitHub:
<https://github.com/Ker0el/Xingyi-UE-Plugin/blob/main/Docs/README_ZH.md>.

---

## 1. Relationship to the engine's Actor Palette plugin

Xingyi addresses the same use case as the experimental **Actor Palette** plugin that ships with Unreal
Engine (`Engine/Plugins/Experimental/ActorPalette/`): both treat an arbitrary level as an asset board,
let you browse its actors and drag them into the level being edited.

Xingyi's source code is an independent implementation and contains no Actor Palette source code.

---

## 2. Improvements and additions

**Fixes** (behaviour that the Actor Palette plugin exposes in practice)

- **Competing preview lighting**: the preview scene brings its own sun at the same priority as the sun in
  the opened level, so the renderer reports "multiple directional lights are competing to be the single
  one used for forward shading, translucency, water or volumetric fog" and the message stays pinned to the
  viewport. Xingyi pins its own light to the **lowest priority**, so the two can never tie.
- **Null-pointer crashes**: clicking the panel while its widget is outside a window crashed in
  `ToSharedRef()`, and reopening the same panel tripped a `check()` assertion. Xingyi null-checks every
  path and downgrades assertions to notices.
- **Stale cross-world selection**: selecting an actor in the panel added it to the editor selection and
  never removed it (Delete removed the panel copy while the level copy remained). Xingyi clears the
  selection when a drag ends, the level changes, the panel closes, the selection is cleared or the mode
  changes.
- **Outline not clearing**: after clearing the selection the orange outline stayed visible. The panel's
  preview world and the level are two different worlds, while `GEditor->NoteSelectionChange()` only
  repaints the level viewport and the panel is an independent viewport client that is not realtime by
  default. Xingyi repaints the panel inside the clear function, so every path is covered.
- **Black preview for levels without lights**: such a level renders completely black. Xingyi lights the
  fallback light.

**Additions**

- **Multi-select mode**: click unselected actors to add them to the selection, then click a selected actor
  to drag the whole group out with its relative positions and rotations preserved; each actor's instance
  scale is carried over as well.
- **Clear Selection button**: levels are full of sky spheres, atmosphere and volume boxes, so clicking
  empty space to clear the selection rarely works.
- A status bar showing the current level, the lighting source and the number of selected actors.
- **Drag a level asset straight onto the panel** to open it.
- Empty-state guidance; up to 4 independent panels can be open at the same time.
- Settings: fallback light mode / intensity / rotation, ground grid, billboard sprites, performance mode,
  and whether to frame the content after loading a level.
- 4 automation tests (lighting decision, recent and favorites lists, viewport construction smoke test,
  World Partition diagnostic).

---

## 3. Known limitations

- **World Partition levels only show the "always loaded" part.** The bulk of the content lives in external
  actor files loaded by region by the streaming system, and the preview world does not run that streaming -
  an engine-level limitation shared with the Actor Palette plugin. To use content from such a level, copy
  the actors you need into a regular level first, then open that level.

---

## 4. Disclaimer

This plugin is provided "as is", without any express or implied warranty.

---

## 4.1 MIT license full text

Fab packaging rules require that repository metadata files such as `LICENSE` are not shipped inside the
package, so the full license text is reproduced here (a standalone copy also exists in the repository:
[LICENSE](LICENSE)):

```
MIT License

Copyright (c) 2026 StarGame <https://space.bilibili.com/177308205>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 5. Not covered by this notice

- Unreal Engine itself, along with any of its assets or trademarks.
- **Works created with this plugin and Unreal Engine** - licensing of those works is governed by your
  agreement with Epic Games and is unrelated to this plugin.

---

<div align="center">

**Xingyi** · by [StarGame](https://space.bilibili.com/177308205)

</div>
