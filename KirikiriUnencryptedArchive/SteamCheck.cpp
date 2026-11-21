#include "stdafx.h"
#include "SteamCheck.h"

namespace fs = std::filesystem;

bool SteamCheck::VerifyGenuineSteamGame(std::wstring* pFailReason)
{
    if (GetModuleHandleW(L"SmartSteamEmu.dll") != nullptr ||
        GetModuleHandleW(L"SmartSteamEmu64.dll") != nullptr)
    {
        if (pFailReason != nullptr)
            *pFailReason = L"ตรวจพบ Steam Emulator กำลังทำงานอยู่ (SmartSteamEmu)";
        return false;
    }

    std::error_code ec;
    fs::path gameDir(Path::GetModuleFolderPath(nullptr));

    static constexpr const wchar_t* kCrackArtifacts[] = {
        L"SmartSteamEmu.dll",
        L"SmartSteamEmu64.dll",
        L"SmartSteamEmu.ini",
        L"SmartSteamEmu.txt",
        L"SmartSteamLoader.exe",
        L"SSELauncher.exe",
        L"SteamEmu",
        L"ds.ini",
        L"steam_settings",
        L"steamclient.dll",
        L"steamclient.ini",
        L"steam_interfaces.txt",
        L"ColdClientLoader.ini",
        L"steam_emu.ini",
        L"ALI213.ini",
        L"codex.ini",
        L"cream_api.ini",
        L"SmokeAPI.config",
        L"local_save"
    };

    for (const wchar_t* artifact : kCrackArtifacts)
    {
        if (fs::exists(gameDir / artifact, ec))
        {
            if (pFailReason != nullptr)
                *pFailReason = std::wstring(L"ตรวจพบไฟล์จำลอง Steam เถื่อน (") + artifact + L")";
            return false;
        }
    }

    return true;
}
