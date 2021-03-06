/** @file
  This file contains the implementation of Usb Hc Protocol.

Copyright(c) 2013 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**/

#include "Ohci.h"
#include "OhciDebug.h"

EFI_DRIVER_BINDING_PROTOCOL gOhciDriverBinding = {
  OhciDriverBindingSupported,
  OhciDriverBindingStart,
  OhciDriverBindingStop,
  0x10,
  NULL,
  NULL
};

/* Template for Ohci's Usb2 Host Controller Protocol Instance */

EFI_USB2_HC_PROTOCOL gOhciUsb2HcTemplate = {
  OhciGetCapability,
  OhciReset,
  OhciGetState,
  OhciSetState,
  OhciControlTransfer,
  OhciBulkTransfer,
  OhciAsyncInterruptTransfer,
  OhciSyncInterruptTransfer,
  OhciIsochronousTransfer,
  OhciAsyncIsochronousTransfer,
  OhciGetRootHubPortStatus,
  OhciSetRootHubPortFeature,
  OhciClearRootHubPortFeature,
  0x01,
  0x01
};

/**
  Retrieves the capability of root hub ports.

  @param  This                  The EFI_USB2_HC_PROTOCOL instance.
  @param  MaxSpeed              Max speed supported by the controller.
  @param  PortNumber            Number of the root hub ports.
  @param  Is64BitCapable        Whether the controller supports 64-bit memory
                                addressing.

  @retval EFI_SUCCESS           Host controller capability were retrieved successfully.
  @retval EFI_INVALID_PARAMETER Either of the three capability pointer is NULL.
**/

