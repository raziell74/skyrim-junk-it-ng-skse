Scriptname JunkIt_MCM extends MCM_ConfigBase

;--- JunkIt Properties --------------------------------------------------------------

Actor Property PlayerRef Auto
Keyword Property IsJunkKYWD Auto
FormList Property JunkList Auto
FormList Property UnjunkedList Auto
FormList Property JunkHistory Auto

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

GlobalVariable Property AutoLoadJunkListFromFile Auto
GlobalVariable Property AutoSaveJunkListToFile Auto
GlobalVariable Property ReplaceJunkListOnLoad Auto

Message Property TransferConfirmationMsg Auto
Message Property RetrievalConfirmationMsg Auto
Message Property SellConfirmationMsg Auto
Message Property UpdateMessage Auto

MiscObject Property Gold001 Auto

;--- JunkIt Non Property MCM Variables ----------------------------------------------

Int UserJunkKey = 50
Int UserTransferKey = 49

Int WarnInventorySizeThreshold = 500

Bool UIFrozen = False

;--- JunkIt Private Variables -------------------------------------------------------

Bool migrated = False
String plugin = "JunkIt.esp"

String ActiveMenu = ""

Int _page = 0
Int _totalPages = 0
Int _itemsPerPage = 40

Bool bAggressiveRefresh = False
Int iAggressiveRefreshMaxInterval = 10
Int iAggressiveUpdateTimer = 0

; --- JunkIt.dll Native Functions ---------------------------------------------------

Function RefreshDllSettings() global native

Form Function ToggleSelectedAsJunk() global native
Int Function AddJunkKeyword(Form a_form) global native
Int Function RemoveJunkKeyword(Form a_form) global native
Function RefreshUIIcons() global native

Int Function GetContainerMode() global native
ObjectReference Function GetContainerMenuContainer() global native
ObjectReference Function GetBarterMenuContainer() global native
ObjectReference Function GetBarterMenuMerchantContainer() global native
Int Function GetMenuItemValue(Form a_form) global native

FormList Function GetTransferFormList() global native
FormList Function GetSellFormList() global native

Function SaveJunkListToFile() global native
FormList Function LoadJunkListFromFile() global native
Function UpdateItemKeywords() global native

; [Experimental]

Int Function ProcessItemListTransfer(FormList a_itemList, ObjectReference a_fromContainer, ObjectReference a_toContainer, Int a_isBarter) global native
Int Function GetContainerItemListCount(ObjectReference a_container, FormList a_itemList) global native
Int Function GetContainerSingleItemCount(ObjectReference a_container, Form a_item) global native

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
    
    ; JunkHistory was introduced in Version 1.2.0 ~ This code will run the update to fill out the new formlist
    ; Check if there is no JunkHistory but there are items in the JunkList or UnjunkedList
    If JunkHistory.GetSize() == 0 && (JunkList.GetSize() > 0 || UnjunkedList.GetSize() > 0)
        Utility.Wait(10.0)
        Debug.Notification("Updating to Version 1.2.0 - Please wait for the update to complete before opening the MCM.")
        ; UpdateMessage.Show(1.20)

        ; iterate through the UnjunkedList and add the items from the junk history list
        Int i = 0
        Int iTotal = UnjunkedList.GetSize()
        While i < iTotal
            Form item = UnjunkedList.GetAt(i)
            JunkHistory.AddForm(item)
            i += 1
        EndWhile

        ; iterate through the junk list and add the items to the junk history list
        i = 0
        iTotal = JunkList.GetSize()
        While i < iTotal
            Form item = JunkList.GetAt(i)
            JunkHistory.AddForm(item)
            i += 1
        EndWhile

        Int JunkHistoryCount = JunkHistory.GetSize()
        Debug.Notification("JunkIt Version 1.2.0 - Junk History now tracking " + JunkHistoryCount + " items")
    EndIf

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

    RegisterForMenu("InventoryMenu")
    RegisterForMenu("ContainerMenu")
    RegisterForMenu("BarterMenu")

    UserJunkKey = MarkJunkKey.GetValue() as Int
    UserTransferKey = TransferJunkKey.GetValue() as Int

    If UserJunkKey != -1
        RegisterForKey(UserJunkKey)
    EndIf

    If UserTransferKey != -1
        RegisterForKey(UserTransferKey)
    EndIf

    If GamepadJunkKey.GetValue() != -1
        RegisterForKey(GamepadJunkKey.GetValue() as Int)
    EndIf

    If (AutoLoadJunkListFromFile.GetValue() == 1)
        Utility.Wait(GetModSettingInt("iLoadingDelay:Maintenance") + 10.0)
        TriggerLoadJunkListFromFile()
        Debug.Notification("JunkIt - Junk List Auto-Imported!")
    EndIf
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
    SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_LoadJunkListFromFile")
    SetModSettingString("sSaveJunkListToFile:Utility", "$JunkIt_SaveJunkListToFile")
    
    ; Prep the Junk List Page with the first page of items
    _page = 0
    _totalPages = (JunkHistory.GetSize() / _itemsPerPage) + 1
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
        UnregisterForKey(UserJunkKey)
        UserJunkKey = GetModSettingInt(a_ID)
        RegisterForKey(UserJunkKey)
        MarkJunkKey.SetValue(UserJunkKey as Float)
        RefreshMenu()
    ElseIf a_ID == "iTransferJunkKey:Hotkey"
        UnregisterForKey(UserTransferKey)
        UserTransferKey = GetModSettingInt(a_ID)
        RegisterForKey(UserTransferKey)
        TransferJunkKey.SetValue(UserTransferKey as Float)
        RefreshMenu()
    ElseIf a_ID == "iGamepadJunkKey:Hotkey"
        UnregisterForKey(GamepadJunkKey.GetValue() as Int)
        GamepadJunkKey.SetValue(GetModSettingInt(a_ID) as Float)
        RegisterForKey(GetModSettingInt(a_ID))
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

    ; Export / Import Settings
    ElseIf a_ID == "bAutoLoadJunkListFromFile:Maintenance"
        AutoLoadJunkListFromFile.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bAutoSaveJunkListToFile:Maintenance"
        AutoSaveJunkListToFile.SetValue(GetModSettingBool(a_ID) as Float)
    ElseIf a_ID == "bReplaceJunkListOnLoad:Utility"
        ReplaceJunkListOnLoad.SetValue(GetModSettingBool(a_ID) as Float)
    
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

    ; Maintenance Settings
    SetModSettingBool("bEnabled:Maintenance", True)
    SetModSettingInt("iLoadingDelay:Maintenance", 0)
    SetModSettingBool("bLoadSettingsonReload:Maintenance", False)
    SetModSettingBool("bVerbose:Maintenance", False)
    SetModSettingBool("bAutoSaveJunkListToFile:Maintenance", False)
    SetModSettingBool("bAutoLoadJunkListFromFile:Maintenance", False)
    SetModSettingBool("bReplaceJunkListOnLoad:Utility", False)
    
    VerboseMessage("Settings reset!", True)
    Load()
