#define PUT_GUIDS_HERE
#include <initguid.h>
#include "driver.h"
#include "ringbuffer.h"
#include "minwavert.h"
#include "mintopo.h"
#include "mcvad_ioctl.h"

// Nama Subdevice yang diekspos ke Windows
const PCWSTR g_CableNames[MAX_CABLES] = {
    L"DF-OpenVCable A",
    L"DF-OpenVCable B",
    L"DF-OpenVCable C",
    L"DF-OpenVCable D",
    L"DF-OpenVCable E"
};

const PCWSTR g_CableSubdeviceNames[MAX_CABLES] = {
    L"CableA",
    L"CableB",
    L"CableC",
    L"CableD",
    L"CableE"
};

// Reference string subdevice Topology.
//
// Suffix "_v2" WAJIB sama persis dengan AddInterface=%KSCATEGORY_TOPOLOGY% di mcvad.inf.
// Bumping string ini mengubah device interface path, sehingga AudioEndpointBuilder
// memperlakukannya sebagai endpoint baru dan membuang cache PKEY_Device_DeviceDesc
// lama (tanpa ini, nama "Speakers" dari build sebelumnya tetap menempel).
const PCWSTR g_CableTopoSubdeviceNames[MAX_CABLES] = {
    L"CableA_Topo_v2",
    L"CableB_Topo_v2",
    L"CableC_Topo_v2",
    L"CableD_Topo_v2",
    L"CableE_Topo_v2"
};

// Global array konteks virtual cable
static CABLE_CONTEXT g_Cables[MAX_CABLES];
static PDEVICE_OBJECT g_pControlDeviceObject = NULL;
static ULONG g_ConfiguredSampleRate[MAX_CABLES] = { 48000, 48000, 48000, 48000, 48000 };
static ULONG g_ConfiguredBufferMs[MAX_CABLES] = { 500, 500, 500, 500, 500 };

// ============================================================
// Diagnostik StartDevice
// ------------------------------------------------------------
// Merekam status NTSTATUS setiap langkah pembuatan subdevice ke
// registry HKLM\SOFTWARE\MCVAD\StartDiag (REG_BINARY) supaya
// kegagalan PcNewPort / Init / PcRegisterSubdevice bisa dibaca
// dengan 'reg query' tanpa perlu kernel debugger.
//
// Layout: array of { ULONG Cable; ULONG Stage; ULONG Status; }
// ============================================================
#define MCVAD_STAGE_WAVE_PORT       1
#define MCVAD_STAGE_WAVE_INIT       2
#define MCVAD_STAGE_WAVE_REGISTER   3
#define MCVAD_STAGE_TOPO_PORT       4
#define MCVAD_STAGE_TOPO_INIT       5
#define MCVAD_STAGE_TOPO_REGISTER   6
#define MCVAD_STAGE_CONN_PLAYBACK   7
#define MCVAD_STAGE_CONN_CAPTURE    8
#define MCVAD_DIAG_MAX_RECORDS      (MAX_CABLES * 8)

typedef struct _MCVAD_DIAG_RECORD {
    ULONG Cable;
    ULONG Stage;
    ULONG Status;
} MCVAD_DIAG_RECORD;

static MCVAD_DIAG_RECORD g_Diag[MCVAD_DIAG_MAX_RECORDS];
static ULONG g_DiagCount = 0;

static VOID McvadDiagReset(VOID) {
    g_DiagCount = 0;
    RtlZeroMemory(g_Diag, sizeof(g_Diag));
}

static VOID McvadDiag(ULONG Cable, ULONG Stage, NTSTATUS Status) {
    if (g_DiagCount < MCVAD_DIAG_MAX_RECORDS) {
        g_Diag[g_DiagCount].Cable  = Cable;
        g_Diag[g_DiagCount].Stage  = Stage;
        g_Diag[g_DiagCount].Status = (ULONG)Status;
        g_DiagCount++;
    }
}

static VOID McvadDiagFlush(VOID) {
    RtlWriteRegistryValue(
        RTL_REGISTRY_ABSOLUTE,
        L"\\Registry\\Machine\\SOFTWARE\\MCVAD",
        L"StartDiag",
        REG_BINARY,
        g_Diag,
        g_DiagCount * sizeof(MCVAD_DIAG_RECORD));
}

// Handler asli PortCls yang disimpan saat DriverEntry
static PDRIVER_DISPATCH g_pfnPortClsCreateClose = NULL;
static PDRIVER_DISPATCH g_pfnPortClsDeviceControl = NULL;
static PDRIVER_UNLOAD   g_pfnPortClsUnload = NULL;

