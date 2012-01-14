/******************************************************************************
pstgdump - by fizzgig and the foofus.net group
Copyright (C) 2006 by fizzgig
http://www.foofus.net

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
******************************************************************************/
#include "StdAfx.h"
#include "protectedstorage.h"

#define MAX_KEY_SIZE 200

// The format for the output - should be ResourceName|ResourceType|UserName|Password
#define OUTPUT_FORMAT "%s|%s|%s|%s\n"

typedef HRESULT (WINAPI *PSTORECREATEINSTANCE)(IPStore **, DWORD, DWORD, DWORD);


ProtectedStorage::ProtectedStorage(void)
{
	m_nOutlookCount = 0;
	m_pOutlookDataHead = NULL;
}

ProtectedStorage::~ProtectedStorage(void)
{
	if (m_pOutlookDataHead != NULL)
	{
		free(m_pOutlookDataHead);
	}
}

bool ProtectedStorage::GetProtectedData(void)
{
	bool bSuccess = true;

	if (!EnumOutlookExpressAccounts())
		bSuccess = false;

	if (!EnumerateIdentities())
		bSuccess = false;

	if (!EnumProtectedStorage())
		bSuccess = false;
	
	return bSuccess;
}

BOOL ProtectedStorage::EnumOutlookExpressAccounts(void)
{
	// Define important registry keys
	const char* OUTLOOK_KEY = "Software\\Microsoft\\Internet Account Manager\\Accounts";
	
	int oIndex = 0;
	HKEY hkey, hkey2;
	long nEnumResult = 0;
	char name[MAX_KEY_SIZE], skey[MAX_KEY_SIZE];
	DWORD dwKeyLen;
	FILETIME timeLastWrite;
	BYTE Data[150];
	BYTE Data1[150];
	DWORD size;
	int j = 0, i = 0;
	LONG lResult;
	DWORD type = REG_BINARY;

	strncpy_s(skey, MAX_KEY_SIZE, OUTLOOK_KEY, MAX_KEY_SIZE);
	lResult = RegOpenKeyEx(HKEY_CURRENT_USER, (LPCTSTR)skey, 0, KEY_ALL_ACCESS, &hkey);

	if(ERROR_SUCCESS != lResult)
	{
		printf("Unable to enumerate Outlook Express accounts: could not open HKCU\\%s\n", OUTLOOK_KEY);
		return FALSE;
	}

	while(nEnumResult != ERROR_NO_MORE_ITEMS)
	{
		dwKeyLen = MAX_KEY_SIZE;
		nEnumResult = RegEnumKeyEx(hkey, i, name, &dwKeyLen, NULL, NULL, NULL, &timeLastWrite);
		strncpy_s(skey, MAX_KEY_SIZE, OUTLOOK_KEY, MAX_KEY_SIZE - strlen(OUTLOOK_KEY) - 2);
		strcat_s(skey, MAX_KEY_SIZE, "\\");
		strcat_s(skey, MAX_KEY_SIZE, name);

		if (RegOpenKeyEx(HKEY_CURRENT_USER, (LPCTSTR)skey, 0, KEY_ALL_ACCESS, &hkey2) == ERROR_SUCCESS)
		{
			size = sizeof(Data);
			if(RegQueryValueEx(hkey2, "HTTPMail User Name" , 0, &type, Data, &size) == ERROR_SUCCESS)
			{
				++m_nOutlookCount;
				if (m_pOutlookDataHead == NULL)
				{
					OutlookData = (OEDATA*)malloc(sizeof(OEDATA));
					m_pOutlookDataHead = OutlookData;
				}
				else
				{
					OutlookData = (OEDATA*)realloc(m_pOutlookDataHead, m_nOutlookCount * sizeof(OEDATA));
					OutlookData += (m_nOutlookCount - 1);
				}

				strcpy_s(OutlookData->POPuser, sizeof(Data), (char*)Data);
				ZeroMemory(Data, sizeof(Data));
				strcpy_s(OutlookData->POPserver, 7, "Hotmail");
				size = sizeof(Data);
				if(RegQueryValueEx(hkey2, "HTTPMail Password2" , 0, &type, Data1, &size) == ERROR_SUCCESS)
				{
					int pass = 0;
					for(DWORD i = 2; i < size; i++)
					{
						if(IsCharAlphaNumeric(Data1[i])||(Data1[i]=='(')||(Data1[i]==')')||(Data1[i]=='.')||(Data1[i]==' ')||(Data1[i]=='-'))
						{
							OutlookData->POPpass[pass] = Data1[i];
						}
					}
					pass++;
					OutlookData->POPpass[pass]=0;
				}
				ZeroMemory(Data1, sizeof(Data));
				oIndex++;
			}
			else if(RegQueryValueEx(hkey2, "POP3 User Name" , 0, &type, Data, &size) == ERROR_SUCCESS)
			{
				++m_nOutlookCount;
				if (m_pOutlookDataHead == NULL)
				{
					OutlookData = (OEDATA*)malloc(sizeof(OEDATA));
					m_pOutlookDataHead = OutlookData;
				}
				else
				{
					m_pOutlookDataHead = OutlookData = (OEDATA*)realloc(m_pOutlookDataHead, m_nOutlookCount * sizeof(OEDATA));
					OutlookData += (m_nOutlookCount - 1);
				}

				lstrcpy(OutlookData->POPuser,(char*)Data);
				ZeroMemory(Data,sizeof(Data));
				size = sizeof(Data);
				RegQueryValueEx (hkey2, "POP3 Server" , 0, &type, Data, &size);
				lstrcpy(OutlookData->POPserver,(char*)Data);
				ZeroMemory(Data,sizeof(Data));
				size = sizeof(Data);
				if(RegQueryValueEx(hkey2, "POP3 Password2" , 0, &type, Data1, &size) == ERROR_SUCCESS)
				{
					int pass = 0;
					for(DWORD i = 2; i < size; i++)
					{
						if(IsCharAlphaNumeric(Data1[i])||(Data1[i]=='(')||(Data1[i]==')')||(Data1[i]=='.')||(Data1[i]==' ')||(Data1[i]=='-'))
						{
							OutlookData->POPpass[pass] = Data1[i];
							pass++;
						}
					}
					OutlookData->POPpass[pass] = 0;
				}
			}

			RegCloseKey(hkey2);
		}

		ZeroMemory(Data1, sizeof(Data1));
		oIndex++;

		j++;
		i++;
	}

	RegCloseKey(hkey);

	return TRUE;
}