EndFunction

; Load
; Loads the settings from the MCM
;
; @returns  None
Function Load()
    ; Hotkey Settings
    UnregisterForKey(UserJunkKey)
    UserJunkKey = GetModSettingInt("iJunkKey:Hotkey")
    MarkJunkKey.SetValue(UserJunkKey as Float)
    RegisterForKey(UserJunkKey)

    UnregisterForKey(UserTransferKey)
    UserTransferKey = GetModSettingInt("iTransferJunkKey:Hotkey")
    TransferJunkKey.SetValue(UserTransferKey as Float)
    RegisterForKey(UserTransferKey)

    ; Gamepad Hotkey Settings
    UnregisterForKey(GamepadJunkKey.GetValue() as Int)
    GamepadJunkKey.SetValue(GetModSettingInt("iGamepadJunkKey:Hotkey") as Float)
    RegisterForKey(GetModSettingInt("iGamepadJunkKey:Hotkey"))

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
    AutoLoadJunkListFromFile.SetValue(GetModSettingBool("bAutoLoadJunkListFromFile:Maintenance") as Float)
    AutoSaveJunkListToFile.SetValue(GetModSettingBool("bAutoSaveJunkListToFile:Maintenance") as Float)
    ReplaceJunkListOnLoad.SetValue(GetModSettingBool("bReplaceJunkListOnLoad:Utility") as Float)

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

    ; Maintenance Settings
    SetModSettingBool("bAutoLoadJunkListFromFile:Maintenance", AutoLoadJunkListFromFile.GetValue() as Bool)
    SetModSettingBool("bAutoSaveJunkListToFile:Maintenance", AutoSaveJunkListToFile.GetValue() as Bool)
    SetModSettingBool("bReplaceJunkListOnLoad:Utility", ReplaceJunkListOnLoad.GetValue() as Bool)

EndFunction

; JunkListPageUpdate
; Updates the Junk List page with the current items
;
; @returns  None
Function JunkListPageUpdate()
    Int i = _page * _itemsPerPage
    Int optionIndex = 0
    Int iTotal = i + _itemsPerPage
    Int junkHistoryCount = JunkHistory.GetSize()

    ;VerboseMessage("JunkHistory Form Count " + junkHistoryCount)
    ;VerboseMessage("Starting Index " + i)
    ;VerboseMessage("Total for this page " + iTotal)
    
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

    ; Go through our history and update the pages itemSlots with the current items
    While i < iTotal && i < junkHistoryCount && optionIndex < _itemsPerPage
        Form item = JunkHistory.GetAt(i)

        String name = item.GetName() as String
        String o_ID = "sItem" + (optionIndex + 1) + ":JunkList"
        
        if(item.HasKeyword(IsJunkKYWD))
            name = FormatJunkItemName(name, "junk")
        Else
            name = FormatJunkItemName(name, "not junk")
        EndIf

        ; Update the item slot with the formated item name
        SetModSettingString(o_ID, name as String)

        i += 1
        optionIndex += 1
    EndWhile

    ; Clear any unused item slots @TODO - This should be handled by MCM Helpers Group Control but every slot would need its own hidden toggle
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
    
    Form item = JunkHistory.GetAt(index + (_page * _itemsPerPage))
    Bool status = JunkList.HasForm(item)
    String name = item.GetName()

    String UpdatingText = FormatJunkItemName(name, "updating")
    SetModSettingString(o_ID, UpdatingText)
    RefreshMenu()

    If status
        RemoveJunkKeyword(item)
        JunkList.RemoveAddedForm(item)

        name = FormatJunkItemName(name, "not junk")
    Else
        AddJunkKeyword(item)
        JunkList.AddForm(item)

        name = FormatJunkItemName(name, "junk")
    EndIf

    SetModSettingString(o_ID, name)

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

