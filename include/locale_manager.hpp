#pragma once
#include <map>
#include <string>

class LocaleManager {
public:
  static LocaleManager &GetInstance() {
    static LocaleManager instance;
    return instance;
  }

  void SetData(const std::map<std::wstring,
                              std::map<std::wstring, std::wstring>> &data) {
    m_data = data;
  }

  std::wstring GetString(const std::wstring &section, const std::wstring &key,
                         const std::wstring &defaultVal) {
    auto sectionIt = m_data.find(section);
    if (sectionIt == m_data.end()) {
      return defaultVal;
    }

    auto keyIt = sectionIt->second.find(key);
    if (keyIt == sectionIt->second.end()) {
      return defaultVal;
    }

    return keyIt->second;
  }

private:
  LocaleManager() {}
  std::map<std::wstring, std::map<std::wstring, std::wstring>> m_data;
};

#define I18N(section, key, defaultVal)                                         \
  LocaleManager::GetInstance().GetString(section, key, defaultVal)
