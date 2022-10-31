/*++

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF

--*/

#include "driver.h"
#include "driver.tmh"

#include <stdio.h>

using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD EventDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY EventDeviceD0Entry;

EVT_IDD_CX_DEVICE_IO_CONTROL EventDeviceIoControl;
EVT_IDD_CX_ADAPTER_INIT_FINISHED EventAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES EventAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION EventParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES EventMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES UniShareMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN EventMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN EventMonitorUnassignSwapChain;

#pragma region Logger

//#define VERBOSE_LOG

FILE* LogFile = NULL;

void OpenLogFile()
{
#ifdef VERBOSE_LOG
    LogFile = fopen("D:\\WinVirtualDisplay.log", "w");
#endif // VERBOSE_LOG
}

void CloseLogFile()
{
#ifdef VERBOSE_LOG
    fclose(LogFile);
#endif // VERBOSE_LOG
}

void WriteLogFile(char* fmt, ...)
{
#ifdef VERBOSE_LOG
    va_list args = NULL;
    va_start(args, fmt);
    size_t length = _vscprintf(fmt, args) + 1;
    char* line = new char[length];
    _vsnprintf_s(line, length, length, fmt, args);
    va_end(args);

    line[length - 1] = '\n';
    fwrite(line, length, sizeof(char), LogFile);
    fflush(LogFile);
    
    delete[] line;
#endif // VERBOSE_LOG
}

#pragma endregion

struct IndirectDeviceContextWrapper
{
    IndirectDeviceContext* pContext;

    void Cleanup()
    {
        WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

        delete pContext;
        pContext = nullptr;

        CloseLogFile();
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

    WDF_DRIVER_CONFIG_INIT(&Config,
        EventDeviceAdd
    );

    Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    OpenLogFile();

    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    return Status;
}

_Use_decl_annotations_
NTSTATUS EventDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    // Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = EventDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue.
    IddConfig.EvtIddCxDeviceIoControl = EventDeviceIoControl;

    IddConfig.EvtIddCxAdapterInitFinished = EventAdapterInitFinished;

    IddConfig.EvtIddCxParseMonitorDescription = EventParseMonitorDescription;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = EventMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes = UniShareMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes = EventAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain = EventMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain = EventMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
    Attr.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    WDFDEVICE Device = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = IddCxDeviceInitialize(Device);

    // Create a new device context object and attach it to the WDF device object
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext = new IndirectDeviceContext(Device);

    //// {DE54DC57-4676-4EA6-B748-94EBADE0C36D}
    //GUID interfaceClassGUID{ 0xde54dc57, 0x4676, 0x4ea6, { 0xb7, 0x48, 0x94, 0xeb, 0xad, 0xe0, 0xc3, 0x6d } };
    //Status = WdfDeviceCreateDeviceInterface(Device, &interfaceClassGUID, NULL);
    DECLARE_CONST_UNICODE_STRING(symbolicLink, L"\\DosDevices\\Global\\WinVirtualDisplay");
    WdfDeviceCreateSymbolicLink(Device, &symbolicLink);

    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    return Status;
}

_Use_decl_annotations_
NTSTATUS EventDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    UNREFERENCED_PARAMETER(PreviousState);

    // This function is called by WDF to start the device in the fully-on power state.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext->InitAdapter();

    return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);
}

//Direct3DDevice::Direct3DDevice()
//{
//
//}