; ResetJunk
; Resets the junk list
;
; @returns  None
Function ResetJunk()
    VerboseMessage("Resetting Junk List...")
    SetModSettingString("sResetJunk:Utility", "$JunkIt_ResetingJunk")
    RefreshMenu()

    Int i = 0
    Int iTotal = JunkList.GetSize()
    While i < iTotal
        Form item = JunkList.GetAt(i)

        If item.HasKeyword(IsJunkKYWD)
            RemoveJunkKeyword(item)

            ; We still need to track historical junk marking since Skyrim refuses to not save keywords on items even if they are removed
            If !UnjunkedList.HasForm(item)
                UnjunkedList.AddForm(item)
            EndIf
        EndIf

        i += 1
    EndWhile

    JunkList.Revert()
    VerboseMessage("Junk List reset!", True)
    VerboseMessage("Junk List size after reset: " + JunkList.GetSize())

    SetModSettingString("sResetJunk:Utility", "$JunkIt_JunkReset")
    RefreshMenu()
EndFunction

; TriggerSaveJunkListToFile
; Triggers the save junk list to file function
;
; @returns  None
Function TriggerSaveJunkListToFile()
    VerboseMessage("Saving Junk List To File...")
    SetModSettingString("sSaveJunkListToFile:Utility", "$JunkIt_SavingJunkList")
    RefreshMenu()

    SaveJunkListToFile()

    VerboseMessage("Junk List saved!", True)

    SetModSettingString("sSaveJunkListToFile:Utility", "$JunkIt_JunkSaved")

    ; Prevent text flicker by waiting a second before updating the UI again
    Utility.WaitMenuMode(0.5)
    RefreshMenu()
EndFunction

; TriggerLoadJunkListFromFile
; Triggers the load junk list from file function
;
; @returns  None
Function TriggerLoadJunkListFromFile()
    VerboseMessage("Loading Junk List From File...")
    SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_LoadingJunkList")
    RefreshMenu()

    Int i = 0
    Int iTotal = 0
    FormList NewJunkList = LoadJunkListFromFile()

    If ReplaceJunkListOnLoad.GetValue() > 0
        ; Reset the current junk list and add any forms that were removed that aren't in the imported list to the unjunked list
        i = 0
        iTotal = JunkList.GetSize()
        While i < iTotal
            Form item = JunkList.GetAt(i)

            If !NewJunkList.HasForm(item) && !UnjunkedList.HasForm(item)
                ; Track Unjunked item if it is not in the new list
                UnjunkedList.AddForm(item)
            ElseIf UnjunkedList.HasForm(item)
                ; If it is in the new list, and is currently unjunked, remove it from the unjunked list
                UnjunkedList.RemoveAddedForm(item)
            EndIf

            i += 1
        EndWhile

        JunkList.Revert()
    EndIf

    ; Now iterate through the new List and adjust the JunkList and UnjunkedList accordingly
    i = 0
    iTotal = NewJunkList.GetSize()
    While i < iTotal
        Form item = NewJunkList.GetAt(i)

        ; Add form to junk list if it isn't already there
        If !JunkList.HasForm(item)
            JunkList.AddForm(item)
            JunkHistory.AddForm(item)
        EndIf
        
        ; Ensure that any forms in the new list are removed from the unjunked list if they were previously unjunked
        If UnjunkedList.HasForm(item)
            UnjunkedList.RemoveAddedForm(item)
        EndIf

        i += 1
    EndWhile

    ; Once our junk lists are updated, we can process the keywords on the items
    UpdateItemKeywords()

    If (ReplaceJunkListOnLoad.GetValue() == 0)
        VerboseMessage("Junk List loaded!", True)
        SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_JunkLoaded")
    Else
        VerboseMessage("Junk List replaced!", True)
        SetModSettingString("sLoadJunkListFromFile:Utility", "$JunkIt_JunkReplaced")
    EndIf

    ; Prevent text flicker by waiting a second before updating the UI again
    Utility.WaitMenuMode(0.5)
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

; --- JunkIt Functionality ---------------------------------------------------

; OnMenuOpen
; Enables the hotkey when the player opens the Inventory Menu.
;
; @param MenuName String  the name of the menu
; @returns  None
Event OnMenuOpen(String MenuName)
    ActiveMenu = MenuName
    GotoState("")
EndEvent

; OnMenuClose
; Disables the hotkey when the player closes the Inventory Menu.
;
; @param MenuName String  the name of the menu
; @returns  None
Event OnMenuClose(String MenuName)
    ActiveMenu = ""
    GotoState("busy")
EndEvent

