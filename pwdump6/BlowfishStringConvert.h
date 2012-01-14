
	void ConvertToBlowfishLongs(char* string, DWORD* L, DWORD* R)
	{
		//int *data = string;

		*L = *((int*)(string + 0));
		*R = *((int*)(string + 4)); 
	}

	void ConvertFromBlowfishLongs(DWORD L, DWORD R, char* string)
	{
		*((int*)(string +0)) = L;
		*((int*)(string +4)) = R; 
	}

