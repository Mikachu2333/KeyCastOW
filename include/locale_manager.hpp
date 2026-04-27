#pragma once
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <windows.h>

class LocaleManager {
public:
  static LocaleManager &GetInstance() {
    static LocaleManager instance;
    return instance;
  }

  void Load(const std::wstring &filePath) {
    m_data.clear();
    ParseIniFile(filePath);
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

  std::wstring Trim(const std::wstring &str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (std::wstring::npos == first) {
      return L"";
    }
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
  }

  void ParseIniFile(const std::wstring &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
      return;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    if (content.empty())
      return;

    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
      content.erase(0, 3);
    }

    if (content.empty())
      return;

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &content[0],
                                          (int)content.size(), NULL, 0);
    if (size_needed <= 0)
      return;
    std::wstring wcontent(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &content[0], (int)content.size(),
                        &wcontent[0], size_needed);

    std::wstringstream ss(wcontent);
    std::wstring line;
    std::wstring currentSection;

    while (std::getline(ss, line)) {
      line = Trim(line);
      if (line.empty() || line[0] == L';' || line[0] == L'#')
        continue;

      if (line.front() == L'[' && line.back() == L']') {
        currentSection = line.substr(1, line.size() - 2);
      } else {
        size_t eqPos = line.find(L'=');
        if (eqPos != std::wstring::npos) {
          std::wstring key = Trim(line.substr(0, eqPos));
          std::wstring value = Trim(line.substr(eqPos + 1));
          if (!currentSection.empty()) {
            m_data[currentSection][key] = value;
          }
        }
      }
    }
  }
};

#define I18N(section, key, defaultVal)                                         \
  LocaleManager::GetInstance().GetString(section, key, defaultVal)