; OnKeyUp
; Handles gamepad key operations for the hotkey and performs the appropriate action based on the active menu and key hold time.
; @note purposely did not use `Game.UsingGamepad()` for those using the SKSE plugin to dynamically switch input modes, 
;       the hotkey should be enough to determine if the gamepad is being used or not.
;
; @param KeyCode Int  the key code
; @param HoldTime Float  the hold time
; @returns  None
Event OnKeyUp(Int KeyCode, Float HoldTime)
    If ActiveMenu != "" && !UI.IsTextInputEnabled() && KeyCode == (GamepadJunkKey.GetValue() as Int)
        GotoState("busy")
        If HoldTime < GamepadTransferHoldTime.GetValue() - 1
            ToggleIsJunk()
        Else
            If ActiveMenu == "ContainerMenu"
                TransferJunk()
            ElseIf ActiveMenu == "BarterMenu"
                SellJunk()
            EndIf
        EndIf

        If bAggressiveRefresh
            RefreshUIIcons()
        EndIf

        GotoState("")
    EndIf
EndEvent

; OnKeyDown
; Listens for the hotkey and performs the appropriate action based on the active menu.
;
; @param KeyCode Int  the key code
; @returns  None
Event OnKeyDown(Int KeyCode)
    If bAggressiveRefresh
        RefreshUIIcons()
    EndIf

    If ActiveMenu != "" && !UI.IsTextInputEnabled()
        GotoState("busy")
        If KeyCode == UserJunkKey
            ToggleIsJunk()
        EndIf

        If KeyCode == UserTransferKey
            If ActiveMenu == "ContainerMenu"
                TransferJunk()
            ElseIf ActiveMenu == "BarterMenu"
                SellJunk()
            EndIf
        EndIf

        If bAggressiveRefresh
            RefreshUIIcons()
        EndIf

        GotoState("")
    EndIf
EndEvent

State busy
    ; OnKeyUp
    ; Disables event during busy state
    ;
    ; @param KeyCode Int  the key code
    ; @param HoldTime Float  the hold time
    ; @returns  None
    Event OnKeyUp(Int KeyCode, Float HoldTime)
    EndEvent
    
    ; OnKeyDown
    ; Disables event during busy state
    ;
    ; @param KeyCode Int  the key code
    ; @returns  None
    Event OnKeyDown(Int KeyCode)
    EndEvent
EndState

; ToggleIsJunk
; Toggles the selected item in an Item Menu as junk or not junk.
;
; @returns  None
Function ToggleIsJunk()
    Form item = ToggleSelectedAsJunk()
    If !item
        return
    EndIF

    ; Process the Results
    If item.HasKeyword(IsJunkKYWD)
        MarkAsJunk(item)
    Else
        UnmarkAsJunk(item)
    EndIf
EndFunction

; MarkAsJunk
; Marks the selected item in an Item Menu as junk.
;
; @param item Form  the item to mark as junk
; @returns  None
Function MarkAsJunk(Form item)
    VerboseMessage("Form: " + item.GetName() + " has been marked as junk")
    If NotifyOnMarkUnmark.GetValue() >= 1
        Debug.Notification("JunkIt - " + item.GetName() + " has been marked as junk")
    EndIf

    ; Update Junk FormList
    If !JunkList.HasForm(item)
        JunkList.AddForm(item)
        JunkHistory.AddForm(item)
    EndIf

    ; Stop tracking this item for GameLoad keyword correction
    If UnjunkedList.HasForm(item)
        UnjunkedList.RemoveAddedForm(item)
    EndIf
EndFunction

; UnmarkAsJunk
; Unmarks the selected item in an Item Menu as junk.
;
; @param item Form  the item to unmark as junk
; @returns  None
Function UnmarkAsJunk(Form item)
    VerboseMessage("Form: " + item.GetName() + " is no longer marked as junk")
    If NotifyOnMarkUnmark.GetValue() >= 1
        Debug.Notification("JunkIt - " + item.GetName() + " is no longer marked as junk")
    EndIf

    ; Update Junk FormList
    If JunkList.HasForm(item)
        JunkList.RemoveAddedForm(item)
    EndIf

    ; Keep track of unjunked items for GameLoad keyword correction
    If !UnjunkedList.HasForm(item)
        UnjunkedList.AddForm(item)
    EndIf
EndFunction

