#include "stdafx.h"
#include "SteamCheck.h"

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Proxy::Init();

        Debugger::RegisterDllLoadHandler(
            [](const wchar_t* pwszDllPath, HMODULE hDll)
            {
                if (Debugger::FindExport(hDll, "V2Link") != nullptr)
                    Patcher::PatchSignatureCheck(hDll);
            }
        );

        Kirikiri::Init(
            []
            {
                std::wstring failReason;
                if (!SteamCheck::VerifyGenuineSteamGame(&failReason))
                {
                    Debugger::Log(L"Steam authenticity check failed: %s", failReason.c_str());
                    std::wstring msg =
                        L"ม็อดภาษาไทยนี้รองรับเฉพาะตัวเกมแท้บน Steam เท่านั้น\n\n"
                        L"สาเหตุ: " + failReason + L"\n\n"
                        L"กรุณาติดตั้งและเปิดเล่นผ่านตัวเกมแท้บน Steam";
                    MessageBoxW(nullptr, msg.c_str(), L"Thai Mod", MB_OK | MB_ICONWARNING);
                    return;
                }

                CompilerHelper::Init();
                Patcher::PatchXP3StreamCreation();
                Patcher::PatchAutoPathExports();
                Patcher::PatchStorageMediaRegistration();
            }
        );
    }

    return TRUE;
}