EFI_STATUS
EFIAPI
OhciGetCapability (
  IN  EFI_USB2_HC_PROTOCOL  *This,
  OUT UINT8                 *MaxSpeed,
  OUT UINT8                 *PortNumber,
  OUT UINT8                 *Is64BitCapable
  )
{
  USB_OHCI_HC_DEV  *Ohc;
  EFI_TPL          OldTpl;

  if ((MaxSpeed == NULL) || (PortNumber == NULL) || (Is64BitCapable == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  OldTpl          = gBS->RaiseTPL (TPL_NOTIFY);

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);
  *MaxSpeed       = EFI_USB_SPEED_FULL;
  OhciGetRootHubNumOfPorts (This, PortNumber);
  *Is64BitCapable = 0;

  gBS->RestoreTPL (OldTpl);

  return EFI_SUCCESS;
}

/**
  Start current OHCI device activity

  @param  Ohc                   The OHCI device to start
**/

VOID
OhciStartDev (
  IN USB_OHCI_HC_DEV      *Ohc
)
{
  OhciSetHcControl (Ohc, PERIODIC_ENABLE | CONTROL_ENABLE | BULK_ENABLE, 1); /*ISOCHRONOUS_ENABLE*/
  OhciSetHcControl (Ohc, HC_FUNCTIONAL_STATE, HC_STATE_OPERATIONAL);
  gBS->Stall (50*1000);

  // Wait till first SOF occurs, and then clear it

  while (OhciGetHcInterruptStatus (Ohc, START_OF_FRAME) == 0)
    ;
  OhciClearInterruptStatus (Ohc, START_OF_FRAME);
  gBS->Stall (1000);
}

/**
  Stop current OHCI device activity

  @param  Ohc                   The OHCI device to stop
**/

VOID
OhciStopDev (
  IN USB_OHCI_HC_DEV      *Ohc
)
{
  OhciSetHcControl (Ohc, PERIODIC_ENABLE | CONTROL_ENABLE | ISOCHRONOUS_ENABLE | BULK_ENABLE, 0);
}

/**
  Uninstall all Ohci Interface.

  @param  Controller            Controller handle.
  @param  This                  Protocol instance pointer.
**/

/**
  Provides software reset for the USB host controller.

  @param  This                  This EFI_USB2_HC_PROTOCOL instance.
  @param  Attributes            A bit mask of the reset operation to perform.

  @retval EFI_SUCCESS           The reset operation succeeded.
  @retval EFI_INVALID_PARAMETER Attributes is not valid.
  @retval EFI_UNSUPPOURTED      The type of reset specified by Attributes is
                                not currently supported by the host controller.
  @retval EFI_DEVICE_ERROR      Host controller isn't halted to reset.
**/

EFI_STATUS
EFIAPI
OhciReset (
  IN EFI_USB2_HC_PROTOCOL  *This,
  IN UINT16               Attributes
)
{
  EFI_STATUS              Status;
  USB_OHCI_HC_DEV         *Ohc;
  UINT8                   Index;
  UINT8                   NumOfPorts;
  UINT32                  PowerOnGoodTime;
  UINT32                  Data32;
  BOOLEAN                 Flag = FALSE;

  if ((Attributes & ~(EFI_USB_HC_RESET_GLOBAL | EFI_USB_HC_RESET_HOST_CONTROLLER)) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  if ((Attributes & EFI_USB_HC_RESET_HOST_CONTROLLER) != 0) {
    gBS->Stall (50 * 1000);
    Status = OhciSetHcCommandStatus (Ohc, HC_RESET, HC_RESET);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
    gBS->Stall (50 * 1000);

    // Wait for host controller reset.

    PowerOnGoodTime = 50;
    do {
      gBS->Stall (1 * 1000);
      Data32 = OhciGetOperationalReg (Ohc->PciIo, HC_COMMAND_STATUS);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }
      if ((Data32 & HC_RESET) == 0) {
        Flag = TRUE;
        break;
      }
    } while (PowerOnGoodTime--);
    if (!Flag) {
      return EFI_DEVICE_ERROR;
    }
  }

  OhciFreeIntTransferMemory (Ohc);
  Status = OhciInitializeInterruptList (Ohc);
  OhciSetFrameInterval (Ohc, FRAME_INTERVAL, 0x2edf);
  if ((Attributes &  EFI_USB_HC_RESET_GLOBAL) != 0) {
    Status = OhciSetHcControl (Ohc, HC_FUNCTIONAL_STATE, HC_STATE_RESET);
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
    gBS->Stall (50 * 1000);
  }

  // Initialize host controller operational registers

  OhciSetFrameInterval (Ohc, FS_LARGEST_DATA_PACKET, 0x2778);
  OhciSetFrameInterval (Ohc, FRAME_INTERVAL, 0x2edf);
  OhciSetPeriodicStart (Ohc, 0x2a2f);
  OhciSetHcControl (Ohc, CONTROL_BULK_RATIO, 0x3);
  OhciSetHcCommandStatus (Ohc, CONTROL_LIST_FILLED | BULK_LIST_FILLED, 0);
  OhciSetRootHubDescriptor (Ohc, RH_PSWITCH_MODE, 0);
  OhciSetRootHubDescriptor (Ohc, RH_NO_PSWITCH | RH_NOC_PROT, 1);

  OhciSetRootHubDescriptor (Ohc, RH_DEV_REMOVABLE, 0);
  OhciSetRootHubDescriptor (Ohc, RH_PORT_PWR_CTRL_MASK, 0xffff);
  OhciSetRootHubStatus (Ohc, RH_LOCAL_PSTAT_CHANGE);
  OhciSetRootHubPortStatus (Ohc, 0, RH_SET_PORT_POWER);
  OhciGetRootHubNumOfPorts (This, &NumOfPorts);
  for (Index = 0; Index < NumOfPorts; Index++) {
    if (!EFI_ERROR (OhciSetRootHubPortFeature (This, Index, EfiUsbPortReset))) {
      gBS->Stall (200 * 1000);
      OhciClearRootHubPortFeature (This, Index, EfiUsbPortReset);
      gBS->Stall (1000);
      OhciSetRootHubPortFeature (This, Index, EfiUsbPortEnable);
      gBS->Stall (1000);
    }
  }
  OhciSetMemoryPointer (Ohc, HC_HCCA, Ohc->HccaMemoryBlock);
  OhciSetMemoryPointer (Ohc, HC_CONTROL_HEAD, NULL);
  OhciSetMemoryPointer (Ohc, HC_BULK_HEAD, NULL);

  return Status;
}

/**
  Retrieve the current state of the USB host controller.

  @param  This                  This EFI_USB2_HC_PROTOCOL instance.
  @param  State                 Variable to return the current host controller
                                state.

  @retval EFI_SUCCESS           Host controller state was returned in State.
  @retval EFI_INVALID_PARAMETER State is NULL.
  @retval EFI_DEVICE_ERROR      An error was encountered while attempting to
                                retrieve the host controller's current state.
**/

EFI_STATUS
EFIAPI
OhciGetState (
  IN  EFI_USB2_HC_PROTOCOL  *This,
  OUT EFI_USB_HC_STATE     *State
)
{
  USB_OHCI_HC_DEV         *Ohc;
  UINT32                  FuncState;

  if (State == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  FuncState = OhciGetHcControl (Ohc, HC_FUNCTIONAL_STATE);

  switch (FuncState) {
    case HC_STATE_RESET:
      *State = EfiUsbHcStateHalt;
      break;
    case HC_STATE_RESUME:
      *State = EfiUsbHcStateHalt;
      break;

    case HC_STATE_OPERATIONAL:
      *State = EfiUsbHcStateOperational;
      break;

    case HC_STATE_SUSPEND:
      *State = EfiUsbHcStateSuspend;
      break;

    default:
      ASSERT (FALSE);
  }
  return EFI_SUCCESS;
}

/**
  Sets the USB host controller to a specific state.

  @param  This                  This EFI_USB2_HC_PROTOCOL instance.
  @param  State                 The state of the host controller that will be set.

  @retval EFI_SUCCESS           The USB host controller was successfully placed
                                in the state specified by State.
  @retval EFI_INVALID_PARAMETER State is invalid.
  @retval EFI_DEVICE_ERROR      Failed to set the state due to device error.
**/

EFI_STATUS
EFIAPI
OhciSetState (
  IN EFI_USB2_HC_PROTOCOL  *This,
  IN EFI_USB_HC_STATE     State
)
{
  EFI_STATUS              Status;
  USB_OHCI_HC_DEV         *Ohc;

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  switch (State) {
    case EfiUsbHcStateHalt:
      Status = OhciSetHcControl (Ohc, HC_FUNCTIONAL_STATE, HC_STATE_RESET);
      break;

    case EfiUsbHcStateOperational:
      OhciStartDev (Ohc);
      Status = EFI_SUCCESS;
      break;

    case EfiUsbHcStateSuspend:
      Status = OhciSetHcControl (Ohc, HC_FUNCTIONAL_STATE, HC_STATE_SUSPEND);
      break;

    default:
      Status = EFI_INVALID_PARAMETER;
  }

  gBS->Stall (1000);

  return Status;
}

/**
  Submits control transfer to a target USB device.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB,
                                which is assigned during USB enumeration.
  @param  Device Speed          Indicates target device speed.
  @param  MaxPaketLength        Indicates the maximum packet size that the
                                default control transfer endpoint is capable of
                                sending or receiving.
  @param  Request               A pointer to the USB device request that will be sent
                                to the USB device.
  @param  TransferDirection     Specifies the data direction for the transfer.
                                There are three values available, DataIn, DataOut
                                and NoData.
  @param  Data                  A pointer to the buffer of data that will be transmitted
                                to USB device or received from USB device.
  @param  DataLength            Indicates the size, in bytes, of the data buffer
                                specified by Data.
  @param  TimeOut               Indicates the maximum time, in microseconds,
                                which the transfer is allowed to complete.
  @param  Translator            A pointer to the transaction translator data.
  @param  TransferResult        A pointer to the detailed result information generated
                                by this control transfer.

  @retval EFI_SUCCESS           The control transfer was completed successfully.
  @retval EFI_OUT_OF_RESOURCES  The control transfer could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_TIMEOUT           The control transfer failed due to timeout.
  @retval EFI_DEVICE_ERROR      The control transfer failed due to host controller or device error.
                                Caller should check TranferResult for detailed error information.
**/

EFI_STATUS
EFIAPI
OhciControlTransfer (
  IN     EFI_USB2_HC_PROTOCOL    *This,
  IN     UINT8                   DeviceAddress,
  IN     UINT8                   DeviceSpeed,
  IN     UINTN                   MaximumPacketLength,
  IN     EFI_USB_DEVICE_REQUEST  *Request,
  IN     EFI_USB_DATA_DIRECTION  TransferDirection,
  IN OUT VOID                    *Data                 OPTIONAL,
  IN OUT UINTN                   *DataLength           OPTIONAL,
  IN     UINTN                   TimeOut,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  OUT    UINT32                  *TransferResult
)
{
  USB_OHCI_HC_DEV                *Ohc;
  ED_DESCRIPTOR                  *HeadEd;
  ED_DESCRIPTOR                  *Ed;
  TD_DESCRIPTOR                  *HeadTd;
  TD_DESCRIPTOR                  *DataTd;
  EFI_STATUS                     Status;
  UINT8                          DataPidDir;
  UINT32                         StatusPidDir;
  UINTN                          TimeCount;
  OHCI_ED_RESULT                 EdResult;

  EFI_PCI_IO_PROTOCOL_OPERATION  MapOp;

  VOID                           *ReqMapping = NULL;
  UINTN                          ReqMapLength = 0;
  EFI_PHYSICAL_ADDRESS           ReqMapPhyAddr = 0;

  VOID                           *DataMapping = NULL;
  UINTN                          DataMapLength = 0;
  EFI_PHYSICAL_ADDRESS           DataMapPhyAddr = 0;

  HeadTd = NULL;
  DataTd = NULL;

  //
  // Parameters Checking
  //
  if (Request == NULL || TransferResult == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (DeviceSpeed == EFI_USB_SPEED_LOW && (MaximumPacketLength != 8)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((MaximumPacketLength != 8) &&  (MaximumPacketLength != 16) &&
      (MaximumPacketLength != 32) && (MaximumPacketLength != 64)) {

    return EFI_INVALID_PARAMETER;
  }

  if ((TransferDirection != EfiUsbNoData) && (Data == NULL || DataLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((TransferDirection == EfiUsbNoData) && (Data != NULL || DataLength == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (*DataLength > MAX_BYTES_PER_TD) {
    return EFI_INVALID_PARAMETER;
  }

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS(This);

  if (TransferDirection == EfiUsbDataIn) {
    DataPidDir = TD_IN_PID;
    StatusPidDir = TD_OUT_PID;
  } else {
    DataPidDir = TD_OUT_PID;
    StatusPidDir = TD_IN_PID;
  }

  Status = OhciSetHcControl (Ohc, CONTROL_ENABLE, 0);
  if (EFI_ERROR(Status)) {
    *TransferResult = EFI_USB_ERR_SYSTEM;
    return EFI_DEVICE_ERROR;
  }
  Status = OhciSetHcCommandStatus (Ohc, CONTROL_LIST_FILLED, 0);
  if (EFI_ERROR(Status)) {
    *TransferResult = EFI_USB_ERR_SYSTEM;
    return EFI_DEVICE_ERROR;
  }
  gBS->Stall(20 * 1000);

  OhciSetMemoryPointer (Ohc, HC_CONTROL_HEAD, NULL);
  Ed = OhciCreateED (Ohc, DeviceAddress, 0, DeviceSpeed, MaximumPacketLength);
  if (Ed == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CTRL_EXIT;
  }

  HeadEd = OhciAttachEDToList (Ohc, CONTROL_LIST, Ed, NULL);

  if (Request != NULL) {
    ReqMapLength = sizeof(EFI_USB_DEVICE_REQUEST);
    MapOp = EfiPciIoOperationBusMasterRead;
    Status = Ohc->PciIo->Map (Ohc->PciIo, MapOp, (UINT8 *)Request, &ReqMapLength, &ReqMapPhyAddr, &ReqMapping);
    if (EFI_ERROR(Status)) {
      goto FREE_ED_BUFF;
    }
  }
  if (TransferDirection == EfiUsbDataIn) {
    MapOp = EfiPciIoOperationBusMasterWrite;
  } else {
    MapOp = EfiPciIoOperationBusMasterRead;
  }
  DataMapLength = *DataLength;
  if ((Data != NULL) && (DataMapLength != 0)) {
    Status = Ohc->PciIo->Map (Ohc->PciIo, MapOp, Data, &DataMapLength, &DataMapPhyAddr, &DataMapping);
    if (EFI_ERROR(Status)) {
      goto FREE_TD_BUFF;
    }
  }

  HeadTd = OhciCreateCtrlTds (
             Ohc,
             DeviceAddress,
             DataPidDir,
             (UINT8 *) Request,
             (UINT8 *)(UINTN) ReqMapPhyAddr,
             (UINT8 *) Data,
             (UINT8 *)(UINTN) DataMapPhyAddr,
             *DataLength,
             MaximumPacketLength
           ); 
  if (HeadTd == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto UNMAP_DATA_BUFF;
  }

  OhciAttachTDListToED (Ed, HeadTd);

#if 0
  //
  // For debugging, dump ED & TD buffer before transfer
  //
  OhciDumpEdTdInfo (Ohc, Ed, HeadTd, TRUE);
#endif

  OhciSetEDField (Ed, ED_SKIP, 0);
  Status = OhciSetHcControl (Ohc, CONTROL_ENABLE, 1);
  if (EFI_ERROR(Status)) {
    *TransferResult = EFI_USB_ERR_SYSTEM;
    Status = EFI_DEVICE_ERROR;
    goto UNMAP_DATA_BUFF;
  }
  Status = OhciSetHcCommandStatus (Ohc, CONTROL_LIST_FILLED, 1);
  if (EFI_ERROR(Status)) {
    *TransferResult = EFI_USB_ERR_SYSTEM;
    Status = EFI_DEVICE_ERROR;
    goto UNMAP_DATA_BUFF;
  }
  gBS->Stall(20 * 1000);

  TimeCount = 0;
  Status = OhciCheckIfDone (Ohc, CONTROL_LIST, Ed, HeadTd, &EdResult);

  while (Status == EFI_NOT_READY && TimeCount <= TimeOut) {
    gBS->Stall (1000);
    TimeCount++;
    Status = OhciCheckIfDone (Ohc, CONTROL_LIST, Ed, HeadTd, &EdResult);
  }

#if 0
  //
  // For debugging, dump ED & TD buffer after transfer
  //
  OhciDumpEdTdInfo (Ohc, Ed, HeadTd, FALSE);
#endif

  *TransferResult = OhciConvertErrorCode (EdResult.ErrorCode);

  if (EdResult.ErrorCode != TD_NO_ERROR) {
    if (EdResult.ErrorCode == TD_TOBE_PROCESSED) {
      DEBUG ((EFI_D_INFO, "%a: Control pipe timeout, > %d mS\n", __FUNCTION__, TimeOut));
    } else {
      DEBUG ((EFI_D_INFO, "%a: Control pipe broken\n", __FUNCTION__));
    }
    *DataLength = 0;
  }

UNMAP_DATA_BUFF:
  OhciSetEDField (Ed, ED_SKIP, 1);
  if (HeadEd == Ed) {
    OhciSetMemoryPointer (Ohc, HC_CONTROL_HEAD, NULL);
  } else {
    HeadEd->NextED = Ed->NextED;
  }

  if (DataMapping != NULL) {
    Ohc->PciIo->Unmap(Ohc->PciIo, DataMapping);
  }

FREE_TD_BUFF:
  OhciDestroyTds (Ohc, HeadTd);

  if (ReqMapping != NULL) {
    Ohc->PciIo->Unmap(Ohc->PciIo, ReqMapping);
  }

FREE_ED_BUFF:
  OhciFreeED (Ohc, Ed);

CTRL_EXIT:
  return Status;
}

/**
  Submits bulk transfer to a bulk endpoint of a USB device.

  @param  This                A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress       Represents the address of the target device on the USB.
  @param  EndPointAddress     The combination of an endpoint number and an endpoint direction of the
                              target USB device.
  @param  DeviceSpeed         Indicates device speed.
  @param  MaximumPacketLength Indicates the maximum packet size the target endpoint is capable of
                              sending or receiving.
  @param  DataBuffersNumber   Number of data buffers prepared for the transfer.
  @param  Data                Array of pointers to the buffers of data that will be transmitted to USB
                              device or received from USB device.
  @param  DataLength          When input, indicates the size, in bytes, of the data buffers specified by
                              Data. When output, indicates the actually transferred data size.
  @param  DataToggle          A pointer to the data toggle value.
  @param  TimeOut             Indicates the maximum time, in milliseconds, which the transfer is
                              allowed to complete.
  @param  Translator          A pointer to the transaction translator data.
  @param  TransferResult      A pointer to the detailed result information of the bulk transfer.

  @retval EFI_SUCCESS           The bulk transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The bulk transfer could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The bulk transfer failed due to timeout.
  @retval EFI_DEVICE_ERROR      The bulk transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.
**/

EFI_STATUS
EFIAPI
OhciBulkTransfer (
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              EndPointAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     UINT8                              DataBuffersNumber,
  IN OUT VOID                               *Data[EFI_USB_MAX_BULK_BUFFER_NUM],
  IN OUT UINTN                              *DataLength,
  IN OUT UINT8                              *DataToggle,
  IN     UINTN                              TimeOut,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  OUT    UINT32                             *TransferResult
)
{
  USB_OHCI_HC_DEV                *Ohc;
  ED_DESCRIPTOR                  *HeadEd;
  ED_DESCRIPTOR                  *Ed;
  UINT8                          EdDir;
  UINT8                          DataPidDir;
  TD_DESCRIPTOR                  *HeadTd;
  EFI_STATUS                     Status;
  EFI_USB_DATA_DIRECTION         TransferDirection;
  UINT8                          EndPointNum;
  UINTN                          TimeCount;
  OHCI_ED_RESULT                 EdResult;

  EFI_PCI_IO_PROTOCOL_OPERATION  MapOp;
  VOID                           *Mapping;
  UINTN                          MapLength;
  EFI_PHYSICAL_ADDRESS           MapPhyAddr;

  Mapping = NULL;
  MapLength = 0;
  MapPhyAddr = 0;
  Status = EFI_SUCCESS;

  if (Data == NULL || DataLength == NULL || DataToggle == NULL || TransferResult == NULL ||
      *DataLength == 0 || (*DataToggle != 0 && *DataToggle != 1) ||
      (MaximumPacketLength != 8 && MaximumPacketLength != 16 &&
       MaximumPacketLength != 32 && MaximumPacketLength != 64)) {
    return EFI_INVALID_PARAMETER;
  }

  if (DataBuffersNumber != 1) {
    DEBUG ((EFI_D_ERROR, "%a: OOPS! DataBuffersNumber != 1\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  if ((EndPointAddress & 0x80) != 0) {
    TransferDirection = EfiUsbDataIn;
    EdDir = ED_IN_DIR;
    DataPidDir = TD_IN_PID;
    MapOp = EfiPciIoOperationBusMasterWrite;
  } else {
    TransferDirection = EfiUsbDataOut;
    EdDir = ED_OUT_DIR;
    DataPidDir = TD_OUT_PID;
    MapOp = EfiPciIoOperationBusMasterRead;
  }

  EndPointNum = (EndPointAddress & 0xF);
  EdResult.NextToggle = *DataToggle;

  Status = OhciSetHcControl (Ohc, BULK_ENABLE, 0);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: fail to disable BULK_ENABLE\n", __FUNCTION__));
    *TransferResult = EFI_USB_ERR_SYSTEM;
    return EFI_DEVICE_ERROR;
  }
  Status = OhciSetHcCommandStatus (Ohc, BULK_LIST_FILLED, 0);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: fail to disable BULK_LIST_FILLED\n", __FUNCTION__));
    *TransferResult = EFI_USB_ERR_SYSTEM;
    return EFI_DEVICE_ERROR;
  }
  gBS->Stall(20 * 1000);

  OhciSetMemoryPointer (Ohc, HC_BULK_HEAD, NULL);

  Ed = OhciCreateED (Ohc, DeviceAddress, EndPointNum, /* XXX */ ED_HI_SPEED, MaximumPacketLength);
  if (Ed == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  HeadEd = OhciAttachEDToList (Ohc, BULK_LIST, Ed, NULL);

  MapLength = *DataLength;
  Status = Ohc->PciIo->Map (Ohc->PciIo, MapOp, (UINT8 *)Data[0], &MapLength, &MapPhyAddr, &Mapping);
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Fail to Map Data Buffer for Bulk transfer\n", __FUNCTION__));    
    goto FREE_ED_BUFF;
  }

  HeadTd = OhciCreateBulkOrIntTds (
             Ohc,
             DeviceAddress,
             EndPointNum,
             DataPidDir,
             (UINT8 *) Data[0],
             (UINT8 *)(UINTN) MapPhyAddr,
             *DataLength,
             DataToggle,
             MaximumPacketLength
           ); 
  if (HeadTd == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FREE_OHCI_TDBUFF;
  }

  OhciAttachTDListToED (Ed, HeadTd);

  OhciSetEDField (Ed, ED_SKIP, 0);
  Status = OhciSetHcCommandStatus (Ohc, BULK_LIST_FILLED, 1);
  if (EFI_ERROR(Status)) {
    *TransferResult = EFI_USB_ERR_SYSTEM;
    DEBUG ((EFI_D_ERROR, "%a: Fail to enable BULK_LIST_FILLED\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto FREE_OHCI_TDBUFF;
  }
  Status = OhciSetHcControl (Ohc, BULK_ENABLE, 1);
  if (EFI_ERROR(Status)) {
    *TransferResult = EFI_USB_ERR_SYSTEM;
    DEBUG ((EFI_D_ERROR, "%a: Fail to enable BULK_ENABLE\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto FREE_OHCI_TDBUFF;
  }
  gBS->Stall(20 * 1000);

  TimeCount = 0;
  Status = OhciCheckIfDone (Ohc, BULK_LIST, Ed, HeadTd, &EdResult);
  while (Status == EFI_NOT_READY && TimeCount <= TimeOut) {
    gBS->Stall (1000);
    TimeCount++;
    Status = OhciCheckIfDone (Ohc, BULK_LIST, Ed, HeadTd, &EdResult);
  }

  *TransferResult = OhciConvertErrorCode (EdResult.ErrorCode);

  if (EdResult.ErrorCode != TD_NO_ERROR) {
    if (EdResult.ErrorCode == TD_TOBE_PROCESSED) {
      DEBUG ((EFI_D_ERROR, "%a: Bulk pipe timeout (%d mS)\n", __FUNCTION__, TimeOut));
    } else {
      DEBUG ((EFI_D_ERROR, "%a: Bulk pipe broken (0x%x)\n", __FUNCTION__, EdResult.ErrorCode));
      *DataToggle = EdResult.NextToggle;
    }
    *DataLength = 0;
  }

FREE_OHCI_TDBUFF:
  OhciSetEDField (Ed, ED_SKIP, 1);
  if (HeadEd == Ed) {
    OhciSetMemoryPointer (Ohc, HC_BULK_HEAD, NULL);
  }else {
    HeadEd->NextED = Ed->NextED;
  }

  OhciDestroyTds (Ohc, HeadTd);

  if (Mapping != NULL) {
    Ohc->PciIo->Unmap(Ohc->PciIo, Mapping);
  }

FREE_ED_BUFF:
  OhciFreeED (Ohc, Ed);

  return Status;
}

/**
  Submits an interrupt transfer to an interrupt endpoint of a USB device.

  @param  Ohc                   Device private data
  @param  DeviceAddress         Represents the address of the target device on the USB,
                                which is assigned during USB enumeration.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint
                                direction of the target USB device. Each endpoint address
                                supports data transfer in one direction except the
                                control endpoint (whose default endpoint address is 0).
                                It is the caller's responsibility to make sure that
                                the EndPointAddress represents an interrupt endpoint.
  @param  DeviceSpeed           Indicates target device speed.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint
                                is capable of sending or receiving.
  @param  IsNewTransfer         If TRUE, an asynchronous interrupt pipe is built between
                                the host and the target interrupt endpoint.
                                If FALSE, the specified asynchronous interrupt pipe
                                is canceled.
  @param  DataToggle            A pointer to the data toggle value.  On input, it is valid
                                when IsNewTransfer is TRUE, and it indicates the initial
                                data toggle value the asynchronous interrupt transfer
                                should adopt.
                                On output, it is valid when IsNewTransfer is FALSE,
                                and it is updated to indicate the data toggle value of
                                the subsequent asynchronous interrupt transfer.
  @param  PollingInterval       Indicates the interval, in milliseconds, that the
                                asynchronous interrupt transfer is polled.
                                This parameter is required when IsNewTransfer is TRUE.
  @param  UCBuffer              Uncacheable buffer
  @param  DataLength            Indicates the length of data to be received at the
                                rate specified by PollingInterval from the target
                                asynchronous interrupt endpoint.  This parameter
                                is only required when IsNewTransfer is TRUE.
  @param  CallBackFunction      The Callback function.This function is called at the
                                rate specified by PollingInterval.This parameter is
                                only required when IsNewTransfer is TRUE.
  @param  Context               The context that is passed to the CallBackFunction.
                                This is an optional parameter and may be NULL.
  @param  IsPeriodic            Periodic interrupt or not
  @param  OutputED              The correspoding ED carried out
  @param  OutputTD              The correspoding TD carried out


  @retval EFI_SUCCESS           The asynchronous interrupt transfer request has been successfully
                                submitted or canceled.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
**/

EFI_STATUS
OhciInterruptTransfer (
  IN     USB_OHCI_HC_DEV                  *Ohc,
  IN     UINT8                            DeviceAddress,
  IN     UINT8                            EndPointAddress,
  IN     UINT8                            DeviceSpeed,
  IN     UINTN                            MaximumPacketLength,
  IN     BOOLEAN                          IsNewTransfer,
  IN OUT UINT8                            *DataToggle        OPTIONAL,
  IN     UINTN                            PollingInterval    OPTIONAL,
  IN     VOID                             *UCBuffer          OPTIONAL,
  IN     UINTN                            DataLength         OPTIONAL,
  IN     EFI_ASYNC_USB_TRANSFER_CALLBACK  CallBackFunction   OPTIONAL,
  IN     VOID                             *Context           OPTIONAL,
  IN     BOOLEAN                          IsPeriodic         OPTIONAL,
  OUT    ED_DESCRIPTOR                    **OutputED         OPTIONAL,
  OUT    TD_DESCRIPTOR                    **OutputTD         OPTIONAL
)
{
  ED_DESCRIPTOR            *Ed;
  UINT8                    EdDir;
  ED_DESCRIPTOR            *HeadEd;
  TD_DESCRIPTOR            *HeadTd;
  UINTN                    Depth;
  UINTN                    Index;
  EFI_STATUS               Status;
  UINT8                    EndPointNum;
  UINT8                    DataPidDir;
  EFI_USB_DATA_DIRECTION   TransferDirection;
  INTERRUPT_CONTEXT_ENTRY  *Entry;
  EFI_TPL                  OldTpl;

  VOID                     *Mapping;
  UINTN                    MapLength;
  EFI_PHYSICAL_ADDRESS     MapPhyAddr;

  if (DataLength > MAX_BYTES_PER_TD) {
    return EFI_INVALID_PARAMETER;
  }

  if ((EndPointAddress & 0x80) != 0) {
    TransferDirection = EfiUsbDataIn;
    EdDir = ED_IN_DIR;
    DataPidDir = TD_IN_PID;
  } else {
    TransferDirection = EfiUsbDataOut;
    EdDir = ED_OUT_DIR;
    DataPidDir = TD_OUT_PID;
  }

  EndPointNum = (EndPointAddress & 0xF);

  if (!IsNewTransfer) {
    OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
    OhciSetHcControl (Ohc, PERIODIC_ENABLE, 0);
    OhciFreeInterruptContext (Ohc, DeviceAddress, EndPointAddress, DataToggle);
    Status = OhciFreeInterruptEdByAddr (Ohc, DeviceAddress, EndPointNum);
    OhciSetHcControl (Ohc, PERIODIC_ENABLE, 1);
    gBS->RestoreTPL (OldTpl);
    return Status;
  }
  MapLength = DataLength;
  Status = Ohc->PciIo->Map (
                         Ohc->PciIo,
                         EfiPciIoOperationBusMasterWrite,
                         UCBuffer,
                         &MapLength,
                         &MapPhyAddr,
                         &Mapping
                       );
  if (EFI_ERROR (Status)) {
    goto EXIT;
  }
  Depth = 5;
  Index = 1;
  while (PollingInterval >= Index * 2 && Depth > 0) {
    Index *= 2;
    Depth--;
  }

  // ED Stage

  HeadEd = OhciFindMinInterruptEDList (Ohc, (UINT32) Depth);
  if ((Ed = OhciFindWorkingEd (HeadEd, DeviceAddress, EndPointNum, EdDir)) != NULL) {
    OhciSetEDField (Ed, ED_SKIP, 1);
  } else {
    Ed = OhciCreateED (Ohc, DeviceAddress, EndPointNum, DeviceSpeed, MaximumPacketLength);
    if (Ed == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto UNMAP_OHCI_XBUFF;
    }

    OhciAttachEDToList (Ohc, INTERRUPT_LIST, Ed, HeadEd);
  }

  HeadTd = OhciCreateBulkOrIntTds (
             Ohc,
             DeviceAddress,
             EndPointNum,
             DataPidDir,
             (UINT8 *) UCBuffer,
             (UINT8 *)(UINTN) MapPhyAddr,
             DataLength,
             DataToggle,
             MaximumPacketLength
           ); 
  if (HeadTd == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FREE_OHCI_TDBUFF;
  }

  OhciAttachTDListToED (Ed, HeadTd);

  if (OutputED != NULL) {
    *OutputED = Ed;
  }
  if (OutputTD != NULL) {
    *OutputTD = HeadTd;
  }

  if (CallBackFunction != NULL) {
    Entry = AllocateZeroPool (sizeof (INTERRUPT_CONTEXT_ENTRY));
    if (Entry == NULL) {
      goto FREE_OHCI_TDBUFF;
    }

    Entry->DeviceAddress = DeviceAddress;
    Entry->EndPointAddress = EndPointAddress;
    Entry->Ed = Ed;
    Entry->DataTd = HeadTd;
    Entry->DeviceSpeed = DeviceSpeed;
    Entry->MaximumPacketLength = MaximumPacketLength;
    Entry->PollingInterval = PollingInterval;
    Entry->CallBackFunction = CallBackFunction;
    Entry->Context = Context;
    Entry->IsPeriodic = IsPeriodic;
    Entry->UCBuffer = UCBuffer;
    Entry->UCBufferMapping = Mapping;
    Entry->DataLength = DataLength;
    Entry->Toggle = DataToggle;
    OhciAddInterruptContextEntry (Ohc, Entry);
  }
  OhciSetEDField (Ed, ED_SKIP, 0);

  if (OhciGetHcControl (Ohc, PERIODIC_ENABLE) == 0) {
    Status = OhciSetHcControl (Ohc, PERIODIC_ENABLE, 1);
    gBS->Stall (1000);
  }

  return EFI_SUCCESS;

FREE_OHCI_TDBUFF:
  OhciDestroyTds (Ohc, HeadTd);

  if ((HeadEd != Ed) && HeadEd && Ed) {
    while (HeadEd->NextED != (UINT32)(UINTN) Ed) {
      HeadEd = (ED_DESCRIPTOR *)(UINTN) (HeadEd->NextED);
    }
    HeadEd->NextED = Ed->NextED;
    OhciFreeED (Ohc, Ed);
  }

UNMAP_OHCI_XBUFF:
  Ohc->PciIo->Unmap(Ohc->PciIo, Mapping);

EXIT:
  return Status;
}

/**
  Submits an asynchronous interrupt transfer to an interrupt endpoint of a USB device.
  Translator parameter doesn't exist in UEFI2.0 spec, but it will be updated in the following specification version.

  @param  This                A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress       Represents the address of the target device on the USB.
  @param  EndPointAddress     The combination of an endpoint number and an endpoint direction of the
                              target USB device.
  @param  DeviceSpeed         Indicates device speed.
  @param  MaximumPacketLength Indicates the maximum packet size the target endpoint is capable of
                              sending or receiving.
  @param  IsNewTransfer       If TRUE, an asynchronous interrupt pipe is built between the host and the
                              target interrupt endpoint. If FALSE, the specified asynchronous interrupt
                              pipe is canceled. If TRUE, and an interrupt transfer exists for the target
                              end point, then EFI_INVALID_PARAMETER is returned.
  @param  DataToggle          A pointer to the data toggle value.
  @param  PollingInterval     Indicates the interval, in milliseconds, that the asynchronous interrupt
                              transfer is polled.
  @param  DataLength          Indicates the length of data to be received at the rate specified by
                              PollingInterval from the target asynchronous interrupt endpoint.
  @param  Translator          A pointr to the transaction translator data.
  @param  CallBackFunction    The Callback function. This function is called at the rate specified by
                              PollingInterval.
  @param  Context             The context that is passed to the CallBackFunction. This is an
                              optional parameter and may be NULL.

  @retval EFI_SUCCESS           The asynchronous interrupt transfer request has been successfully
                                submitted or canceled.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
**/

EFI_STATUS
EFIAPI
OhciAsyncInterruptTransfer (
  IN     EFI_USB2_HC_PROTOCOL                                *This,
  IN     UINT8                                               DeviceAddress,
  IN     UINT8                                               EndPointAddress,
  IN     UINT8                                               DeviceSpeed,
  IN     UINTN                                               MaximumPacketLength,
  IN     BOOLEAN                                             IsNewTransfer,
  IN OUT UINT8                                               *DataToggle,
  IN     UINTN                                               PollingInterval  OPTIONAL,
  IN     UINTN                                               DataLength       OPTIONAL,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR                  *Translator      OPTIONAL,
  IN     EFI_ASYNC_USB_TRANSFER_CALLBACK                     CallBackFunction OPTIONAL,
  IN     VOID                                                *Context         OPTIONAL
)
{
  EFI_STATUS              Status;
  USB_OHCI_HC_DEV         *Ohc;
  VOID                    *UCBuffer;

  if (DataToggle == NULL || (EndPointAddress & 0x80) == 0 ||
    (IsNewTransfer && (DataLength == 0 ||
    (*DataToggle != 0 && *DataToggle != 1) || (PollingInterval < 1 || PollingInterval > 255)))) {
    return EFI_INVALID_PARAMETER;
  }

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);
  if (IsNewTransfer) {
    UCBuffer = AllocatePool (DataLength);
    if (UCBuffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    UCBuffer = NULL;
  }
  Status = OhciInterruptTransfer (
             Ohc,
             DeviceAddress,
             EndPointAddress,
             DeviceSpeed,
             MaximumPacketLength,
             IsNewTransfer,
             DataToggle,
             PollingInterval,
             UCBuffer,
             DataLength,
             CallBackFunction,
             Context,
             TRUE,
             NULL,
             NULL
           );
  if (IsNewTransfer) {
    if (EFI_ERROR(Status)) {
      gBS->FreePool (UCBuffer);
    }
  }
  return Status;
}

/**
  Submits synchronous interrupt transfer to an interrupt endpoint of a USB device.
  Translator parameter doesn't exist in UEFI2.0 spec, but it will be updated in the following specification version.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint direction of the
                                target USB device.
  @param  DeviceSpeed           Indicates device speed.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint is capable of
                                sending or receiving.
  @param  Data                  A pointer to the buffer of data that will be transmitted to USB device or
                                received from USB device.
  @param  DataLength            On input, the size, in bytes, of the data buffer specified by Data. On
                                output, the number of bytes transferred.
  @param  DataToggle            A pointer to the data toggle value.
  @param  TimeOut               Indicates the maximum time, in milliseconds, which the transfer is
                                allowed to complete.
  @param  Translator            A pointr to the transaction translator data.
  @param  TransferResult        A pointer to the detailed result information from the synchronous
                                interrupt transfer.

  @retval EFI_SUCCESS           The synchronous interrupt transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The synchronous interrupt transfer could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The synchronous interrupt transfer failed due to timeout.
  @retval EFI_DEVICE_ERROR      The synchronous interrupt transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.
**/

EFI_STATUS
EFIAPI
OhciSyncInterruptTransfer (
  IN     EFI_USB2_HC_PROTOCOL                        *This,
  IN     UINT8                                       DeviceAddress,
  IN     UINT8                                       EndPointAddress,
  IN     UINT8                                       DeviceSpeed,
  IN     UINTN                                       MaximumPacketLength,
  IN OUT VOID                                        *Data,
  IN OUT UINTN                                       *DataLength,
  IN OUT UINT8                                       *DataToggle,
  IN     UINTN                                       TimeOut,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR          *Translator,
  OUT    UINT32                                      *TransferResult
)
{
  USB_OHCI_HC_DEV         *Ohc;
  EFI_STATUS              Status;
  ED_DESCRIPTOR           *Ed;
  TD_DESCRIPTOR           *HeadTd;
  OHCI_ED_RESULT          EdResult;
  VOID                    *UCBuffer;

  if ((EndPointAddress & 0x80) == 0 || Data == NULL || DataLength == NULL || *DataLength == 0 ||
      DataToggle == NULL || (*DataToggle != 0 && *DataToggle != 1) || TransferResult == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (DeviceSpeed == EFI_USB_SPEED_FULL) {
    if (MaximumPacketLength > 64) {
      return EFI_INVALID_PARAMETER;
    }
  } else {
    if (MaximumPacketLength > 8) {
      return EFI_INVALID_PARAMETER;
    }
  }

  HeadTd = NULL;

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);
  UCBuffer = AllocatePool (*DataLength);
  if (UCBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  Status = OhciInterruptTransfer (
             Ohc,
             DeviceAddress,
             EndPointAddress,
             DeviceSpeed,
             MaximumPacketLength,
             TRUE,
             DataToggle,
             1,
             UCBuffer,
             *DataLength,
             NULL,
             NULL,
             FALSE,
             &Ed,
             &HeadTd
           );

  if (!EFI_ERROR (Status)) {
    Status = OhciCheckIfDone (Ohc, INTERRUPT_LIST, Ed, HeadTd, &EdResult);
    while (Status == EFI_NOT_READY && TimeOut > 0) {
      gBS->Stall (1000);
      TimeOut--;
      Status = OhciCheckIfDone (Ohc, INTERRUPT_LIST, Ed, HeadTd, &EdResult);
    }

    *TransferResult = OhciConvertErrorCode (EdResult.ErrorCode);
  }
  CopyMem(Data, UCBuffer, *DataLength);
  Status = OhciInterruptTransfer (
             Ohc,
             DeviceAddress,
             EndPointAddress,
             DeviceSpeed,
             MaximumPacketLength,
             FALSE,
             DataToggle,
             0,
             NULL,
             0,
             NULL,
             NULL,
             FALSE,
             NULL,
             NULL
           );

  return Status;
}

/**
  Submits isochronous transfer to an isochronous endpoint of a USB device.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint direction of the
                                target USB device.
  @param  DeviceSpeed           Indicates device speed.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint is capable of
                                sending or receiving.
  @param  DataBuffersNumber     Number of data buffers prepared for the transfer.
  @param  Data                  Array of pointers to the buffers of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            Specifies the length, in bytes, of the data to be sent to or received from
                                the USB device.
  @param  Translator            A pointer to the transaction translator data.
  @param  TransferResult        A pointer to the detailed result information of the isochronous transfer.

  @retval EFI_SUCCESS           The isochronous transfer was completed successfully.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The isochronous transfer could not be submitted due to a lack of resources.
  @retval EFI_TIMEOUT           The isochronous transfer cannot be completed within the one USB frame time.
  @retval EFI_DEVICE_ERROR      The isochronous transfer failed due to host controller or device error.
                                Caller should check TransferResult for detailed error information.
**/

EFI_STATUS
EFIAPI
OhciIsochronousTransfer (
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              EndPointAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     UINT8                              DataBuffersNumber,
  IN OUT VOID                               *Data[EFI_USB_MAX_ISO_BUFFER_NUM],
  IN     UINTN                              DataLength,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  OUT    UINT32                             *TransferResult
)
{
  return EFI_UNSUPPORTED;
}

/**
  Submits nonblocking isochronous transfer to an isochronous endpoint of a USB device.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  DeviceAddress         Represents the address of the target device on the USB.
  @param  EndPointAddress       The combination of an endpoint number and an endpoint direction of the
                                target USB device.
  @param  DeviceSpeed           Indicates device speed.
  @param  MaximumPacketLength   Indicates the maximum packet size the target endpoint is capable of
                                sending or receiving.
  @param  DataBuffersNumber     Number of data buffers prepared for the transfer.
  @param  Data                  Array of pointers to the buffers of data that will be transmitted to USB
                                device or received from USB device.
  @param  DataLength            Specifies the length, in bytes, of the data to be sent to or received from
                                the USB device.
  @param  Translator            A pointer to the transaction translator data.
  @param  IsochronousCallback   The Callback function. This function is called if the requested
                                isochronous transfer is completed.
  @param  Context               Data passed to the IsochronousCallback function. This is an
                                optional parameter and may be NULL.

  @retval EFI_SUCCESS           The asynchronous isochronous transfer request has been successfully
                                submitted or canceled.
  @retval EFI_INVALID_PARAMETER Some parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The asynchronous isochronous transfer could not be submitted due to
                                a lack of resources.
**/

EFI_STATUS
EFIAPI
OhciAsyncIsochronousTransfer (
  IN     EFI_USB2_HC_PROTOCOL               *This,
  IN     UINT8                              DeviceAddress,
  IN     UINT8                              EndPointAddress,
  IN     UINT8                              DeviceSpeed,
  IN     UINTN                              MaximumPacketLength,
  IN     UINT8                              DataBuffersNumber,
  IN OUT VOID                               *Data[EFI_USB_MAX_ISO_BUFFER_NUM],
  IN     UINTN                              DataLength,
  IN     EFI_USB2_HC_TRANSACTION_TRANSLATOR *Translator,
  IN     EFI_ASYNC_USB_TRANSFER_CALLBACK    IsochronousCallBack,
  IN     VOID                               *Context OPTIONAL
)
{
  return EFI_UNSUPPORTED;
}

/**
  Retrieves the number of root hub ports.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  NumOfPorts            A pointer to the number of the root hub ports.

  @retval EFI_SUCCESS           The port number was retrieved successfully.
**/

EFI_STATUS
EFIAPI
OhciGetRootHubNumOfPorts (
  IN  EFI_USB2_HC_PROTOCOL  *This,
  OUT UINT8                *NumOfPorts
)
{
  USB_OHCI_HC_DEV  *Ohc;
  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  if (NumOfPorts == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *NumOfPorts = (UINT8)OhciGetRootHubDescriptor (Ohc, RH_NUM_DS_PORTS);

  return EFI_SUCCESS;
}

/**
  Retrieves the current status of a USB root hub port.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL.
  @param  PortNumber            Specifies the root hub port from which the status
                                is to be retrieved.  This value is zero-based. For example,
                                if a root hub has two ports, then the first port is numbered 0,
                                and the second port is numbered 1.
  @param  PortStatus            A pointer to the current port status bits and
                                port status change bits.

  @retval EFI_SUCCESS           The status of the USB root hub port specified by PortNumber
                                was returned in PortStatus.
  @retval EFI_INVALID_PARAMETER Port number not valid
**/

EFI_STATUS
EFIAPI
OhciGetRootHubPortStatus (
  IN  EFI_USB2_HC_PROTOCOL  *This,
  IN  UINT8                PortNumber,
  OUT EFI_USB_PORT_STATUS  *PortStatus
)
{
  USB_OHCI_HC_DEV  *Ohc;
  UINT8            NumOfPorts;

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  OhciGetRootHubNumOfPorts (This, &NumOfPorts);
  if (PortNumber >= NumOfPorts) {
    return EFI_INVALID_PARAMETER;
  }
  PortStatus->PortStatus = 0;
  PortStatus->PortChangeStatus = 0;

  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_CURR_CONNECT_STAT)) {
    PortStatus->PortStatus |= USB_PORT_STAT_CONNECTION;
  }
  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_PORT_ENABLE_STAT)) {
    PortStatus->PortStatus |= USB_PORT_STAT_ENABLE;
  }
  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_PORT_SUSPEND_STAT)) {
    PortStatus->PortStatus |= USB_PORT_STAT_SUSPEND;
  }
  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_PORT_OC_INDICATOR)) {
    PortStatus->PortStatus |= USB_PORT_STAT_OVERCURRENT;
  }
  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_PORT_RESET_STAT)) {
    PortStatus->PortStatus |= USB_PORT_STAT_RESET;
  }
  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_PORT_POWER_STAT)) {
    PortStatus->PortStatus |= USB_PORT_STAT_POWER;
  }
  if (OhciReadRootHubPortStatus (Ohc,PortNumber, RH_LSDEVICE_ATTACHED)) {
    PortStatus->PortStatus |= USB_PORT_STAT_LOW_SPEED;
  }
  if (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_ENABLE_STAT_CHANGE)) {
    PortStatus->PortChangeStatus |= USB_PORT_STAT_C_ENABLE;
  }
  if (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_CONNECT_STATUS_CHANGE)) {
    PortStatus->PortChangeStatus |= USB_PORT_STAT_C_CONNECTION;
  }
  if (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_SUSPEND_STAT_CHANGE)) {
    PortStatus->PortChangeStatus |= USB_PORT_STAT_C_SUSPEND;
  }
  if (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_OC_INDICATOR_CHANGE)) {
    PortStatus->PortChangeStatus |= USB_PORT_STAT_C_OVERCURRENT;
  }
  if (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_RESET_STAT_CHANGE)) {
    PortStatus->PortChangeStatus |= USB_PORT_STAT_C_RESET;
  }

  return EFI_SUCCESS;
}