; TransferJunk
; Transfers Junk Items to the container/NPC or retrieves them 
; from the container if UI is showing the players inventory
;
; @returns  None
Function TransferJunk()
    If JunkList.GetSize() <= 0
        Return
    EndIf

    Float playerCarryWeight = PlayerRef.GetActorValue("CarryWeight")

    ObjectReference transferContainer = GetContainerMenuContainer()
    Int menuView = UI.GetInt("ContainerMenu", "_root.Menu_mc.inventoryLists.categoryList.activeSegment")

    FormList TransferList = GetTransferFormList()
    Bool canRetrieve = FALSE
    Bool canTransfer = FALSE

    Int containerMode = GetContainerMode()

    ; disable if pickpocketing
    If containerMode == 2
        VerboseMessage("Junk Transfer disabled while pickpocketing")
        Debug.MessageBox("Junk Transfer is disabled while pickpocketing")
        Return
    EndIf

    if menuView == 0 ; VIEWING CONTAINER
        ; Retrieve from container
        If GetContainerItemListCount(transferContainer, TransferList) <= 0
            VerboseMessage("No Junk to retrieve!")
            Debug.MessageBox("No Junk to take!")
            Return
        EndIf

        If ConfirmTransfer.GetValue() >= 1
            Int iConfChoice = RetrievalConfirmationMsg.Show()
            If(iConfChoice == 0) ;Yes
                canRetrieve = TRUE
            ElseIf(iConfChoice == 1) ;No
                canRetrieve = FALSE
                Return
            EndIf
        Else
            canRetrieve = TRUE
        EndIf
    Else  ; VIEWING PLAYER INVENTORY
        ; Transfer to container
        If GetContainerItemListCount(PlayerRef, TransferList) <= 0
            VerboseMessage("No Junk to transfer!")
            Debug.MessageBox("No Junk to transfer!")
            Return
        EndIf

        If ConfirmTransfer.GetValue() >= 1
            Int iConfChoice = TransferConfirmationMsg.Show()
            If(iConfChoice == 0) ;Yes
                canTransfer = TRUE
            ElseIf(iConfChoice == 1) ;No
                canTransfer = FALSE
                Return
            EndIf
        Else
            canTransfer = TRUE
        EndIf
    EndIf

    If canRetrieve == TRUE
        ; Check for large inventories and warn that they could take longer to process
        WarnLargeInventory(PlayerRef, transferContainer)

        If NotifyOnJunkTransfer.GetValue() >= 1
            Debug.Notification("JunkIt - Processing Retrieval...")
        EndIf

        LockItemListUI()

        Int iRetrievedCount = ProcessItemListTransfer(TransferList, transferContainer, PlayerRef, 0)
        
        VerboseMessage("Junk Retrieved!")
        If NotifyOnJunkTransfer.GetValue() >= 1
            Debug.Notification("JunkIt - " + iRetrievedCount + " Junk Items Retrieved!")
        EndIf
        UnlockItemListUI()
        Return
    EndIf

    If canTransfer == TRUE
        ; Check for large inventories and warn that they could take longer to process
        WarnLargeInventory(PlayerRef, transferContainer)

        If NotifyOnJunkTransfer.GetValue() >= 1
            Debug.Notification("JunkIt - Processing Transfer...")
        EndIf
        
        ; Find out if we're trading with an NPC and account for their carry weight
        If(containerMode == 3) ; NPC Mode
            Actor transferActor = transferContainer as Actor
            Float maxWeight = transferActor.GetActorValue("CarryWeight")
            Float currentWeight = transferContainer.GetTotalItemWeight()
            VerboseMessage("[NPC Mode] CarryWeight " + currentWeight + "/" + maxWeight, True)

            Int iTotal = TransferList.GetSize()
            Int iCurrent = 0
            Int TotalTransferred = 0
            Int TotalPossibleTransferred = 0

            FormList TransferAllList = TransferList
            LockItemListUI()

            While iCurrent < iTotal
                Form item = TransferList.GetAt(iCurrent)
                Int iCount = 0
                Int iTotalCount = 0

                if item
                    iCount = GetContainerSingleItemCount(PlayerRef, item)
                    iTotalCount = iCount
                    TotalPossibleTransferred += iCount
                EndIf

                If iCount > 0
                    Float itemWeight = item.GetWeight()
                    Float currentWeightWithItems = (itemWeight * iCount) + currentWeight
                    
                    While currentWeightWithItems > maxWeight
                        iCount -= 1
                        currentWeightWithItems = (itemWeight * iCount) + currentWeight
                    EndWhile

                    If iCount > 0 && iCount < iTotalCount
                        ; Transfer only a limited quantity of this item
                        PlayerRef.RemoveItem(item, iCount, true, transferContainer)
                        currentWeight += (itemWeight * iCount)
                        TotalTransferred += iCount

                        ; Ignore this item for the bulk transfer
                        TransferAllList.RemoveAddedForm(item)

                        VerboseMessage("Transferred limited quantity " + iCount + " " + item.GetName() + " to " + transferActor.GetName() + " [" + RoundNumber(currentWeight) + "/" + RoundNumber(maxWeight) + "]" )
                    ElseIf iCount <= 0
                        ; We cannot transfer any of this item so remove it from the bulk transfer list
                        TransferAllList.RemoveAddedForm(item)
                    Else
                        ; Can transfer the full quanity of this item, let it remain in the bulk transfer list
                        TotalTransferred += iCount
                        currentWeight += (itemWeight * iCount)
                        VerboseMessage("Listing " + iCount + " " + item.GetName() + " for full quantity transfer to " + transferActor.GetName() + " [" + RoundNumber(currentWeight) + "/" + RoundNumber(maxWeight) + "]" )
                    EndIf
                EndIf

                iCurrent += 1
            EndWhile

            ProcessItemListTransfer(TransferAllList, PlayerRef, transferContainer, 0)

            If TotalTransferred == 0
                VerboseMessage("[NPC Mode] NPC cannot carry any more junk")
                Debug.MessageBox("This person cannot carry any more")
            ElseIf TotalTransferred >= TotalPossibleTransferred
                VerboseMessage("[NPC Mode] Transferred All " + TotalTransferred + " Junk Items to " + transferActor.GetName() + " [" + RoundNumber(currentWeight) + "/" + RoundNumber(maxWeight) + "]")
                If NotifyOnJunkTransfer.GetValue() >= 1
                    Debug.Notification("JunkIt - Transferred All " + TotalTransferred + " Junk Items!")
                EndIf
            Else
                VerboseMessage("[NPC Mode] Transferred " + TotalTransferred + " Junk Items to " + transferActor.GetName() + " [" + RoundNumber(currentWeight) + "/" + RoundNumber(maxWeight) + "]")
                If NotifyOnJunkTransfer.GetValue() >= 1
                    Debug.Notification("JunkIt - Transferred " + TotalTransferred + " Junk Items!")
                EndIf
            EndIf

            UnlockItemListUI()
        Else
            LockItemListUI()
            
            Int iTransferredCount = ProcessItemListTransfer(TransferList, PlayerRef, transferContainer, 0)
            
            If NotifyOnJunkTransfer.GetValue() >= 1
                Debug.Notification("JunkIt - Transferred " + iTransferredCount + " Junk Items!")
            EndIf
            
            UnlockItemListUI()
        EndIf
    EndIf

    If PlayerRef.GetActorValue("CarryWeight") != playerCarryWeight
        PlayerRef.SetActorValue("CarryWeight", playerCarryWeight)
    EndIf

    ; Wait a moment to allow the transfer operation to fully complete
    ; Utility.wait(0.5)
    ; RefreshUIIcons()
