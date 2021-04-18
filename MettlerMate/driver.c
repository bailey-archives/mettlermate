#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, FilterEvtDeviceAdd)
#endif

/// <summary>
/// The main entry point for the driver. 
/// This will be called automatically by the framework via FxDriverEntry after libs are loaded.
/// </summary>
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDFDRIVER hDriver;

	KdPrint(("MettlerMate: Initializing\n"));

	// Initialize the driver config to control global attributes
	WDF_DRIVER_CONFIG_INIT(&config, FilterEvtDeviceAdd);

	// Create the FDO for our driver
	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &hDriver);

	if (!NT_SUCCESS(status)) {
		KdPrint(("MettlerMate: Failed WdfDriverCreate with status 0x%x\n", status));
	}

	return status;
}

/// <summary>
/// Invoked by the PnP manager when a new device is available.
/// We should use this to grab the device's properties and determine how we want to interact with it.
/// If we're not interested in this device, we can simply ignore it and return success.
/// </summary>
NTSTATUS FilterEvtDeviceAdd(IN WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit) {
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	PFILTER_EXTENSION filterExt;
	NTSTATUS status;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG ioQueueConfig;

	PAGED_CODE();
	UNREFERENCED_PARAMETER(Driver);

	KdPrint(("MettlerMate: Initializing a new device..."));

	// Tell the framework that we're a filter driver
	WdfFdoInitSetFilter(DeviceInit);

	// Set the size of our context
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, FILTER_EXTENSION);

	// Create the FDO for the device
	// This will automatically attach to the lower stack and set appropriate flags and attribs
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
	if (!NT_SUCCESS(status)) {
		KdPrint(("MettlerMate: Failed WdfDeviceCreate on DeviceInit with status 0x%x\n", status));
		return status;
	}

	// Get a filter context object for the device
	filterExt = FilterGetData(device);

	// Configure the default queue to be parallel
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);

	// Hook into the queue and grab the data we want to intercept
	ioQueueConfig.EvtIoDeviceControl = FilterEvtIoDeviceControl;
	ioQueueConfig.EvtIoRead = FilterEvtIoRead;

	// Create the queue with our configuration and a default handler
	status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
	if (!NT_SUCCESS(status)) {
		KdPrint(("MettlerMate: Failed WdfIoQueueCreate with status 0x%x\n", status));
		return status;
	}

	return status;
}

/// <summary>
/// Dispatch routine for internal device control requests.
/// This is where you can modify the HID report and device descriptors. If you're going to modify the size of data in reports, you
/// will need to make sure the descriptors are compatible.
/// </summary>
VOID FilterEvtIoDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t OutputBufferLength, IN size_t InputBufferLength, IN ULONG IoControlCode) {
	PFILTER_EXTENSION filterExt;
	NTSTATUS status = STATUS_SUCCESS;
	WDFDEVICE device;

	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	KdPrint(("MettlerMate: FilterEvtIoDeviceControl code 0x%x (input: %zu output: %zu)\n", IoControlCode, InputBufferLength, OutputBufferLength));

	// Grab the device for this request and its filter context
	device = WdfIoQueueGetDevice(Queue);
	filterExt = FilterGetData(device);

	switch (IoControlCode) {
		// TODO: Handle some control codes!
	}

	// Report errors to halt the request
	if (!NT_SUCCESS(status)) {
		WdfRequestComplete(Request, status);
		return;
	}

	// Forward the request down the stack
	// Note: WdfDeviceGetIoTarget returns the next device below us in the stack
	FilterForwardRequest(Request, WdfDeviceGetIoTarget(device));

	return;
}

/// <summary>
/// Dispatch routine for internal read requests.
/// </summary>
VOID FilterEvtIoRead(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length) {
	WDFDEVICE device;

	UNREFERENCED_PARAMETER(Length);

	// Grab the device for this request
	device = WdfIoQueueGetDevice(Queue);

	// Log the buffer for now
	KdPrint(("MettlerMate: Forwarding read request of size %zu for post-processing!\n", Length));

	// Forward the request down the stack with our completion routine
	FilterForwardRequestWithCompletionRoutine(Request, WdfDeviceGetIoTarget(device));

	return;
}

