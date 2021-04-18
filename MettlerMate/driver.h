#include <ntddk.h>
#include <wdf.h>
#include <wdmsec.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#if !defined(_FILTER_H_)
#define _FILTER_H_

typedef struct _FILTER_EXTENSION {
	WDFDEVICE WdfDevice;
	// TODO: Add context stuffs here
} FILTER_EXTENSION, * PFILTER_EXTENSION;

typedef struct _WEIGHT_REPORT {
	unsigned char ReportId;
	unsigned char Status;
	unsigned char Unit;
	unsigned char Precision;
	unsigned char WeightLSB;
	unsigned char WeightMSB;
} WEIGHT_REPORT, * PWEIGHT_REPORT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILTER_EXTENSION, FilterGetData)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD FilterEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FilterEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_READ FilterEvtIoRead;

VOID ConvertWeightBuffer(IN PWEIGHT_REPORT Buffer);

VOID FilterForwardRequest(IN WDFREQUEST Request, IN WDFIOTARGET Target);
VOID FilterForwardRequestWithCompletionRoutine(IN WDFREQUEST Request, IN WDFIOTARGET Target);
VOID FilterRequestCompletionRoutine(IN WDFREQUEST Request, IN WDFIOTARGET Target, PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, IN WDFCONTEXT Context);

#endif