EndFunction

; SellJunk
; Sells all junk items to the vendor
;
; @returns  None
Function SellJunk()
    If JunkList.GetSize() <= 0
        VerboseMessage("No Junk to sell!")
        Debug.MessageBox("No Junk to sell!")
        ; RefreshUIIcons()
        Return
    EndIf

    Float playerCarryWeight = PlayerRef.GetActorValue("CarryWeight")

    ; JunkIt.dll Native function gets a filtered 
    ; version of the junk list that is 
    ; sorted by priority, equip and favorite filtered, 
    ; and limited to only items in this barter session
    FormList SellList = GetSellFormList()
    Int PlayerItemListCount = GetContainerItemListCount(PlayerRef, SellList)

    VerboseMessage("SellList generated from ItemList. SellList Form Count " + SellList.GetSize())
    VerboseMessage("Player has " + PlayerItemListCount + " junk items in the SellList to sell!")

    ; Check if the players inventory has any junk to sell
    If PlayerItemListCount <= 0
        VerboseMessage("No Junk to sell!")
        Debug.MessageBox("No Junk to sell!")
        Return
    EndIf
    
    ; Get the actors and containers involved in the barter
    Actor vendorActor = GetBarterMenuContainer() as Actor
    ObjectReference vendorContainer = GetBarterMenuMerchantContainer()

    If !vendorActor || vendorActor == PlayerRef
        VerboseMessage("SKSE Failed to get a valid vendor actor. Exiting Bulk Sale process.")
        Debug.MessageBox("JunkIt encountered an error attemping to sell items. Please report this on the JunkIt mod page along with the version of the game you are using.")
    EndIf

    If !vendorContainer
        VerboseMessage("Vendor Container not found, using Vendor Actor as Container.")
        vendorContainer = vendorActor as ObjectReference
    EndIf

    VerboseMessage("Vendor Actor FormId: " + vendorActor.GetFormID())
    VerboseMessage("Vendor Container FormId: " + vendorContainer.GetFormID())

    If NotifyOnJunkSell.GetValue() >= 1
        Debug.Notification("JunkIt - Selling junk please wait...")
    EndIf
    LockItemListUI()

    Float vendorGoldDisplay = UI.GetFloat("BarterMenu", "_root.Menu_mc._vendorGold")
    Float buyMult = UI.GetFloat("BarterMenu", "_root.Menu_mc._buyMult")
    Float sellMult = UI.GetFloat("BarterMenu", "_root.Menu_mc._sellMult")

    VerboseMessage("Vendor Gold: " + vendorGoldDisplay)
    VerboseMessage("Vendor Buy Mult: " + buyMult)
    VerboseMessage("Vendor Sell Mult: " + sellMult)

    Int iTotal = SellList.GetSize()
    Int iCurrent = 0
    Int TotalToSell = 0
    Int TotalPossibleToSell = 0
    Float calculatedVendorGold = vendorGoldDisplay
    Float totalSellValue = 0

    ; These represent the final items to be sold
    FormList SellAllList = SellList
    Form[] SellPartialList = new Form[125]
    Int[] SellPartialCounts = new Int[125]
    Int PartialSellItemCount = 0

    VerboseMessage("Sell List Size: " + SellList.GetSize())
    
    While iCurrent < iTotal
        Form item = SellList.GetAt(iCurrent)
        Int iCount = 0
        Int iTotalCount = 0

        if item
            iCount = GetContainerSingleItemCount(PlayerRef, item)
            iTotalCount = iCount
            TotalPossibleToSell += iCount
        EndIf

        VerboseMessage("Calculating Sell Item: " + item.GetName() + " -- player has " + iCount + " of this item")

        ; Calculate how many junk items we can sell based on the vendors gold
        If iCount > 0
            ; My native function has a more accurate gold value calculation than the papyrus item.GetGoldValue()
            ; Native function also accurately calculates values for stock enchantments and custom player enchantments
            Float itemGoldValue = GetMenuItemValue(item) as Float
            
            VerboseMessage("SKSE Sell Value: " + item.GetName() + " sells for " + RoundNumber(itemGoldValue * sellMult))
            VerboseMessage("Papyrus Sell Value: " + item.GetName() + " sells for " + RoundNumber(item.GetGoldValue() * sellMult))

            Float sellValue = (itemGoldValue * sellMult)
            Float goldDifferential = calculatedVendorGold - (sellValue * iCount)
            
            While RoundNumber(goldDifferential) <= 0 && iCount > 0
                iCount -= 1
                goldDifferential = calculatedVendorGold - (sellValue * iCount)
            EndWhile

            If iCount > 0 && iCount < iTotalCount
                ; We can only sell a limited amount of this item
                SellAllList.RemoveAddedForm(item)

                ; Add this item as a listing for partial sale and track the quantity to be sold
                SellPartialCounts[PartialSellItemCount] = iCount
                SellPartialList[PartialSellItemCount] = item
                VerboseMessage("Creating partial listing for " + iCount + " " + item.GetName() + " at index " + PartialSellItemCount + " for " + (sellValue * iCount) + " gold. Post sale VendorGold is " + RoundNumber(vendorGoldDisplay - totalSellValue) + " gold")
                PartialSellItemCount += 1

                ; Update our totals for the confirmation message
                calculatedVendorGold -= sellValue * iCount
                totalSellValue += sellValue * iCount
                TotalToSell += iCount
            ElseIf iCount <= 0
                ; We cannot sell any of this item so remove it from the bulk sell list
                VerboseMessage("Cannot sell any of this item, removing from bulk sale list")
                SellAllList.RemoveAddedForm(item)
            Else
                ; We can sell the full quanity of this item
                calculatedVendorGold -= sellValue * iCount
                totalSellValue += sellValue * iCount
                TotalToSell += iCount

                VerboseMessage("Calculated Full Quantity Sell " + iCount + " " + item.GetName() + " for " + (sellValue * iCount) + " gold. Post sale VendorGold is " + RoundNumber(vendorGoldDisplay - totalSellValue) + " gold")
            EndIf
        EndIf

        iCurrent += 1
    EndWhile

    ; After calculations check if the vendor could afford any of the junk
    If TotalToSell <= 0
        VerboseMessage("Vendor cannot afford to buy any junk!")
        Debug.MessageBox("Vendor cannot afford to buy any junk!")
        UnlockItemListUI()
        Return
    EndIf

    ; Confirm the sale
    If ConfirmSell.GetValue() >= 1
        Int iConfChoice = SellConfirmationMsg.Show(TotalToSell, RoundNumber(totalSellValue))
        If(iConfChoice == 1) ;No
            UnlockItemListUI()
            Return
        EndIf
    EndIf

    ; Check for large inventories and warn that they could take longer to process
    WarnLargeInventory(PlayerRef, vendorContainer)

    If NotifyOnJunkSell.GetValue() >= 1
        Debug.Notification("JunkIt - Processing Sale...")
    EndIf

    ; Get payout from vendors on hand gold first then container
    Int goldToGimme = RoundNumber(totalSellValue)
    Int vendorActorGold = vendorActor.GetItemCount(Gold001)
    If vendorActorGold > 0
        Int onHandGoldToGimme = goldToGimme
        If vendorActorGold < goldToGimme
            onHandGoldToGimme = vendorActorGold
        EndIf

        VerboseMessage("Vendor has " + vendorActorGold + " gold on hand. Taking " + onHandGoldToGimme + " gold from vendor...")
        vendorActor.RemoveItem(Gold001, onHandGoldToGimme, false, PlayerRef)
        goldToGimme -= vendorActorGold
    Endif

    ; If the vendors on hand gold was not enough, take the rest from the container
    If goldToGimme > 0
        Int containerGold = vendorContainer.GetItemCount(Gold001)
        If containerGold > 0
            Int containerGoldToGimme = goldToGimme
            If containerGold < goldToGimme
                containerGoldToGimme = containerGold
            EndIf

            VerboseMessage("Vendor Container has " + containerGold + " gold. Taking " + containerGoldToGimme + " gold from vendor container...")
            vendorContainer.RemoveItem(Gold001, containerGoldToGimme, false, PlayerRef)
            goldToGimme -= containerGold
        EndIf

        ; This case actually should never happen, but just in case shit gets wild pay the player what is owed
        If goldToGimme > 0
            VerboseMessage("Vendor ran out of money! Gold owed to player " + goldToGimme)
            PlayerRef.AddItem(Gold001, goldToGimme, false)
        EndIf
    EndIf

    ; Update UI with the new vendor gold total, the itemlist update can not be trusted
    Int totalVendorGoldLeft = RoundNumber(vendorGoldDisplay - totalSellValue)
    If totalVendorGoldLeft < 0
        totalVendorGoldLeft = 0
    EndIf
    UI.SetFloat("BarterMenu", "_root.Menu_mc._vendorGold", totalVendorGoldLeft)

    ; Transfer partial quantity item listings
    ; *Note: Partial transfers should be done first to avoid any issues with the full quantity items
    ;        they should also only be "maybe" one or two items at most so doing it in papyrus should be fast enough
    Int PartialIndex = 0
    VerboseMessage("SellPartialList Size: " + PartialSellItemCount)
    While PartialIndex < PartialSellItemCount
        Form item = SellPartialList[PartialIndex]
        Int iCount = SellPartialCounts[PartialIndex]
        
        ; Double check item sale count
        If iCount > 0
            ; Do the item exchange
            PlayerRef.RemoveItem(item, iCount, true, vendorContainer)
            VerboseMessage("Transaction for partial quantity listing " + iCount + " " + item.GetName() + " at index " + PartialIndex + " complete")
        EndIf
        PartialIndex += 1
    EndWhile

    VerboseMessage("ProcessItemListTransfer(SellAllList) - SellAllList.Size: " + SellAllList.GetSize())
    Int iTotalFullQuantityItems = ProcessItemListTransfer(SellAllList, PlayerRef, vendorContainer, 1)
    VerboseMessage("Transaction " + iTotalFullQuantityItems + " full quantity item sales complete", True)

    ; Speechcraft experience is calculated by 1 base XP per gold used in transactions.
    ; Formula: skillUseMult * (base Xp * fSpeechCraftMult) + skillUseOffset
    ; The experience gained by passing in The base XP to the AvanceSkill function should adhere to the correct experience gain formula
    Game.AdvanceSkill("SpeechCraft", totalSellValue)

    ; Also increment the game stats for the number of barters. Some other mods rely on this for quirky fun reasons
    Game.IncrementStat("Barters", TotalToSell)

    If TotalToSell >= TotalPossibleToSell
        VerboseMessage("Sold All Junk Items for " + totalSellValue + " Gold")
        If NotifyOnJunkSell.GetValue() >= 1
            Debug.Notification("JunkIt - Sold All Junk Items!")
        EndIf
    Else
        VerboseMessage("Sold " + TotalToSell + " Junk Items for " + totalSellValue + " Gold")
        If NotifyOnJunkSell.GetValue() >= 1
            Debug.Notification("JunkIt - Sold " + TotalToSell + " Junk Items!")
        EndIf
    EndIf

    If PlayerRef.GetActorValue("CarryWeight") != playerCarryWeight
        PlayerRef.SetActorValue("CarryWeight", playerCarryWeight)
    EndIf

    UnlockItemListUI()