BOOL ProtectedStorage::EnumProtectedStorage(void)
{
	IPStorePtr pStore; 
	IEnumPStoreTypesPtr EnumPStoreTypes;
	IEnumPStoreTypesPtr EnumSubTypes;
	IEnumPStoreItemsPtr spEnumItems;
	PSTORECREATEINSTANCE pPStoreCreateInstance;
	
	HMODULE hpsDLL = LoadLibrary("pstorec.dll");
	HRESULT hr;
	GUID TypeGUID, subTypeGUID;
	LPWSTR itemName;
	unsigned long psDataLen = 0;
	unsigned char *psData = NULL;
	int i = 0;
	char szItemName[512];       
	char szItemData[512];
	char szResName[512];
	char szResData[512];
	char szItemGUID[50];
	char szTemp[256];

	pPStoreCreateInstance = (PSTORECREATEINSTANCE)GetProcAddress(hpsDLL, "PStoreCreateInstance");
	if (pPStoreCreateInstance == NULL)
	{
		printf("Unable to obtain handle to PStoreCreateInstance in pstorec.dll\n");
		return FALSE;
	}

	hr = pPStoreCreateInstance(&pStore, 0, 0, 0); 
	if (FAILED(hr))
	{
		printf("Unable to create protected storage instance (error code %X)\n", hr);
		return FALSE;
	}

	hr = pStore->EnumTypes(0, 0, &EnumPStoreTypes);
	if (FAILED(hr))
	{
		printf("Unable to enumerate protected storage types (error code %X)\n", hr);
		return FALSE;
	}

	while(EnumPStoreTypes->raw_Next(1, &TypeGUID, 0) == S_OK)
	{
		wsprintf(szItemGUID, "%x", TypeGUID);
		hr = pStore->EnumSubtypes(0, &TypeGUID, 0, &EnumSubTypes);
		if (FAILED(hr))
		{
			printf("Unable to enumerate protected storage subtypes for GUID %S (error code %X)\n", szItemGUID, hr);
			continue;
		}

		while(EnumSubTypes->raw_Next(1, &subTypeGUID, 0) == S_OK)
		{
			hr = pStore->EnumItems(0, &TypeGUID, &subTypeGUID, 0, &spEnumItems);
			if (FAILED(hr))
			{
				printf("Unable to enumerate protected storage items for GUID %S (error code %X)\n", szItemGUID, hr);
				continue;
			}

			while(spEnumItems->raw_Next(1,&itemName,0) == S_OK)
			{             
				_PST_PROMPTINFO *pstiinfo = NULL;
				psDataLen = 0;
				psData = NULL;

				wsprintf(szItemName, "%ws", itemName);			 
				hr = pStore->ReadItem(0, &TypeGUID, &subTypeGUID, itemName, &psDataLen, &psData, pstiinfo, 0);
				if (FAILED(hr))
				{
					printf("Unable to read protected storage item %S (error code %X)\n", szItemName, hr);
					continue;
				}

				if(strlen((char*)psData) < (psDataLen - 1))
				{
					i = 0;
					for(DWORD m = 0; m < psDataLen; m += 2)
					{
						if(psData[m] == 0)
							szItemData[i] = ',';
						else
							szItemData[i] = psData[m];
						
						i++;
					}

					if (i > 0)
						szItemData[i - 1] = 0;	
					else
						szItemData[0] = 0;
				}
				else 
				{		  				  
					wsprintf(szItemData, "%s", psData);				  
				}	

				strcpy_s(szResName, 512, "");
				strcpy_s(szResData, 512, "");

				if(_stricmp(szItemGUID, "220d5cc1") == 0)
				{
					// GUIDs beginning with "220d5cc1" are Outlook Express
					BOOL bDeletedOEAccount = TRUE;		
					for(i = 0; i < m_nOutlookCount; i++)
					{				  
						if(strcmp(m_pOutlookDataHead[i].POPpass, szItemName) == 0)
						{				   			
							bDeletedOEAccount = FALSE;
							printf(OUTPUT_FORMAT, m_pOutlookDataHead[i].POPserver, "Outlook Express Account", m_pOutlookDataHead[i].POPuser, szItemData);
							break;
						}
					}

					if(bDeletedOEAccount)
						printf(OUTPUT_FORMAT, szItemName, "Deleted Outlook Express Account", m_pOutlookDataHead[i].POPuser, szItemData);
				}				 
				else if(_stricmp(szItemGUID, "5e7e8100") == 0)
				{				  
					// GUIDs beginning with 5e7e8100 are IE password-protected sites

					strcpy_s(szTemp, 512, "");

					// If the item begins with DPAPI, it has been protected using the CryptProtectData call.
					// Decrypt it using the opposite call. This is a HUGE assumption on my part, but so far
					// appears to be the case
					if (strncmp(szItemName, "DPAPI:", 6) == 0)
					{
						char* szDecryptedPassword = DecryptData(psDataLen, psData);

						if (szDecryptedPassword != NULL)
						{
							char szUser[200];
							memset(szUser, 0, 200);

							// Also have to figure out the user name. This section may need some work
							if (strncmp(szItemName + 7, "ftp://", 6) == 0)
							{
								size_t nPos = strcspn(szItemName + 13, "@");
								if (nPos > 0 && nPos < strlen(szItemName + 13))
								{
									// Found the @ sign - copy everything between ftp:// and the @ sign
									strncpy_s(szUser, 200, szItemName + 13, nPos);
								}
								else
								{
									strcpy_s(szUser, 200, szItemName + 13);
								}
							}
							else
							{
								// Just copy user name verbatim I guess
								strcpy_s(szUser, 200, szItemName);
							}

							printf(OUTPUT_FORMAT, szItemName, "IE Password-Protected Site", szUser, szDecryptedPassword);
							free(szDecryptedPassword);
						}
						else
						{
							printf(OUTPUT_FORMAT, szItemName, "IE Password-Protected Site", szItemName, "ERROR DECRYPTING");
							//printf("Decryption error for item %s: error %d\n", szItemName, GetLastError());
						}
					}
					else if(strstr(szItemData, ":") != 0)
					{
						strcpy_s(szTemp, 512, strstr(szItemData, ":") + 1);
						*(strstr(szItemData, ":")) = 0;				  
						printf(OUTPUT_FORMAT, szItemName, "IE Password-Protected Site", szItemData, szTemp);
					}

				}
				else if(_stricmp(szItemGUID, "b9819c52") == 0)
				{
					// GUIDs beginning with b9819c52 are MSN Explorer Signup
					char msnid[100];
					char msnpass[100];
					BOOL first = TRUE;
					char *p;

					for(DWORD m = 0; m < psDataLen; m += 2)
					{
						if(psData[m] == 0)
						{									
							szItemData[i] = ',';					
							i++;
						}
						else
						{
							if(IsCharAlphaNumeric(psData[m])||(psData[m]=='@')||(psData[m]=='.')||(psData[m]=='_'))
							{
								szItemData[i] = psData[m];					
								i++;
							}							
						}
					}

					szItemData[i - 1] = 0;
					p = szItemData + 2;

					//psData[4] - number of msn accounts 
					for(int ii = 0; ii < psData[4]; ii++)
					{
						strcpy_s(msnid, 100, p + 1);

						if(strstr(msnid,",") != 0) 
							*strstr(msnid,",") = 0;

						if(strstr(p + 1, ",") != 0)
							strcpy_s(msnpass, 100, strstr(p + 1, ",") + 2);	

						if(strstr(msnpass, ",") != 0) 
							*strstr(msnpass, ",") = 0;	

						p = strstr(p + 1, ",") + 2 + strlen(msnpass) + 7;

						printf(OUTPUT_FORMAT, msnid, "MSN Explorer Signup", msnid, msnpass);
					}
				}
				else if(_stricmp(szItemGUID, "e161255a") == 0)
				{
					// GUIDs beginning with e161255a are other stored IE credentials 
					if(strstr(szItemName, "StringIndex") == 0)
					{
						if(strstr(szItemName, ":String") != 0) 
							*strstr(szItemName, ":String") = 0;

						strncpy_s(szTemp, 512, szItemName, 8);			  
						if((strstr(szTemp, "http:/") == 0) && (strstr(szTemp, "https:/") == 0))
							printf(OUTPUT_FORMAT, szItemName, "IE Auto Complete Fields", szItemData, "");
						else
						{
							strcpy_s(szTemp, 512, "");
							if(strstr(szItemData, ",") != 0)
							{
								strcpy_s(szTemp, 512, strstr(szItemData, ",") + 1);
								*(strstr(szItemData, ",")) = 0;				  
							}
					
							printf(OUTPUT_FORMAT, szItemName, "AutoComplete Passwords", szItemData, szTemp);				
						}
					}
				}
				else if(_stricmp(szItemGUID, "89c39569") == 0)
				{
					// IdentitiesPass info. It's already been displayed, so just supress these
				}
				else
				{
					// Catch-all for miscellaneous data
					strcpy_s(szTemp, 512, "");
					if(strstr(szItemData, ":") != 0)
					{
						strcpy_s(szTemp, 512, strstr(szItemData, ":") + 1);
						*(strstr(szItemData, ":")) = 0;				  
					}

					printf(OUTPUT_FORMAT, szItemName, "Unknown", szItemData, szTemp);
				}
				ZeroMemory(szItemName, sizeof(szItemName));
				ZeroMemory(szItemData, sizeof(szItemData));		
			}
		}
	}

	return TRUE;
}

