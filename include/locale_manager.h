#pragma once
#include <windows.h>
#include <string>
#include <map>

class LocaleManager {
public:
    static LocaleManager& GetInstance() {
        static LocaleManager instance;
        return instance;
    }

    void Load(const std::wstring& filePath) {
        m_filePath = filePath;
    }

    std::wstring GetString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultVal) {
        WCHAR buffer[1024];
        GetPrivateProfileString(section.c_str(), key.c_str(), defaultVal.c_str(), buffer, 1024, m_filePath.c_str());
        return std::wstring(buffer);
    }

private:
    LocaleManager() {}
    std::wstring m_filePath;
};

#define I18N(section, key, defaultVal) LocaleManager::GetInstance().GetString(section, key, defaultVal)
