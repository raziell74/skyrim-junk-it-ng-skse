Scriptname JunkIt_MCM extends MCM_ConfigBase

;--- JunkIt Properties --------------------------------------------------------------

Actor Property PlayerRef Auto
GlobalVariable Property MarkJunkKey Auto
GlobalVariable Property TransferJunkKey Auto
GlobalVariable Property GamepadJunkKey Auto
GlobalVariable Property GamepadTransferHoldTime Auto

GlobalVariable Property ConfirmTransfer Auto
GlobalVariable Property ConfirmSell Auto

GlobalVariable Property TransferPriority Auto
GlobalVariable Property SellPriority Auto

GlobalVariable Property ProtectEquipped Auto
GlobalVariable Property ProtectFavorites Auto
GlobalVariable Property ProtectEnchanted Auto

GlobalVariable Property NotifyOnMarkUnmark Auto
GlobalVariable Property NotifyOnJunkTransfer Auto
GlobalVariable Property NotifyOnJunkSell Auto
GlobalVariable Property NotifyLargeInventoryLag Auto

GlobalVariable Property AutoExport Auto
GlobalVariable Property AutoImport Auto

GlobalVariable Property UpdateItemIcon Auto
GlobalVariable Property UpdateSubTypeDisplay Auto
GlobalVariable Property UseDynamicInventoryIcon Auto

Message Property TransferConfirmationMsg Auto
Message Property RetrievalConfirmationMsg Auto
Message Property SellConfirmationMsg Auto
Message Property UpdateMessage Auto

MiscObject Property Gold001 Auto

;--- JunkIt Non Property MCM Variables ----------------------------------------------

Int WarnInventorySizeThreshold = 500

Bool UIFrozen = False

;--- JunkIt Private Variables -------------------------------------------------------

Bool migrated = False
String plugin = "JunkIt.esp"

Int _page = 0
Int _totalPages = 0
Int _itemsPerPage = 40

Bool bAggressiveRefresh = False
Int iAggressiveRefreshMaxInterval = 10
Int iAggressiveUpdateTimer = 0

; --- JunkIt.dll Native Functions ---------------------------------------------------

Function RefreshDllSettings() global native
Form Function ToggleSelectedAsJunk() global native
Function RefreshUIIcons() global native

Int Function GetContainerMode() global native
Int Function GetMenuItemValue(Form a_form) global native

Bool Function IsItemJunk(Form a_form) global native
Int Function GetJunkListSize() global native
String Function GetJunkItemNameAt(Int index) global native
Bool Function RemoveJunkItemAtIndex(Int index) global native
Function ClearAllJunk() global native

Bool Function SaveJunkListToFile() global native
Bool Function LoadJunkListFromFile(Bool replace) global native

; --- MCM Helper Functions ----------------------------------------------------------

; GetVersion
; Returns the version of the MCM Helper
;
; @returns  Int  the version of the MCM Helper
Int Function GetVersion()
    return 2 ;MCM Helper
EndFunction

; OnVersionUpdate
; Event called when the MCM Helper version is updated
;
; @param aVersion Int  the new version of the MCM Helper
; @returns  None
Event OnVersionUpdate(int aVersion)
	parent.OnVersionUpdate(aVersion)
    VerboseMessage("MCM Successfully Updated to the latest version", True)
    RefreshMenu()
EndEvent

; OnModConfigMenuOpen
; Event called periodically if the active magic effect/alias/form is registered for update events. This event will not be sent if the game is in menu mode. 
;
; @returns  None
Event OnUpdate()
    parent.OnUpdate()
    If !migrated
        MigrateToMCMHelper()
        migrated = True
        VerboseMessage("OnUpdate: Settings imported!", True)
    EndIf

    ; Aggresive Refresh will refresh the UIIcons every 5 seconds up to the max interval
    If bAggressiveRefresh
        RefreshUIIcons()
        If iAggressiveUpdateTimer < iAggressiveRefreshMaxInterval
            iAggressiveUpdateTimer += 5
            RegisterForSingleUpdate(5.0)
        EndIf
    EndIf
EndEvent

; OnGameReload
; Event called when the game is reloaded
;
; @returns  None
Event OnGameReload()
    parent.OnGameReload()
    If !migrated
        MigrateToMCMHelper()
        migrated = True
        VerboseMessage("OnGameReload: Settings imported!", True)
    EndIf
    ;If GetModSettingBool("bLoadSettingsonReload:Maintenance")
    ;    LoadSettings()
    ;    VerboseMessage("OnGameReload: Settings autoloaded!", True)
    ;EndIf

    LoadSettings()
