/****************************************************************************
*                                                                           *
* Project 64 - A Nintendo 64 emulator.                                      *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/
#include "stdafx.h"

CN64Rom::CN64Rom()
{
	m_hRomFile        = NULL;
	m_hRomFileMapping = NULL;
	m_ROMImage        = NULL; 
	m_ErrorMsg        = EMPTY_STRING;
	m_RomName         = "";
	m_FileName        = "";
	m_MD5             = "";
	m_Country         = UnknownCountry;
	m_CicChip         = CIC_UNKNOWN;
}

CN64Rom::~CN64Rom()
{
	UnallocateRomImage();
}

bool CN64Rom::AllocateAndLoadN64Image ( const char * FileLoc, bool LoadBootCodeOnly ) {
	//Try to open the target file
	HANDLE hFile = CreateFile(FileLoc,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
		NULL);		
	if (hFile == INVALID_HANDLE_VALUE) { return false; }

	//Read the first 4 bytes and make sure it is a valid n64 image
	DWORD dwRead; BYTE Test[4];
	
	SetFilePointer(hFile,0,0,FILE_BEGIN);
	ReadFile(hFile,Test,4,&dwRead,NULL);
	if (!IsValidRomImage(Test)) { 
		CloseHandle( hFile );
		return false;
	}

	//Get the size of the rom and try to allocate the memory needed.
	DWORD RomFileSize = GetFileSize(hFile,NULL);
	//if loading boot code then just load the first 0x1000 bytes
	if (LoadBootCodeOnly) { RomFileSize = 0x1000; }

	BYTE * Image = (BYTE *)VirtualAlloc(NULL,RomFileSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
	if (Image == NULL) {
		SetError(MSG_MEM_ALLOC_ERROR);
		CloseHandle( hFile );
		return false;
	}

	//Load the n64 rom to the allocated memory
	g_Notify->DisplayMessage(5,MSG_LOADING);
	SetFilePointer(hFile,0,0,FILE_BEGIN);

	DWORD count, TotalRead = 0;
	for (count = 0; count < (int)RomFileSize; count += ReadFromRomSection) {
		DWORD dwToRead = RomFileSize - count;
		if (dwToRead > ReadFromRomSection) { dwToRead = ReadFromRomSection; }

		if (!ReadFile(hFile,&Image[count],dwToRead,&dwRead,NULL)) {
			VirtualFree(Image,0,MEM_RELEASE);
			CloseHandle( hFile );
			SetError(MSG_FAIL_IMAGE);
			return false;
		}
		TotalRead += dwRead;

		//Show Message of how much % wise of the rom has been loaded
		g_Notify->DisplayMessage(0,L"%s: %.2f%c",GS(MSG_LOADED),((float)TotalRead/(float)RomFileSize) * 100.0f,'%');
	}
	dwRead = TotalRead;

	if (RomFileSize != dwRead) {
		VirtualFree(Image,0,MEM_RELEASE);
		CloseHandle( hFile );
		SetError(MSG_FAIL_IMAGE);
		return false;
	}

	//save information about the rom loaded
	m_hRomFile        = hFile;
	m_ROMImage        = Image; 
	m_RomFileSize     = RomFileSize;

	g_Notify->DisplayMessage(5,MSG_BYTESWAP);
	ByteSwapRom();

	//Protect the memory so that it can not be written to.
	DWORD OldProtect;
	VirtualProtect(m_ROMImage,m_RomFileSize,PAGE_READONLY,&OldProtect);
	
	return true;
}

bool CN64Rom::AllocateAndLoadZipImage(const char * FileLoc, bool LoadBootCodeOnly) {
	unzFile file = unzOpen(FileLoc);
	if (file == NULL)
		return false;

	int port = unzGoToFirstFile(file);
	bool FoundRom = false; 
	
	//scan through all files in zip to a suitable file is found
	while(port == UNZ_OK && !FoundRom) {
		unz_file_info info;
		char zname[_MAX_PATH];

		unzGetCurrentFileInfo(file, &info, zname, sizeof(zname), NULL,0, NULL,0);
		if (unzLocateFile(file, zname, 1) != UNZ_OK ) {
			SetError(MSG_FAIL_ZIP);
			break;	
		}
		if( unzOpenCurrentFile(file) != UNZ_OK ) {
			SetError(MSG_FAIL_ZIP);
			break;
		}

		//Read the first 4 bytes to check magic number
		BYTE Test[4];
		unzReadCurrentFile(file,Test,sizeof(Test));
		if (IsValidRomImage(Test)) {
			//Get the size of the rom and try to allocate the memory needed.
			DWORD RomFileSize = info.uncompressed_size;
			if (LoadBootCodeOnly) {
				RomFileSize = 0x1000;
			}
			BYTE * Image = (BYTE *)VirtualAlloc(NULL,RomFileSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
			if (Image == NULL) {
				SetError(MSG_MEM_ALLOC_ERROR);
				unzCloseCurrentFile(file);
				break;
			}

			//Load the n64 rom to the allocated memory
			g_Notify->DisplayMessage(5,MSG_LOADING);
			memcpy(Image,Test,4);

			DWORD dwRead, count, TotalRead = 0;
			for (count = 4; count < (int)RomFileSize; count += ReadFromRomSection) {
				DWORD dwToRead = RomFileSize - count;
				if (dwToRead > ReadFromRomSection) { dwToRead = ReadFromRomSection; }

				dwRead = unzReadCurrentFile(file,&Image[count],dwToRead);
				if (dwRead == 0) {
					SetError(MSG_FAIL_ZIP);
					VirtualFree(Image,0,MEM_RELEASE);
					unzCloseCurrentFile(file);
					break;
				}
				TotalRead += dwRead;

				//Show Message of how much % wise of the rom has been loaded
				g_Notify->DisplayMessage(5,L"%s: %.2f%c",GS(MSG_LOADED),((float)TotalRead/(float)RomFileSize) * 100.0f,'%');
			}
			dwRead = TotalRead + 4;

			if (RomFileSize != dwRead) {
				VirtualFree(Image,0,MEM_RELEASE);
				unzCloseCurrentFile(file);
				SetError(MSG_FAIL_ZIP);
				g_Notify->DisplayMessage(1,L"");
				break;
			}

			//save information about the rom loaded
			m_hRomFile        = NULL;
			m_ROMImage        = Image; 
			m_RomFileSize     = RomFileSize;
			FoundRom          = true;

			g_Notify->DisplayMessage(5,MSG_BYTESWAP);
			ByteSwapRom();

			//Protect the memory so that it can not be written to.
			DWORD OldProtect;
			VirtualProtect(m_ROMImage,m_RomFileSize,PAGE_READONLY,&OldProtect);
			g_Notify->DisplayMessage(1,L"");
		}
		unzCloseCurrentFile(file);

		if (!FoundRom)
			port = unzGoToNextFile(file);
	}
	unzClose(file);
	
	return FoundRom;
}

void CN64Rom::ByteSwapRom() {
	DWORD count;

	switch (*((DWORD *)&m_ROMImage[0])) {
	case 0x12408037:
		for( count = 0 ; count < m_RomFileSize; count += 4 ) {
			m_ROMImage[count] ^= m_ROMImage[count+2];
			m_ROMImage[count + 2] ^= m_ROMImage[count];
			m_ROMImage[count] ^= m_ROMImage[count+2];
			m_ROMImage[count + 1] ^= m_ROMImage[count + 3];
			m_ROMImage[count + 3] ^= m_ROMImage[count + 1];
			m_ROMImage[count + 1] ^= m_ROMImage[count + 3];
		}
		break;
	case 0x40072780: //64DD IPL
	case 0x40123780:
		for( count = 0 ; count < m_RomFileSize; count += 4 ) {
			m_ROMImage[count] ^= m_ROMImage[count+3];
			m_ROMImage[count + 3] ^= m_ROMImage[count];
			m_ROMImage[count] ^= m_ROMImage[count+3];
			m_ROMImage[count + 1] ^= m_ROMImage[count + 2];
			m_ROMImage[count + 2] ^= m_ROMImage[count + 1];
			m_ROMImage[count + 1] ^= m_ROMImage[count + 2];
		}
		break;
	case 0x80371240: break;
	default:
		g_Notify->DisplayError(L"ByteSwapRom: %X",m_ROMImage[0]);
	}
}

void CN64Rom::CalculateCicChip()
{
	__int64 CRC = 0;
	
	if (m_ROMImage == NULL) 
	{
		m_CicChip = CIC_UNKNOWN; 
		return;
	}

	for (int count = 0x40; count < 0x1000; count += 4) {
		CRC += *(DWORD *)(m_ROMImage+count);
	}
	switch (CRC) {
	case 0x000000D0027FDF31: m_CicChip = CIC_NUS_6101; break;
	case 0x000000CFFB631223: m_CicChip = CIC_NUS_6101; break;
	case 0x000000D057C85244: m_CicChip = CIC_NUS_6102; break;
	case 0x000000D6497E414B: m_CicChip = CIC_NUS_6103; break;
	case 0x0000011A49F60E96: m_CicChip = CIC_NUS_6105; break;
	case 0x000000D6D5BE5580: m_CicChip = CIC_NUS_6106; break;
	case 0x000001053BC19870: m_CicChip = CIC_NUS_5167; break;	//64DD CONVERSION CIC
	case 0x000000D2E53EF008: m_CicChip = CIC_NUS_8303; break;	//64DD IPL
	default:
		if (bHaveDebugger())
			g_Notify->DisplayError(L"Unknown CIC checksum:\n%I64X.", CRC);
		m_CicChip = CIC_UNKNOWN; break;
	}

}

void CN64Rom::CalculateRomCrc()
{
	DWORD t0, t2, t3, t4, t5;
	DWORD a0, a1, a2, a3;
	DWORD s0;
	DWORD v0, v1;

	// CIC_NUS_6101	at=0x5D588B65 , s6=0x3F
	// CIC_NUS_6102	at=0x5D588B65 , s6=0x3F
	// CIC_NUS_6103	at=0x6C078965 , s6=0x78
	// CIC_NUS_6105	at=0x5d588b65 , s6=0x91
	// CIC_NUS_6106	at=0x6C078965 , s6=0x85

	// 64DD IPL		at=0x02E90EDD , s6=0xdd

	//v0 = 0xFFFFFFFF & (0x3F * at) + 1;
	switch (m_CicChip){
	case CIC_NUS_6101:
	case CIC_NUS_6102: v0 = 0xF8CA4DDC; break;
	case CIC_NUS_6103: v0 = 0xA3886759; break;
	case CIC_NUS_6105: v0 = 0xDF26F436; break;
	case CIC_NUS_6106: v0 = 0x1FEA617A; break;
	default:
		return;
	}

	DWORD OldProtect;
	VirtualProtect(m_ROMImage, m_RomFileSize, PAGE_READWRITE, &OldProtect);

	v1 = 0;
	t0 = 0;
	t5 = 0x20;

	a3 = v0;
	t2 = v0;
	t3 = v0;
	s0 = v0;
	a2 = v0;
	t4 = v0;

	for (t0 = 0; t0 < 0x00100000; t0 += 4){
		v0 = *(DWORD *)(m_ROMImage + t0 + 0x1000);

		v1 = a3 + v0;
		a1 = v1;

		if (v1 < a3) t2 += 0x1;
		v1 = v0 & 0x001F;

		a0 = (v0 << v1) | (v0 >> (t5 - v1));

		a3 = a1;
		t3 = t3 ^ v0;

		s0 = s0 + a0;
		if (a2 < v0) a2 = a3 ^ v0 ^ a2;
		else a2 = a2 ^ a0;

		if (m_CicChip == CIC_NUS_6105){
			t4 = (v0 ^ (*(DWORD *)(m_ROMImage + (0xFF & t0) + 0x750))) + t4;
		}
		else t4 = (v0 ^ s0) + t4;
	}
	if (m_CicChip == CIC_NUS_6103){
		a3 = (a3 ^ t2) + t3;
		s0 = (s0 ^ a2) + t4;
	}
	else if (m_CicChip == CIC_NUS_6106){
		a3 = 0xFFFFFFFF & (a3 * t2) + t3;
		s0 = 0xFFFFFFFF & (s0 * a2) + t4;
	}
	else {
		a3 = a3 ^ t2 ^ t3;
		s0 = s0 ^ a2 ^ t4;
	}

	*(DWORD *)(m_ROMImage + 0x10) = a3;
	*(DWORD *)(m_ROMImage + 0x14) = s0;

	VirtualProtect(m_ROMImage, m_RomFileSize, PAGE_READONLY, &OldProtect);
}

CICChip CN64Rom::CicChipID()
{
	return m_CicChip;
}

bool CN64Rom::IsValidRomImage ( BYTE Test[4] ) {
	if ( *((DWORD *)&Test[0]) == 0x40123780 ) { return true; }
	if ( *((DWORD *)&Test[0]) == 0x12408037 ) { return true; }
	if ( *((DWORD *)&Test[0]) == 0x80371240 ) { return true; }
	if ( *((DWORD *)&Test[0]) == 0x40072780 ) { return true; } //64DD IPL
	return false;
}

void CN64Rom::NotificationCB ( LPCWSTR Status, CN64Rom * /*_this*/ )
{
	g_Notify->DisplayMessage(5,L"%s",Status);
}