HRESULT Direct3DDevice::Init()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> Device, HANDLE NewFrameEvent)
    : m_hSwapChain(hSwapChain), m_Device(Device), m_hAvailableBufferEvent(NewFrameEvent)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristics(L"Distribution", &AvTask);

    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    // Get the DXGI device interface
    ComPtr<IDXGIDevice> DxgiDevice;
    HRESULT hr = m_Device->Device.As(&DxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = DxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if (FAILED(hr))
    {
        return;
    }

    // Acquire and release buffers in a loop
    for (;;)
    {
        ComPtr<IDXGIResource> AcquiredBuffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            HANDLE WaitHandles[] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

            // ==============================
            // TODO: Process the frame here
            //
            // This is the most performance-critical section of code in an IddCx driver. It's important that whatever
            // is done with the acquired surface be finished as quickly as possible. This operation could be:
            //  * a GPU copy to another buffer surface for later processing (such as a staging surface for mapping to CPU memory)
            //  * a GPU encode operation
            //  * a GPU VPBlt to another surface
            //  * a GPU custom compute shader encode operation
            // ==============================

            AcquiredBuffer.Reset();
            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if (FAILED(hr))
            {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

const UINT64 MHZ = 1000000;
const UINT64 KHZ = 1000;

// A list of modes exposed by the sample monitor EDID - FOR SAMPLE PURPOSES ONLY
// https://timetoexplore.net/blog/video-timings-vga-720p-1080p
const DISPLAYCONFIG_VIDEO_SIGNAL_INFO IndirectDeviceContext::s_KnownMonitorModes[] =
{
    // 1920 x 1080 @ 60Hz
    {
          14850 * KHZ,                                   // pixel clock rate [Hz]
        { 14850 * KHZ, 1920 + 280 },                     // fractional horizontal refresh rate [Hz]
        { 14850 * KHZ, (1920 + 280) * (1080 + 46) },     // fractional vertical refresh rate [Hz]
        { 1920, 1080 },                                  // (horizontal, vertical) active pixel resolution
        { 1920 + 280, 1080 + 46 },                       // (horizontal, vertical) total pixel resolution
        { { 255, 0 }},                                   // video standard and vsync divider
        DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE
    },
    // 1280 x 720 @ 60Hz
    {
          74250 * KHZ,                                   // pixel clock rate [Hz]
        { 74250 * KHZ, 1280 + 370 },                     // fractional horizontal refresh rate [Hz]
        { 74250 * KHZ, (1280 + 370) * (720 + 30) },      // fractional vertical refresh rate [Hz]
        { 1280, 720 },                                   // (horizontal, vertical) active pixel resolution
        { 1280 + 370, 720 + 30 },                        // (horizontal, vertical) total pixel resolution
        { { 255, 0 }},                                   // video standard and vsync divider
        DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE
    },
};

// This is a sample monitor EDID - FOR SAMPLE PURPOSES ONLY
const BYTE IndirectDeviceContext::s_KnownMonitorEdid[] =
{
    0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x05,0x32,0x06,0x04,0x00,0x00,0x00,0x00,
    0x00,0xA6,0x01,0x03,0x80,0x28,0x1E,0x78,0x0A,0xEE,0x91,0xA3,0x54,0x4C,0x99,0x26,
    0x0F,0x50,0x54,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x2D,0x36,0x80,0xA0,0x70,0x38,0x1F,0x40,0x30,0x20,
    0x35,0x00,0x35,0x2C,0x11,0x00,0x00,0x1E,0x00,0x00,0x00,0xFE,0x00,0x41,0x69,0x72,
    0x46,0x6C,0x79,0x44,0x69,0x73,0x70,0x6C,0x61,0x79,0x00,0x00,0x00,0x10,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xB5
};

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice)
    : m_WdfDevice(WdfDevice)
    , m_Adapter(NULL)
    , m_Monitor(NULL)
    , m_MonitorPlugged(false)
    , m_Event(NULL)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    m_Event = CreateEvent(NULL, FALSE, FALSE, NULL);
}

IndirectDeviceContext::~IndirectDeviceContext()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    m_ProcessingThread.reset();

    CloseHandle(m_Event);
}

void IndirectDeviceContext::InitAdapter()
{
    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    // ==============================

    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    AdapterCaps.MaxMonitorsSupported = 1;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"WinVirtualDisplay Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Microsoft";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"WinVirtualDisplay Model";

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION Version = {};
    Version.Size = sizeof(Version);
    Version.MajorVer = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;
    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if (NT_SUCCESS(Status))
    {
        // Store a reference to the WDF adapter handle
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
}

void IndirectDeviceContext::FinishInit()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    PlugInMonitor();
}


