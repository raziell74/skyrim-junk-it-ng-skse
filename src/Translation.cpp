#include "Translation.h"

#include "settings.h"
#include "util.h"

#include <cstdint>
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
            if (language == "CHINESE") return "junkit_chinese.txt";
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

        bool CodepointNeedsCjkFont(std::uint32_t cp) {
            return (cp >= 0x1100 && cp <= 0x11FF) ||
                (cp >= 0x3000 && cp <= 0x30FF) ||
                (cp >= 0x3130 && cp <= 0x318F) ||
                (cp >= 0x31F0 && cp <= 0x31FF) ||
                (cp >= 0x3400 && cp <= 0x4DBF) ||
                (cp >= 0x4E00 && cp <= 0x9FFF) ||
                (cp >= 0xAC00 && cp <= 0xD7AF) ||
                (cp >= 0xF900 && cp <= 0xFAFF) ||
                (cp >= 0xFF66 && cp <= 0xFF9D);
        }
    }

    void Translation::Load() {
        strings.clear();
        const std::filesystem::path translationsDir("Data/Interface/translations");
        LoadFile(translationsDir / "junkit_english.txt", strings);

        language = Settings::EffectiveLanguage(GetGameLanguage());
        const auto* fileName = LanguageFileName(language);
        if (!Util::String::iEquals(fileName, "junkit_english.txt")) {
            LoadFile(translationsDir / fileName, strings);
        }

        SKSE::log::info("Loaded {} translation strings (language {})", strings.size(), language);
    }

    std::string_view Translation::Language() {
        return language;
    }

    const std::string& Translation::Get(std::string_view key) {
        auto it = strings.find(std::string(key));
        if (it != strings.end()) {
            return it->second;
        }
        missing.assign(key);
        return missing;
    }

    bool Translation::TextNeedsCjkFont(std::string_view text) {
        std::size_t i = 0;
        while (i < text.size()) {
            const auto lead = static_cast<unsigned char>(text[i]);
            std::uint32_t cp = 0;
            std::size_t width = 1;
            if (lead < 0x80) {
                cp = lead;
            } else if ((lead & 0xE0) == 0xC0 && i + 1 < text.size()) {
                width = 2;
                cp = (lead & 0x1F) << 6;
                cp |= static_cast<unsigned char>(text[i + 1]) & 0x3F;
            } else if ((lead & 0xF0) == 0xE0 && i + 2 < text.size()) {
                width = 3;
                cp = (lead & 0x0F) << 12;
                cp |= (static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6;
                cp |= static_cast<unsigned char>(text[i + 2]) & 0x3F;
            } else if ((lead & 0xF8) == 0xF0 && i + 3 < text.size()) {
                width = 4;
                cp = (lead & 0x07) << 18;
                cp |= (static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12;
                cp |= (static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6;
                cp |= static_cast<unsigned char>(text[i + 3]) & 0x3F;
            } else {
                ++i;
                continue;
            }
            if (CodepointNeedsCjkFont(cp)) {
                return true;
            }
            i += width;
        }
        return false;
    }
}