/// <summary>
/// Passes a request on to the lower driver.
/// </summary>
VOID FilterForwardRequest(IN WDFREQUEST Request, IN WDFIOTARGET Target) {
	WDF_REQUEST_SEND_OPTIONS options;
	BOOLEAN ret;
	NTSTATUS status;

	// We are not interested in post processing the IRP so fire and forget
	WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

	// Send the request
	ret = WdfRequestSend(Request, Target, &options);

	if (ret == FALSE) {
		status = WdfRequestGetStatus(Request);
		KdPrint(("MettlerMate: FilterForwardRequest failed on WdfRequestSend with status 0x%x\n", status));
		WdfRequestComplete(Request, status);
	}

	return;
}

/// <summary>
/// Forwards the given request with a completion routine that will be invoked for post-processing.
/// </summary>
VOID FilterForwardRequestWithCompletionRoutine(IN WDFREQUEST Request, IN WDFIOTARGET Target) {
	BOOLEAN ret;
	NTSTATUS status;

	WdfRequestFormatRequestUsingCurrentType(Request);
	WdfRequestSetCompletionRoutine(Request, FilterRequestCompletionRoutine, WDF_NO_CONTEXT);

	ret = WdfRequestSend(Request, Target, WDF_NO_SEND_OPTIONS);

	if (ret == FALSE) {
		status = WdfRequestGetStatus(Request);
		KdPrint(("MettlerMate: FilterForwardRequestWithCompletionRoutine failed on WdfRequestSend with status 0x%x\n", status));
		WdfRequestComplete(Request, status);
	}

	return;
}

/// <summary>
/// The completion routine for read requests.
/// This is where we can access the HID data and modify it.
/// </summary>
VOID FilterRequestCompletionRoutine(IN WDFREQUEST Request, IN WDFIOTARGET Target, PWDF_REQUEST_COMPLETION_PARAMS CompletionParams, IN WDFCONTEXT Context) {
	PWEIGHT_REPORT buffer;

	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Context);

	// Get the buffer for this request
	NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(WEIGHT_REPORT), &buffer, NULL);
	if (!NT_SUCCESS(status)) {
		KdPrint(("MettlerMate: FilterRequestCompletionRoutine failed on WdfRequestRetrieveInputBuffer with status 0x%x\n", status));
		WdfRequestComplete(Request, status);
		return;
	}

	// Log the buffer for now
	KdPrint((
		"MettlerMate: Got completion routine buffer: %x %x %x %x %x %x\n", 
		buffer->ReportId, 
		buffer->Status, 
		buffer->Unit, 
		buffer->Precision, 
		buffer->WeightLSB, 
		buffer->WeightMSB
	));

	// Convert the buffer data for compatibility
	ConvertWeightBuffer(buffer);

	// Mark the request as completed
	WdfRequestComplete(Request, CompletionParams->IoStatus.Status);

	return;
}

VOID ConvertWeightBuffer(IN PWEIGHT_REPORT Buffer) {
	int unit = (int) Buffer->Unit;
	int precision = (int) Buffer->Precision;

	unsigned short data = (unsigned char) Buffer->WeightMSB;
	data <<= 8;
	data += (unsigned char) Buffer->WeightLSB;

	KdPrint(("MettlerMate: Parsed (unit=%d, precision=%d, data=%u)\n", unit, precision, data));

	if (unit == 11 && precision == 254) {
		int remainder = data % 10;

		data = data / 10;
		precision = 255;

		if (remainder >= 5) {
			data += 1;
		}

		Buffer->Precision = (unsigned char) precision;
		Buffer->WeightLSB = (unsigned char) data;
		Buffer->WeightMSB = (unsigned char) (data >> 8);

		KdPrint(("MettlerMate: Converted (unit=%d, precision=%d, data=%u)\n", unit, precision, data));
		KdPrint((
			"MettlerMate: New completion routine buffer: %x %x %x %x %x %x\n",
			Buffer->ReportId,
			Buffer->Status,
			Buffer->Unit,
			Buffer->Precision,
			Buffer->WeightLSB,
			Buffer->WeightMSB
		));
	}
}