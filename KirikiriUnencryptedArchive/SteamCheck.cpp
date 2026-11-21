#include "stdafx.h"
#include "SteamCheck.h"

#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wintrust.lib")

namespace fs = std::filesystem;

namespace
{
    bool SetReason(std::wstring* pOut, const wchar_t* msg)
    {
        if (pOut != nullptr)
            *pOut = msg;
        return false;
    }

    bool VerifyValveSignature(const fs::path& filePath, std::wstring* pReason)
    {
        WINTRUST_FILE_INFO fileInfo{};
        fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
        fileInfo.pcwszFilePath = filePath.c_str();

        GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA winTrustData{};
        winTrustData.cbStruct = sizeof(WINTRUST_DATA);
        winTrustData.dwUIChoice = WTD_UI_NONE;
        winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
        winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
        winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
        winTrustData.pFile = &fileInfo;
        winTrustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

        LONG trustStatus = WinVerifyTrust(nullptr, &policyGuid, &winTrustData);
        winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(nullptr, &policyGuid, &winTrustData);
        if (trustStatus != ERROR_SUCCESS)
            return SetReason(pReason, L"ไฟล์ steam_api.dll ไม่มี Digital Signature ที่ถูกต้องจาก Valve Corp.");

        HCERTSTORE hStore = nullptr;
        HCRYPTMSG hMsg = nullptr;
        BOOL queried = CryptQueryObject(
            CERT_QUERY_OBJECT_FILE,
            filePath.c_str(),
            CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
            CERT_QUERY_FORMAT_FLAG_BINARY,
            0,
            nullptr,
            nullptr,
            nullptr,
            &hStore,
            &hMsg,
            nullptr
        );
        if (!queried || hStore == nullptr || hMsg == nullptr)
        {
            if (hStore) CertCloseStore(hStore, 0);
            if (hMsg) CryptMsgClose(hMsg);
            return SetReason(pReason, L"ไม่สามารถอ่านใบรับรองจากไฟล์ steam_api.dll ได้");
        }

        bool isValve = false;
        DWORD signerInfoSize = 0;
        if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &signerInfoSize) && signerInfoSize > 0)
        {
            std::vector<BYTE> signerBuf(signerInfoSize);
            if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, signerBuf.data(), &signerInfoSize))
            {
                auto* pSignerInfo = reinterpret_cast<PCMSG_SIGNER_INFO>(signerBuf.data());
                CERT_INFO certInfo{};
                certInfo.Issuer = pSignerInfo->Issuer;
                certInfo.SerialNumber = pSignerInfo->SerialNumber;

                PCCERT_CONTEXT pCert = CertFindCertificateInStore(
                    hStore,
                    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                    0,
                    CERT_FIND_SUBJECT_CERT,
                    &certInfo,
                    nullptr
                );
                if (pCert != nullptr)
                {
                    wchar_t subjectBuf[512]{};
                    if (CertNameToStrW(X509_ASN_ENCODING, &pCert->pCertInfo->Subject, CERT_X500_NAME_STR, subjectBuf, _countof(subjectBuf)) > 0)
                    {
                        isValve = wcsstr(subjectBuf, L"Valve") != nullptr;
                    }
                    CertFreeCertificateContext(pCert);
                }
            }
        }

        CertCloseStore(hStore, 0);
        CryptMsgClose(hMsg);

        if (!isValve)
            return SetReason(pReason, L"ไฟล์ steam_api.dll ไม่ใช่ของแท้จาก Valve Corp. (ตรวจพบไฟล์ Crack)");

        return true;
    }
}

bool SteamCheck::VerifyGenuineSteamGame(std::wstring* pFailReason)
{
    std::error_code ec;
    fs::path gameDir(Path::GetModuleFolderPath(nullptr));

    // Check 1: steam_api.dll must exist
    fs::path steamApi = gameDir / L"steam_api.dll";
    if (!fs::is_regular_file(steamApi, ec))
        return SetReason(pFailReason, L"ไม่พบไฟล์ steam_api.dll");

    // Check 2: Valid Authenticode signature from Valve
    if (!VerifyValveSignature(steamApi, pFailReason))
        return false;

    // Check 3: Reject known Steam emulator / crack artifacts
    static constexpr const wchar_t* kCrackArtifacts[] = {
        L"steam_settings",
        L"steamclient.dll",
        L"steamclient.ini",
        L"steam_interfaces.txt",
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
