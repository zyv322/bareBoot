/* $Id: Console.c $ */
/** @file
 * Console.c - VirtualBox Console control emulation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "macosx.h"

BOOLEAN
EfiGrowBuffer (
  IN OUT EFI_STATUS   *Status,
  IN OUT VOID         **Buffer,
  IN UINTN            BufferSize
  );

CHAR8*  DefaultMemEntry = "N/A";
CHAR8*  DefaultSerial = "CT288GT9VT6";
CHAR8*  BiosVendor = "Apple Inc.";
CHAR8*  AppleManufacturer = "Apple Computer, Inc.";

CHAR8* AppleFirmwareVersion[24] = {
  "MB11.88Z.0061.B03.0809221748",
  "MB21.88Z.00A5.B07.0706270922",
  "MB41.88Z.0073.B00.0809221748",
  "MB52.88Z.0088.B05.0809221748",
  "MBP51.88Z.007E.B06.0906151647",
  "MBP81.88Z.0047.B27.1102071707",
  "MBP83.88Z.0047.B24.1110261426",
  "MBP91.88Z.00D3.B08.1205101904",
  "MBA31.88Z.0061.B07.0712201139",
  "MBA51.88Z.00EF.B01.1205221442",
  "MM21.88Z.009A.B00.0706281359",
  "MM51.88Z.0077.B10.1102291410",
  "MM61.88Z.0106.B00.1208091121",
  "IM81.88Z.00C1.B00.0803051705",
  "IM101.88Z.00CC.B00.0909031926",
  "IM111.88Z.0034.B02.1003171314",
  "IM112.88Z.0057.B01.0802091538",
  "IM113.88Z.0057.B01.1005031455",
  "IM121.88Z.0047.B1F.1201241648",
  "IM122.88Z.0047.B1F.1223021110",
  "IM131.88Z.010A.B00.1209042338",
  "MP31.88Z.006C.B05.0802291410",
  "MP41.88Z.0081.B04.0903051113",
  "MP51.88Z.007F.B00.1008031144"

};

CHAR8* AppleBoardID[24] = {
  "Mac-F4208CC8", //MB11 - yonah
  "Mac-F4208CA9",  //MB21 - merom 05/07
  "Mac-F22788A9",  //MB41 - penryn
  "Mac-F22788AA",  //MB52
  "Mac-F42D86C8",  //MBP51
  "Mac-94245B3640C91C81",  //MBP81 - i5 SB IntelHD3000
  "Mac-942459F5819B171B",  //MBP83 - i7 SB  ATI
  "Mac-6F01561E16C75D06",  //MBP92 - i5-3210M IvyBridge HD4000
  "Mac-942452F5819B1C1B",  //MBA31
  "Mac-2E6FAB96566FE58C",  //MBA52 - i5-3427U IVY BRIDGE IntelHD4000 did=166
  "Mac-F4208EAA",          //MM21 - merom GMA950 07/07
  "Mac-8ED6AF5B48C039E1",  //MM51 - Sandy + Intel 30000
  "Mac-F65AE981FFA204ED",  //MM62 - Ivy
  "Mac-F227BEC8",  //IM81 - merom 01/09
  "Mac-F2268CC8",  //IM101 - wolfdale? E7600 01/
  "Mac-F2268DAE",  //IM111 - Nehalem
  "Mac-F2238AC8",  //IM112 - Clarkdale
  "Mac-F2238BAE",  //IM113 - lynnfield
  "Mac-942B5BF58194151B",  //IM121 - i5-2500 - sandy
  "Mac-942B59F58194171B",  //IM122 - i7-2600
  "Mac-00BE6ED71E35EB86",  //IM131 - -i5-3470S -IVY
  "Mac-F2268DC8",  //MP31 - xeon quad 02/09 conroe
  "Mac-F4238CC8",  //MP41 - xeon wolfdale
  "Mac-F221BEC8"   //MP51 - Xeon Nehalem 4 cores
};

CHAR8* AppleReleaseDate[24] = {
  "09/22/08",  //mb11
  "06/27/07",
  "09/22/08",
  "01/21/09",
  "06/15/09",  //mbp51
  "02/07/11",
  "10/26/11",
  "05/10/2012", //MBP92
  "12/20/07",
  "05/22/2012", //mba52
  "08/07/07",  //mm21
  "02/29/11",  //MM51
  "08/09/2012", //MM62
  "03/05/08",
  "09/03/09",  //im101
  "03/17/10",
  "03/17/10",  //11,2
  "05/03/10",
  "01/24/12",  //121 120124
  "02/23/12",  //122
  "09/04/2012",  //131
  "02/29/08",
  "03/05/09",
  "08/03/10"
};

CHAR8* AppleProductName[24] = {
  "MacBook1,1",
  "MacBook2,1",
  "MacBook4,1",
  "MacBook5,2",
  "MacBookPro5,1",
  "MacBookPro8,1",
  "MacBookPro8,3",
  "MacBookPro9,2",
  "MacBookAir3,1",
  "MacBookAir5,2",
  "Macmini2,1",
  "Macmini5,1",
  "Macmini6,2",
  "iMac8,1",
  "iMac10,1",
  "iMac11,1",
  "iMac11,2",
  "iMac11,3",
  "iMac12,1",
  "iMac12,2",
  "iMac13,1",
  "MacPro3,1",
  "MacPro4,1",
  "MacPro5,1"
};

CHAR8* AppleFamilies[24] = {
  "MacBook",
  "MacBook",
  "MacBook",
  "MacBook",
  "MacBookPro",
  "MacBookPro",
  "MacBookPro",
  "MacBook Pro",
  "MacBookAir",
  "MacBook Air",
  "Macmini",
  "Mac mini",
  "Macmini",
  "iMac",
  "iMac",
  "iMac",
  "iMac",
  "iMac",
  "iMac",
  "iMac",
  "iMac",
  "MacPro",
  "MacPro",
  "MacPro"
};

CHAR8* AppleSystemVersion[24] = {
  "1.1",
  "1.2",
  "1.3",
  "1.3",
  "1.0",
  "1.0",
  "1.0",
  "1.0",
  "1.0",
  "1.0",
  "1.1",
  "1.0", //MM51
  "1.0",
  "1.3",
  "1.0",
  "1.0",
  "1.2",
  "1.0",
  "1.9",
  "1.9",
  "1.0",
  "1.3",
  "1.4",
  "1.2"
};

CHAR8* AppleSerialNumber[24] = {
  "W80A041AU9B", //MB11
  "W88A041AWGP", //MB21 - merom 05/07
  "W88A041A0P0", //MB41
  "W88AAAAA9GU", //MB52
  "W88439FE1G0", //MBP51
  "W89F9196DH2G", //MBP81 - i5 SB IntelHD3000
  "W88F9CDEDF93", //MBP83 -i7 SB  ATI
  "C02HA041DTY3", //MBP92 - i5 IvyBridge HD4000
  "W8649476DQX",  //MBA31
  "C02HA041DRVC", //MBA52 - IvyBridge
  "W88A56BYYL2",  //MM21 - merom GMA950 07/07
  "C07GA041DJD0", //MM51 - sandy
  "C07JD041DWYN", //MM62 - IVY
  "W89A00AAX88", //IM81 - merom 01/09
  "W80AA98A5PE", //IM101 - wolfdale? E7600 01/09
  "G8942B1V5PJ", //IM111 - Nehalem
  "W8034342DB7", //IM112 - Clarkdale
  "QP0312PBDNR", //IM113 - lynnfield
  "W80CF65ADHJF", //IM121 - i5-2500 - sandy
  "W88GG136DHJQ", //IM122 -i7-2600
  "C02JA041DNCT", //IM131 -i5-3470S -IVY
  "W88A77AA5J4", //MP31 - xeon quad 02/09
  "CT93051DK9Y", //MP41
  "CG154TB9WU3"  //MP51 C07J50F7F4MC
};

CHAR8* AppleChassisAsset[24] = {
  "MacBook-White",
  "MacBook-White",
  "MacBook-Black",
  "MacBook-Black",
  "MacBook-Aluminum",
  "MacBook-Aluminum",
  "MacBook-Aluminum",
  "MacBook-Aluminum",
  "Air-Enclosure",
  "Air-Enclosure",
  "Mini-Aluminum",
  "Mini-Aluminum",
  "Mini-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "iMac-Aluminum",
  "Pro-Enclosure",
  "Pro-Enclosure",
  "Pro-Enclosure"
};

CHAR8* AppleBoardSN = "C02032101R5DC771H";
CHAR8* AppleBoardLocation = "Part Component";

//---------------------------------------------------------------------------------

VOID *
GetDataSetting (
  IN TagPtr dict,
  IN CHAR8 *propName,
  OUT UINTN *dataLen
  )
{
  TagPtr  prop;
  UINT8   *data = NULL;
  UINT32   len;
#if 0
  UINTN   i;
#endif
  prop = GetProperty (dict, propName);

  if (prop != NULL) {
    if (prop->data != NULL && prop->dataLen > 0) {
      // data property
      data = AllocateZeroPool (prop->dataLen);
      CopyMem (data, prop->data, prop->dataLen);

      if (dataLen != NULL) {
        *dataLen = prop->dataLen;
      }

#if 0
      DBG("Data: %p, Len: %d = ", data, prop->dataLen);
      for (i = 0; i < prop->dataLen; i++) DBG("%02x ", data[i]);
      DBG("\n");
#endif
    } else {
      // assume data in hex encoded string property
      len = (UINT32) (AsciiStrLen (prop->string) >> 1);       // 2 chars per byte
      data = AllocateZeroPool (len);
      len = hex2bin (prop->string, data, len);

      if (dataLen != NULL) {
        *dataLen = len;
      }

#if 0
      DBG("Data(str): %p, Len: %d = ", data, len);
      for (i = 0; i < len; i++) DBG("%02x ", data[i]);
      DBG("\n");
#endif
    }
  }

  return data;
}

EFI_STATUS
StrToBuf (
  OUT UINT8    *Buf,
  IN  UINTN    BufferLength,
  IN  CHAR16   *Str
)
{
  UINTN       Index;
  UINTN       StrLength;
  UINT8       Digit;
  UINT8       Byte;

  Digit = 0;
  //
  // Two hex char make up one byte
  //
  StrLength = BufferLength * sizeof (CHAR16);

  for (Index = 0; Index < StrLength; Index++, Str++) {
    if ((*Str >= L'a') && (*Str <= L'f')) {
      Digit = (UINT8) (*Str - L'a' + 0x0A);
    } else if ((*Str >= L'A') && (*Str <= L'F')) {
      Digit = (UINT8) (*Str - L'A' + 0x0A);
    } else if ((*Str >= L'0') && (*Str <= L'9')) {
      Digit = (UINT8) (*Str - L'0');
    } else {
      return EFI_INVALID_PARAMETER;
    }

    //
    // For odd characters, write the upper nibble for each buffer byte,
    // and for even characters, the lower nibble.
    //
    if ((Index & 1) == 0) {
      Byte = (UINT8) (Digit << 4);
    } else {
      Byte = Buf[Index / 2];
      Byte &= 0xF0;
      Byte = (UINT8) (Byte | Digit);
    }

    Buf[Index / 2] = Byte;
  }

  return EFI_SUCCESS;
}

/**
 Converts a string to GUID value.
 Guid Format is xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

 @param Str              The registry format GUID string that contains the GUID value.
 @param Guid             A pointer to the converted GUID value.

 @retval EFI_SUCCESS     The GUID string was successfully converted to the GUID value.
 @retval EFI_UNSUPPORTED The input string is not in registry format.
 @return others          Some error occurred when converting part of GUID value.

 **/

