# Release Version 1.2.0

- [x] Export/Import JunkList so it can be shared between game saves
- [x] Gamepad Support
- [-] View/Edit items currently in the JunkList through the MCM

## v1.2.0 Change Log

- Implemented functionality for importing and exporting the JunkList to an external file
- New setting to auto save the Junk List when ever the player saves their game. Default: disabled
- New setting to auto load the Junk List from the file when starting a new game. Default: disabled
- A "Replace JunkList On Import" option is available. If enabled the import will replace any current Junk markings with the imported list. If disabled the import will add to the current saves JunkList. Default: disabled
- Moved JunkList Utilities import, export, and reset to the Maintenance page
- Re-Added SKSE UI Pausing during item transfers and sales, but now it will force thaw the UI after 5 seconds if the operation has timed out
- If the UI is still frozen when the player presses one of the JunkIt keys it will force thaw the UI
- The forced UI Thaw in OnUpdate will also reset the busy state so you don't have to exit and re-enter the menu to continue using JunkIt keys
- Overhauled the SKSE junk toggle to provide better error handling and notifications for protected items such as Quest, Protected Equipped, and Protected Favorites
- There will now be a notification for when a transfer or sale starts processing. They are subject to the notification settings and will not appear if the transfer or sale notifications are disabled.
- Bulk Sales will now also show the Large Inventory Warning, if enabled
- Gamepad Support has been added. The default hotkey for GamePads is the "Start" button as it doesn't conflict with any other default menu controls.
- Since there is only one Gamepad hotkey, functionality for a 'short press' and 'long press' has been added. A short press will mark the item as junk, and a long press will trigger a bulk transfer or sale depending on the menu that is open. The "Hold Time" for a long press can be configured in the MCM. Defaults to 2 seconds, that's what felt most natural to me.
- A new "Junk List" page was added to the MCM where you can view and update any items you have marked or unmarked as Junk. Future updates will add additional quality of life features to this page.
- Safe to update mid-game but allow some time for the new Junk History to populate before opening the JunkIt MCM when you load in. It can take anywhere from a few seconds to almost a minute to process the update depending on how many items you have marked as Junk. I have around 1,500 marked items on my current play through and it takes about 30 seconds to fully process the update. A Notification message will let you know when it is completed.

## v1.2.0 Release Checklist

- [x] Update all translations files with newest version of the strings
- [x] Test edge cases with large transfers and sales to ensure the UI Thaw and Busy state reset are working as intended
- [x] Test the new Gamepad support with a variety of gamepads to ensure it's working as intended
- [x] Test the new Import/Export functionality to ensure it's working as intended
- [x] Test the new Auto Save/Load functionality to ensure it's working as intended
- [x] Test the new Replace JunkList On Import functionality to ensure it's working as intended
- [x] Test MCM OnVersionUpdate on a save with an older version of the mod to ensure the Junk History and notifications of the update are working as intended
- [x] Test New Game with no junk list json file to import from while auto import is enabled. Expected behavior is that the JunkList will be empty and there is no CTD
- [x] Remove large quantities of MCM Junk List debug output to the console
- [x] ~~Test Export, Import and Rest colored MCM strings~~
- [x] Final Code Review to ensure code quality and best practices
- [-] Package the mod for release
- [x] Write up all the changes in a change log for the Nexus page 

# Release Version 1.2.1

- [ ] Refactor Import JunkList Feature to optimize load speed when replacing the current JunkList
- [ ] Refactor Junk List Reset functionality to better optimize processing speed
- [ ] SKSE speed and reliability optimizations for item transfers
- [ ] Add a search input to the JunkList MCM page to make it easier to find items in the list
- [ ] Add an item type filter to the JunkList MCM page to make it easier to find items in the list
- [ ] Add a Refresh button to the JunkList MCM page to force a refresh after Resetting or Importing
- [ ] Switch from a form id config string to using the EditorID when saving and loading the JunkList from JSON. Should make it a bit more human readable and easier to manually edit if it strikes your fancy.

# Release Version 1.3.0

- [ ] Decrease in save bloat by removing reliance on keywords to get i4 to work
- [ ] Better control over menu icons so that protected items (equipped/favorited/enchanted) won't use the Junk icon (better user experience)
