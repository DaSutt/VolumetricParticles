/*
MIT License

Copyright(c) 2017 Daniel Suttor

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "FileDialog.h"

#include <wrl.h>
#include <ShObjIdl.h>
#include <codecvt>
#include <Shlwapi.h>
#include <algorithm>

using namespace Microsoft::WRL;

namespace FileIO
{
  std::string ConvertUTF16StringToUTF8String(const std::wstring& input)
  {
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::string result = converter.to_bytes(input);
    return result;
  }

  // https://msdn.microsoft.com/en-us/library/windows/desktop/ff485843%28v=vs.85%29.aspx
  std::string OpenFileDialog(const std::wstring fileExtension)
  {
    std::string filePath;

    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
      COINIT_DISABLE_OLE1DDE)))
    {

      ComPtr<IFileOpenDialog> fileOpenDialog;
      HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(fileOpenDialog.GetAddressOf()));
      if (SUCCEEDED(hr))
      {
				if (!fileExtension.empty())
				{
					const std::wstring fileExtensionFilter = L"*." + fileExtension;
					COMDLG_FILTERSPEC filterSpec[] =
					{
						{ fileExtension.data(), fileExtensionFilter.data() }
					};
					
					hr = fileOpenDialog->SetFileTypes(1, filterSpec);
					if (FAILED(hr))
					{
						printf("Failed setting default extension for file open dialog\n");
					}
				}
        hr = fileOpenDialog->Show(NULL);
        if (SUCCEEDED(hr))
        {
          ComPtr<IShellItem> shellItem;
          hr = fileOpenDialog->GetResult(shellItem.GetAddressOf());
          if (SUCCEEDED(hr))
          {
            PWSTR path;
            hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (SUCCEEDED(hr))
            {
              filePath = ConvertUTF16StringToUTF8String(path);
              CoTaskMemFree(path);
            }
          }
        }
      }
    }

    return filePath;
  }

  std::string SaveFileDialog(const std::wstring extension)
  {
    std::string filePath;

    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
      COINIT_DISABLE_OLE1DDE)))
    {
      ComPtr<IFileSaveDialog> fileSaveDialog;
      HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(fileSaveDialog.GetAddressOf()));
      if (SUCCEEDED(hr))
      {
				if (!extension.empty())
				{
					const std::wstring fileExtensionFilter = L"*." + extension;
					COMDLG_FILTERSPEC filterSpec[] =
					{
						{ extension.data(), fileExtensionFilter.data() }
					};

					hr = fileSaveDialog->SetFileTypes(1, filterSpec);
					if (FAILED(hr))
					{
						printf("Failed setting default extension for file open dialog\n");
					}
					hr = fileSaveDialog->SetDefaultExtension(extension.data());
				}
        hr = fileSaveDialog->Show(NULL);
        if (SUCCEEDED(hr))
        {
          ComPtr<IShellItem> shellItem;
          hr = fileSaveDialog->GetResult(shellItem.GetAddressOf());
          if (SUCCEEDED(hr))
          {
            PWSTR path;
            hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (SUCCEEDED(hr))
            {
							filePath = ConvertUTF16StringToUTF8String(path);
							
              CoTaskMemFree(path);
            }
          }
        }
      }
    }

    return filePath;
  }

  std::string GetFileName(const std::string& path)
  {
    std::string fileName;
    const auto fileNameStart = PathFindFileNameA(path.c_str());
    if (fileNameStart != path.c_str())
    {
      PathRemoveExtensionA(fileNameStart);
      fileName = std::string(fileNameStart);
    }
    return fileName;
  }

	std::string GetFileNameWithExtension(const std::string& path)
	{
		std::string fileName;
		const auto fileNameStart = PathFindFileNameA(path.c_str());
		if (fileNameStart != path.c_str())
		{
			fileName = std::string(fileNameStart);
		}
		return fileName;
	}

	std::string RemoveExtension(const std::string& path)
	{
		const auto pos = path.find_last_of(".");
		return path.substr(0, pos + 1);
	}

	std::string ReplaceExtension(const std::string& path, const std::string& newExtension)
	{
		return RemoveExtension(path) + newExtension;
	}

	std::string ChangeToBackSlashes(const std::string& path)
	{
		std::string result = path;
		std::replace(result.begin(), result.end(), '\\', '/');
		return result;
	}
}