/**
  Sets a feature for the specified root hub port.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL.
  @param  PortNumber            Specifies the root hub port whose feature
                                is requested to be set.
  @param  PortFeature           Indicates the feature selector associated
                                with the feature set request.

  @retval EFI_SUCCESS           The feature specified by PortFeature was set for the
                                USB root hub port specified by PortNumber.
  @retval EFI_DEVICE_ERROR      Set feature failed because of hardware issue
  @retval EFI_INVALID_PARAMETER PortNumber is invalid or PortFeature is invalid.
**/

EFI_STATUS
EFIAPI
OhciSetRootHubPortFeature (
  IN EFI_USB2_HC_PROTOCOL   *This,
  IN UINT8                 PortNumber,
  IN EFI_USB_PORT_FEATURE  PortFeature
)
{
  USB_OHCI_HC_DEV         *Ohc;
  EFI_STATUS              Status;
  UINT8                   NumOfPorts;
  UINTN                   RetryTimes;

  OhciGetRootHubNumOfPorts (This, &NumOfPorts);
  if (PortNumber >= NumOfPorts) {
    return EFI_INVALID_PARAMETER;
  }

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  Status = EFI_SUCCESS;

  switch (PortFeature) {
    case EfiUsbPortPower:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_SET_PORT_POWER);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_POWER_STAT) == 0 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortReset:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_SET_PORT_RESET);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while ((OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_RESET_STAT_CHANGE) == 0 ||
                OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_RESET_STAT) == 1) &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }

      OhciSetRootHubPortStatus (Ohc, PortNumber, RH_PORT_RESET_STAT_CHANGE);
      break;

    case EfiUsbPortEnable:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_SET_PORT_ENABLE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_ENABLE_STAT) == 0 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortSuspend:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_SET_PORT_SUSPEND);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_SUSPEND_STAT) == 0 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    default:
      return EFI_INVALID_PARAMETER;
  }

  return Status;
}