void IndirectDeviceContext::PlugInMonitor()
{
    // ==============================
    // TODO: In a real driver, the EDID should be retrieved dynamically from a connected physical monitor. The EDID
    // provided here is purely for demonstration, as it describes only 1920x1080 @ 60 Hz and 1280x720 @ 60 Hz. Monitor
    // manufacturers are required to correctly fill in physical monitor attributes in order to allow the OS to optimize
    // settings like viewing distance and scale factor. Manufacturers should also use a unique serial number every
    // single device to ensure the OS can tell the monitors apart.
    // ==============================

    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDDCX_MONITOR_INFO MonitorInfo = {};
    MonitorInfo.Size = sizeof(MonitorInfo);
    MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
    MonitorInfo.ConnectorIndex = 0;
    MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
    MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    MonitorInfo.MonitorDescription.DataSize = sizeof(s_KnownMonitorEdid);
    MonitorInfo.MonitorDescription.pData = const_cast<BYTE*>(s_KnownMonitorEdid);

    // ==============================
    // TODO: The monitor's container ID should be distinct from "this" device's container ID if the monitor is not
    // permanently attached to the display adapter device object. The container ID is typically made unique for each
    // monitor and can be used to associate the monitor with other devices, like audio or input devices. In this
    // sample we generate a random container ID GUID, but it's best practice to choose a stable container ID for a
    // unique monitor or to use "this" device's container ID for a permanent/integrated monitor.
    // ==============================

    //// Create a container ID
    //CoCreateGuid(&MonitorInfo.MonitorContainerId);
    // {3EC81FF0-5314-464C-8B0F-CFEA703825F6}
    MonitorInfo.MonitorContainerId = { 0x3ec81ff0, 0x5314, 0x464c, { 0x8b, 0xf, 0xcf, 0xea, 0x70, 0x38, 0x25, 0xf6 } };


    IDARG_IN_MONITORCREATE MonitorCreate = {};
    MonitorCreate.ObjectAttributes = &Attr;
    MonitorCreate.pMonitorInfo = &MonitorInfo;

    // Create a monitor object with the specified monitor descriptor
    IDARG_OUT_MONITORCREATE MonitorCreateOut;
    NTSTATUS Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
    if (NT_SUCCESS(Status))
    {
        m_Monitor = MonitorCreateOut.MonitorObject;

        // Associate the monitor with this device context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorCreateOut.MonitorObject);
        pContext->pContext = this;

        // Tell the OS that the monitor has been plugged in
        IDARG_OUT_MONITORARRIVAL ArrivalOut;
        Status = IddCxMonitorArrival(m_Monitor, &ArrivalOut);
    }
}


void IndirectDeviceContext::PlugOutMonitor()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    IddCxMonitorDeparture(m_Monitor);
}


void IndirectDeviceContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    m_ProcessingThread.reset();

    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    if (FAILED(Device->Init()))
    {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(SwapChain);
    }
    else
    {
        // Create a new swap-chain processing thread
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, NewFrameEvent));
    }

    // Enable hardware cursor support for this monitor
    // It will return STATUS_INVALID_PARAMETER (0xc000000d) when it is called before the monitor has arrived
    IDARG_IN_SETUP_HWCURSOR hwCursor;
    memset(&hwCursor, 0, sizeof(hwCursor));

    hwCursor.hNewCursorDataAvailable = m_Event;
    hwCursor.CursorInfo.Size = sizeof(IDDCX_CURSOR_CAPS);
    hwCursor.CursorInfo.AlphaCursorSupport = true;
    hwCursor.CursorInfo.ColorXorCursorSupport = IDDCX_XOR_CURSOR_SUPPORT_FULL;
    hwCursor.CursorInfo.MaxX = 64;
    hwCursor.CursorInfo.MaxY = 64;
    
    NTSTATUS Status = IddCxMonitorSetupHardwareCursor((IDDCX_MONITOR)m_Monitor, &hwCursor);
    WriteLogFile("[%s %d %s] 0x%x", __FILE__, __LINE__, __FUNCDNAME__, Status);

}

