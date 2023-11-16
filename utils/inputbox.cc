#include <utils/inputbox.h>

// Windows API
#include <windows.h>

// VBScript InputBox
#include <atlbase.h>
#include <activscp.h>
#include <comdef.h>

// UTF-8 Support
#include <cwchar>
#include <string>
#include <vector>

using std::string;
using std::wstring;
using std::vector;

static wstring StringWiden(const string&Str) {
    const size_t szCount = Str.size() + 1;
    vector<wchar_t> Buffer(szCount);
    return wstring{
        Buffer.data(), static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, Str.c_str(), -1, Buffer.data(), szCount))
    };
}

static string StringNarrow(const wstring&Str) {
    const int nBytes = static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, Str.c_str(), (int)Str.length(), nullptr, 0,
                                                               nullptr, nullptr));
    vector<char> Buffer(static_cast<size_t>(nBytes));
    return string{
        Buffer.data(),
        static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, Str.c_str(), (int)Str.length(), Buffer.data(), nBytes,
                                                nullptr,
                                                nullptr))
    };
}

static string StringReplaceAll(string Str, const string&SubStr, const string&NewStr) {
    size_t Position = 0;
    const size_t SubLen = SubStr.length(), NewLen = NewStr.length();
    while ((Position = Str.find(SubStr, Position)) != string::npos) {
        Str.replace(Position, SubLen, NewStr);
        Position += NewLen;
    }
    return Str;
}

static string CPPNewLineToVBSNewLine(string NewLine) {
    size_t Position = 0;
    while (Position < NewLine.length()) {
        if (NewLine[Position] == '\r' || NewLine[Position] == '\n')
            NewLine.replace(Position, 2, "\" + vbNewLine + \"");
        Position += 1;
    }
    return NewLine;
}

class CSimpleScriptSite final :
        public IActiveScriptSite,
        public IActiveScriptSiteWindow {
public:
    CSimpleScriptSite() : m_cRefCount(1), m_hWnd(nullptr) {
    }

    // IUnknown
    STDMETHOD_(ULONG, AddRef)();

    STDMETHOD_(ULONG, Release)();

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject);

    // IActiveScriptSite
    STDMETHOD(GetLCID)(LCID* plcid) {
        *plcid = 0;
        return S_OK;
    }

    STDMETHOD(GetItemInfo)(LPCOLESTR pstrName, DWORD dwReturnMask, IUnknown** ppiunkItem,
                           ITypeInfo** ppti) { return TYPE_E_ELEMENTNOTFOUND; }

    STDMETHOD(GetDocVersionString)(BSTR* pbstrVersion) {
        *pbstrVersion = SysAllocString(L"1.0");
        return S_OK;
    }

    STDMETHOD(OnScriptTerminate)(const VARIANT* pvarResult, const EXCEPINFO* pexcepinfo) { return S_OK; }

    STDMETHOD(OnStateChange)(SCRIPTSTATE ssScriptState) { return S_OK; }

    STDMETHOD(OnScriptError)(IActiveScriptError* pIActiveScriptError) { return S_OK; }

    STDMETHOD(OnEnterScript)(void) { return S_OK; }

    STDMETHOD(OnLeaveScript)(void) { return S_OK; }

    // IActiveScriptSiteWindow
    STDMETHOD(GetWindow)(HWND* phWnd) {
        *phWnd = m_hWnd;
        return S_OK;
    }

    STDMETHOD(EnableModeless)(BOOL fEnable) { return S_OK; }

    // Miscellaneous
    STDMETHOD(SetWindow)(const HWND hWnd) {
        m_hWnd = hWnd;
        return S_OK;
    }

public:
    LONG m_cRefCount;
    HWND m_hWnd;
};

STDMETHODIMP_(ULONG) CSimpleScriptSite::AddRef() {
    return InterlockedIncrement(&m_cRefCount);
}

STDMETHODIMP_(ULONG) CSimpleScriptSite::Release() {
    if (!InterlockedDecrement(&m_cRefCount)) {
        delete this;
        return 0;
    }
    return m_cRefCount;
}