/**
  Clears a feature for the specified root hub port.

  @param  This                  A pointer to the EFI_USB2_HC_PROTOCOL instance.
  @param  PortNumber            Specifies the root hub port whose feature
                                is requested to be cleared.
  @param  PortFeature           Indicates the feature selector associated with the
                                feature clear request.

  @retval EFI_SUCCESS           The feature specified by PortFeature was cleared for the
                                USB root hub port specified by PortNumber.
  @retval EFI_INVALID_PARAMETER PortNumber is invalid or PortFeature is invalid.
  @retval EFI_DEVICE_ERROR      Some error happened when clearing feature
**/

EFI_STATUS
EFIAPI
OhciClearRootHubPortFeature (
  IN EFI_USB2_HC_PROTOCOL   *This,
  IN UINT8                 PortNumber,
  IN EFI_USB_PORT_FEATURE  PortFeature
)
{
  USB_OHCI_HC_DEV         *Ohc;
  EFI_STATUS              Status;
  UINT8                   NumOfPorts;
  UINTN                   RetryTimes;

  OhciGetRootHubNumOfPorts (This, &NumOfPorts);
  if (PortNumber >= NumOfPorts) {
    return EFI_INVALID_PARAMETER;
  }

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  Status = EFI_SUCCESS;

  switch (PortFeature) {
    case EfiUsbPortEnable:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_CLEAR_PORT_ENABLE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_ENABLE_STAT) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortSuspend:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_CLEAR_SUSPEND_STATUS);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_SUSPEND_STAT) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortReset:
      break;

    case EfiUsbPortPower:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_CLEAR_PORT_POWER);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_POWER_STAT) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortConnectChange:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_CONNECT_STATUS_CHANGE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_CONNECT_STATUS_CHANGE) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortResetChange:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_PORT_RESET_STAT_CHANGE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_RESET_STAT_CHANGE) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortEnableChange:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_PORT_ENABLE_STAT_CHANGE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_ENABLE_STAT_CHANGE) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortSuspendChange:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_PORT_SUSPEND_STAT_CHANGE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_PORT_SUSPEND_STAT_CHANGE) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    case EfiUsbPortOverCurrentChange:
      Status = OhciSetRootHubPortStatus (Ohc, PortNumber, RH_OC_INDICATOR_CHANGE);

      // Verify the state

      RetryTimes = 0;
      do {
        gBS->Stall (1000);
        RetryTimes++;
      } while (OhciReadRootHubPortStatus (Ohc, PortNumber, RH_OC_INDICATOR_CHANGE) == 1 &&
               RetryTimes < MAX_RETRY_TIMES);

      if (RetryTimes >= MAX_RETRY_TIMES) {
        return EFI_DEVICE_ERROR;
      }
      break;

    default:
      return EFI_INVALID_PARAMETER;
  }

  return Status;
}

