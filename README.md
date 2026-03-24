# KenshiRotate

Inventory item rotation mod for Kenshi.

Rotate items in the grid-based inventory by middle-clicking while hovering over them.

## Features

- Rotate any non-square item (e.g., turn a 7x1 katana into 1x7) to optimize inventory space
- Works in all inventory types: character, backpack, container
- True 90-degree texture rotation with correct icon rendering
- Charge bars (food, medkits) display correctly on rotated items
- Rotation state persists across save/load
- Safe fallback if rotation doesn't fit: item reverts to original orientation or is dropped if no longer fits.

## Requirements

- [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) ([Nexus](https://www.nexusmods.com/kenshi/mods/847)) — includes KenshiLib

## Installation

1. Download the latest release
2. Copy the `KenshiRotate` folder into Kenshi's `mods\` directory
3. Launch Kenshi via the RE_Kenshi launcher

## Usage

Open any inventory and **middle-click** on a non-square item to rotate it. The item's grid dimensions and icon will swap (e.g., a 2x1 item becomes 1x2). Middle-click again to rotate back.

Rotation is blocked if there isn't enough space for the rotated dimensions.

## Building from Source

Requires:
- Visual Studio 2010 v100 x64 toolset
- RE_Kenshi / KenshiLib headers and libraries

Link dependencies: `KenshiLib.lib`, `MyGUIEngine_x64.lib`, `OgreMain_x64.lib`, `user32.lib`

## How It Works

The plugin hooks five game functions via KenshiLib:

1. **`InventoryGUI::update`** - Detects middle-click input and performs the rotation (remove item, swap dimensions, re-add with validation)
2. **`InventoryIcon::_CONSTRUCTOR`** - Resizes icons and applies rotated textures when inventory icons are created
3. **`InventorySectionGUI::getBestPositionSlot`** - Fixes grid highlight bounds for rotated items
4. **`Item::serialiseInInventory`** - Saves rotation state into the item's GameData record
5. **`Item::loadFromSerialiseInInventory`** - Restores rotation state on load

Rotation state is persisted by injecting a custom boolean field into each item's save record. No external state files are needed - rotation data travels with the item through Kenshi's native save system.

### Compatibility

All rotations happen at runtime - the plugin doesn't modify any game data or item templates. This means it's compatible with saves made before the mod was installed and with mods that add new items.

### Removing the mod

If you uninstall KenshiRotate, all previously rotated items will revert to their default orientation on the next load. Items that overlap or no longer fit in your inventory after reverting will be dropped or equipped.

## Known Limitations

- Rotation only works on hover, not while holding an item on the cursor
- Stacking a rotated item (e.g., food) into a non-rotated stack causes the entire stack to appear rotated until the rotated item is removed from the stack
- Loading a save, then loading a different save without saving in between, can leave stale rotation tracking from the first save. In theory this could cause an item to falsely appear rotated, but the odds of a handle collision are incredibly small (handles include index, serial, container, and container serial fields that would all need to match)

## License

GPL-3.0 - see [LICENSE](LICENSE) for details.
