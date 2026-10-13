#define COBJMACROS

#include <stdlib.h>
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
    Device dev[MAX_DEVICES];
    int    count;
    int    selected;
} DeviceList;

struct Audio {
    DeviceList           lists[AUDIO_KIND_COUNT];
    int                  com_initialized;
    IMMDeviceEnumerator *pEnumerator;
};

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

    for(int k = 0;k < AUDIO_KIND_COUNT;k++) {
        a->lists[k].selected = -1;
    }

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
    }

    return a;

    cleanup: 
    audio_destroy(a);
    return NULL;
}

void audio_destroy(Audio* a) {
    if(!a) return;
    if(a->pEnumerator)     IMMDeviceEnumerator_Release(a->pEnumerator);
    if(a->com_initialized) CoUninitialize();
    free(a);
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
    return a->lists[kind].selected;
}

int audio_select(Audio* a, AudioKind kind, int index) {
    DeviceList* l = &a->lists[kind];
    if(index < 0 || index >= l->count) return -1;
    l->selected = index;

    return 0;
}

int audio_refresh(Audio* a) {
    
    HRESULT hr;

    for(AudioKind k = 0;k < AUDIO_KIND_COUNT;k++) {
        hr = populate_devices(a, k);
        if (FAILED(hr)) goto cleanup;
    }

    cleanup: 
        audio_destroy(a);
}

void console_init(void) {
    SetConsoleOutputCP(CP_UTF8);
}