/**
  Allocate and initialize the empty OHCI device.

  @param  PciIo                  The PCIIO to use.
  @param  OriginalPciAttributes  The original PCI attributes.

  @return Allocated OHCI device  If err, return NULL.
**/

USB_OHCI_HC_DEV *
OhciAllocateDev (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT64               OriginalPciAttributes
)
{
  USB_OHCI_HC_DEV         *Ohc;
  EFI_STATUS              Status;
  VOID                    *Buf;
  EFI_PHYSICAL_ADDRESS    PhyAddr;
  VOID                    *Map;
  UINTN                   Pages;
  UINTN                   Bytes;

  Ohc = AllocateZeroPool (sizeof (USB_OHCI_HC_DEV));
  if (Ohc == NULL) {
    return NULL;
  }

  Ohc->Signature                      = USB_OHCI_HC_DEV_SIGNATURE;
  Ohc->PciIo                          = PciIo;

  CopyMem (&Ohc->Usb2Hc, &gOhciUsb2HcTemplate, sizeof (EFI_USB2_HC_PROTOCOL));

  Ohc->OriginalPciAttributes = OriginalPciAttributes;

  Ohc->MemPool = UsbHcInitMemPool (PciIo, TRUE, 0);
  if (Ohc->MemPool == NULL) {
    goto FREE_DEV_BUFFER;
  }

  Bytes = 4096;
  Pages = EFI_SIZE_TO_PAGES (Bytes);

  Status = PciIo->AllocateBuffer (
                    PciIo,
                    AllocateAnyPages,
                    EfiBootServicesData,
                    Pages,
                    &Buf,
                    0
                  );

  if (EFI_ERROR (Status)) {
    goto FREE_MEM_POOL;
  }

  Status = PciIo->Map (
                    PciIo,
                    EfiPciIoOperationBusMasterCommonBuffer,
                    Buf,
                    &Bytes,
                    &PhyAddr,
                    &Map
                  );

  if (EFI_ERROR (Status) || (Bytes != 4096)) {
    goto FREE_MEM_PAGE;
  }

  Ohc->HccaMemoryBlock = (HCCA_MEMORY_BLOCK *)(UINTN) PhyAddr;
  Ohc->HccaMemoryMapping = Map;
  Ohc->HccaMemoryBuf = (VOID *)(UINTN) Buf;
  Ohc->HccaMemoryPages = Pages;

  return Ohc;

FREE_MEM_PAGE:
  PciIo->FreeBuffer (PciIo, Pages, Buf);
FREE_MEM_POOL:
  UsbHcFreeMemPool (Ohc->MemPool);
FREE_DEV_BUFFER:
  FreePool(Ohc);

  return NULL;
}

