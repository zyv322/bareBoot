/** @file

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

/**
  Create a TD

  @Param  Ohc                   Device private data

  @retval                       TD structure pointer
**/

TD_DESCRIPTOR *
OhciCreateTD (
  IN USB_OHCI_HC_DEV      *Ohc
)
{
  TD_DESCRIPTOR           *Td;

  Td = UsbHcAllocateMem (Ohc->MemPool, sizeof (TD_DESCRIPTOR));
  if (Td == NULL) {
    DEBUG ((EFI_D_INFO, "%a: TD allocation failed!\n", __FUNCTION__));
  }
  return Td;
}

/**
  Free a TD
	
  @Param  Ohc                   Device private data
  @Param  Td                    Pointer to a TD to free

  @retval  EFI_SUCCESS          TD freed
**/

EFI_STATUS
OhciFreeTD (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN TD_DESCRIPTOR        *Td
)
{
  if (Td == NULL) {
    return EFI_SUCCESS;
  }
  UsbHcFreeMem (Ohc->MemPool, Td, sizeof (TD_DESCRIPTOR));

  return EFI_SUCCESS;
}

/**
  Delete a list of TDs.

  @param  Ohc         The OHCI device.
  @param  FirstTd     TD link list head.

  @return None.
**/

VOID
OhciDestroyTds (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN TD_DESCRIPTOR        *FirstTd
)
{
  TD_DESCRIPTOR        *NextTd;
  TD_DESCRIPTOR        *ThisTd;

  NextTd = FirstTd;

  while (NextTd != NULL) {
    ThisTd  = NextTd;
    NextTd  = (TD_DESCRIPTOR *)(UINTN) ThisTd->NextTDPointer;
    OhciFreeTD (Ohc, ThisTd);
  }
}

/**
  Create a ED

  @Param   Ohc                  Device private data

  @retval  Ed                   descriptor pointer
**/

ED_DESCRIPTOR *
OhciCreateED (
  USB_OHCI_HC_DEV *Ohc,
  IN UINT8        DeviceAddress,
  IN UINT8        EndPointNum,
  IN UINT8        DeviceSpeed,
  IN UINTN        MaxPacket
)
{
  ED_DESCRIPTOR   *Ed;

  Ed = UsbHcAllocateMem (Ohc->MemPool, sizeof (ED_DESCRIPTOR));
  if (Ed == NULL) {
    DEBUG ((EFI_D_INFO, "%a: ED allocation failed!\n", __FUNCTION__));	
    return NULL;
  }

  Ed->Word0.Direction = ED_FROM_TD_DIR;
  Ed->Word0.EndPointNum = EndPointNum;
  Ed->Word0.FunctionAddress = DeviceAddress;
  Ed->Word0.MaxPacketSize = (UINT32) MaxPacket;
  Ed->Word0.Skip = 1;
  Ed->Word0.Speed = DeviceSpeed;

  return Ed;
}

/**
  Free a ED

  @Param  Ohc                   Device private data
  @Param  Ed                    Pointer to a ED to free

  @retval  EFI_SUCCESS          ED freed
**/

EFI_STATUS
OhciFreeED (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN ED_DESCRIPTOR        *Ed
)
{
  if (Ed == NULL) {
    return EFI_SUCCESS;
  }
  UsbHcFreeMem (Ohc->MemPool, Ed, sizeof (ED_DESCRIPTOR));

  return EFI_SUCCESS;
}

/**
  Free all TD from ED

  @Param  Ohc                    Device private data
  @Param  Ed                     Pointer to a ED to free

  @retval  EFI_SUCCESS           ED freed
**/

EFI_STATUS
OhciFreeAllTDFromED (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN ED_DESCRIPTOR        *Ed
)
{
  TD_DESCRIPTOR           *HeadTd;
  TD_DESCRIPTOR           *TailTd;
  TD_DESCRIPTOR           *Td;
  TD_DESCRIPTOR           *TempTd;

  if (Ed == NULL) {
    return EFI_SUCCESS;
  }

  HeadTd = TD_PTR (Ed->Word2.TdHeadPointer);
  TailTd = (TD_DESCRIPTOR *)(UINTN) (Ed->TdTailPointer);

  Td = HeadTd;
  while (Td != TailTd) {
    TempTd = Td;
    Td = (TD_DESCRIPTOR *)(UINTN) (Td->NextTDPointer);
    OhciFreeTD (Ohc, TempTd);
  }

  return EFI_SUCCESS;
}

