#include "stdafx.h"
#include "FmvDetection.h"

#include <cstring>
#include <mutex>
#include <set>
#include <mfidl.h>
#include <mfobjects.h>

namespace FmvDetection
{
    using CreateMediaSession_t = HRESULT (WINAPI*)(IMFAttributes*, IMFMediaSession**);
    using GetEvent_t = HRESULT (STDMETHODCALLTYPE*)(IMFMediaSession*, DWORD, IMFMediaEvent**);
    using EndGetEvent_t = HRESULT (STDMETHODCALLTYPE*)(IMFMediaSession*, IMFAsyncResult*, IMFMediaEvent**);
    using Shutdown_t = HRESULT (STDMETHODCALLTYPE*)(IMFMediaSession*);

    // IMFMediaSession vtable: IUnknown (0-2), IMFMediaEventGenerator (3-6), then IMFMediaSession (7-16)
    constexpr size_t SLOT_GET_EVENT = 3;
    constexpr size_t SLOT_END_GET_EVENT = 5;
    constexpr size_t SLOT_SHUTDOWN = 13;

    static void (*onChange)(bool);
    static CreateMediaSession_t origCreateMediaSession;
    static GetEvent_t origGetEvent;
    static EndGetEvent_t origEndGetEvent;
    static Shutdown_t origShutdown;
    static void** hookedVtable;

    static std::mutex mutex;
    static std::set<IMFMediaSession*> playingSessions;

    static void SetPlaying(IMFMediaSession* session, bool playing) {
        std::lock_guard<std::mutex> lock(mutex);
        const bool wasPlaying = !playingSessions.empty();

        if (playing)
            playingSessions.insert(session);
        else
            playingSessions.erase(session);

        const bool isPlaying = !playingSessions.empty();
        if (isPlaying != wasPlaying)
            onChange(isPlaying);
    }

    static void OnSessionEvent(IMFMediaSession* session, IMFMediaEvent* event) {
        MediaEventType type;
        HRESULT status;
        if (FAILED(event->GetType(&type)) || FAILED(event->GetStatus(&status)))
            return;

        switch (type) {
            case MESessionStarted:
                SetPlaying(session, SUCCEEDED(status));
                break;
            // MESessionPaused is deliberately absent: a paused FMV is still on screen and keeps its bars
            case MESessionStopped:
            case MESessionEnded:
            case MESessionClosed:
                SetPlaying(session, false);
                break;
            default:
                break;
        }
    }

    static HRESULT STDMETHODCALLTYPE GetEvent_Hook(IMFMediaSession* self, DWORD flags, IMFMediaEvent** event) {
        HRESULT hr = origGetEvent(self, flags, event);
        if (SUCCEEDED(hr) && event != nullptr && *event != nullptr)
            OnSessionEvent(self, *event);
        return hr;
    }

    // WmfMedia reads session events asynchronously, so this is the path that normally fires
    static HRESULT STDMETHODCALLTYPE EndGetEvent_Hook(IMFMediaSession* self, IMFAsyncResult* result, IMFMediaEvent** event) {
        HRESULT hr = origEndGetEvent(self, result, event);
        if (SUCCEEDED(hr) && event != nullptr && *event != nullptr)
            OnSessionEvent(self, *event);
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Shutdown_Hook(IMFMediaSession* self) {
        SetPlaying(self, false);
        return origShutdown(self);
    }

    // Every media session is an instance of the same Media Foundation class, so hooking the first
    // session's vtable covers all of them
    static void HookSessionVtable(IMFMediaSession* session) {
        void** vtable = *reinterpret_cast<void***>(session);

        std::lock_guard<std::mutex> lock(mutex);
        if (hookedVtable != nullptr)
            return;

        origGetEvent = reinterpret_cast<GetEvent_t>(vtable[SLOT_GET_EVENT]);
        origEndGetEvent = reinterpret_cast<EndGetEvent_t>(vtable[SLOT_END_GET_EVENT]);
        origShutdown = reinterpret_cast<Shutdown_t>(vtable[SLOT_SHUTDOWN]);

        DWORD protect;
        const size_t size = (SLOT_SHUTDOWN - SLOT_GET_EVENT + 1) * sizeof(void*);
        if (!VirtualProtect(vtable + SLOT_GET_EVENT, size, PAGE_READWRITE, &protect))
            return;
        vtable[SLOT_GET_EVENT] = reinterpret_cast<void*>(&GetEvent_Hook);
        vtable[SLOT_END_GET_EVENT] = reinterpret_cast<void*>(&EndGetEvent_Hook);
        vtable[SLOT_SHUTDOWN] = reinterpret_cast<void*>(&Shutdown_Hook);
        VirtualProtect(vtable + SLOT_GET_EVENT, size, protect, &protect);

        hookedVtable = vtable;
    }

    static HRESULT WINAPI CreateMediaSession_Hook(IMFAttributes* config, IMFMediaSession** session) {
        // Resolved here rather than at install time: Unreal delay-loads mf.dll, so the import slot we
        // replaced may only have held the delay-load stub
        if (origCreateMediaSession == nullptr) {
            HMODULE mf = LoadLibraryW(L"mf.dll");
            if (mf != nullptr)
                origCreateMediaSession = reinterpret_cast<CreateMediaSession_t>(GetProcAddress(mf, "MFCreateMediaSession"));
            if (origCreateMediaSession == nullptr)
                return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
        }

        HRESULT hr = origCreateMediaSession(config, session);
        if (SUCCEEDED(hr) && session != nullptr && *session != nullptr)
            HookSessionVtable(*session);
        return hr;
    }

    static bool IsCreateMediaSessionImport(BYTE* base, ULONGLONG nameThunk) {
        if (IMAGE_SNAP_BY_ORDINAL(nameThunk))
            return false;
        auto import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk);
        return strcmp(reinterpret_cast<const char*>(import->Name), "MFCreateMediaSession") == 0;
    }

    static int HookImportTable(BYTE* base, DWORD nameTableRva, DWORD addressTableRva) {
        auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + nameTableRva);
        auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + addressTableRva);
        int hooked = 0;

        for (; names->u1.AddressOfData != 0; names++, slots++) {
            if (!IsCreateMediaSessionImport(base, names->u1.AddressOfData))
                continue;

            DWORD protect;
            if (!VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &protect))
                continue;
            slots->u1.Function = reinterpret_cast<ULONGLONG>(&CreateMediaSession_Hook);
            VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), protect, &protect);
            hooked++;
        }
        return hooked;
    }

    bool Install(void (*onChangeCallback)(bool playing)) {
        onChange = onChangeCallback;

        auto base = reinterpret_cast<BYTE*>(GetModuleHandle(nullptr));
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        const auto& directories = nt->OptionalHeader.DataDirectory;
        int hooked = 0;

        const auto& imports = directories[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (imports.VirtualAddress != 0) {
            for (auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress); desc->Name != 0; desc++) {
                if (desc->OriginalFirstThunk != 0)
                    hooked += HookImportTable(base, desc->OriginalFirstThunk, desc->FirstThunk);
            }
        }

        // Unreal's WmfMedia plugin delay-loads mf.dll, so the import normally lives in this table
        const auto& delayImports = directories[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
        if (delayImports.VirtualAddress != 0) {
            for (auto desc = reinterpret_cast<IMAGE_DELAYLOAD_DESCRIPTOR*>(base + delayImports.VirtualAddress); desc->DllNameRVA != 0; desc++) {
                if (desc->Attributes.RvaBased)
                    hooked += HookImportTable(base, desc->ImportNameTableRVA, desc->ImportAddressTableRVA);
            }
        }

        return hooked > 0;
    }
}