/**
  Free the OHCI device and release its associated resources.

  @param  Ohc                   The OHCI device to release.
**/

VOID
OhciFreeDev (
  IN USB_OHCI_HC_DEV      *Ohc
)
{
  OhciFreeFixedIntMemory (Ohc);

  if (Ohc->ExitBootServiceEvent != NULL) {
    gBS->CloseEvent (Ohc->ExitBootServiceEvent);
  }

  if (Ohc->HouseKeeperTimer != NULL) {
    gBS->CloseEvent (Ohc->HouseKeeperTimer);
  }

  if (Ohc->MemPool != NULL) {
    UsbHcFreeMemPool (Ohc->MemPool);
  }

  if (Ohc->HccaMemoryMapping != NULL) {
    Ohc->PciIo->FreeBuffer (Ohc->PciIo, Ohc->HccaMemoryPages, Ohc->HccaMemoryBuf);
  }

  if (Ohc->ControllerNameTable != NULL) {
    FreeUnicodeStringTable (Ohc->ControllerNameTable);
  }

  FreePool (Ohc);
}

VOID
OhciCleanDevUp (
  IN  EFI_HANDLE           Controller,
  IN  EFI_USB2_HC_PROTOCOL  *This
)
{
  EFI_STATUS              Status;
  USB_OHCI_HC_DEV         *Ohc;

  // Uninstall the USB_HC and USB_HC2 protocol, then disable the controller

  Ohc = USB2_OHCI_HC_DEV_FROM_THIS (This);

  Status = gBS->UninstallProtocolInterface (
                  Controller,
                  &gEfiUsb2HcProtocolGuid,
                  &Ohc->Usb2Hc
                );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "%a: leave for %p/%p (%r)\n", __FUNCTION__, This, Controller, Status));
    return;
  }

  OhciStopDev (Ohc);
  This->Reset (This, EFI_USB_HC_RESET_GLOBAL);
  This->SetState (This, EfiUsbHcStateHalt);

  OhciFreeDynamicIntMemory(Ohc);

  // Restore original PCI attributes

  Ohc->PciIo->Attributes (
                Ohc->PciIo,
                EfiPciIoAttributeOperationSet,
                Ohc->OriginalPciAttributes,
                NULL
              );

  OhciFreeDev (Ohc);

  return;
}