char* ProtectedStorage::DecryptData(int nDataLen, BYTE* pData)
{
	DATA_BLOB DataOut;
	DATA_BLOB DataIn;
	char* pReturn = NULL;

	// Set up the input data structure
	DataIn.cbData = nDataLen;
	DataIn.pbData = (BYTE*)malloc(nDataLen);
	memcpy(DataIn.pbData, pData, nDataLen);

	if (CryptUnprotectData(&DataIn,	NULL, NULL, NULL, NULL, 0, &DataOut))
	{
		pReturn = (char*)malloc(DataOut.cbData + 1);
		memset(pReturn, 0, DataOut.cbData + 1);
		_snprintf_s(pReturn, DataOut.cbData, sizeof(DataOut.pbData), "%S", DataOut.pbData);

		//memcpy(pReturn, DataOut.pbData, DataOut.cbData);
		//pReturn[DataOut.cbData + 1] = 0;

		free(DataIn.pbData);
		return pReturn;
	}
	else
	{
		free(DataIn.pbData);
		return NULL;
	}
}

BOOL ProtectedStorage::EnumerateIdentities(void)
{
	const char* IDENTITIES_KEY = "Identities";
	
	int oIndex = 0;
	HKEY hkey, hkey2;
	long nEnumResult = 0;
	char name[MAX_KEY_SIZE], skey[MAX_KEY_SIZE];
	DWORD dwKeyLen;
	FILETIME timeLastWrite;
	BYTE Data[150];
	DWORD size;
	int j = 0, i = 0;
	LONG lResult;
	DWORD type = REG_BINARY;

	lResult = RegOpenKeyEx(HKEY_CURRENT_USER, IDENTITIES_KEY, 0, KEY_ALL_ACCESS, &hkey);

	if(ERROR_SUCCESS != lResult)
	{
		printf("Unable to enumerate Outlook Express accounts: could not open HKCU\\%s\n", IDENTITIES_KEY);
		return FALSE;
	}
		
	dwKeyLen = MAX_KEY_SIZE;
	nEnumResult = RegEnumKeyEx(hkey, i++, name, &dwKeyLen, NULL, NULL, NULL, &timeLastWrite);

	while(nEnumResult != ERROR_NO_MORE_ITEMS)
	{
		strncpy_s(skey, MAX_KEY_SIZE, IDENTITIES_KEY, MAX_KEY_SIZE - strlen(IDENTITIES_KEY) - 2);
		strcat_s(skey, MAX_KEY_SIZE, "\\");
		strcat_s(skey, MAX_KEY_SIZE, name);

		lResult = RegOpenKeyEx(HKEY_CURRENT_USER, (LPCTSTR)skey, 0, KEY_ALL_ACCESS, &hkey2);
		if (lResult == ERROR_SUCCESS)
		{
			size = sizeof(Data);
			
			if(RegQueryValueEx(hkey2, "Username" , 0, &type, Data, &size) == ERROR_SUCCESS)
			{
				// Output the value
				printf(OUTPUT_FORMAT, "Identity", "IdentitiesPass", (char*)Data, "");
			}

			RegCloseKey(hkey2);
		}

		nEnumResult = RegEnumKeyEx(hkey, i++, name, &dwKeyLen, NULL, NULL, NULL, &timeLastWrite);
	}

	RegCloseKey(hkey);

	return TRUE;
}
