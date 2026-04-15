#include "Translate.h"

#include <Debug.h>

#include <Windows.h>

#include <fstream>
#include <sstream>
#include <string>

enum LangId {
	LANG_EN, LANG_JA, LANG_ZH_CN, LANG_ZH_TW, LANG_KO,
	LANG_RU, LANG_DE, LANG_FR, LANG_ES, LANG_PT,
	LANG_COUNT
};

static const char* const g_englishBaked[TR_COUNT] = {
#define X(n, s) s,
	TR_KEYS(X)
#undef X
};

static const char* const g_keyNames[TR_COUNT] = {
#define X(n, s) #n,
	TR_KEYS(X)
#undef X
};

static std::string g_loaded[TR_COUNT];
static bool        g_loadedValid[TR_COUNT];
static LangId      g_currentLang = LANG_EN;

// Address-of-function locates the hosting DLL regardless of its filename
// (mirrors Settings.cpp::GetConfigFilePath).
static std::string GetLangDir()
{
	char path[MAX_PATH];
	HMODULE hm = NULL;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&GetLangDir, &hm);
	GetModuleFileNameA(hm, path, sizeof(path));
	std::string dir(path);
	size_t pos = dir.find_last_of("\\/");
	if (pos != std::string::npos)
		dir = dir.substr(0, pos + 1);
	return dir + "lang\\";
}

static const char* LangCode(LangId l)
{
	switch (l)
	{
	case LANG_JA:    return "ja";
	case LANG_ZH_CN: return "zh_CN";
	case LANG_ZH_TW: return "zh_TW";
	case LANG_KO:    return "ko";
	case LANG_RU:    return "ru";
	case LANG_DE:    return "de";
	case LANG_FR:    return "fr";
	case LANG_ES:    return "es";
	case LANG_PT:    return "pt";
	default:         return "en";
	}
}

static int FindKeyIndex(const std::string& name)
{
	for (int i = 0; i < TR_COUNT; ++i)
		if (name == g_keyNames[i])
			return i;
	return -1;
}

static void LoadLanguageFile(LangId lang)
{
	std::string path = GetLangDir() + LangCode(lang) + ".txt";
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		DebugLog("[KenshiRotate] lang file not found, using English: " + path);
		return;
	}

	int loaded = 0, unknown = 0, malformed = 0;
	std::string line;
	bool firstLine = true;

	while (std::getline(file, line))
	{
		if (firstLine)
		{
			firstLine = false;
			if (line.size() >= 2 &&
				(((unsigned char)line[0] == 0xFE && (unsigned char)line[1] == 0xFF) ||
				 ((unsigned char)line[0] == 0xFF && (unsigned char)line[1] == 0xFE)))
			{
				DebugLog("[KenshiRotate] UTF-16 lang file not supported, using English: " + path);
				return;
			}
			if (line.size() >= 3 &&
				(unsigned char)line[0] == 0xEF &&
				(unsigned char)line[1] == 0xBB &&
				(unsigned char)line[2] == 0xBF)
			{
				line.erase(0, 3);
			}
		}

		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		if (line.empty() || line[0] == '#')
			continue;

		size_t eq = line.find('=');
		if (eq == std::string::npos)
		{
			++malformed;
			continue;
		}

		std::string key = line.substr(0, eq);
		int idx = FindKeyIndex(key);
		if (idx < 0)
		{
			++unknown;
			continue;
		}

		g_loaded[idx] = line.substr(eq + 1);
		g_loadedValid[idx] = true;
		++loaded;
	}

	std::stringstream ss;
	ss << "[KenshiRotate] Loaded " << path << ": "
	   << loaded << "/" << TR_COUNT << " keys";
	if (unknown)   ss << ", " << unknown << " unknown";
	if (malformed) ss << ", " << malformed << " malformed";
	DebugLog(ss.str());
}

const char* Tr(TrKey key)
{
	return g_loadedValid[key] ? g_loaded[key].c_str() : g_englishBaked[key];
}

void DetectLanguage()
{
	std::ifstream file("settings.cfg");  // Kenshi's file, resolved via CWD
	if (file.is_open())
	{
		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == '#' || line[0] == '[')
				continue;
			size_t eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			std::string k = line.substr(0, eq);
			if (k != "language")
				continue;

			std::string lang = line.substr(eq + 1);
			DebugLog("[KenshiRotate] Detected language: " + lang);

			if      (lang.substr(0, 2) == "ja")    g_currentLang = LANG_JA;
			else if (lang.substr(0, 5) == "zh_CN" ||
			         lang.substr(0, 5) == "zh_SG") g_currentLang = LANG_ZH_CN;
			else if (lang.substr(0, 5) == "zh_TW" ||
			         lang.substr(0, 5) == "zh_HK") g_currentLang = LANG_ZH_TW;
			else if (lang.substr(0, 2) == "ko")    g_currentLang = LANG_KO;
			else if (lang.substr(0, 2) == "ru")    g_currentLang = LANG_RU;
			else if (lang.substr(0, 2) == "de")    g_currentLang = LANG_DE;
			else if (lang.substr(0, 2) == "fr")    g_currentLang = LANG_FR;
			else if (lang.substr(0, 2) == "es")    g_currentLang = LANG_ES;
			else if (lang.substr(0, 2) == "pt")    g_currentLang = LANG_PT;
			break;
		}
	}
	else
	{
		DebugLog("[KenshiRotate] Could not open settings.cfg, defaulting to English");
	}

	if (g_currentLang != LANG_EN)
		LoadLanguageFile(g_currentLang);
}