/**
  One notified function to stop the Host Controller when gBS->ExitBootServices() called.

  @param  Event                 Pointer to this event
  @param  Context               Event handler private data
**/

VOID
EFIAPI
OhcExitBootService (
  EFI_EVENT                      Event,
  VOID                           *Context
)
{
  USB_OHCI_HC_DEV           *Ohc;
  EFI_USB2_HC_PROTOCOL       *UsbHc;

  Ohc = (USB_OHCI_HC_DEV *) Context;

  UsbHc = &Ohc->Usb2Hc;

  OhciStopDev (Ohc);

  UsbHc->Reset (UsbHc, EFI_USB_HC_RESET_GLOBAL);
  UsbHc->SetState (UsbHc, EfiUsbHcStateHalt);

  return;
}

/**
  Starting the Usb OHCI Driver.

  @param  This                  Protocol instance pointer.
  @param  Controller            Handle of device to test.
  @param  RemainingDevicePath   Not used.

  @retval EFI_SUCCESS           This driver supports this device.
  @retval EFI_UNSUPPORTED       This driver does not support this device.
  @retval EFI_DEVICE_ERROR      This driver cannot be started due to device Error.
                                EFI_OUT_OF_RESOURCES- Failed due to resource shortage.
**/

EFI_STATUS
EFIAPI
OhciDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
)
{
  EFI_STATUS              Status;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  USB_OHCI_HC_DEV         *Ohc;
  UINT64                  Supports;
  UINT64                  OriginalPciAttributes;
  BOOLEAN                 PciAttributesSaved;

  DEBUG ((EFI_D_INFO, "%a: enter for %p/%p\n", __FUNCTION__, This, Controller));

  // Open PCIIO, then enable the HC device and turn off emulation

  Ohc = NULL;
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "%a: leave for %p/%p (%r)\n", __FUNCTION__, This, Controller, Status));
    return Status;
  }

  PciAttributesSaved = FALSE;

  // Save original PCI attributes

  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationGet,
                    0,
                    &OriginalPciAttributes
                  );

  if (EFI_ERROR (Status)) {
    goto CLOSE_PCIIO;
  }
  PciAttributesSaved = TRUE;

