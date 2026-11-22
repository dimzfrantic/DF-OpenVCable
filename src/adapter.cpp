#define PUT_GUIDS_HERE
#include <initguid.h>
#include "driver.h"
#include "ringbuffer.h"
#include "minwavert.h"
#include "mintopo.h"
#include "mcvad_ioctl.h"

// Nama Subdevice yang diekspos ke Windows
const PCWSTR g_CableNames[MAX_CABLES] = {
    L"Virtual Audio Cable 1",
    L"Virtual Audio Cable A",
    L"Virtual Audio Cable B",
    L"Virtual Audio Cable C",
    L"Virtual Audio Cable D"
};

const PCWSTR g_CableSubdeviceNames[MAX_CABLES] = {
    L"Cable1",
    L"CableA",
    L"CableB",
    L"CableC",
    L"CableD"
};

const PCWSTR g_CableTopoSubdeviceNames[MAX_CABLES] = {
    L"Cable1_Topo",
    L"CableA_Topo",
    L"CableB_Topo",
    L"CableC_Topo",
    L"CableD_Topo"
};

// Global array konteks virtual cable
static CABLE_CONTEXT g_Cables[MAX_CABLES];
static PDEVICE_OBJECT g_pControlDeviceObject = NULL;
static ULONG g_ConfiguredSampleRate[MAX_CABLES] = { 48000, 48000, 48000, 48000, 48000 };
static ULONG g_ConfiguredBufferMs[MAX_CABLES] = { 500, 500, 500, 500, 500 };

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

    // Buat Control Device Object untuk komunikasi IOCTL dengan Control Panel GUI
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
        }
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = McvadCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = McvadCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = McvadDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    // Inisialisasi adapter PortCls
    NTSTATUS ntStatus = PcInitializeAdapterDriver(
        DriverObject,
        RegistryPath,
        (PDRIVER_ADD_DEVICE)AddDevice
    );

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DriverObject);
    UNICODE_STRING symName;
    RtlInitUnicodeString(&symName, MCVAD_SYMBOLIC_NAME);
    IoDeleteSymbolicLink(&symName);
    if (g_pControlDeviceObject) {
        IoDeleteDevice(g_pControlDeviceObject);
        g_pControlDeviceObject = NULL;
    }
}

NTSTATUS McvadCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS McvadDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DeviceObject);

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

    ULONG bufferSize = DEFAULT_SAMPLE_RATE * DEFAULT_CHANNELS * (DEFAULT_CONTAINER_SIZE / 8) * BUFFER_DURATION_SECONDS;

    for (ULONG i = 0; i < MAX_CABLES; i++) {
        g_Cables[i].CableIndex = i;
        g_Cables[i].Name = g_CableNames[i];
        g_Cables[i].SubdeviceName = g_CableSubdeviceNames[i];
        g_Cables[i].TopoSubdeviceName = g_CableTopoSubdeviceNames[i];

        // 1. Alokasi Ring Buffer terpisah untuk tiap kabel agar tidak bercampur
        g_Cables[i].pSharedBuffer = new (NonPagedPoolNx, MCVAD_POOL_TAG) CCircularRingBuffer();
        if (g_Cables[i].pSharedBuffer) {
            g_Cables[i].pSharedBuffer->Initialize(bufferSize);
        }

        // 2. Buat miniport WaveRT
        CMiniportWaveRT* pMiniportWave = new (NonPagedPoolNx, MCVAD_POOL_TAG) CMiniportWaveRT(NULL);
        if (!pMiniportWave) continue;
        pMiniportWave->AddRef();
        pMiniportWave->Init(i, g_Cables[i].pSharedBuffer);

        // 3. Buat Port WaveRT
        PPORTWAVERT pPortWave = NULL;
        NTSTATUS status = PcNewPort((PPORT*)&pPortWave, CLSID_PortWaveRT);
        if (NT_SUCCESS(status) && pPortWave) {
            status = pPortWave->Init(DeviceObject, Irp, pMiniportWave, NULL, ResourceList);
            if (NT_SUCCESS(status)) {
                status = PcRegisterSubdevice(DeviceObject, (PWSTR)g_Cables[i].SubdeviceName, pPortWave);
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

        // 5. Buat Port Topology
        PPORTTOPOLOGY pPortTopo = NULL;
        status = PcNewPort((PPORT*)&pPortTopo, CLSID_PortTopology);
        if (NT_SUCCESS(status) && pPortTopo) {
            status = pPortTopo->Init(DeviceObject, Irp, pMiniportTopo, NULL, ResourceList);
            if (NT_SUCCESS(status)) {
                status = PcRegisterSubdevice(DeviceObject, (PWSTR)g_Cables[i].TopoSubdeviceName, pPortTopo);
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
                (PPORT)pPortWave,
                PIN_WAVE_RENDER_SOURCE,
                (PPORT)pPortTopo,
                KSPIN_TOPO_WAVEOUT_SOURCE
            );
            if (!NT_SUCCESS(status)) {
                KdPrint(("[MCVAD] PcRegisterPhysicalConnection (Playback) %ws gagal: 0x%08X\n", g_Cables[i].SubdeviceName, status));
            }

            // Capture: Topology Out (Pin 3) -> WaveRT In (Pin 2)
            status = PcRegisterPhysicalConnection(
                DeviceObject,
                (PPORT)pPortTopo,
                KSPIN_TOPO_WAVEIN_DEST,
                (PPORT)pPortWave,
                PIN_WAVE_CAPTURE_SINK
            );
            if (!NT_SUCCESS(status)) {
                KdPrint(("[MCVAD] PcRegisterPhysicalConnection (Capture) %ws gagal: 0x%08X\n", g_Cables[i].SubdeviceName, status));
            }
        }
    }

    return STATUS_SUCCESS;
}
#pragma code_seg()
