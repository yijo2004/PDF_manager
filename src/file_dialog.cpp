#include "file_dialog.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>

namespace
{
    // Convert UTF-16 (Windows wide string) to UTF-8
    std::string WideToUtf8(const std::wstring& wstr)
    {
        if (wstr.empty()) return std::string();
        
        int size_needed = WideCharToMultiByte(
            CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
            nullptr, 0, nullptr, nullptr);
        
        std::string result(size_needed, 0);
        WideCharToMultiByte(
            CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
            &result[0], size_needed, nullptr, nullptr);
        
        return result;
    }

    // Convert UTF-8 to UTF-16 (Windows wide string)
    std::wstring Utf8ToWide(const std::string& str)
    {
        if (str.empty()) return std::wstring();
        
        int size_needed = MultiByteToWideChar(
            CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
            nullptr, 0);
        
        std::wstring result(size_needed, 0);
        MultiByteToWideChar(
            CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
            &result[0], size_needed);
        
        return result;
    }
}

namespace FileDialog
{
    std::string OpenPDF()
    {
        return Open("PDF Files", "*.pdf");
    }

    std::string Open(const char* filterName, const char* filterPattern)
    {
        std::string result;

        // Build filter string: "Name\0Pattern\0All Files\0*.*\0\0"
        std::wstring filterNameW = Utf8ToWide(filterName);
        std::wstring filterPatternW = Utf8ToWide(filterPattern);
        
        // Windows filter format requires double-null terminated strings
        std::wstring filter;
        filter += filterNameW + L" (" + filterPatternW + L")";
        filter.push_back(L'\0');
        filter += filterPatternW;
        filter.push_back(L'\0');
        filter += L"All Files (*.*)";
        filter.push_back(L'\0');
        filter += L"*.*";
        filter.push_back(L'\0');

        wchar_t filename[MAX_PATH] = L"";

        OPENFILENAMEW ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Select a File";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameW(&ofn))
        {
            result = WideToUtf8(filename);
        }

        return result;
    }

    std::string OpenFolder()
    {
        std::string result;
        
        // Use modern IFileDialog for folder selection
        IFileDialog* pfd = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, 
                                       IID_PPV_ARGS(&pfd));
        
        if (SUCCEEDED(hr))
        {
            // Set options to pick folders
            DWORD dwOptions;
            hr = pfd->GetOptions(&dwOptions);
            if (SUCCEEDED(hr))
            {
                hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            }
            
            // Set title
            if (SUCCEEDED(hr))
            {
                hr = pfd->SetTitle(L"Select PDF Folder");
            }
            
            // Show dialog
            if (SUCCEEDED(hr))
            {
                hr = pfd->Show(NULL);
            }
            
            // Get result
            if (SUCCEEDED(hr))
            {
                IShellItem* psi = nullptr;
                hr = pfd->GetResult(&psi);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszPath = nullptr;
                    hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr) && pszPath)
                    {
                        result = WideToUtf8(pszPath);
                        CoTaskMemFree(pszPath);
                    }
                    psi->Release();
                }
            }
            
            pfd->Release();
        }
        
        return result;
    }
}

#else
// Non-Windows stub implementations
// TODO: Implement for Linux (GTK/Qt) and macOS (NSOpenPanel)

#include <cstdio>

namespace FileDialog
{
    std::string OpenPDF()
    {
        printf("[FileDialog] File dialogs not implemented for this platform.\n");
        return "";
    }

    std::string Open(const char* /*filterName*/, const char* /*filterPattern*/)
    {
        printf("[FileDialog] File dialogs not implemented for this platform.\n");
        return "";
    }

    std::string OpenFolder()
    {
        printf("[FileDialog] Folder dialogs not implemented for this platform.\n");
        return "";
    }
}
#endif