EndEvent

; OnPlayerLoadGame
; Event is only sent to the player actor. This would probably be on a magic effect or alias script
;
; @returns  None
Event OnPlayerLoadGame()
    VerboseMessage("OnPlayerLoadGame: Applying keyword corrections")
endEvent

; OnConfigInit
; Called when this config menu is initialized.
;
; @returns  None
Event OnConfigInit()
    parent.OnConfigInit()
    migrated = True
    LoadSettings()
EndEvent

; OnConfigOpen
; Called when this config menu is opened.
;
; @returns  None
Event OnConfigOpen()
    parent.OnConfigOpen()
    If !migrated
        MigrateToMCMHelper()
        migrated = True
        VerboseMessage("OnConfigOpen: Settings imported!", True)
    EndIf
EndEvent

; OnPageSelect
; Event called when the player selects a page in the MCM
;
; @param a_page String  the name of the page
; @returns  None
Event OnPageSelect(String a_page)
    parent.OnPageSelect(a_page)

    SetModSettingString("sResetJunk:Utility", "$JunkIt_ResetJunk")
    
    ; Prep the Junk List Page with the first page of items
    _page = 0
    _totalPages = (GetJunkListSize() / _itemsPerPage) + 1
    SetModSettingString("sNextPage:JunkList", "$JunkIt_NextPage")
    SetModSettingString("sPreviousPage:JunkList", "$JunkIt_PreviousPage")
    SetModSettingString("sNextPage2:JunkList", "$JunkIt_NextPage")
    SetModSettingString("sPreviousPage2:JunkList", "$JunkIt_PreviousPage")
    JunkListPageUpdate()

    RefreshMenu()
EndEvent

; OnSettingChange
; Called when a setting is changed in the MCM
;
; @param a_ID String  the ID of the setting
; @returns  None
Event OnSettingChange(String a_ID)
    ; Hotkey Settings
    If a_ID == "iJunkKey:Hotkey"
        MarkJunkKey.SetValue(GetModSettingInt(a_ID) as Float)
        RefreshMenu()
    ElseIf a_ID == "iTransferJunkKey:Hotkey"
        TransferJunkKey.SetValue(GetModSettingInt(a_ID) as Float)
        RefreshMenu()
    ElseIf a_ID == "iGamepadJunkKey:Hotkey"
        GamepadJunkKey.SetValue(GetModSettingInt(a_ID) as Float)
        RefreshMenu()
    ElseIf a_ID == "iGamepadTransferHoldTime:Hotkey"
        GamepadTransferHoldTime.SetValue(GetModSettingInt(a_ID) as Float)
    
    ; Confirmation Settings
    ElseIf a_ID == "bConfirmTransfer:Confirmation"
        ConfirmTransfer.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bConfirmSell:Confirmation"
        ConfirmSell.SetValue(GetModSettingBool(a_ID) as Float)
    
    ; Bulk Action Priority Settings
    ElseIf a_ID == "iTransferPriority:Priority"
        TransferPriority.SetValue(GetModSettingInt(a_ID) as Float)
    ElseIf a_ID == "iSellPriority:Priority"
        SellPriority.SetValue(GetModSettingInt(a_ID) as Float)

    ; Protection Settings
    ElseIf a_ID == "bProtectEquipped:Protection"
        ProtectEquipped.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bProtectFavorites:Protection"
        ProtectFavorites.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bProtectEnchanted:Protection"
        ProtectEnchanted.SetValue(GetModSettingBool(a_ID) as Float)

    ; Misc Settings
    ElseIf a_ID == "bNotifyOnMarkUnmark:MiscSettings"
        NotifyOnMarkUnmark.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bNotifyOnJunkTransfer:MiscSettings"
        NotifyOnJunkTransfer.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bNotifyOnJunkSell:MiscSettings"
        NotifyOnJunkSell.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bNotifyLargeInventoryLag:MiscSettings"
        NotifyLargeInventoryLag.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "iWarnInventorySizeThreshold:MiscSettings"
        WarnInventorySizeThreshold = GetModSettingInt(a_ID)
    ElseIf a_ID == "bAggressiveRefresh:Utility"
        bAggressiveRefresh = GetModSettingBool(a_ID)
    ElseIf a_ID == "iAggressiveRefreshMaxInterval:Utility"
        iAggressiveRefreshMaxInterval = GetModSettingInt(a_ID)
    ElseIf a_ID == "bAutoSaveJunkListToFile:Maintenance"
        AutoExport.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bAutoLoadJunkListFromFile:Maintenance"
        AutoImport.SetValue(GetModSettingBool(a_ID) as Float)

    ; Integration Settings
    ElseIf a_ID == "bUpdateItemIcon:IntegrationSettings"
        UpdateItemIcon.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bUpdateSubTypeDisplay:IntegrationSettings"
        UpdateSubTypeDisplay.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bUseDynamicInventoryIcon:IntegrationSettings"
        UseDynamicInventoryIcon.SetValue(GetModSettingBool(a_ID) as Float)
    
    EndIf

    RefreshDllSettings()