bool CN64Rom::LoadN64Image ( const char * FileLoc, bool LoadBootCodeOnly ) {
	UnallocateRomImage();
	m_ErrorMsg = EMPTY_STRING;

	char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	_splitpath(FileLoc,drive,dir,fname,ext);
	bool Loaded7zFile = false;

	if (strstr(FileLoc,"?") != NULL || strcmp(ext,".7z") == 0)
	{
		char FullPath[MAX_PATH + 100];
		strcpy(FullPath,FileLoc);

		//this should be a 7zip file
		char * SubFile = strstr(FullPath,"?");
		if (SubFile == NULL)
		{
			//Pop up a dialog and select file
			//allocate memory for sub name and copy selected file name to var
			//return false; //remove once dialog is done
		} else {
			*SubFile = '\0';
			SubFile += 1;
		}	

		C7zip ZipFile(FullPath);
		ZipFile.SetNotificationCallback((C7zip::LP7ZNOTIFICATION)NotificationCB,this);
		for (int i = 0; i < ZipFile.NumFiles(); i++)
		{
			CSzFileItem * f = ZipFile.FileItem(i);
			if (f->IsDir)
			{
				continue;
			}

			stdstr ZipFileName;
			ZipFileName.FromUTF16(ZipFile.FileNameIndex(i).c_str());
			if (SubFile != NULL)
			{
				if (_stricmp(ZipFileName.c_str(), SubFile) != 0)
				{
					continue;
				}
			}

			//Get the size of the rom and try to allocate the memory needed.
			DWORD RomFileSize = (DWORD)f->Size;
			//if loading boot code then just load the first 0x1000 bytes
			if (LoadBootCodeOnly) { RomFileSize = 0x1000; }

			BYTE * Image = (BYTE *)VirtualAlloc(NULL,RomFileSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
			if (Image == NULL) {
				SetError(MSG_MEM_ALLOC_ERROR);
				return false;
			}

			//Load the n64 rom to the allocated memory
			g_Notify->DisplayMessage(5,MSG_LOADING);
			if (!ZipFile.GetFile(i,Image,RomFileSize)) 
			{
				VirtualFree(Image,0,MEM_RELEASE);
				SetError(MSG_FAIL_IMAGE);
				return false;
			}

			if (!IsValidRomImage(Image)) 
			{ 
				VirtualFree(Image,0,MEM_RELEASE);
				SetError(MSG_FAIL_IMAGE);
				return false;
			}

			//save information about the rom loaded
			m_ROMImage        = Image; 
			m_RomFileSize     = RomFileSize;

			g_Notify->DisplayMessage(5,MSG_BYTESWAP);
			ByteSwapRom();

			//Protect the memory so that it can not be written to.
			DWORD OldProtect;
			VirtualProtect(m_ROMImage,m_RomFileSize,PAGE_READONLY,&OldProtect);
	
			Loaded7zFile = true;
			break;
		}
		if (!Loaded7zFile)
		{
			SetError(MSG_7Z_FILE_NOT_FOUND);
			return false;
		}
	}

	//Try to open the file as a zip file
	if (!Loaded7zFile)
	{
		if (!AllocateAndLoadZipImage(FileLoc,LoadBootCodeOnly)) {
			if (m_ErrorMsg != EMPTY_STRING) {
				return false;
			}

			//other wise treat if normal file and open the passed file
			HANDLE hFile = CreateFile(FileLoc,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,NULL);
			if (hFile == INVALID_HANDLE_VALUE) { SetError(MSG_FAIL_OPEN_IMAGE); return false;  }

			//Create a file map
			HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
			if (hFileMapping == NULL) {
				CloseHandle(hFile);
				SetError(MSG_FAIL_OPEN_IMAGE);
				return false;
			}

			//Map the file to a memory pointer .. ie a way of pretending to load the rom
			//loose the bonus of being able to flip it on the fly tho.
			BYTE * Image = (PBYTE)MapViewOfFile(hFileMapping,FILE_MAP_READ,0,0,0);
			if (Image == NULL) {
				CloseHandle(hFileMapping);
				CloseHandle(hFile);
				SetError(MSG_FAIL_OPEN_IMAGE);
				return false;
			}

			if (!IsValidRomImage(Image)) { 
				UnmapViewOfFile (Image);
				CloseHandle(hFileMapping);
				CloseHandle(hFile);
				SetError(MSG_FAIL_IMAGE);
				return false;
			}
			
			if (*((DWORD *)Image) == 0x80371240) {
				m_hRomFile        = hFile;
				m_hRomFileMapping = hFileMapping;
				m_ROMImage        = Image; 
				m_RomFileSize     = GetFileSize(hFile,NULL);
			} else {
				if (!AllocateAndLoadN64Image(FileLoc,LoadBootCodeOnly)) { return false; }
			}
		}
	}
	
	char RomName[260];
	int  count;
	//Get the header from the rom image
	memcpy(&RomName[0],(void *)(m_ROMImage + 0x20),20);
	for( count = 0 ; count < 20; count += 4 ) {
		RomName[count]     ^= RomName[count+3];
		RomName[count + 3] ^= RomName[count];
		RomName[count]     ^= RomName[count+3];			
		RomName[count + 1] ^= RomName[count + 2];
		RomName[count + 2] ^= RomName[count + 1];
		RomName[count + 1] ^= RomName[count + 2];			
	}
	
	//truncate all the spaces at the end of the string
	for( count = 19 ; count >= 0; count -- ) {
		if (RomName[count] == ' ') {
			RomName[count] = '\0';
		} else if (RomName[count] == '\0') {
		} else {
			count = -1;
		}
	}
	RomName[20] = '\0';
	if (strlen(RomName) == 0) { 
		char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
		_splitpath(FileLoc,drive,dir,fname,ext);
		strcpy(RomName,fname); 
	}

	//remove all /,\,: from the string 
	for( count = 0 ; count < (int)strlen(RomName); count ++ ) {
		switch (RomName[count]) {
		case '/': case '\\': RomName[count] = '-'; break;
		case ':': RomName[count] = ';'; break;
		}
	}

	m_RomName  = RomName;
	m_FileName = FileLoc;
	m_MD5      = "";

	if (!LoadBootCodeOnly) {
		//Calculate files MD5
		m_MD5 = MD5((const unsigned char *)m_ROMImage, m_RomFileSize).hex_digest();
	}

	m_Country = (Country)m_ROMImage[0x3D];
	m_RomIdent.Format("%08X-%08X-C:%X",*(DWORD *)(&m_ROMImage[0x10]),*(DWORD *)(&m_ROMImage[0x14]),m_ROMImage[0x3D]);
	CalculateCicChip();

	if (!LoadBootCodeOnly && g_Rom == this) 
	{
		SaveRomSettingID(false);
	}
	
	if (g_Settings->LoadBool(Game_CRC_Recalc))
	{
		//Calculate ROM Header CRC
		CalculateRomCrc();
	}
	
	return true;
}

//Save the settings of the loaded rom, so all loaded settings about rom will be identified with
//this rom
void CN64Rom::SaveRomSettingID ( bool temp ) 
{
	g_Settings->SaveBool(Game_TempLoaded,temp);	
	g_Settings->SaveString(Game_GameName,m_RomName.c_str());
	g_Settings->SaveString(Game_IniKey,m_RomIdent.c_str());

	switch (GetCountry())
	{
	case Germany: case french:  case Italian:
	case Europe:  case Spanish: case Australia:
	case X_PAL:   case Y_PAL:
		g_Settings->SaveDword(Game_SystemType,SYSTEM_PAL);
		break;
	default:
		g_Settings->SaveDword(Game_SystemType,SYSTEM_NTSC);
		break;
	}
}

void CN64Rom::ClearRomSettingID()
{
	g_Settings->SaveString(Game_GameName,"");
	g_Settings->SaveString(Game_IniKey,"");
}

void CN64Rom::SetError(LanguageStringID ErrorMsg)
{
	m_ErrorMsg = ErrorMsg;
}

void CN64Rom::UnallocateRomImage()
{
	if (m_hRomFileMapping) {
		UnmapViewOfFile (m_ROMImage);
        CloseHandle(m_hRomFileMapping);
		m_ROMImage        = NULL;
		m_hRomFileMapping = NULL;
	}
	if (m_hRomFile) {
        CloseHandle(m_hRomFile);
		m_hRomFile = NULL;
	}

	//if this value is still set then the image was not created a map 
	//file but created with VirtualAllocate
	if (m_ROMImage) {
		VirtualFree(m_ROMImage,0,MEM_RELEASE);
		m_ROMImage = NULL;
	}
}