/**
  Find a working ED match the requirement

  @Param  EdHead                Head of the ED list
  @Param  DeviceAddress         Device address to search
  @Param  EndPointNum           End point num to search
  @Param  EdDir                 ED Direction to search

  @retval   ED descriptor searched
**/

ED_DESCRIPTOR *
OhciFindWorkingEd (
  IN ED_DESCRIPTOR       *EdHead,
  IN UINT8               DeviceAddress,
  IN UINT8               EndPointNum,
  IN UINT8               EdDir
)
{
  ED_DESCRIPTOR           *Ed;

  for (Ed = EdHead; Ed != NULL; Ed = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED)) {
    if (Ed->Word2.Halted == 0 && Ed->Word0.Skip == 0 &&
        Ed->Word0.FunctionAddress == DeviceAddress && Ed->Word0.EndPointNum == EndPointNum &&
        Ed->Word0.Direction == EdDir) {
      break;
    }
  }

  return Ed;
}

/**
  Initialize interrupt list.

  @Param Ohc                    Device private data

  @retval  EFI_SUCCESS          Initialization done
**/

EFI_STATUS
OhciInitializeInterruptList (
  USB_OHCI_HC_DEV          *Ohc
)
{
  static UINT32     Leaf[32] = { 0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30, 1, 17,
                                9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31 };
  UINT32            *HccaInterruptTable;
  UINTN             Index;
  UINTN             Level;
  UINTN             Count;
  ED_DESCRIPTOR     *Edp;

  HccaInterruptTable = Ohc->HccaMemoryBlock->HccaInterruptTable;

  for (Index = 0; Index < 32; Index++) {
    Edp = UsbHcAllocateMem (Ohc->MemPool, sizeof (ED_DESCRIPTOR));
    HccaInterruptTable[Index] = (UINT32)(UINTN) Edp;
    if (HccaInterruptTable[Index] == 0) {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  for (Index = 0; Index < 32; Index++) {
    Ohc->IntervalList[0][Index] = (ED_DESCRIPTOR *)(UINTN) (HccaInterruptTable[Leaf[Index]]);
  }

  Count = 32;
  for (Level = 1; Level <= 5; Level++) {
    Count = Count >> 1;

    for (Index = 0; Index < Count; Index++) {
      Edp = UsbHcAllocateMem (Ohc->MemPool, sizeof (ED_DESCRIPTOR));
      Ohc->IntervalList[Level][Index] = Edp;
      if (HccaInterruptTable[Index] == 0) {
        return EFI_OUT_OF_RESOURCES;
      }
      Ohc->IntervalList[Level - 1][Index * 2]->NextED =
              (UINT32)(UINTN) (Ohc->IntervalList[Level][Index]);
      Ohc->IntervalList[Level - 1][Index * 2 + 1]->NextED =
              (UINT32)(UINTN) (Ohc->IntervalList[Level][Index]);
    }
  }

  return EFI_SUCCESS;
}

/**
  Attach an ED

  @Param  Ed                    Ed to be attached
  @Param  NewEd                 Ed to attach

  @retval EFI_SUCCESS           NewEd attached to Ed
  @retval EFI_INVALID_PARAMETER Ed is NULL
**/

EFI_STATUS
OhciAttachED (
  IN ED_DESCRIPTOR        *Ed,
  IN ED_DESCRIPTOR        *NewEd
)
{
  ED_DESCRIPTOR           *Temp;

  if (Ed == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Ed->NextED == 0) {
    Ed->NextED = (UINT32)(UINTN) NewEd;
  } else {
    Temp = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED);
    Ed->NextED = (UINT32)(UINTN) NewEd;
    NewEd->NextED = (UINT32)(UINTN) Temp;
  }

  return EFI_SUCCESS;
}


/**
  Count ED number on a ED chain

  @Param  Ed                    Head of the ED chain

  @retval                       ED number on the chain
**/

UINTN
CountEdNum (
  IN ED_DESCRIPTOR      *Ed
)
{
  UINTN     Count;

  Count = 0;

  while (Ed != NULL) {
    Ed = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED);
    Count++;
  }

  return Count;
}

/**
  Find the minimal burn ED list on a specific depth level

  @Param  Ohc                   Device private data
  @Param  Depth                 Depth level

  @retval                       ED list found
**/