EndEvent

; Default
; Resets the settings to their default values
;
; @returns  None
Function Default()
    ; Hotkey Settings
    SetModSettingInt("iJunkKey:Hotkey", 50)
    SetModSettingInt("iTransferJunkKey:Hotkey", 49)
    SetModSettingInt("iGamepadJunkKey:Hotkey", 270)
    SetModSettingInt("iGamepadTransferHoldTime:Hotkey", 2)

    ; Confirmation Settings
    SetModSettingBool("bConfirmTransfer:Confirmation", True)
    SetModSettingBool("bConfirmSell:Confirmation", True)
    
    ; Bulk Action Priority Settings
    SetModSettingInt("iTransferPriority:Priority", 0)
    SetModSettingInt("iSellPriority:Priority", 4)

    ; Protection Settings
    SetModSettingBool("bProtectEquipped:Protection", True)
    SetModSettingBool("bProtectFavorites:Protection", True)
    SetModSettingBool("bProtectEnchanted:Protection", False)

    ; Misc Settings
    SetModSettingBool("bNotifyOnMarkUnmark:MiscSettings", True)
    SetModSettingBool("bNotifyOnJunkTransfer:MiscSettings", True)
    SetModSettingBool("bNotifyOnJunkSell:MiscSettings", True)
    SetModSettingBool("bNotifyLargeInventoryLag:MiscSettings", True)
    SetModSettingInt("iWarnInventorySizeThreshold:MiscSettings", 500)
    WarnInventorySizeThreshold = 500
    SetModSettingBool("bAggressiveRefresh:Utility", False)
    SetModSettingInt("iAggressiveRefreshMaxInterval:Utility", 10)

    ; Integration Settings
    SetModSettingBool("bUpdateItemIcon:IntegrationSettings", True)
    SetModSettingBool("bUpdateSubTypeDisplay:IntegrationSettings", True)
    SetModSettingBool("bUseDynamicInventoryIcon:IntegrationSettings", True)

    ; Maintenance Settings
    SetModSettingBool("bEnabled:Maintenance", True)
    SetModSettingInt("iLoadingDelay:Maintenance", 0)
    SetModSettingBool("bLoadSettingsonReload:Maintenance", False)
    SetModSettingBool("bVerbose:Maintenance", False)
    SetModSettingBool("bAutoSaveJunkListToFile:Maintenance", False)
    SetModSettingBool("bAutoLoadJunkListFromFile:Maintenance", False)
    
    VerboseMessage("Settings reset!", True)
    Load()
EndFunction

