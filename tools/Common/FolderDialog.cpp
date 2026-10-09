#include "FolderDialog.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shobjidl.h>

#include <wrl/client.h>

std::optional<std::filesystem::path> PickFolder(const std::filesystem::path& initial)
{
    // COM(Windows の部品の仕組み)を、この関数の間だけ、使えるようにする
    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool needsUninitialize = SUCCEEDED(initResult); // RPC_E_CHANGED_MODE のときは、すでに別の形で初期化済み(そのまま使う)

    std::optional<std::filesystem::path> result;
    {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        {
            DWORD options = 0;
            dialog->GetOptions(&options);
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM); // ファイルではなく、フォルダーを選ぶ

            // 最初に開くフォルダー
            if (!initial.empty())
            {
                Microsoft::WRL::ComPtr<IShellItem> folder;
                if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder))))
                {
                    dialog->SetFolder(folder.Get());
                }
            }

            if (SUCCEEDED(dialog->Show(nullptr))) // キャンセルすると、失敗が返る
            {
                Microsoft::WRL::ComPtr<IShellItem> item;
                if (SUCCEEDED(dialog->GetResult(&item)))
                {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path)
                    {
                        result = std::filesystem::path(path);
                        CoTaskMemFree(path);
                    }
                }
            }
        }
    }

    if (needsUninitialize)
    {
        CoUninitialize();
    }
    return result;
}
