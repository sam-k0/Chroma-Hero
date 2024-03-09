#pragma once
#include <Windows.h>
#include <string>
#include "Filesystem.h"

// writes value to a file
// bool: success
bool WriteIniValue(std::string filename, std::string section, std::string keyname, std::string value)
{
    // Change settings in "options"
    return (WritePrivateProfileStringA(section.c_str(), keyname.c_str(), value.c_str(), filename.c_str()));
}

// Reads value from file
// bool: success
// if error occured, outValue is set to empty string
std::string ReadIniValue(std::string filename, std::string section, std::string keyname, std::string def)
{
    char buffer[256]; // Adjust the buffer size as needed
    GetPrivateProfileStringA(section.c_str(), keyname.c_str(), def.c_str(), buffer, sizeof(buffer), filename.c_str());
    return std::string(buffer);
}

bool KeySectionExists(const std::string& filename, const std::string& section, const std::string& keyname)
{
    char buffer[256]; // Adjust the buffer size as needed
    DWORD bytesRead = GetPrivateProfileStringA(section.c_str(), keyname.c_str(), "", buffer, sizeof(buffer), filename.c_str());

    // If bytesRead > 0, the key exists in the section
    return bytesRead > 0;
}


int ReadIntFromIni(const std::string& filename, const std::string& section, const std::string& keyname, int defaultValue = 0)
{
    char buffer[256]; // Adjust the buffer size as needed
    GetPrivateProfileStringA(section.c_str(), keyname.c_str(), std::to_string(defaultValue).c_str(), buffer, sizeof(buffer), filename.c_str());

    // Convert the string buffer to an integer
    return std::stoi(buffer);
}