; Load
; Loads the settings from the MCM
;
; @returns  None
Function Load()
    ; Hotkey Settings
    MarkJunkKey.SetValue(GetModSettingInt("iJunkKey:Hotkey") as Float)
    TransferJunkKey.SetValue(GetModSettingInt("iTransferJunkKey:Hotkey") as Float)

    ; Gamepad Hotkey Settings
    GamepadJunkKey.SetValue(GetModSettingInt("iGamepadJunkKey:Hotkey") as Float)
    GamepadTransferHoldTime.SetValue(GetModSettingInt("iGamepadTransferHoldTime:Hotkey") as Float)

    ; Confirmation Settings
    ConfirmTransfer.SetValue(GetModSettingBool("bConfirmTransfer:Confirmation") as Float)
    ConfirmSell.SetValue(GetModSettingBool("bConfirmSell:Confirmation") as Float)
    
    ; Bulk Action Priority Settings
    TransferPriority.SetValue(GetModSettingInt("iTransferPriority:Priority") as Float)
    SellPriority.SetValue(GetModSettingInt("iSellPriority:Priority") as Float)

    ; Protection Settings
    ProtectEquipped.SetValue(GetModSettingBool("bProtectEquipped:Protection") as Float)
    ProtectFavorites.SetValue(GetModSettingBool("bProtectFavorites:Protection") as Float)
    ProtectEnchanted.SetValue(GetModSettingBool("bProtectEnchanted:Protection") as Float)

    ; Misc Settings
    NotifyOnMarkUnmark.SetValue(GetModSettingBool("bNotifyOnMarkUnmark:MiscSettings") as Float)
    NotifyOnJunkTransfer.SetValue(GetModSettingBool("bNotifyOnJunkTransfer:MiscSettings") as Float)
    NotifyOnJunkSell.SetValue(GetModSettingBool("bNotifyOnJunkSell:MiscSettings") as Float)
    NotifyLargeInventoryLag.SetValue(GetModSettingBool("bNotifyLargeInventoryLag:MiscSettings") as Float)
    WarnInventorySizeThreshold = GetModSettingInt("iWarnInventorySizeThreshold:MiscSettings")
    bAggressiveRefresh = GetModSettingBool("bAggressiveRefresh:Utility")
    iAggressiveRefreshMaxInterval = GetModSettingInt("iAggressiveRefreshMaxInterval:Utility")
    
    ; Maintenance Settings
    AutoExport.SetValue(GetModSettingBool("bAutoSaveJunkListToFile:Maintenance") as Float)
    AutoImport.SetValue(GetModSettingBool("bAutoLoadJunkListFromFile:Maintenance") as Float)

    ; Integration Settings
    UpdateItemIcon.SetValue(GetModSettingBool("bUpdateItemIcon:IntegrationSettings") as Float)
    UpdateSubTypeDisplay.SetValue(GetModSettingBool("bUpdateSubTypeDisplay:IntegrationSettings") as Float)
    UseDynamicInventoryIcon.SetValue(GetModSettingBool("bUseDynamicInventoryIcon:IntegrationSettings") as Float)

    RefreshDllSettings()
    VerboseMessage("Settings applied!", True)
EndFunction

; LoadSettings
; Load on game reload if enabled in the MCM
;
; @returns  None
Function LoadSettings()
    If GetModSettingBool("bEnabled:Maintenance") == false
        return
    EndIf
    Utility.Wait(GetModSettingInt("iLoadingDelay:Maintenance"))
    VerboseMessage("Settings autoloaded!", True)
    Load()
EndFunction

; MigrateToMCMHelper
; Migrates settings from the old MCM to the MCM Helper
;
; @returns  None
Function MigrateToMCMHelper()
    ; Hotkey Settings
    SetModSettingInt("iJunkKey:Hotkey", MarkJunkKey.GetValue() as Int)
    SetModSettingInt("iTransferJunkKey:Hotkey", TransferJunkKey.GetValue() as Int)
    SetModSettingInt("iGamepadJunkKey:Hotkey", GamepadJunkKey.GetValue() as Int)
    SetModSettingInt("iGamepadTransferHoldTime:Hotkey", GamepadTransferHoldTime.GetValue() as Int)

    ; Confirmation Settings
    SetModSettingBool("bConfirmTransfer:Confirmation", ConfirmTransfer.GetValue() as Bool)
    SetModSettingBool("bConfirmSell:Confirmation", ConfirmSell.GetValue() as Bool)
    
    ; Bulk Action Priority Settings
    SetModSettingInt("iTransferPriority:Priority", TransferPriority.GetValue() as Int)
    SetModSettingInt("iSellPriority:Priority", SellPriority.GetValue() as Int)

    ; Protection Settings
    SetModSettingBool("bProtectEquipped:Protection", ProtectEquipped.GetValue() as Bool)
    SetModSettingBool("bProtectFavorites:Protection", ProtectFavorites.GetValue() as Bool)
    SetModSettingBool("bProtectEnchanted:Protection", ProtectEnchanted.GetValue() as Bool)

    ; Misc Settings
    SetModSettingBool("bNotifyOnMarkUnmark:MiscSettings", NotifyOnMarkUnmark.GetValue() as Bool)
    SetModSettingBool("bNotifyOnJunkTransfer:MiscSettings", NotifyOnJunkTransfer.GetValue() as Bool)
    SetModSettingBool("bNotifyOnJunkSell:MiscSettings", NotifyOnJunkSell.GetValue() as Bool)
    SetModSettingBool("bNotifyLargeInventoryLag:MiscSettings", NotifyLargeInventoryLag.GetValue() as Bool)
    SetModSettingInt("iWarnInventorySizeThreshold:MiscSettings", WarnInventorySizeThreshold)
    SetModSettingBool("bAggressiveRefresh:Utility", bAggressiveRefresh)
    SetModSettingInt("iAggressiveRefreshMaxInterval:Utility", iAggressiveRefreshMaxInterval)

    ; Integration Settings
    SetModSettingBool("bUpdateItemIcon:IntegrationSettings", UpdateItemIcon.GetValue() as Bool)
    SetModSettingBool("bUpdateSubTypeDisplay:IntegrationSettings", UpdateSubTypeDisplay.GetValue() as Bool)
    SetModSettingBool("bUseDynamicInventoryIcon:IntegrationSettings", UseDynamicInventoryIcon.GetValue() as Bool)

    ; Maintenance Settings
    SetModSettingBool("bAutoLoadJunkListFromFile:Maintenance", AutoImport.GetValue() as Bool)
    SetModSettingBool("bAutoSaveJunkListToFile:Maintenance", AutoExport.GetValue() as Bool)
    SetModSettingBool("bReplaceJunkListOnLoad:Utility", False)