// Prototipe IOCTL Dispatcher
NTSTATUS McvadCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS McvadDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS StartDevice(PDEVICE_OBJECT DeviceObject, PIRP Irp, PRESOURCELIST ResourceList);
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
VOID     DriverUnload(PDRIVER_OBJECT DriverObject);

#pragma code_seg("INIT")
extern "C" DRIVER_INITIALIZE DriverEntry;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    PAGED_CODE();
    KdPrint(("[MCVAD] DriverEntry: Menginisialisasi All-in-One Multi Virtual Cable\n"));

    // 1. Inisialisasi adapter PortCls terlebih dahulu agar PortCls mendaftarkan dispatch handler dasarnya
    NTSTATUS ntStatus = PcInitializeAdapterDriver(
        DriverObject,
        RegistryPath,
        (PDRIVER_ADD_DEVICE)AddDevice
    );

    if (!NT_SUCCESS(ntStatus)) {
        KdPrint(("[MCVAD] PcInitializeAdapterDriver gagal: 0x%08X\n", ntStatus));
        return ntStatus;
    }

    // 2. Buat Control Device Object untuk komunikasi IOCTL dengan Control Panel GUI
    UNICODE_STRING devName, symName;
    RtlInitUnicodeString(&devName, MCVAD_DEVICE_NAME);
    RtlInitUnicodeString(&symName, MCVAD_SYMBOLIC_NAME);

    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        &devName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_pControlDeviceObject
    );

    if (NT_SUCCESS(status) && g_pControlDeviceObject) {
        status = IoCreateSymbolicLink(&symName, &devName);
        if (NT_SUCCESS(status)) {
            KdPrint(("[MCVAD] Control device object created successfully\n"));
        } else {
            KdPrint(("[MCVAD] IoCreateSymbolicLink gagal: 0x%08X\n", status));
            IoDeleteDevice(g_pControlDeviceObject);
            g_pControlDeviceObject = NULL;
        }
    } else {
        KdPrint(("[MCVAD] IoCreateDevice untuk control device gagal: 0x%08X\n", status));
    }

    // 3. Simpan dispatch routine asli dari PortCls
    g_pfnPortClsCreateClose = DriverObject->MajorFunction[IRP_MJ_CREATE];
    g_pfnPortClsDeviceControl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    g_pfnPortClsUnload = DriverObject->DriverUnload;

    // 4. Pasang hook wrapper MCVAD untuk memisahkan IRP Control Device vs PortCls
    DriverObject->MajorFunction[IRP_MJ_CREATE] = McvadCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = McvadCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = McvadDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    PAGED_CODE();
    UNICODE_STRING symName;
    RtlInitUnicodeString(&symName, MCVAD_SYMBOLIC_NAME);
    IoDeleteSymbolicLink(&symName);
    if (g_pControlDeviceObject) {
        IoDeleteDevice(g_pControlDeviceObject);
        g_pControlDeviceObject = NULL;
    }
    if (g_pfnPortClsUnload) {
        g_pfnPortClsUnload(DriverObject);
    }
}