ED_DESCRIPTOR *
OhciFindMinInterruptEDList (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN UINT32               Depth
)
{
  UINTN                   EdNum;
  UINTN                   MinEdNum;
  ED_DESCRIPTOR           *TempEd;
  ED_DESCRIPTOR           *HeadEd;
  UINTN                   Index;

  if (Depth > 5) {
    return NULL;
  }

  MinEdNum = 0xFFFFFFFF;
  TempEd = NULL;
  for (Index = 0; Index < (UINTN) (32 >> Depth); Index++) {
    HeadEd = Ohc->IntervalList[Depth][Index];
    EdNum = CountEdNum (HeadEd);
    if (EdNum < MinEdNum) {
      MinEdNum = EdNum;
      TempEd = HeadEd;
    }
  }

  ASSERT (TempEd != NULL);

  return TempEd;
}

/**
  Attach an ED to an ED list

  @Param  OHC                   Device private data
  @Param  ListType              Type of the ED list
  @Param  Ed                    ED to attach
  @Param  EdList                ED list to be attached

  @retval  EFI_SUCCESS          ED attached to ED list
**/

ED_DESCRIPTOR *
OhciAttachEDToList (
  IN USB_OHCI_HC_DEV       *Ohc,
  IN DESCRIPTOR_LIST_TYPE  ListType,
  IN ED_DESCRIPTOR         *Ed,
  IN ED_DESCRIPTOR         *EdList
)
{
  ED_DESCRIPTOR            *HeadEd;

  HeadEd = NULL;

  switch (ListType) {
    case CONTROL_LIST:
      HeadEd = (ED_DESCRIPTOR *) OhciGetMemoryPointer (Ohc, HC_CONTROL_HEAD);
      if (HeadEd == NULL) {
        OhciSetMemoryPointer (Ohc, HC_CONTROL_HEAD, Ed);
        HeadEd = Ed;
      } else {
        OhciAttachED (HeadEd, Ed);
      }
    break;
    case BULK_LIST:
      HeadEd = (ED_DESCRIPTOR *) OhciGetMemoryPointer (Ohc, HC_BULK_HEAD);
      if (HeadEd == NULL) {
        OhciSetMemoryPointer (Ohc, HC_BULK_HEAD, Ed);
        HeadEd = Ed;
      } else {
        OhciAttachED (HeadEd, Ed);
      }
    break;
    case INTERRUPT_LIST:
      OhciAttachED (EdList, Ed);
      break;

    default:
      ASSERT (FALSE);
  }

  return HeadEd;
}

/**
  Remove interrupt EDs that match requirement

  @Param  Ohc                   Device private data
  @Param  IntEd                 The address of Interrupt endpoint

  @retval  EFI_SUCCESS          EDs match requirement removed
**/

EFI_STATUS
OhciFreeInterruptEdByEd (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN ED_DESCRIPTOR        *IntEd
)
{
  ED_DESCRIPTOR           *Ed;
  ED_DESCRIPTOR           *TempEd;
  UINTN                   Index;

  if (IntEd == NULL) {
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < 32; Index++) {
    Ed = (ED_DESCRIPTOR *)(UINTN) (Ohc->HccaMemoryBlock->HccaInterruptTable[Index]);
    if (Ed == NULL) {
      continue;
    }
    while (Ed->NextED != 0) {
      if (Ed->NextED == (UINT32)(UINTN) IntEd) {
        TempEd = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED);
        Ed->NextED = ((ED_DESCRIPTOR *)(UINTN)(Ed->NextED))->NextED;
        OhciFreeED (Ohc, TempEd);
      } else {
        Ed = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED);
      }
    }
  }
  return EFI_SUCCESS;
}

/**
  Remove interrupt EDs that match requirement

  @Param  Ohc                   Device private data
  @Param  FunctionAddress       Requirement on function address
  @Param  EndPointNum           Requirement on end point number

  @retval  EFI_SUCCESS          EDs match requirement removed
**/