EndFunction

; JunkListPageUpdate
; Updates the Junk List page with the current items
;
; @returns  None
Function JunkListPageUpdate()
    Int i = _page * _itemsPerPage
    Int optionIndex = 0
    Int iTotal = i + _itemsPerPage
    Int junkListCount = GetJunkListSize()

    ; Should only enable the next page button if the _page is less than the _totalPages
    If _page < _totalPages - 1
        SetModSettingInt("iNextPageToggle:Hidden", 1)
    Else
        SetModSettingInt("iNextPageToggle:Hidden", 0)
    Endif

    ; Should only enable the previous page button if the _page is greater than 0
    If _page > 0
        SetModSettingInt("iPreviousPageToggle:Hidden", 1)
    Else
        SetModSettingInt("iPreviousPageToggle:Hidden", 0)
    Endif

    SetModSettingString("sPageCount:JunkList", "<font color='#9498B3'>Page " + (_page + 1) + " of " + _totalPages + "</font>")
    SetModSettingString("sPageCount2:JunkList", "<font color='#9498B3'>Page " + (_page + 1) + " of " + _totalPages + "</font>")

    ; Go through our junk list and update the pages itemSlots with the current items
    While i < iTotal && i < junkListCount && optionIndex < _itemsPerPage
        String name = GetJunkItemNameAt(i)
        String o_ID = "sItem" + (optionIndex + 1) + ":JunkList"
        
        ; All items in the list are junk
        name = FormatJunkItemName(name, "junk")

        ; Update the item slot with the formatted item name
        SetModSettingString(o_ID, name as String)

        i += 1
        optionIndex += 1
    EndWhile

    ; Clear any unused item slots
    If optionIndex < _itemsPerPage
        While optionIndex < _itemsPerPage
            String o_ID = "sItem" + (optionIndex + 1) + ":JunkList"
            SetModSettingString(o_ID, "")
            optionIndex += 1
        EndWhile
    EndIf
EndFunction

; MCMJunkListPreviousPage
; Moves the Junk List page to the previous page
;
; @returns  None
Function MCMJunkListPreviousPage()
    If _page > 0
        _page = _page - 1
        JunkListPageUpdate()
        RefreshMenu()
    EndIf 
EndFunction

; MCMJunkListNextPage
; Moves the Junk List page to the next page
;
; @returns  None
Function MCMJunkListNextPage()
    If _page < _totalPages - 1
        _page = _page + 1
        JunkListPageUpdate()
        RefreshMenu()
    EndIf
EndFunction