#if 0
  // Robustnesss improvement such as for UoL
  // Default is not required.

  if (FeaturePcdGet (PcdTurnOffUsbLegacySupport)) {
    OhciTurnOffUsbEmulation (PciIo);
  }
#endif

  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationSupported,
                    0,
                    &Supports
                  );

  if (EFI_ERROR (Status)) {
    goto CLOSE_PCIIO;
  }

  Supports &= (UINT64) EFI_PCI_DEVICE_ENABLE;
  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationEnable,
                    Supports,
                    NULL
                  );

  // Allocate memory for OHC private data structure

  Ohc = OhciAllocateDev (PciIo, OriginalPciAttributes);
  if (Ohc == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CLOSE_PCIIO;
  }

#if 0
  Status = OhciInitializeInterruptList (Uhc);
  if (EFI_ERROR (Status)) {
    goto FREE_OHC;
  }
#endif

  // Set 0.1 s timer

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OhciHouseKeeper,
                  Ohc,
                  &Ohc->HouseKeeperTimer
                );
  if (EFI_ERROR (Status)) {
    goto FREE_OHC;
  }

  // Polling at every 0.1s is too slow, use 0.05s like with UhciDxe

  Status = gBS->SetTimer (Ohc->HouseKeeperTimer, TimerPeriodic, 50 * 1000 * 10);

  if (EFI_ERROR (Status)) {
    goto FREE_OHC;
  }

  // Install Host Controller Protocol

  Status = gBS->InstallProtocolInterface (
                  &Controller,
                  &gEfiUsb2HcProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &Ohc->Usb2Hc
                );
  if (EFI_ERROR (Status)) {
    goto FREE_OHC;
  }

  // Create event to stop the HC when exit boot service.

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OhcExitBootService,
                  Ohc,
                  &gEfiEventExitBootServicesGuid,
                  &Ohc->ExitBootServiceEvent
                );

  if (EFI_ERROR (Status)) {
    goto UNINSTALL_USBHC;
  }

  AddUnicodeString2 (
    "eng",
    gOhciComponentName.SupportedLanguages,
    &Ohc->ControllerNameTable,
    L"Usb Open Host Controller",
    TRUE
  );

  AddUnicodeString2 (
    "en",
    gOhciComponentName2.SupportedLanguages,
    &Ohc->ControllerNameTable,
    L"Usb Open Host Controller",
    FALSE
  );

  DEBUG ((EFI_D_INFO, "%a: leave for %p/%p (Success)\n", __FUNCTION__, This, Controller));
  return EFI_SUCCESS;

UNINSTALL_USBHC:
  gBS->UninstallMultipleProtocolInterfaces (
         Controller,
         &gEfiUsb2HcProtocolGuid,
         &Ohc->Usb2Hc,
         NULL
       );

FREE_OHC:
  OhciFreeDev (Ohc);

CLOSE_PCIIO:
  if (PciAttributesSaved) {

  // Restore original PCI attributes

    PciIo->Attributes (
             PciIo,
             EfiPciIoAttributeOperationSet,
             OriginalPciAttributes,
             NULL
           );
  }

  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
       );

  DEBUG ((EFI_D_INFO, "%a: leave for %p/%p (%r)\n", __FUNCTION__, This, Controller, Status));
  return Status;
}

/**
  Stop this driver on ControllerHandle. Support stoping any child handles
  created by this driver.

  @param  This                  Protocol instance pointer.
  @param  Controller            Handle of device to stop driver on.
  @param  NumberOfChildren      Number of Children in the ChildHandleBuffer.
  @param  ChildHandleBuffer     List of handles for the children we need to stop.

  @return EFI_SUCCESS
  @return others
**/

EFI_STATUS
EFIAPI
OhciDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
)
{
  EFI_STATUS           Status;
  EFI_USB2_HC_PROTOCOL  *UsbHc;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsb2HcProtocolGuid,
                  (VOID **)&UsbHc,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "%a: leave for %p/%p (%r)\n", __FUNCTION__, This, Controller, Status));
    return Status;
  }

  OhciCleanDevUp (Controller, UsbHc);

  Status = gBS->CloseProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  This->DriverBindingHandle,
                  Controller
                );
  return Status;
}

/**
  Test to see if this driver supports ControllerHandle. Any
  ControllerHandle that has UsbHcProtocol installed will be supported.

  @param  This                 Protocol instance pointer.
  @param  Controller           Handle of device to test.
  @param  RemainingDevicePath  Not used.

  @return EFI_SUCCESS          This driver supports this device.
  @return EFI_UNSUPPORTED      This driver does not support this device.
**/

EFI_STATUS
EFIAPI
OhciDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
)
{
  EFI_STATUS              Status;
  EFI_PCI_IO_PROTOCOL     *PciIo;
  PCI_TYPE00              PciType;

  // Test whether there is PCI IO Protocol attached on the controller handle.

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                );

  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0,
                        sizeof (PciType) / sizeof (UINT32),
                        &PciType
                      );

  if (EFI_ERROR (Status)) {
    Status = EFI_UNSUPPORTED;
    goto ON_EXIT;
  }

  // Test whether the controller belongs to OHCI type

  if (!IS_PCI_USB (&PciType) || PciType.Hdr.ClassCode[0] != PCI_IF_OHCI) {
    Status = EFI_UNSUPPORTED;
  }

ON_EXIT:
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
       );

  return Status;
}

/**
  Entry point for EFI drivers.

  @param  ImageHandle           EFI_HANDLE.
  @param  SystemTable           EFI_SYSTEM_TABLE.

  @retval EFI_SUCCESS           Driver is successfully loaded.
  @return Others                Failed.
**/

EFI_STATUS
EFIAPI
OhciDriverEntryPoint (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
)
{
  return EfiLibInstallDriverBindingComponentName2 (
           ImageHandle,
           SystemTable,
           &gOhciDriverBinding,
           ImageHandle,
           &gOhciComponentName,
           &gOhciComponentName2
         );
}