void IndirectDeviceContext::UnassignSwapChain()
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    // Stop processing the last swap-chain
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region DDI Callbacks


#define IOCTL_MONITOR_PLUG_IN     CTL_CODE(0x00009528, 0xcc1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MONITOR_PLUG_OUT    CTL_CODE(0x00009528, 0xcc2, METHOD_BUFFERED, FILE_ANY_ACCESS)


_Use_decl_annotations_
void EventDeviceIoControl(WDFDEVICE Device, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device)->pContext;

    if (IoControlCode == IOCTL_MONITOR_PLUG_IN)
    {
        if (!pContext->m_MonitorPlugged)
        {
            pContext->PlugInMonitor();
            pContext->m_MonitorPlugged = true;
        }
    }
    else if (IoControlCode == IOCTL_MONITOR_PLUG_OUT)
    {
        if (pContext->m_MonitorPlugged)
        {
            pContext->PlugOutMonitor();
            pContext->m_MonitorPlugged = false;
        }
    }

    WdfRequestComplete(Request, STATUS_SUCCESS);
}


_Use_decl_annotations_
NTSTATUS EventAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.

    //auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
    //if (NT_SUCCESS(pInArgs->AdapterInitStatus))
    //{
    //    pContext->pContext->FinishInit();
    //}

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS EventAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS EventParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for an EDID by parsing it. In
    // this sample driver, we hard-code the EDID, so this function can generate known modes.
    // ==============================

    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    pOutArgs->MonitorModeBufferOutputCount = ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes);

    if (pInArgs->MonitorModeBufferInputCount < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes))
    {
        // Return success if there was no buffer, since the caller was only asking for a count of modes
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }
    else
    {
        // Copy the known modes to the output buffer
        for (DWORD ModeIndex = 0; ModeIndex < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes); ModeIndex++)
        {
            pInArgs->pMonitorModes[ModeIndex].Size = sizeof(IDDCX_MONITOR_MODE);
            pInArgs->pMonitorModes[ModeIndex].Origin = IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR;
            pInArgs->pMonitorModes[ModeIndex].MonitorVideoSignalInfo = IndirectDeviceContext::s_KnownMonitorModes[ModeIndex];
        }

        // Set the preferred mode as represented in the EDID
        pOutArgs->PreferredMonitorModeIdx = 0;

        return STATUS_SUCCESS;
    }
}

_Use_decl_annotations_
NTSTATUS EventMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    UNREFERENCED_PARAMETER(MonitorObject);
    UNREFERENCED_PARAMETER(pInArgs);
    UNREFERENCED_PARAMETER(pOutArgs);

    // Should never be called since we create a single monitor with a known EDID in this sample driver.

    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    // ==============================

    return STATUS_NOT_IMPLEMENTED;
}

/// <summary>
/// Creates a target mode from the fundamental mode attributes.
/// </summary>
void CreateTargetMode(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, UINT Width, UINT Height, UINT VSync)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;
    Mode.AdditionalSignalInfo.vSyncFreqDivider = 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;
    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = Mode.hSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    Mode.pixelRate = VSync * Width * Height;
}

void CreateTargetMode(IDDCX_TARGET_MODE& Mode, UINT Width, UINT Height, UINT VSync)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    Mode.Size = sizeof(Mode);
    CreateTargetMode(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync);
}

_Use_decl_annotations_
NTSTATUS UniShareMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    UNREFERENCED_PARAMETER(MonitorObject);

    vector<IDDCX_TARGET_MODE> TargetModes(2);

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.

    CreateTargetMode(TargetModes[0], 1920, 1080, 60);
    CreateTargetMode(TargetModes[1], 1280, 720, 60);

    pOutArgs->TargetModeBufferOutputCount = (UINT)TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS EventMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);
    pContext->pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS EventMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    WriteLogFile("[%s %d %s]", __FILE__, __LINE__, __FUNCDNAME__);

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);
    pContext->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

#pragma endregion