; MCMToggleJunkItem
; Toggles the junk status of an item in the Junk List
;
; @param index Int  the index of the item in the Junk List
; @returns  None
Function MCMToggleJunkItem(Int index)
    ; o_ID is the sItem slot on the page. 
    ; The MCMHelper config.json predefines 40 of these on the Junk List page
    String o_ID = "sItem" + (index + 1) + ":JunkList"
    String itemSlotText = GetModSettingString(o_ID)

    ; Do nothing if the item slot is empty
    If itemSlotText == ""
        return
    EndIf
    
    ; Get the actual item index in the full list
    Int actualIndex = index + (_page * _itemsPerPage)
    String name = GetJunkItemNameAt(actualIndex)

    String UpdatingText = FormatJunkItemName(name, "updating")
    SetModSettingString(o_ID, UpdatingText)
    RefreshMenu()

    ; Remove the item from junk (since all items in the list are junk)
    RemoveJunkItemAtIndex(actualIndex)

    ; Update the page to reflect the removal
    JunkListPageUpdate()

    ; Prevent text flicker by waiting a second before updating the UI again
    Utility.WaitMenuMode(0.5)
    RefreshMenu()
EndFunction

; FormatJunkItemName
; MCM Junk Item Name Formatting Utility
;
; @param name String  the name of the item
; @param status String  Enum { "updating", "junk", "not junk" }  the status of the item
; @returns  String  the formatted item name
String Function FormatJunkItemName(String name, String status)
    If status == "updating"
        name = "<font color='#30DEDF'>[Updating]</font> <font color='#675151'>" + name + "</font>"
    ElseIf status == "junk"
        name = "<font color='#71C56C'>[Junk]</font> " + name
    ElseIf status == "not junk"
        name = "<font color='#C58975'>[Not Junk]</font> <font color='#7D7D7D'>" + name + "</font>"
    EndIf

    return name
EndFunction

; TriggerSaveJunkListToFile
; Exports the current junk list to a JSON file
;
; @returns  None
Function TriggerSaveJunkListToFile()
    SetModSettingString("sSaveJunkListToFile:Utility", "$JunkIt_SavingJunkList")
    RefreshMenu()

    Bool success = SaveJunkListToFile()

    If success
        SetModSettingString("sSaveJunkListToFile:Utility", "$JunkIt_JunkSaved")
        VerboseMessage("Junk list exported successfully!", True)
    Else
        SetModSettingString("sSaveJunkListToFile:Utility", "$JunkIt_SaveJunkListToFile")
        VerboseMessage("Failed to export junk list", True)
    EndIf
    RefreshMenu()
EndFunction

; TriggerLoadJunkListFromFile
; Imports a junk list from a JSON file
;
; @returns  None
Function TriggerLoadJunkListFromFile()
    SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_LoadingJunkList")
    RefreshMenu()

    Bool bReplace = GetModSettingBool("bReplaceJunkListOnLoad:Utility")
    Bool success = LoadJunkListFromFile(bReplace)

    If success
        If bReplace
            SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_JunkReplaced")
            VerboseMessage("Junk list replaced with imported list!", True)
        Else
            SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_JunkLoaded")
            VerboseMessage("Junk list merged with imported list!", True)
        EndIf
    Else
        SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_LoadJunkListFromFile")
        VerboseMessage("Failed to import junk list", True)
    EndIf
    RefreshMenu()
EndFunction

; ResetJunk
; Resets the junk list
;
; @returns  None
Function ResetJunk()
    VerboseMessage("Resetting Junk List...")
    SetModSettingString("sResetJunk:Utility", "$JunkIt_ResetingJunk")
    RefreshMenu()

    ClearAllJunk()

    VerboseMessage("Junk List reset!", True)
    VerboseMessage("Junk List size after reset: 0")

    SetModSettingString("sResetJunk:Utility", "$JunkIt_JunkReset")
    RefreshMenu()
EndFunction

; VerboseMessage
; If DebugMode is enabled, logs a message to the console and the papyrus logging output.
; If VerboseMode is enabled, logs are also sent to a player notification.
;
; @param m String  the message to log
; @param displayNotification Bool  whether to display a notification
; @returns  None
Function VerboseMessage(String m, Bool displayNotification = False)
    If GetModSettingBool("bDebug:Maintenance")
        Debug.Trace("JunkIt - " + m)
        MiscUtil.PrintConsole("JunkIt - " + m)
    EndIf

    If GetModSettingBool("bVerbose:Maintenance") && displayNotification
        Debug.Notification("JunkIt - " + m)
    EndIf
EndFunction
