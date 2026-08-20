#include "Translation.h"

#include "util.h"

#include <filesystem>
#include <fstream>
#include <windows.h>

namespace JunkIt {
    namespace {
        std::string Utf16LeToUtf8(std::string_view raw) {
            if (raw.size() < 2) {
                return {};
            }

            std::size_t offset = 0;
            if (static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
                offset = 2;
            }

            const auto* begin = reinterpret_cast<const wchar_t*>(raw.data() + offset);
            const int wcharCount = static_cast<int>((raw.size() - offset) / sizeof(wchar_t));
            if (wcharCount <= 0) {
                return {};
            }

            const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, begin, wcharCount, nullptr, 0, nullptr, nullptr);
            if (utf8Size <= 0) {
                return {};
            }

            std::string utf8(static_cast<std::size_t>(utf8Size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, begin, wcharCount, utf8.data(), utf8Size, nullptr, nullptr);
            while (!utf8.empty() && (utf8.back() == '\0' || utf8.back() == '\r')) {
                utf8.pop_back();
            }
            return utf8;
        }

        void LoadFile(const std::filesystem::path& path, std::unordered_map<std::string, std::string>& out) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return;
            }

            const std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (raw.empty()) {
                return;
            }

            std::string text;
            if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
                text = Utf16LeToUtf8(raw);
            } else {
                text = raw;
            }

            std::size_t lineStart = 0;
            while (lineStart < text.size()) {
                auto lineEnd = text.find('\n', lineStart);
                if (lineEnd == std::string::npos) {
                    lineEnd = text.size();
                }
                std::string_view line(text.data() + lineStart, lineEnd - lineStart);
                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                const auto tab = line.find('\t');
                if (tab != std::string_view::npos && line.front() == '$') {
                    out.insert_or_assign(std::string(line.substr(0, tab)), std::string(line.substr(tab + 1)));
                }

                lineStart = lineEnd + 1;
            }
        }

        std::string GetGameLanguage() {
            auto* collection = RE::INISettingCollection::GetSingleton();
            if (!collection) {
                return "ENGLISH";
            }
            auto* setting = collection->GetSetting("sLanguage:General");
            if (!setting) {
                return "ENGLISH";
            }
            const char* value = setting->GetString();
            if (!value || !*value) {
                return "ENGLISH";
            }
            return Util::String::ToUpper(value);
        }

        const char* LanguageFileName(std::string_view language) {
            if (language == "CZECH") return "junkit_czech.txt";
            if (language == "FRENCH") return "junkit_french.txt";
            if (language == "GERMAN") return "junkit_german.txt";
            if (language == "ITALIAN") return "junkit_italian.txt";
            if (language == "JAPANESE") return "junkit_japanese.txt";
            if (language == "POLISH") return "junkit_polish.txt";
            if (language == "RUSSIAN") return "junkit_russian.txt";
            if (language == "SPANISH") return "junkit_spanish.txt";
            return "junkit_english.txt";
        }
    }

    void Translation::Load() {
        strings.clear();
        const std::filesystem::path translationsDir("Data/Interface/translations");
        LoadFile(translationsDir / "junkit_english.txt", strings);

        const auto language = GetGameLanguage();
        const auto* fileName = LanguageFileName(language);
        if (!Util::String::iEquals(fileName, "junkit_english.txt")) {
            LoadFile(translationsDir / fileName, strings);
        }

        SKSE::log::info("Loaded {} translation strings (language {})", strings.size(), language);
    }

    const std::string& Translation::Get(std::string_view key) {
        auto it = strings.find(std::string(key));
        if (it != strings.end()) {
            return it->second;
        }
        missing.assign(key);
        return missing;
    }
}