EndFunction

; --- JunkIt Utilities --------------------------------------------------------------

; CorrectJunkListKeywords 
; Iterates through a formlist to add or remove the junk keyword
;
; @param List   FormList  the formlist to iterate through
; @param IsJunk Bool  whether to add or remove the junk keyword
; @returns  FormList  the modified formlist
FormList Function CorrectJunkListKeywords(FormList List, Bool IsJunk = True)
    Int i = 0
    Int iTotal = List.GetSize()
    
    While i < iTotal
        Form item = List.GetAt(i)

        If IsJunk && !item.HasKeyword(IsJunkKYWD)
            VerboseMessage("Item Correction: Marking " + item.GetName() + " as junk")
            AddJunkKeyword(item)
        ElseIf !IsJunk && item.HasKeyword(IsJunkKYWD)
            VerboseMessage("Item Correction: Removing junk keyword from " + item.GetName())
            RemoveJunkKeyword(item)
        EndIf

        i += 1
    EndWhile

    Return List
EndFunction

; RoundNumber
; Rounds a float to the nearest integer
;
; @param number Float  the number to round
; @returns  Int  the rounded number
Int Function RoundNumber (Float number)
    Float ceilingNumber = Math.Ceiling(number) as Float
    If ((ceilingNumber - number) > 0.5)
        Return Math.Floor(number)
    Else
        Return Math.Ceiling(number)
    EndIf
