# KenshiRotate

Inventory item rotation mod for Kenshi.

Rotate items in the grid-based inventory by hovering over them and pressing your configured rotation key (middle-click by default).

## Features

- Rotate any non-square item (e.g., turn a 7x1 katana into 1x7) to optimize inventory space
- Works on both grid-hover items and cursor-held items (cursor-held takes priority)
- Works in all inventory types: character, backpack, container
- True 90-degree texture rotation with correct icon rendering
- Charge bars (food, medkits) display correctly on rotated items
- Rotation state persists across save/load
- Configurable key binding with primary + optional secondary bind via Options > MODS tab
- Key conflict detection warns if your chosen key is already bound in-game
- Safe fallback if rotation doesn't fit: item reverts to original orientation
- Rotated and unrotated items cannot stack together during drag/drop (prevents visual desync); right-click quick-transfer bypasses this so stacks merge into the destination's rotation
- Items split from rotated stacks inherit the rotation
- Public C API for cross-mod integration (e.g., StackSort auto-sort with rotation)

## Requirements

- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) ([Nexus](https://www.nexusmods.com/kenshi/mods/847)) — includes KenshiLib

## Installation

1. Download the latest release
2. Copy the `KenshiRotate` folder into Kenshi's `mods\` directory
3. Launch Kenshi via the RE_Kenshi launcher

## Usage

Open any inventory and **middle-click** (default) on a non-square item to rotate it. The item's grid dimensions and icon will swap (e.g., a 2x1 item becomes 1x2). Press the rotation key again to rotate back.

Rotation works both on items in the grid (hover and press) and on items held on the cursor. Rotation is blocked if there isn't enough space for the rotated dimensions.

### Changing the rotation key

Go to **Options > MODS** tab and find the **KenshiRotate** section. Click **Change**, then press the key you want to use (any keyboard key or middle mouse button). Press **Escape** to cancel, or **Reset** to revert to middle mouse. Your binding is saved to `KenshiRotate.cfg` and persists across game restarts.

## Building from Source

Requires:
- Visual Studio 2010 v100 x64 toolset
- RE_Kenshi / KenshiLib headers and libraries

Link dependencies: `KenshiLib.lib`, `MyGUIEngine_x64.lib`, `OgreMain_x64.lib`, `user32.lib`

## How It Works

The plugin hooks eleven game functions via KenshiLib:

1. **`InventoryGUI::update`** — Detects the configured rotation key and performs the rotation (remove item, swap dimensions, re-add with validation). Also handles cursor-held rotation and caches hovered item position for grab offset correction.
2. **`InventoryIcon::_CONSTRUCTOR`** — Resizes icons and applies rotated textures when inventory icons are created. Detects cursor icons for cursor-held rotation tracking.
3. **`InventorySectionGUI::getBestPositionSlot`** — Fixes grid highlight bounds for rotated items
4. **`Item::serialiseInInventory`** — Saves rotation state into the item's GameData record
5. **`Item::loadFromSerialiseInInventory`** — Restores rotation state on load
6. **`OptionsWindow::update`** — Injects the key binding UI into the MODS tab and handles key capture
7. **`InventoryGUI::placeItemFromMouse`** — Tracks when cursor-held items are placed or swapped
8. **`InventoryItemBase::canStackWith`** — Prevents rotated and unrotated items from stacking together (bypassed during right-click quick-transfer)
9. **`InventoryGUI::takeCertainAmountFrom`** — Propagates rotation state when splitting stacks
10. **`InventoryItemBase::addQuantity`** — Defense-in-depth block on cross-rotation quantity transfer (bypassed during right-click quick-transfer)
11. **`InventoryGUI::sectionMouseButtonReleased`** — Marks the right-click quick-transfer window so hooks 8 and 10 allow the merge

Rotation state is persisted by injecting a custom boolean field into each item's save record. No external state files are needed — rotation data travels with the item through Kenshi's native save system.

### Compatibility

All rotations happen at runtime - the plugin doesn't modify any game data or item templates. This means it's compatible with saves made before the mod was installed and with mods that add new items.

### Removing the mod

If you uninstall KenshiRotate, all previously rotated items will revert to their default orientation on the next load. Items that overlap or no longer fit in your inventory after reverting will be dropped or equipped.

## Known Limitations

None.

## Bug Reports

If you run into a problem, [open an issue](../../issues/new) and include:

- **Other mods installed** — list of all mods you have enabled
- **RE_Kenshi version** — shown in the launcher or in `RE_Kenshi_log.txt`
- **RE_Kenshi_log.txt** — found in your Kenshi game directory

## License

GPL-3.0 - see [LICENSE](LICENSE) for details.
