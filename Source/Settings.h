#pragma once
#include <juce_data_structures/juce_data_structures.h>

// Settings that belong to the person, not to a project or a preset: interface scale, band colours, film grain, the first-run hint.
// One small file shared by every instance (Application Support/Dypelv/Dypelv EQ.settings, %APPDATA%\Dypelv on Windows).
namespace Settings
{
    inline juce::PropertiesFile& file()
    {
        static std::unique_ptr<juce::PropertiesFile> pf = []
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "Dypelv EQ";
            o.filenameSuffix = ".settings";
            o.folderName = "Dypelv";
            o.osxLibrarySubFolder = "Application Support";
            o.commonToAllUsers = false;
            o.storageFormat = juce::PropertiesFile::storeAsXML;
            o.millisecondsBeforeSaving = 400;
            return std::make_unique<juce::PropertiesFile> (o);
        }();
        return *pf;
    }

    inline int  getInt  (const char* key, int def)   { return file().getIntValue (key, def); }
    inline bool getBool (const char* key, bool def)  { return file().getBoolValue (key, def); }
    inline void setInt  (const char* key, int v)     { file().setValue (key, v);  file().saveIfNeeded(); }
    inline void setBool (const char* key, bool v)    { file().setValue (key, v);  file().saveIfNeeded(); }
}