EndFunction

; LockItemListUI
; Disables the UI for the ItemList in the InventoryMenu
;
; @returns  None
Function LockItemListUI()
    iAggressiveUpdateTimer = 0
EndFunction

; UnlockItemListUI
; Enables the UI for the ItemList in the InventoryMenu
;
; @param bUpdateUI Bool  whether to update the UI icons
; @returns  None
Function UnlockItemListUI(bool bUpdateUI = true)
    If bAggressiveRefresh
        RefreshUIIcons()
        RegisterForSingleUpdate(5.0)
    EndIf
EndFunction

; WarnLargeInventory
; Checks the total number of items in the container and warns the player if it exceeds a certain threshold
;
; @param a_container1 ObjectReference  the first container
; @param a_container2 ObjectReference  the second container
; @returns  None
Bool Function WarnLargeInventory(ObjectReference a_container1, ObjectReference a_container2)
    Int ItemCount1 = a_container1.GetContainerForms().Length
    Int ItemCount2 = a_container2.GetContainerForms().Length
    Int iCount = ItemCount1 + ItemCount2
    VerboseMessage("Large Inventory Check: Total Menu Form Count: " + iCount)

    If iCount >= WarnInventorySizeThreshold
        VerboseMessage("Large Container Inventory Detected!")
        If NotifyLargeInventoryLag.GetValue() >= 1
            Debug.MessageBox("Large Inventory detected, transfer could lag. Please allow for a few additional seconds for the transfer to complete.")
        EndIf
        Return True
    EndIf

    Return False
EndFunction
