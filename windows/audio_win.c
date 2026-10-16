#define COBJMACROS

#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include "headers/audio.h"

#define MAX_DEVICES 32
#define NAME_LEN    128
#define ID_LEN      128

typedef struct {
    char    name[NAME_LEN];
    wchar_t id[ID_LEN];
} Device;

typedef struct {
    Device  dev[MAX_DEVICES];
    int     count;
    wchar_t selected_id[ID_LEN]; // L"" = nothing selected 
} DeviceList;

struct Audio {
    DeviceList           lists[AUDIO_KIND_COUNT];
    int                  com_initialized;
    IMMDeviceEnumerator *pEnumerator;
};

static EDataFlow flow_of(AudioKind k) { return k == AUDIO_OUTPUT ? eRender : eCapture; }

static int find_by_id(const DeviceList* l, const wchar_t* id) {
    for(int i = 0;i < l->count;i++) {
        if(wcscmp(l->dev[i].id, id) == 0) return i;
    }
    return -1;
}

static int in_ids(wchar_t ids[MAX_DEVICES][ID_LEN], int n, wchar_t* id) {
    for(int i = 0;i < n;i++) {
        if(wcscmp(ids[i], id) == 0) return i;
    }
    return -1;
}

static void snapshot_ids(const DeviceList* l, wchar_t ids[MAX_DEVICES][ID_LEN], int* n) {
    *n = l->count;
    for (int i = 0; i < l->count; i++) wcscpy_s(ids[i], ID_LEN, l->dev[i].id);
}

static HRESULT select_default(Audio* a, AudioKind kind) {
    a->lists[kind].selected_id[0] = L'\0';
    IMMDevice  *pEndpoint = NULL;
    EDataFlow   data_flow = flow_of(kind);
    LPWSTR      pwszID = NULL;

    HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(a->pEnumerator, data_flow, eConsole, &pEndpoint);
    if(FAILED(hr)) goto cleanup;

    hr = IMMDevice_GetId(pEndpoint, &pwszID);
    if(FAILED(hr)) goto cleanup;

    wcscpy_s(a->lists[kind].selected_id, ID_LEN, pwszID);
    cleanup:
    if(pEndpoint) IMMDevice_Release(pEndpoint);
    if(pwszID) CoTaskMemFree(pwszID);

    return hr;
}

static HRESULT add_device(DeviceList* l, IMMDeviceCollection* pCollection, UINT i) {
    IMMDevice           *pEndpoint = NULL;
    LPWSTR               pwszID = NULL;
    IPropertyStore      *pProps = NULL;
    PROPVARIANT          var_name;
    PropVariantInit(&var_name);
    
    HRESULT hr = IMMDeviceCollection_Item(pCollection, i, &pEndpoint);
    if (FAILED(hr)) goto cleanup;

    hr = IMMDevice_GetId(pEndpoint, &pwszID);    
    if (FAILED(hr)) goto cleanup;

    hr = IMMDevice_OpenPropertyStore(pEndpoint, STGM_READ, &pProps);
    if (FAILED(hr)) goto cleanup;

    hr = IPropertyStore_GetValue(pProps, &PKEY_Device_FriendlyName, &var_name);
    if (FAILED(hr)) goto cleanup;
        
    if(var_name.vt == VT_LPWSTR) {
        int res = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, var_name.pwszVal, -1, l->dev[l->count].name, NAME_LEN, NULL, NULL);
        if(res == 0) {
            strcpy_s(l->dev[l->count].name, NAME_LEN, "Unnamed device");
        }
        wcscpy_s(l->dev[l->count].id  , ID_LEN  , pwszID);
        l->count++;
    }
    cleanup:
    PropVariantClear(&var_name);    
    CoTaskMemFree(pwszID);
    if(pProps)      IPropertyStore_Release(pProps);
    if(pEndpoint)   IMMDevice_Release(pEndpoint);   

    return hr;
}

