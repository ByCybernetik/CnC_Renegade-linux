#include "win.h"
#include <cstring>

bool Get_Image_File_Header(LPCSTR, IMAGE_FILE_HEADER *file_header)
{
	if (file_header) {
		memset(file_header, 0, sizeof(*file_header));
	}
	return false;
}