NTSTATUS McvadCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PAGED_CODE();

    // Jika target adalah Control Device Object kita, selesaikan langsung
    if (DeviceObject == g_pControlDeviceObject) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    // Jika target adalah audio subdevice PortCls, teruskan ke dispatcher asli PortCls
    if (g_pfnPortClsCreateClose) {
        return g_pfnPortClsCreateClose(DeviceObject, Irp);
    }

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS McvadDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PAGED_CODE();

    // Jika target BUKAN Control Device Object kita, teruskan ke PortCls (properti KS dll)
    if (DeviceObject != g_pControlDeviceObject) {
        if (g_pfnPortClsDeviceControl) {
            return g_pfnPortClsDeviceControl(DeviceObject, Irp);
        }
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG ioControlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
    PVOID pBuffer = Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG bytesReturned = 0;

    switch (ioControlCode) {
        case IOCTL_MCVAD_GET_CABLE_STATUS: {
            if (outLen >= sizeof(MCVAD_ALL_STATUS) && pBuffer) {
                PMCVAD_ALL_STATUS pStatus = (PMCVAD_ALL_STATUS)pBuffer;
                RtlZeroMemory(pStatus, sizeof(MCVAD_ALL_STATUS));
                pStatus->Version = 0x010000;
                pStatus->CableCount = MAX_CABLES;

                for (ULONG i = 0; i < MAX_CABLES; i++) {
                    pStatus->Cables[i].CableIndex = i;
                    RtlStringCchCopyW(pStatus->Cables[i].Name, sizeof(pStatus->Cables[i].Name) / sizeof(WCHAR), g_CableNames[i]);
                    pStatus->Cables[i].SampleRate = g_ConfiguredSampleRate[i];
                    pStatus->Cables[i].BitsPerSample = DEFAULT_BITS_PER_SAMPLE;
                    pStatus->Cables[i].Channels = DEFAULT_CHANNELS;
                    pStatus->Cables[i].BufferDurationMs = g_ConfiguredBufferMs[i];
                    pStatus->Cables[i].IsEnabled = g_Cables[i].bEnabled;
                    if (g_Cables[i].pSharedBuffer) {
                        pStatus->Cables[i].BufferSizeBytes = g_Cables[i].pSharedBuffer->GetBufferSize();
                        pStatus->Cables[i].CurrentBufferFill = g_Cables[i].pSharedBuffer->GetDataAvailable();
                        pStatus->Cables[i].OverrunCount = g_Cables[i].pSharedBuffer->GetOverrunCount();
                        pStatus->Cables[i].UnderrunCount = g_Cables[i].pSharedBuffer->GetUnderrunCount();
                    }
                }
                bytesReturned = sizeof(MCVAD_ALL_STATUS);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case IOCTL_MCVAD_SET_CABLE_CONFIG: {
            if (inLen >= sizeof(MCVAD_SET_CONFIG_REQ) && pBuffer) {
                PMCVAD_SET_CONFIG_REQ pReq = (PMCVAD_SET_CONFIG_REQ)pBuffer;
                if (pReq->CableIndex < MAX_CABLES) {
                    ULONG idx = pReq->CableIndex;

                    // Perbarui status Enable jika diminta
                    if (pReq->EnableState == 0) {
                        g_Cables[idx].bEnabled = FALSE;
                        KdPrint(("[MCVAD] Cable %u Disabled\n", idx));
                    } else if (pReq->EnableState == 1) {
                        g_Cables[idx].bEnabled = TRUE;
                        KdPrint(("[MCVAD] Cable %u Enabled\n", idx));
                    }

                    if (pReq->SampleRate >= 44100 && pReq->SampleRate <= 192000) {
                        g_ConfiguredSampleRate[idx] = pReq->SampleRate;
                    }
                    if (pReq->BufferDurationMs >= 10 && pReq->BufferDurationMs <= 5000) {
                        g_ConfiguredBufferMs[idx] = pReq->BufferDurationMs;
                        if (g_Cables[idx].pSharedBuffer) {
                            ULONG bytesPerSec = g_ConfiguredSampleRate[idx] * DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8);
                            ULONG newSizeBytes = (bytesPerSec * pReq->BufferDurationMs) / 1000;
                            g_Cables[idx].pSharedBuffer->Resize(newSizeBytes);
                        }
                    }
                    status = STATUS_SUCCESS;
                } else {
                    status = STATUS_INVALID_PARAMETER;
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        case IOCTL_MCVAD_RESET_CABLE_STATS: {
            if (inLen >= sizeof(ULONG) && pBuffer) {
                ULONG idx = *(PULONG)pBuffer;
                if (idx < MAX_CABLES && g_Cables[idx].pSharedBuffer) {
                    g_Cables[idx].pSharedBuffer->ResetStats();
                    status = STATUS_SUCCESS;
                }
            }
            break;
        }
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

#pragma code_seg("PAGE")
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject) {
    PAGED_CODE();
    KdPrint(("[MCVAD] AddDevice: Mendaftarkan 10 subdevice virtual audio (5 WaveRT + 5 Topology)\n"));

    NTSTATUS ntStatus = PcAddAdapterDevice(
        DriverObject,
        PhysicalDeviceObject,
        (PCPFNSTARTDEVICE)StartDevice,
        MAX_CABLES * 2, // 5 WaveRT + 5 Topology = 10 Subdevices
        0
    );

    return ntStatus;
}

NTSTATUS StartDevice(PDEVICE_OBJECT DeviceObject, PIRP Irp, PRESOURCELIST ResourceList) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(ResourceList);

    KdPrint(("[MCVAD] StartDevice: Membangun alur loopback dan topologi untuk 5 virtual cable\n"));
    McvadDiagReset();

    ULONG bufferSize = DEFAULT_SAMPLE_RATE * DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8) * BUFFER_DURATION_SECONDS;

    for (ULONG i = 0; i < MAX_CABLES; i++) {
        g_Cables[i].CableIndex = i;
        g_Cables[i].Name = g_CableNames[i];
        g_Cables[i].SubdeviceName = g_CableSubdeviceNames[i];
        g_Cables[i].TopoSubdeviceName = g_CableTopoSubdeviceNames[i];
        g_Cables[i].bEnabled = TRUE;

        // 1. Alokasi Ring Buffer terpisah untuk tiap kabel agar tidak bercampur
        g_Cables[i].pSharedBuffer = new (NonPagedPoolNx, MCVAD_POOL_TAG) CCircularRingBuffer();
        if (g_Cables[i].pSharedBuffer) {
            g_Cables[i].pSharedBuffer->Initialize(bufferSize);
        }

        // 2. Buat miniport WaveRT
        CMiniportWaveRT* pMiniportWave = new (NonPagedPoolNx, MCVAD_POOL_TAG) CMiniportWaveRT(NULL);
        if (!pMiniportWave) continue;
        pMiniportWave->AddRef();
        pMiniportWave->Init(i, g_Cables[i].pSharedBuffer, &g_Cables[i].bEnabled);

        // 3. Buat Port WaveRT
        PPORTWAVERT pPortWave = NULL;
        NTSTATUS status = PcNewPort((PPORT*)&pPortWave, CLSID_PortWaveRT);
        McvadDiag(i, MCVAD_STAGE_WAVE_PORT, status);
        if (NT_SUCCESS(status) && pPortWave) {
            status = pPortWave->Init(DeviceObject, Irp, pMiniportWave, NULL, ResourceList);
            McvadDiag(i, MCVAD_STAGE_WAVE_INIT, status);
            if (NT_SUCCESS(status)) {
                status = PcRegisterSubdevice(DeviceObject, (PWSTR)g_Cables[i].SubdeviceName, pPortWave);
                McvadDiag(i, MCVAD_STAGE_WAVE_REGISTER, status);
                if (NT_SUCCESS(status)) {
                    KdPrint(("[MCVAD] WaveRT Subdevice terdaftar: %ws\n", g_Cables[i].SubdeviceName));
                }
            }
            g_Cables[i].pPort = (PPORT)pPortWave;
        }
        g_Cables[i].pMiniport = (PMINIPORT)pMiniportWave;

        // 4. Buat miniport Topology
        CMiniportTopology* pMiniportTopo = new (NonPagedPoolNx, MCVAD_POOL_TAG) CMiniportTopology(NULL);
        if (!pMiniportTopo) continue;
        pMiniportTopo->AddRef();
        pMiniportTopo->SetCableIndex(i);

        // 5. Buat Port Topology
        PPORTTOPOLOGY pPortTopo = NULL;
        status = PcNewPort((PPORT*)&pPortTopo, CLSID_PortTopology);
        McvadDiag(i, MCVAD_STAGE_TOPO_PORT, status);
        if (NT_SUCCESS(status) && pPortTopo) {
            status = pPortTopo->Init(DeviceObject, Irp, (PMINIPORT)pMiniportTopo, NULL, ResourceList);
            McvadDiag(i, MCVAD_STAGE_TOPO_INIT, status);
            if (NT_SUCCESS(status)) {
                status = PcRegisterSubdevice(DeviceObject, (PWSTR)g_Cables[i].TopoSubdeviceName, (PPORT)pPortTopo);
                McvadDiag(i, MCVAD_STAGE_TOPO_REGISTER, status);
                if (NT_SUCCESS(status)) {
                    KdPrint(("[MCVAD] Topology Subdevice terdaftar: %ws\n", g_Cables[i].TopoSubdeviceName));
                }
            }
            g_Cables[i].pPortTopo = (PPORT)pPortTopo;
        }
        g_Cables[i].pMiniportTopo = (PMINIPORT)pMiniportTopo;

        // 6. Hubungkan WaveRT dan Topology via Physical Connection
        if (pPortWave && pPortTopo) {
            // Playback: WaveRT Out (Pin 1) -> Topology In (Pin 0)
            status = PcRegisterPhysicalConnection(
                DeviceObject,
                (PUNKNOWN)pPortWave,
                PIN_WAVE_RENDER_SOURCE,
                (PUNKNOWN)pPortTopo,
                PIN_TOPO_WAVEOUT_SINK
            );
            McvadDiag(i, MCVAD_STAGE_CONN_PLAYBACK, status);
            if (!NT_SUCCESS(status)) {
                KdPrint(("[MCVAD] PcRegisterPhysicalConnection (Playback) %ws gagal: 0x%08X\n", g_Cables[i].SubdeviceName, status));
            }

            // Capture: Topology Out (Pin 3) -> WaveRT In (Pin 2)
            status = PcRegisterPhysicalConnection(
                DeviceObject,
                (PUNKNOWN)pPortTopo,
                PIN_TOPO_WAVEIN_SOURCE,
                (PUNKNOWN)pPortWave,
                PIN_WAVE_CAPTURE_SINK
            );
            McvadDiag(i, MCVAD_STAGE_CONN_CAPTURE, status);
            if (!NT_SUCCESS(status)) {
                KdPrint(("[MCVAD] PcRegisterPhysicalConnection (Capture) %ws gagal: 0x%08X\n", g_Cables[i].SubdeviceName, status));
            }
        }
    }

    McvadDiagFlush();
    return STATUS_SUCCESS;
}
#pragma code_seg()