EFI_STATUS
OhciFreeInterruptEdByAddr (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN UINT8                FunctionAddress,
  IN UINT8                EndPointNum
)
{
  ED_DESCRIPTOR           *Ed;
  ED_DESCRIPTOR           *TempEd;
  UINTN                   Index;

  for (Index = 0; Index < 32; Index++) {
    Ed = (ED_DESCRIPTOR *)(UINTN) (Ohc->HccaMemoryBlock->HccaInterruptTable[Index]);
    if (Ed == NULL) {
      continue;
    }

    while (Ed->NextED != 0) {
      if (((ED_DESCRIPTOR *)(UINTN) (Ed->NextED))->Word0.FunctionAddress == FunctionAddress &&
          ((ED_DESCRIPTOR *)(UINTN) (Ed->NextED))->Word0.EndPointNum == EndPointNum) {
        TempEd = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED);
        Ed->NextED = ((ED_DESCRIPTOR *)(UINTN) (Ed->NextED))->NextED;
        OhciFreeED (Ohc, TempEd);
      } else {
        Ed = (ED_DESCRIPTOR *)(UINTN) (Ed->NextED);
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Link Td2 to the end of Td1

  @Param Td1                    TD to be linked
  @Param Td2                    TD to link

  @retval EFI_SUCCESS           TD successfully linked
  @retval EFI_INVALID_PARAMETER Td1 is NULL
**/

EFI_STATUS
OhciLinkTD (
  IN TD_DESCRIPTOR        *Td1,
  IN TD_DESCRIPTOR        *Td2
)
{
  TD_DESCRIPTOR           *TempTd;

  if (Td1 == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Td1 == Td2) {
    return EFI_SUCCESS;
  } 	

  TempTd = Td1;
  while (TempTd->NextTDPointer != 0) {
    TempTd = (TD_DESCRIPTOR *)(UINTN) (TempTd->NextTDPointer);
  }

  TempTd->NextTD = (UINT32)(UINTN) Td2;
  TempTd->NextTDPointer = (UINT32)(UINTN) Td2;

  return EFI_SUCCESS;
}

/**
  Attach TD list to ED

  @Param  Ed                    ED which TD list attach on
  @Param  HeadTd                Head of the TD list to attach

  @retval  EFI_SUCCESS          TD list attached on the ED
**/

EFI_STATUS
OhciAttachTDListToED (
  IN ED_DESCRIPTOR        *Ed,
  IN TD_DESCRIPTOR        *HeadTd
)
{
  TD_DESCRIPTOR           *TempTd;

  TempTd = TD_PTR (Ed->Word2.TdHeadPointer);

  if (TempTd != NULL) {
    while (TempTd->NextTD != 0) {
      TempTd = (TD_DESCRIPTOR *)(UINTN) (TempTd->NextTD);
    }
    TempTd->NextTD = (UINT32)(UINTN) HeadTd;
    TempTd->NextTDPointer = (UINT32)(UINTN) HeadTd;
  } else {
    Ed->Word2.TdHeadPointer = RIGHT_SHIFT_4 ((UINT32)(UINTN) HeadTd);
  }

  return EFI_SUCCESS;
}

/**
  Set value to ED specific field

  @Param  Ed                    ED to be set
  @Param  Field                 Field to be set
  @Param  Value                 Value to set

  @retval  EFI_SUCCESS          Value set
**/

EFI_STATUS
OhciSetEDField (
  IN ED_DESCRIPTOR        *Ed,
  IN UINT32               Field,
  IN UINT32               Value
)
{
  if (Field & ED_FUNC_ADD) {
    Ed->Word0.FunctionAddress = Value;
  }
  if (Field & ED_ENDPT_NUM) {
    Ed->Word0.EndPointNum = Value;
  }
  if (Field & ED_DIR) {
    Ed->Word0.Direction = Value;
  }
  if (Field & ED_SPEED) {
    Ed->Word0.Speed = Value;
  }
  if (Field & ED_SKIP) {
    Ed->Word0.Skip = Value;
  }
  if (Field & ED_FORMAT) {
    Ed->Word0.Format = Value;
  }
  if (Field & ED_MAX_PACKET) {
    Ed->Word0.MaxPacketSize = Value;
  }
  if (Field & ED_PDATA) {
    Ed->Word0.FreeSpace = Value;
  }
  if (Field & ED_ZERO) {
    Ed->Word2.Zero = Value;
  }
  if (Field & ED_TDTAIL_PTR) {
    Ed->TdTailPointer = (UINT32)(UINTN) Value;
  }
  if (Field & ED_HALTED) {
    Ed->Word2.Halted = Value;
  }
  if (Field & ED_DTTOGGLE) {
    Ed->Word2.ToggleCarry = Value;
  }
  if (Field & ED_TDHEAD_PTR) {
    Ed->Word2.TdHeadPointer = RIGHT_SHIFT_4 (Value);
  }
  if (Field & ED_NEXT_EDPTR) {
    Ed->NextED = (UINT32)(UINTN) Value;
  }

  return EFI_SUCCESS;
}

/**
  Get value from an ED's specific field

  @Param  Ed                    ED pointer
  @Param  Field                 Field to get value from

  @retval                       Value of the field
**/

UINT32
OhciGetEDField (
  IN ED_DESCRIPTOR        *Ed,
  IN UINT32               Field
)
{
  switch (Field) {
    case ED_FUNC_ADD:
      return Ed->Word0.FunctionAddress;
      break;
    case ED_ENDPT_NUM:
      return Ed->Word0.EndPointNum;
      break;
    case ED_DIR:
      return Ed->Word0.Direction;
      break;
    case ED_SPEED:
      return Ed->Word0.Speed;
      break;
    case ED_SKIP:
      return Ed->Word0.Skip;
      break;
    case ED_FORMAT:
      return Ed->Word0.Format;
      break;
    case ED_MAX_PACKET:
      return Ed->Word0.MaxPacketSize;
      break;
    case ED_TDTAIL_PTR:
      return (UINT32)(UINTN) Ed->TdTailPointer;
      break;
    case ED_HALTED:
      return Ed->Word2.Halted;
      break;
    case ED_DTTOGGLE:
      return Ed->Word2.ToggleCarry;
      break;
    case ED_TDHEAD_PTR:
      return Ed->Word2.TdHeadPointer << 4;
      break;
    case ED_NEXT_EDPTR:
      return (UINT32)(UINTN) Ed->NextED;
      break;
    default:
      ASSERT (FALSE);
  }

  return 0;
}

/**
  Set value to TD specific field

  @Param  Td                    TD to be set
  @Param  Field                 Field to be set
  @Param  Value                 Value to set

  @retval  EFI_SUCCESS          Value set
**/

EFI_STATUS
OhciSetTDField (
  IN TD_DESCRIPTOR        *Td,
  IN UINT32               Field,
  IN UINT32               Value
)
{
  if (Field & TD_PDATA) {
    Td->Word0.Reserved = Value;
  }
  if (Field & TD_BUFFER_ROUND) {
    Td->Word0.BufferRounding = Value;
  }
  if (Field & TD_DIR_PID) {
    Td->Word0.DirPID = Value;
  }
  if (Field & TD_DELAY_INT) {
    Td->Word0.DelayInterrupt = Value;
  }
  if (Field & TD_DT_TOGGLE) {
    Td->Word0.DataToggle = Value | 0x2;
  }
  if (Field & TD_ERROR_CNT) {
    Td->Word0.ErrorCount = Value;
  }
  if (Field & TD_COND_CODE) {
    Td->Word0.ConditionCode = Value;
  }

  if (Field & TD_CURR_BUFFER_PTR) {
    Td->CurrBufferPointer = (UINT32)(UINTN) Value;
  }

  if (Field & TD_NEXT_PTR) {
    Td->NextTD = (UINT32)(UINTN) Value;
  }

  if (Field & TD_BUFFER_END_PTR) {
    Td->BufferEndPointer = (UINT32)(UINTN) Value;
  }

  return EFI_SUCCESS;
}

/**
  Get value from TD specific field

  @Param  Td                    TD pointer
  @Param  Field                 Field to get value from

  @retval                       Value of the field
**/

UINT32
OhciGetTDField (
  IN TD_DESCRIPTOR      *Td,
  IN UINT32             Field
)
{
  switch (Field) {
    case TD_BUFFER_ROUND:
      return Td->Word0.BufferRounding;
      break;
    case TD_DIR_PID:
      return Td->Word0.DirPID;
      break;
    case TD_DELAY_INT:
      return Td->Word0.DelayInterrupt;
      break;
    case TD_DT_TOGGLE:
      return Td->Word0.DataToggle;
      break;
    case TD_ERROR_CNT:
      return Td->Word0.ErrorCount;
      break;
    case TD_COND_CODE:
      return Td->Word0.ConditionCode;
      break;
    case TD_CURR_BUFFER_PTR:
      return (UINT32)(UINTN) Td->CurrBufferPointer;
      break;
    case TD_NEXT_PTR:
      return (UINT32)(UINTN) Td->NextTD;
      break;
    case TD_BUFFER_END_PTR:
      return (UINT32)(UINTN) Td->BufferEndPointer;
      break;
    default:
      ASSERT (FALSE);
  }

  return 0;
}

/**
  Free the Ed, Td & buffer that were created during transferring

  @Param  Ohc                   Device private data
**/

VOID
OhciFreeDynamicIntMemory (
  IN USB_OHCI_HC_DEV      *Ohc
)
{
  INTERRUPT_CONTEXT_ENTRY *Entry;

  if (Ohc == NULL) {
    return;
  }

  while (Ohc->InterruptContextList != NULL) {	
    Entry = Ohc->InterruptContextList;
    OhciFreeInterruptEdByEd (Ohc, Entry->Ed);
    Ohc->InterruptContextList = Entry->NextEntry;
    OhciFreeInterruptContextEntry (Ohc, Entry);
  }
}

/**
  Free the Ed that were initilized during driver was starting,
  those memory were used as interrupt ED head

  @Param  Ohc                   Device private data
**/

VOID
OhciFreeFixedIntMemory (
  IN USB_OHCI_HC_DEV      *Ohc
)
{
  static UINT32           Leaf[] = { 32, 16, 8, 4, 2, 1 };
  UINTN                   Index;
  UINTN                   Level;

  for (Level = 0; Level < 6; Level++) {	
    for (Index = 0; Index < Leaf[Index]; Index++) {
      if (Ohc->IntervalList[Level][Index] != NULL) {
        UsbHcFreeMem (Ohc->MemPool, Ohc->IntervalList[Level][Index], sizeof (ED_DESCRIPTOR));
      }
    }
  }
}

/**
  Release all OHCI used memory when OHCI going to quit

  @Param  Ohc                   Device private data

  @retval EFI_SUCCESS          Memory released
**/

EFI_STATUS
OhciFreeIntTransferMemory (
  IN USB_OHCI_HC_DEV           *Ohc
)
{
  // Free the Ed, Td & buffer that were created during transferring

  OhciFreeDynamicIntMemory (Ohc);

  // Free the Ed that were initilized during driver was starting

  OhciFreeFixedIntMemory (Ohc);
  return EFI_SUCCESS;
}

/**
  Create and initialize a TD for Setup Stage of a control transfer.

  @param  Ohc         The OHCI device.
  @param  DevAddr     Device address.
  @param  Request     A pointer to cpu memory address of Device request.
  @param  RequestPhy  A pointer to pci memory address of Device request.

  @return The created setup Td Pointer.
**/

TD_DESCRIPTOR *
OhciCreateSetupTD (
  IN  USB_OHCI_HC_DEV     *Ohc,
  IN  UINT8               DevAddr,
  IN  UINT8               *Request,
  IN  UINT8               *RequestPhy
)
{
  TD_DESCRIPTOR           *Td;

  Td = OhciCreateTD (Ohc);

  if (Td == NULL) {
    return NULL;
  }

  OhciSetTDField (Td, TD_BUFFER_ROUND, 1);
  OhciSetTDField (Td, TD_DIR_PID, TD_SETUP_PID);
  OhciSetTDField (Td, TD_DELAY_INT, TD_NO_DELAY);
  OhciSetTDField (Td, TD_DT_TOGGLE, 2);
  OhciSetTDField (Td, TD_COND_CODE, TD_TOBE_PROCESSED);
  OhciSetTDField (Td, TD_CURR_BUFFER_PTR, (UINT32)RequestPhy);
  OhciSetTDField (Td, TD_BUFFER_END_PTR, (UINT32)(RequestPhy + sizeof (EFI_USB_DEVICE_REQUEST) - 1));

  Td->ActualSendLength = sizeof (EFI_USB_DEVICE_REQUEST);
  Td->DataBuffer = (UINT32)(UINTN)RequestPhy;

  return Td;
}

/**
  Create a TD for data.

  @param  Ohc         The OHCI device.
  @param  DevAddr     Device address.
  @param  Endpoint    Endpoint number.
  @param  DataPtr     A pointer to cpu memory address of Data buffer.
  @param  DataPhyPtr  A pointer to pci memory address of Data buffer.
  @param  Len         Data length.
  @param  PktId       Packet ID.
  @param  Toggle      Data toggle value.

  @return Data Td pointer if success, otherwise NULL.
**/

TD_DESCRIPTOR *
OhciCreateDataTD (
  IN  USB_OHCI_HC_DEV     *Ohc,
  IN  UINT8               DevAddr,
  IN  UINT8               Endpoint,
  IN  UINT8               *DataPtr,
  IN  UINT8               *DataPhyPtr,
  IN  UINTN               Len,
  IN  UINT8               PktId,
  IN  UINT8               Toggle
)
{
  TD_DESCRIPTOR  *Td;

  Td  = OhciCreateTD (Ohc);

  if (Td == NULL) {
    return NULL;
  }

  OhciSetTDField (Td, TD_BUFFER_ROUND, 1);
  OhciSetTDField (Td, TD_DIR_PID, PktId);
  OhciSetTDField (Td, TD_DELAY_INT, TD_NO_DELAY);
  OhciSetTDField (Td, TD_DT_TOGGLE, Toggle);
  OhciSetTDField (Td, TD_COND_CODE, TD_TOBE_PROCESSED);
  OhciSetTDField (Td, TD_CURR_BUFFER_PTR, (UINT32) DataPhyPtr);
  OhciSetTDField (Td, TD_BUFFER_END_PTR, (UINT32) (DataPhyPtr + Len - 1));

  Td->ActualSendLength = (UINT32) Len;
  Td->DataBuffer = (UINT32)(UINTN) DataPhyPtr;

  return Td;
}

/**
  Create TD for the Status Stage of control transfer.

  @param  Ohc         The OHCI device.
  @param  DevAddr     Device address.
  @param  PktId       Packet ID.

  @return Status Td Pointer.
**/

TD_DESCRIPTOR *
OhciCreateStatusTD (
  IN  USB_OHCI_HC_DEV     *Ohc,
  IN  UINT8               DevAddr,
  IN  UINT8               PktId
)
{
  TD_DESCRIPTOR           *Td;

  Td = OhciCreateTD (Ohc);

  if (Td == NULL) {
    return NULL;
  }

  OhciSetTDField (Td, TD_BUFFER_ROUND, 1);
  OhciSetTDField (Td, TD_DIR_PID, PktId);
  OhciSetTDField (Td, TD_DELAY_INT, TD_NO_DELAY);
  OhciSetTDField (Td, TD_DT_TOGGLE, 3);
  OhciSetTDField (Td, TD_COND_CODE, TD_TOBE_PROCESSED);

  return Td;
}

/**
  Create Tds list for Control Transfer.

  @param  Ohc         The OHCI device.
  @param  DeviceAddr  The device address.
  @param  DataPktId   Packet Identification of Data Tds.
  @param  Request     A pointer to cpu memory address of request structure buffer to transfer.
  @param  RequestPhy  A pointer to pci memory address of request structure buffer to transfer.
  @param  Data        A pointer to cpu memory address of user data buffer to transfer.
  @param  DataPhy     A pointer to pci memory address of user data buffer to transfer.
  @param  DataLen     Length of user data to transfer.
  @param  MaxPacket   Maximum packet size for control transfer.

  @return The Td list head for the control transfer.
**/

TD_DESCRIPTOR *
OhciCreateCtrlTds (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN UINT8                DeviceAddr,
  IN UINT8                DataPktId,
  IN UINT8                *Request,
  IN UINT8                *RequestPhy,
  IN UINT8                *Data,
  IN UINT8                *DataPhy,
  IN UINTN                DataLen,
  IN UINTN                MaxPacket
)
{
  TD_DESCRIPTOR             *SetupTd;
  TD_DESCRIPTOR             *FirstDataTd;
  TD_DESCRIPTOR             *DataTd;
  TD_DESCRIPTOR             *PrevDataTd;
  TD_DESCRIPTOR             *StatusTd;
  UINT8                     DataToggle;
  UINT8                     StatusPktId;
  UINTN                     ThisTdLen;


  DataTd      = NULL;
  SetupTd     = NULL;
  FirstDataTd = NULL;
  PrevDataTd  = NULL;
  StatusTd    = NULL;

  //
  // Create setup packets for the transfer
  //
  SetupTd = OhciCreateSetupTD (Ohc, DeviceAddr, Request, RequestPhy);

  if (SetupTd == NULL) {
    return NULL;
  }

  //
  // Create data packets for the transfer
  //
  DataToggle = 1;

  while (DataLen > 0) {
    //
    // PktSize is the data load size in each Td.
    //
    ThisTdLen = (DataLen > MaxPacket ? MaxPacket : DataLen);

    DataTd = OhciCreateDataTD (
               Ohc,
               DeviceAddr,
               0,
               Data,  //cpu memory address
               DataPhy, //Pci memory address
               ThisTdLen,
               DataPktId,
               DataToggle
               );

    if (DataTd == NULL) {
      goto FREE_TD;
    }

    if (FirstDataTd == NULL) {
      FirstDataTd         = DataTd;
      FirstDataTd->NextTDPointer = 0;
    } else {
      OhciLinkTD (PrevDataTd, DataTd);
    }

    DataToggle ^= 1;
    PrevDataTd = DataTd;
    Data += ThisTdLen;
    DataPhy += ThisTdLen;
    DataLen -= ThisTdLen;
  }

  //
  // Status packet is on the opposite direction to data packets
  //
  if (DataPktId == TD_OUT_PID) {
    StatusPktId = TD_IN_PID;
  } else {
    StatusPktId = TD_OUT_PID;
  }

  StatusTd = OhciCreateStatusTD (Ohc, DeviceAddr, StatusPktId);

  if (StatusTd == NULL) {
    goto FREE_TD;
  }

  //
  // Link setup Td -> data Tds -> status Td together
  //
  if (FirstDataTd != NULL) {
    OhciLinkTD (SetupTd, FirstDataTd);
    OhciLinkTD (PrevDataTd, StatusTd);
  } else {
    OhciLinkTD (SetupTd, StatusTd);
  }

  return SetupTd;

FREE_TD:
  if (SetupTd != NULL) {
    OhciDestroyTds (Ohc, SetupTd);
  }

  if (FirstDataTd != NULL) {
    OhciDestroyTds (Ohc, FirstDataTd);
  }

  return NULL;
}

/**
  Create Tds list for Bulk/Interrupt Transfer.

  @param  Ohc         USB_OHCI_HC_DEV.
  @param  DevAddr     Address of Device.
  @param  EndPoint    Endpoint Number.
  @param  PktId       Packet Identification of Data Tds.
  @param  Data        A pointer to cpu memory address of user data buffer to transfer.
  @param  DataPhy     A pointer to pci memory address of user data buffer to transfer.
  @param  DataLen     Length of user data to transfer.
  @param  DataToggle  Data Toggle Pointer.
  @param  MaxPacket   Maximum packet size for Bulk/Interrupt transfer.

  @return The Tds list head for the bulk transfer.
**/

TD_DESCRIPTOR *
OhciCreateBulkOrIntTds (
  IN USB_OHCI_HC_DEV      *Ohc,
  IN UINT8                DevAddr,
  IN UINT8                EndPoint,
  IN UINT8                PktId,
  IN UINT8                *Data,
  IN UINT8                *DataPhy,
  IN UINTN                DataLen,
  IN OUT UINT8            *DataToggle,
  IN UINTN                MaxPacket
)
{
  TD_DESCRIPTOR          *DataTd;
  TD_DESCRIPTOR          *FirstDataTd;
  TD_DESCRIPTOR          *PrevDataTd;
  UINTN                  ThisTdLen;

  DataTd      = NULL;
  FirstDataTd = NULL;
  PrevDataTd  = NULL;

  //
  // Create data packets for the transfer
  //
  while (DataLen > 0) {
    //
    // PktSize is the data load size that each Td.
    //
    ThisTdLen = DataLen;

    if (DataLen > MaxPacket) {
      ThisTdLen = MaxPacket;
    }

    DataTd = OhciCreateDataTD (
               Ohc,
               DevAddr,
               EndPoint,
               Data,
               DataPhy,
               ThisTdLen,
               PktId,
               *DataToggle
               );

    if (DataTd == NULL) {
      goto FREE_TD;
    }

    if (FirstDataTd == NULL) {
      FirstDataTd         = DataTd;
    } else {
      OhciLinkTD (PrevDataTd, DataTd);
    }

    *DataToggle ^= 1;
    PrevDataTd   = DataTd;
    Data        += ThisTdLen;
    DataPhy     += ThisTdLen;
    DataLen     -= ThisTdLen;
  }

  return FirstDataTd;

FREE_TD:
  if (FirstDataTd != NULL) {
    OhciDestroyTds (Ohc, FirstDataTd);
  }

  return NULL;
}