STDMETHODIMP CSimpleScriptSite::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IActiveScriptSiteWindow) {
        *ppvObject = static_cast<IActiveScriptSiteWindow *>(this);
        AddRef();
        return NOERROR;
    }
    if (riid == IID_IActiveScriptSite) {
        *ppvObject = static_cast<IActiveScriptSite *>(this);
        AddRef();
        return NOERROR;
    }
    return E_NOINTERFACE;
}

static HHOOK hHook = nullptr;
static bool HideInput = false;

static LRESULT CALLBACK InputBoxProc(const int nCode, const WPARAM wParam, const LPARAM lParam) {
    if (nCode < HC_ACTION)
        return CallNextHookEx(hHook, nCode, wParam, lParam);
    if (nCode == HCBT_ACTIVATE) {
        if (HideInput == true) {
            const HWND TextBox = FindWindowExA(reinterpret_cast<HWND>(wParam), nullptr, "Edit", nullptr);
            SendDlgItemMessageW(reinterpret_cast<HWND>(wParam), GetDlgCtrlID(TextBox), EM_SETPASSWORDCHAR, L'\x25cf',
                                0);
        }
    }
    if (nCode == HCBT_CREATEWND) {
        if (!(GetWindowLongPtr(reinterpret_cast<HWND>(wParam), GWL_STYLE) & WS_CHILD))
            SetWindowLongPtr(reinterpret_cast<HWND>(wParam), GWL_EXSTYLE,
                             GetWindowLongPtr(reinterpret_cast<HWND>(wParam), GWL_EXSTYLE) | WS_EX_DLGMODALFRAME);
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

static char* InputBoxHelper(const char* Prompt, const char* Title, const char* Default) {
    // Initialize
    HRESULT hr = S_OK;
    hr = CoInitialize(nullptr);
    auto* pScriptSite = new CSimpleScriptSite();
    CComPtr<IActiveScript> spVBScript;
    CComPtr<IActiveScriptParse> spVBScriptParse;
    hr = spVBScript.CoCreateInstance(OLESTR("VBScript"));
    hr = spVBScript->SetScriptSite(pScriptSite);
    hr = spVBScript->QueryInterface(&spVBScriptParse);
    hr = spVBScriptParse->InitNew();

    // Replace quotes with double quotes
    string strPrompt = StringReplaceAll(Prompt, "\"", "\"\"");
    string strTitle = StringReplaceAll(Title, "\"", "\"\"");
    string strDefault = StringReplaceAll(Default, "\"", "\"\"");

    // Create evaluation string
    string Evaluation = "InputBox(\"" + strPrompt + "\", \"" + strTitle + "\", \"" + strDefault + "\")";
    Evaluation = CPPNewLineToVBSNewLine(Evaluation);
    wstring WideEval = StringWiden(Evaluation);

    // Run InpuBox
    CComVariant result;
    EXCEPINFO ei = {};
    DWORD ThreadID = GetCurrentThreadId();
    HINSTANCE ModHwnd = GetModuleHandle(nullptr);
    hr = pScriptSite->SetWindow(GetAncestor(GetActiveWindow(), GA_ROOTOWNER));
    hHook = SetWindowsHookEx(WH_CBT, &InputBoxProc, ModHwnd, ThreadID);
    hr = spVBScriptParse->ParseScriptText(WideEval.c_str(), nullptr, nullptr, nullptr, 0, 0, SCRIPTTEXT_ISEXPRESSION,
                                          &result,
                                          &ei);
    UnhookWindowsHookEx(hHook);

    // Cleanup
    spVBScriptParse = nullptr;
    spVBScript = nullptr;
    pScriptSite->Release();
    pScriptSite = nullptr;
    CoUninitialize();

    // Result
    static string strResult;
    _bstr_t bstrResult{result};
    strResult = StringNarrow(static_cast<wchar_t *>(bstrResult));
    return const_cast<char *>(strResult.c_str());
}

char* utils::InputBox(const char* Prompt, const char* Title, const char* Default) {
    HideInput = false;
    return InputBoxHelper(Prompt, Title, Default);
}

char* utils::PasswordBox(const char* Prompt, const char* Title, const char* Default) {
    HideInput = true;
    return InputBoxHelper(Prompt, Title, Default);
}