static HRESULT populate_devices(Audio* a, AudioKind kind) {

    a->lists[kind].count = 0;
    EDataFlow            data_flow = (kind == AUDIO_OUTPUT) ? eRender : eCapture; 
    IMMDeviceCollection *pCollection = NULL;

    HRESULT hr = IMMDeviceEnumerator_EnumAudioEndpoints(a->pEnumerator, data_flow, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) goto cleanup;

    UINT count;
    hr = IMMDeviceCollection_GetCount(pCollection, &count);
    if (FAILED(hr)) goto cleanup;
    if(count == 0) goto cleanup;

    for(UINT i = 0;i < count && a->lists[kind].count < MAX_DEVICES;i++) {
        add_device(&a->lists[kind], pCollection, i); // result value ignored on purpose 
    }

    cleanup:
    if(pCollection) IMMDeviceCollection_Release(pCollection);
    return hr;
}

Audio* audio_create(void) {
    Audio* a = calloc(1, sizeof(Audio));
    if(!a) return NULL;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) goto cleanup;
    a->com_initialized = 1;

    hr = CoCreateInstance(
            &CLSID_MMDeviceEnumerator, NULL, 
            CLSCTX_ALL, &IID_IMMDeviceEnumerator,
            (void**)&a->pEnumerator);
    if (FAILED(hr)) goto cleanup;

    for(AudioKind k = 0;k < AUDIO_KIND_COUNT;k++) {
        hr = populate_devices(a, k);
        if (FAILED(hr)) goto cleanup;
        select_default(a, k); // ignoring the return value, on failure selected_id = L""
    }

    return a;

    cleanup: 
    audio_destroy(a);
    return NULL;
}

int audio_device_count(const Audio* a, AudioKind kind) {
    return a->lists[kind].count;
}

const char* audio_device_name(const Audio* a, AudioKind kind, int index) {
    const DeviceList *l = &a->lists[kind];
    if(index < 0 || index >= l->count) return NULL;
    return l->dev[index].name;
}

int audio_selected(const Audio* a, AudioKind kind) {
    const DeviceList* l = &a->lists[kind];
    return l->selected_id[0] ? find_by_id(l, l->selected_id) : -1;
}

AudioResult audio_select(Audio* a, AudioKind kind, int index) {
    DeviceList* l = &a->lists[kind];
    if(index < 0 || index >= l->count) return AUDIO_ERR_INVALID_ARG;
    wcscpy_s(l->selected_id, ID_LEN, l->dev[index].id);
    return AUDIO_OK;
}

AudioResult audio_refresh(Audio* a, AudioKind kind) {
    HRESULT hr;
    DeviceList* l = &a->lists[kind];
    AudioResult result = AUDIO_OK;

    wchar_t old_ids[MAX_DEVICES][ID_LEN];
    int     old_n;
    snapshot_ids(l, old_ids, &old_n);
    int had_selection = l->selected_id[0] != L'\0';

    hr = populate_devices(a, kind);
    if (FAILED(hr)) return AUDIO_ERR_FAILED;

    for (int i = 0; i < l->count; i++) {                    // in new, not in old
        if (in_ids(old_ids, old_n, l->dev[i].id) == -1) { result |= AUDIO_INFO_DEVICES_ADDED; break; }
    }
    for (int i = 0; i < old_n; i++) {                       // in old, not in new
        if (find_by_id(l, old_ids[i]) == -1) { result |= AUDIO_INFO_DEVICES_REMOVED; break; }
    }

    if (audio_selected(a, kind) == -1) {
        if (had_selection) result |= AUDIO_INFO_SELECTION_RESET;
        select_default(a, kind);
    }
    
    return result;
}

void audio_destroy(Audio* a) {
    if(!a) return;
    if(a->pEnumerator)     IMMDeviceEnumerator_Release(a->pEnumerator);
    if(a->com_initialized) CoUninitialize();
    free(a);
}


void console_init(void) {
    SetConsoleOutputCP(CP_UTF8);
}