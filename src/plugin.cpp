#include "log.h"
#include "settings.h"
#include "junk.h"
#include "JunkData.h"
#include "event.h"
#include "DIIIIntegration.h"
#include "I4MovieHook.h"
#include "I4Integration.h"
#include "Translation.h"
#include "UI.h"
#include "SkyPromptIntegration.h"
#include "AutoJunk.h"
#include "PluginAPI.h"
#include "Papyrus.h"

SKSE_EXPORT constinit SKSE::PluginVersionData SKSEPlugin_Version = []() noexcept {
	SKSE::PluginVersionData v;
	v.PluginName("JunkIt");
	v.PluginVersion({ 0, 2, 0, 9 });
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	return v;
}();

SKSE_EXPORT bool SKSEPlugin_Query(SKSE::QueryInterface*, SKSE::PluginInfo* pluginInfo) {
	pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	pluginInfo->name = SKSEPlugin_Version.GetPluginName().data();
	pluginInfo->version = SKSEPlugin_Version.GetPluginVersion().pack();
	return true;
}

void DIIIMessageHandler(SKSE::MessagingInterface::Message* msg) {
	if (msg->type == DIII::kMessage_GetAPI) {
		auto* api = static_cast<DIII::IAPI*>(msg->data);
		api->RegisterCondition("isJunk",
			[](const Json::Value&, RE::FormType) -> std::unique_ptr<DIII::ICondition> {
				return std::make_unique<JunkIt::IsJunkCondition>();
			});
		SKSE::log::info("Registered DIII condition: isJunk");
	}
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
	switch (a_msg->type) {
		case SKSE::MessagingInterface::kInputLoaded:
			JunkIt::I4MovieHook::Install();
			JunkIt::I4JunkConfig::GetSingleton().Load();
			break;
		case SKSE::MessagingInterface::kPostLoad:
		case SKSE::MessagingInterface::kPostPostLoad:
			JunkIt::BroadcastAPI();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			JunkIt::Settings::LoadGameForms();
			JunkIt::Translation::Load();
			JunkIt::UI::Register();
			JunkIt::InputEventHandler::Install();
			JunkIt::JunkHandler::Install();
			JunkIt::SkyPromptIntegration::GetSingleton().Install();
			JunkIt::AutoJunk::Install();
			JunkIt::I4JunkConfig::GetSingleton().Load();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			JunkIt::I4JunkConfig::GetSingleton().Load();
			JunkIt::Settings::LoadGameForms();
			JunkIt::JunkHandler::TryExpireTrash();
			break;
		case SKSE::MessagingInterface::kNewGame:
			JunkIt::I4JunkConfig::GetSingleton().Load();
			JunkIt::Settings::LoadGameForms();
			JunkIt::JunkHandler::RevertTrashState();
			if (JunkIt::Settings::GetAutoImport()) {
				SKSE::log::info("Auto-import: loading junk list from file");
				if (JunkIt::JunkDataManager::GetSingleton().LoadFromFile(true)) {
					UIUtil::ItemList::Refresh();
				} else {
					SKSE::log::warn("Auto-import failed to load junk list from file");
				}
			}
			break;
		case SKSE::MessagingInterface::kSaveGame:
			if (JunkIt::Settings::GetAutoExport() &&
				(JunkIt::JunkDataManager::GetSingleton().Size() > 0 ||
					JunkIt::JunkDataManager::GetSingleton().NoAutoJunkSize() > 0)) {
				JunkIt::JunkDataManager::GetSingleton().SaveToFile();
			}
			break;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
	SetupLog();

	const auto* plugin = SKSE::PluginVersionData::GetSingleton();
	SKSE::log::info("{} loaded (game {})", plugin->GetPluginName(), skse->RuntimeVersion().string("."));

	SKSE::AllocTrampoline(64);

	JunkIt::Settings::LoadFromIni();
	JunkIt::Translation::Load();

	DIII::ListenForRegistration(&DIIIMessageHandler);

	auto papyrus = SKSE::GetPapyrusInterface();
	if (!papyrus || !papyrus->Register(JunkIt::Papyrus::Register)) {
		SKSE::log::error("Failed to register Papyrus functions");
		return false;
	}

    auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	auto serialization = SKSE::GetSerializationInterface();
	if (serialization) {
		serialization->SetUniqueID('JNKT');
		serialization->SetSaveCallback(JunkIt::JunkDataManager::OnSave);
		serialization->SetLoadCallback(JunkIt::JunkDataManager::OnLoad);
		serialization->SetRevertCallback(JunkIt::JunkDataManager::OnRevert);
		SKSE::log::info("Registered SKSE serialization callbacks");
	} else {
		SKSE::log::error("Failed to get SerializationInterface");
		return false;
	}

	SKSE::log::info("Setup Complete");
	
    return true;
}