EFI_STATUS
StrToGuidLE (
  IN  CHAR16   *Str,
  OUT EFI_GUID *Guid
)
{
  UINT8 GuidLE[16];

  StrToBuf (&GuidLE[0], 4, Str);

  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrToBuf (&GuidLE[4], 2, Str);

  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrToBuf (&GuidLE[6], 2, Str);

  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrToBuf (&GuidLE[8], 2, Str);

  while (!IS_HYPHEN (*Str) && !IS_NULL (*Str)) {
    Str ++;
  }

  if (IS_HYPHEN (*Str)) {
    Str++;
  } else {
    return EFI_UNSUPPORTED;
  }

  StrToBuf (&GuidLE[10], 6, Str);
  CopyMem ((UINT8*) Guid, &GuidLE[0], 16);
  return EFI_SUCCESS;
}

BOOLEAN
IsHexDigit (
  CHAR8 c
)
{
  return (IS_DIGIT (c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) ? TRUE : FALSE;
}

UINT8
hexstrtouint8 (
  CHAR8* buf
)
{
  INT8 i;

  if (IS_DIGIT (buf[0])) {
    i = buf[0] - '0';
  } else if (IS_HEX (buf[0])) {
    i = buf[0] - 'a' + 10;
  } else {
    i = buf[0] - 'A' + 10;
  }

  if (AsciiStrLen (buf) == 1) {
    return i;
  }

  i <<= 4;

  if (IS_DIGIT (buf[1])) {
    i += buf[1] - '0';
  } else if (IS_HEX (buf[1])) {
    i += buf[1] - 'a' + 10;
  } else {
    i += buf[1] - 'A' + 10;  //no error checking
  }

  return i;
}

UINT32
hex2bin (
  IN CHAR8 *hex,
  OUT UINT8 *bin,
  INT32 len
)
{
  CHAR8 *p;
  UINT32 i;
  UINT32 outlen;
  CHAR8 buf[3];

  outlen = 0;

  if (hex == NULL || bin == NULL || len <= 0 || (INT32) AsciiStrLen (hex) != len * 2) {
    return 0;
  }

  buf[2] = '\0';
  p = (CHAR8 *) hex;

  for (i = 0; i < (UINT32) len; i++) {
		while ((*p == 0x20) || (*p == ',')) {
			p++;
		}
		if (*p == 0) {
			break;
		}
    if (!IsHexDigit (p[0]) || !IsHexDigit (p[1])) {
      return 0;
    }

    buf[0] = *p++;
    buf[1] = *p++;
    bin[i] = hexstrtouint8 (buf);
    outlen++;
  }
	bin[outlen] = 0;
	return outlen;
}

VOID
Pause (
  IN CHAR16* Message
)
{
  if (Message != NULL) {
    Print (L"%s", Message);
  }

  gBS->Stall (4000000);
}

BOOLEAN
FileExists (
  IN EFI_FILE *RootFileHandle,
  IN CHAR16   *RelativePath
)
{
  EFI_STATUS  Status;
  EFI_FILE    *TestFile;

  Status = RootFileHandle->Open (RootFileHandle, &TestFile, RelativePath, EFI_FILE_MODE_READ, 0);

  if (Status == EFI_SUCCESS) {
    TestFile->Close (TestFile);
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
egLoadFile (
  IN EFI_FILE_HANDLE BaseDir,
  IN CHAR16 *FileName,
  OUT UINT8 **FileData,
  OUT UINTN *FileDataLength
)
{
  EFI_STATUS          Status;
  EFI_FILE_HANDLE     FileHandle;
  EFI_FILE_INFO       *FileInfo;
  UINT64              ReadSize;
  UINTN               BufferSize;
  UINT8               *Buffer;

  Status = BaseDir->Open (BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  FileInfo = EfiLibFileInfo (FileHandle);

  if (FileInfo == NULL) {
    FileHandle->Close (FileHandle);
    return EFI_NOT_FOUND;
  }

  ReadSize = FileInfo->FileSize;

  if (ReadSize > MAX_FILE_SIZE) {
    ReadSize = MAX_FILE_SIZE;
  }

  FreePool (FileInfo);
  BufferSize = (UINTN) ReadSize;   // was limited to 1 GB above, so this is safe
  Buffer = (UINT8 *) AllocateAlignedPages (EFI_SIZE_TO_PAGES (BufferSize), 16);

  if (Buffer == NULL) {
    FileHandle->Close (FileHandle);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = FileHandle->Read (FileHandle, &BufferSize, Buffer);
  FileHandle->Close (FileHandle);

  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  *FileData = Buffer;
  *FileDataLength = BufferSize;
  return EFI_SUCCESS;
}

UINTN
AsciiStr2Uintn (
  CHAR8* ps
)
{
  UINTN val;

  if ((ps[0] == '0')  && (ps[1] == 'x' || ps[1] == 'X')) {
    val = AsciiStrHexToUintn (ps);
  } else {
    val = AsciiStrDecimalToUintn (ps);
  }
  return val;
}

UINTN
GetNumProperty (
  TagPtr dict,
  CHAR8* key,
  UINTN def
)
{
  TagPtr dentry;

  dentry = GetProperty (dict, key);
  if (dentry != NULL) {
    def = AsciiStr2Uintn(dentry->string);
  }
  return def;
}

BOOLEAN
AsciiStr2Bool (
  CHAR8* ps
)
{
  if (ps[0] == 'y' || ps[0] == 'Y') {
    return TRUE;
  }
  return FALSE;
}

BOOLEAN
GetBoolProperty (
  TagPtr dict,
  CHAR8* key,
  BOOLEAN def
)
{
  TagPtr dentry;

  dentry = GetProperty (dict, key);
  if (dentry != NULL) {
    return AsciiStr2Bool (dentry->string);
  }
  return def;
}

VOID
GetAsciiProperty (
  TagPtr dict,
  CHAR8* key,
  CHAR8* aptr
)
{
  TagPtr dentry;

  dentry = GetProperty (dict, key);
  if (dentry != NULL) {
    AsciiStrCpy (aptr, dentry->string);
  }
}

BOOLEAN
GetUnicodeProperty (
  TagPtr dict,
  CHAR8* key,
  CHAR16* uptr
)
{
  TagPtr dentry;

  dentry = GetProperty (dict, key);
  if (dentry != NULL) {
    AsciiStrToUnicodeStr (dentry->string, uptr);
    return TRUE;
  } else {
    return FALSE;
  }
}

// ----============================----

EFI_STATUS
GetBootDefault (
  IN EFI_FILE *RootFileHandle,
  IN CHAR16* ConfigPlistPath
)
{
  EFI_STATUS  Status;
  CHAR8*      gConfigPtr;
  TagPtr      dict;
  TagPtr      dictPointer;
  UINTN       size;

  Status = EFI_NOT_FOUND;
  gConfigPtr = NULL;

  ZeroMem (gSettings.DefaultBoot, 40);

  if ((RootFileHandle != NULL) && FileExists (RootFileHandle, ConfigPlistPath)) {
    Status = egLoadFile (RootFileHandle, ConfigPlistPath, (UINT8**) &gConfigPtr, &size);
  }

  if (EFI_ERROR (Status)) {
    Print (L"Error loading config.plist!\r\n");
    return Status;
  }

  dict = NULL;
  if (gConfigPtr != NULL) {
    if (ParseXML ((const CHAR8*) gConfigPtr, &dict) != EFI_SUCCESS) {
      Print (L"config error\n");
      return EFI_UNSUPPORTED;
    }

    dictPointer = GetProperty (dict, "SystemParameters");
    gSettings.BootTimeout = (UINT16) GetNumProperty (dictPointer, "Timeout", 0);
    if (!GetUnicodeProperty (dictPointer, "DefaultBootVolume", gSettings.DefaultBoot)) {
      gSettings.BootTimeout = 0xFFFF;
    }
  }

  return Status;
}

EFI_STATUS
GetUserSettings (
  IN EFI_FILE *RootFileHandle,
  IN CHAR16   *ConfigPlistPath
)
{
  EFI_STATUS      Status;
  UINTN           size;
  CHAR8*          gConfigPtr;
  TagPtr          dict;
  TagPtr          dictPointer;
  TagPtr          prop;
  CHAR16          cUUID[40];
  MACHINE_TYPES   Model;
  CHAR8           ANum[4];
  UINTN           len;
  UINT32          i;

  Status = EFI_NOT_FOUND;
  gConfigPtr = NULL;
  dict = NULL;
  len = 0;
  i = 0;

  PrepatchSmbios ();

  Model = GetDefaultModel ();

  AsciiStrCpy (gSettings.VendorName,             BiosVendor);
  AsciiStrCpy (gSettings.RomVersion,             AppleFirmwareVersion[Model]);
  AsciiStrCpy (gSettings.ReleaseDate,            AppleReleaseDate[Model]);
  AsciiStrCpy (gSettings.ManufactureName,        BiosVendor);
  AsciiStrCpy (gSettings.ProductName,            AppleProductName[Model]);
  AsciiStrCpy (gSettings.VersionNr,              AppleSystemVersion[Model]);
  AsciiStrCpy (gSettings.SerialNr,               AppleSerialNumber[Model]);
  AsciiStrCpy (gSettings.FamilyName,             AppleFamilies[Model]);
  AsciiStrCpy (gSettings.BoardManufactureName,   BiosVendor);
  AsciiStrCpy (gSettings.BoardSerialNumber,      AppleBoardSN);
  AsciiStrCpy (gSettings.BoardNumber,            AppleBoardID[Model]);
  AsciiStrCpy (gSettings.BoardVersion,           AppleSystemVersion[Model]);
  AsciiStrCpy (gSettings.LocationInChassis,      AppleBoardLocation);
  AsciiStrCpy (gSettings.ChassisManufacturer,    BiosVendor);
  AsciiStrCpy (gSettings.ChassisAssetTag,        AppleChassisAsset[Model]);

  if ((RootFileHandle != NULL) && FileExists (RootFileHandle, ConfigPlistPath)) {
    Status = egLoadFile (RootFileHandle, ConfigPlistPath, (UINT8**) &gConfigPtr, &size);
  }

  if (EFI_ERROR (Status)) {
    Print (L"Error loading config.plist!\r\n");
    return Status;
  }

  if (gConfigPtr != NULL) {
    if (ParseXML ((const CHAR8*) gConfigPtr, &dict) != EFI_SUCCESS) {
      Print (L"config error\n");
      return EFI_UNSUPPORTED;
    }

    ZeroMem (gSettings.Language, 10);
    ZeroMem (gSettings.BootArgs, 120);
    ZeroMem (gSettings.SerialNr, 64);
    ZeroMem (cUUID, 40);
    SystemIDStatus = EFI_UNSUPPORTED;
    PlatformUuidStatus = EFI_UNSUPPORTED;
    gSettings.CustomEDID = NULL;
    gSettings.ProcessorInterconnectSpeed = 0;
    
    dictPointer = GetProperty (dict, "SystemParameters");

    GetAsciiProperty (dictPointer, "prev-lang", gSettings.Language);
    GetAsciiProperty (dictPointer, "boot-args", gSettings.BootArgs);
    if (AsciiStrLen (AddBootArgs) != 0) {
      AsciiStrCat (gSettings.BootArgs, AddBootArgs);
    }
#if 0
    if (AsciiStrStr(gSettings.BootArgs, AddBootArgs) == 0) {
    }
#endif
    if (dictPointer != NULL) {
      prop = GetProperty (dictPointer, "PlatformUUID");

      if (prop != NULL) {
        AsciiStrToUnicodeStr (prop->string, cUUID);
        PlatformUuidStatus = StrToGuidLE (cUUID, &gPlatformUuid);
        //else value from SMBIOS
      }

      prop = GetProperty (dictPointer, "SystemID");

      if (prop != NULL) {
        AsciiStrToUnicodeStr (prop->string, cUUID);
        SystemIDStatus = StrToGuidLE (cUUID, &gSystemID);
      }
    }

    dictPointer = GetProperty (dict, "Graphics");
    
    gSettings.GraphicsInjector = GetBoolProperty (dictPointer, "GraphicsInjector", FALSE);
    gSettings.VRAM = LShiftU64 (GetNumProperty (dictPointer, "VRAM", 0), 20);
    gSettings.LoadVBios = GetBoolProperty (dictPointer, "LoadVBios", FALSE);
    gSettings.VideoPorts = (UINT16) GetNumProperty (dictPointer, "VideoPorts", 0);
    GetUnicodeProperty (dictPointer, "FBName", gSettings.FBName);

    if (dictPointer != NULL) {

      prop = GetProperty (dictPointer, "NVCAP");

      if (prop != NULL) {
        hex2bin (prop->string, (UINT8*) &gSettings.NVCAP[0], 20);
      }

      prop = GetProperty (dictPointer, "DisplayCfg");

      if (prop != NULL) {
        hex2bin (prop->string, (UINT8*) &gSettings.Dcfg[0], 8);
      }

      prop = GetProperty (dictPointer, "CustomEDID");

      if (prop != NULL) {
        gSettings.CustomEDID = GetDataSetting (dictPointer, "CustomEDID", &len);
      }
    }

    dictPointer = GetProperty (dict, "PCI");
    
    gSettings.PCIRootUID = (UINT16) GetNumProperty (dictPointer, "PCIRootUID", 0);
    gSettings.ETHInjection = GetBoolProperty (dictPointer, "ETHInjection", FALSE);
    gSettings.USBInjection = GetBoolProperty (dictPointer, "USBInjection", FALSE);
    gSettings.HDALayoutId = (UINT16) GetNumProperty (dictPointer, "HDAInjection", 0);


    if (dictPointer != NULL) {
      prop = GetProperty (dictPointer, "DeviceProperties");

      if (prop != NULL) {
        cDevProp = AllocateZeroPool (AsciiStrLen (prop->string) + 1);
        AsciiStrCpy (cDevProp, prop->string);
      }
    }

    dictPointer = GetProperty (dict, "ACPI");

    gSettings.DropSSDT = GetBoolProperty (dictPointer, "DropOemSSDT", FALSE);
    gSettings.PatchAPIC = GetBoolProperty (dictPointer, "PatchAPIC", FALSE);
    // known pair for ResetAddr/ResetVal is 0x0[C/2]F9/0x06, 0x64/0xFE
    gSettings.ResetAddr = (UINT64) GetNumProperty (dictPointer, "ResetAddress", 0);
    gSettings.ResetVal = (UINT8) GetNumProperty (dictPointer, "ResetValue", 0);
    gSettings.PMProfile = (UINT8) GetNumProperty (dictPointer, "PMProfile", 0);

    dictPointer = GetProperty (dict, "SMBIOS");

    gSettings.Mobile = GetBoolProperty (dictPointer, "Mobile", gMobile);
    GetAsciiProperty (dictPointer, "BiosVendor", gSettings.VendorName);
    GetAsciiProperty (dictPointer, "BiosVersion", gSettings.RomVersion);
    GetAsciiProperty (dictPointer, "BiosReleaseDate", gSettings.ReleaseDate);
    GetAsciiProperty (dictPointer, "Manufacturer", gSettings.ManufactureName);
    GetAsciiProperty (dictPointer, "ProductName", gSettings.ProductName);
    GetAsciiProperty (dictPointer, "Version", gSettings.VersionNr);
    GetAsciiProperty (dictPointer, "Family", gSettings.FamilyName);
    GetAsciiProperty (dictPointer, "SerialNumber", gSettings.SerialNr);
    GetAsciiProperty (dictPointer, "BoardManufacturer", gSettings.BoardManufactureName);
    GetAsciiProperty (dictPointer, "BoardSerialNumber", gSettings.BoardSerialNumber);
    GetAsciiProperty (dictPointer, "Board-ID", gSettings.BoardNumber);
    GetAsciiProperty (dictPointer, "BoardVersion", gSettings.BoardVersion);
    GetAsciiProperty (dictPointer, "LocationInChassis", gSettings.LocationInChassis);
    GetAsciiProperty (dictPointer, "ChassisManufacturer", gSettings.ChassisManufacturer);
    GetAsciiProperty (dictPointer, "ChassisAssetTag", gSettings.ChassisAssetTag);

    dictPointer = GetProperty (dict, "CPU");

    gSettings.Turbo = GetBoolProperty (dictPointer, "Turbo", FALSE);
    gSettings.CpuFreqMHz = (UINT16) GetNumProperty (dictPointer, "CpuFrequencyMHz", 0);
    gSettings.BusSpeed = (UINT32) GetNumProperty (dictPointer, "BusSpeedkHz", 0);
    gSettings.ProcessorInterconnectSpeed = (UINT32) GetNumProperty (dictPointer, "QPI", 0);
    gSettings.CPUSpeedDetectiond = (UINT8) GetNumProperty (dictPointer, "CPUSpeedDetection", 0);

    // KernelAndKextPatches
    gSettings.KPKernelCpu = FALSE; // disabled by default
    gSettings.KPKextPatchesNeeded = FALSE;

    dictPointer = GetProperty(dict,"KernelPatches");

    gSettings.KPKernelCpu = GetBoolProperty (dictPointer, "KernelCpu", FALSE);

    dictPointer = GetProperty(dict,"KextPatches");

#if 0
    prop = GetProperty(dictPointer,"ATIConnectorsController");
    if(prop) {
      // ATIConnectors patch
      gSettings.KPATIConnectorsController = AllocateZeroPool (AsciiStrSize (prop->string) * sizeof(CHAR16));
      AsciiStrToUnicodeStr (prop->string, gSettings.KPATIConnectorsController);
      gSettings.KPATIConnectorsData = GetDataSetting (
                                        dictPointer,
                                        "ATIConnectorsData",
                                        &gSettings.KPATIConnectorsDataLen
                                      );
      gSettings.KPATIConnectorsPatch = GetDataSetting (dictPointer, "ATIConnectorsPatch", &len);

      if (gSettings.KPATIConnectorsData == NULL ||
          gSettings.KPATIConnectorsPatch == NULL ||
          gSettings.KPATIConnectorsDataLen == 0 ||
          gSettings.KPATIConnectorsDataLen != len) {
        // invalid params - no patching
        if (gSettings.KPATIConnectorsController != NULL) FreePool(gSettings.KPATIConnectorsController);
        if (gSettings.KPATIConnectorsData != NULL) FreePool(gSettings.KPATIConnectorsData);
        if (gSettings.KPATIConnectorsPatch != NULL) FreePool(gSettings.KPATIConnectorsPatch);
        gSettings.KPATIConnectorsController = NULL;
        gSettings.KPATIConnectorsData = NULL;
        gSettings.KPATIConnectorsPatch = NULL;
        gSettings.KPATIConnectorsDataLen = 0;
      } else {
        // ok
        gSettings.KPKextPatchesNeeded = TRUE;
      }
    }
    
    gSettings.KPAsusAICPUPM = GetBoolProperty (dictPointer, "AsusAICPUPM", FALSE);
    gSettings.KPKextPatchesNeeded |= gSettings.KPAsusAICPUPM;
    gSettings.KPAppleRTC = GetBoolProperty (dictPointer, "AppleRTC", FALSE);
    gSettings.KPKextPatchesNeeded |= gSettings.KPAppleRTC;
#endif

    i = 0;
    do {
      AsciiSPrint(ANum, 4, "%d", i);
      dict = GetProperty(dictPointer, ANum);
      if (!dict) {
        break;
      }
      GetAsciiProperty (dict, "Name", gSettings.AnyKext[i]);

      // check if this is Info.plist patch or kext binary patch
      gSettings.AnyKextInfoPlistPatch[i] = GetBoolProperty (dict, "InfoPlistPatch", FALSE);
      
      if (gSettings.AnyKextInfoPlistPatch[i]) {
        // Info.plist
        // Find and Replace should be in <string>...</string>
        prop = GetProperty(dict, "Find");
        gSettings.AnyKextDataLen[i] = 0;
        if(prop && prop->string) {
          gSettings.AnyKextDataLen[i] = AsciiStrLen (prop->string);
          gSettings.AnyKextData[i] = (UINT8 *) AllocateCopyPool (gSettings.AnyKextDataLen[i] + 1, prop->string);
        }
        prop = GetProperty(dict, "Replace");
        if(prop && prop->string) {
          len = AsciiStrLen (prop->string);
          gSettings.AnyKextPatch[i] = (UINT8 *) AllocateCopyPool (AsciiStrLen (prop->string) + 1, prop->string);
        }
      } else {
        // kext binary patch
        // Find and Replace should be in <data>...</data> or <string>...</string>
        gSettings.AnyKextData[i] = GetDataSetting (dict,"Find", &gSettings.AnyKextDataLen[i]);
        gSettings.AnyKextPatch[i] = GetDataSetting (dict,"Replace", &len);
      }

      if (gSettings.AnyKextDataLen[i] != len || len == 0) {
        gSettings.AnyKext[i][0] = 0; //just erase name
        continue; //same i
      }
      gSettings.KPKextPatchesNeeded = TRUE;
      i++;

      if (i>99) {
        break;
      }
    } while (TRUE);

    gSettings.NrKexts = i;

    gMobile = gSettings.Mobile;

    if ((gSettings.BusSpeed > 10 * kilo) &&
        (gSettings.BusSpeed < 500 * kilo)) {
      gCPUStructure.ExternalClock = gSettings.BusSpeed;
    }

    if ((gSettings.CpuFreqMHz > 100) &&
        (gSettings.CpuFreqMHz < 20000)) {
      gCPUStructure.CurrentSpeed = gSettings.CpuFreqMHz;
    }
  }

  GetCPUProperties ();

  if (gConfigPtr != NULL) {
    dictPointer = GetProperty (dict, "CPU");
    gSettings.CpuType = (UINT16) GetNumProperty (dictPointer, "ProcessorType", GetAdvancedCpuType());
  }

  return Status;
}

EFI_STATUS
GetOSVersion(
  IN EFI_FILE   *FileHandle
)
{
	EFI_STATUS  Status = EFI_NOT_FOUND;
	CHAR8       *plistBuffer = 0;
	UINTN       plistLen;
	TagPtr      dictPointer  = NULL;
  CHAR16      *SystemPlist = L"System\\Library\\CoreServices\\SystemVersion.plist";
  CHAR16      *ServerPlist = L"System\\Library\\CoreServices\\ServerVersion.plist";
  CHAR16      *RecoveryPlist = L"\\com.apple.recovery.boot\\SystemVersion.plist";

  if (!FileHandle) {
    return EFI_NOT_FOUND;
  }

	/* Mac OS X */
	if(FileExists(FileHandle, SystemPlist)) {
		Status = egLoadFile(FileHandle, SystemPlist, (UINT8 **)&plistBuffer, &plistLen);
  }	else if(FileExists(FileHandle, ServerPlist)) {
		Status = egLoadFile(FileHandle, ServerPlist, (UINT8 **)&plistBuffer, &plistLen);
  }	else if(FileExists(FileHandle, RecoveryPlist)) {
		Status = egLoadFile(FileHandle, RecoveryPlist, (UINT8 **)&plistBuffer, &plistLen);
  }

	if(!EFI_ERROR(Status)) {
		if(ParseXML(plistBuffer, &dictPointer) != EFI_SUCCESS) {
			FreePool(plistBuffer);
			return EFI_NOT_FOUND;
    }

    GetAsciiProperty (dictPointer, "ProductVersion", OSVersion);

  }

	return Status;
}
