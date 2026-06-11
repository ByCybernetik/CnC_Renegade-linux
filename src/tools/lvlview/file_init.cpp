#include "file_init.h"

#include "ffactory.h"
#include "ffactorylist.h"
#include "mixfile.h"
#include "win32_minimal.h"

#include <cstdio>
#include <cstring>
#include <vector>

#if !defined(_WIN32)
#include <dirent.h>
#include <strings.h>
#endif

static SimpleFileFactoryClass g_writing_factory;
static SimpleFileFactoryClass g_base_factory;
static FileFactoryListClass g_file_factory_list;
static std::vector<MixFileFactoryClass *> g_mix_factories;

#if defined(RENEGADE_LINUX)
static const char *DATA_SUBDIRECTORY = "Data\\";
static const char *SAVE_SUBDIRECTORY = "Data\\save\\";
static const char *CONFIG_SUBDIRECTORY = "Data\\config\\";
#else
static const char *DATA_SUBDIRECTORY = "DATA\\";
static const char *SAVE_SUBDIRECTORY = "DATA\\SAVE\\";
static const char *CONFIG_SUBDIRECTORY = "DATA\\CONFIG\\";
#endif

static int Name_Compare_I(const char *a, const char *b)
{
#if defined(_WIN32)
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

static bool Is_Mix_Archive_Filename(const char *filename)
{
	if (filename == nullptr || filename[0] == '\0') {
		return false;
	}
	const size_t len = strlen(filename);
	if (len < 5) {
		return false;
	}
	const char *ext = filename + len - 4;
	return Name_Compare_I(ext, ".mix") == 0 || Name_Compare_I(ext, ".dat") == 0 ||
		Name_Compare_I(ext, ".dbs") == 0;
}

static void Add_Mix_File(const char *filename)
{
	if (!Is_Mix_Archive_Filename(filename)) {
		return;
	}

	MixFileFactoryClass *mix =
		new MixFileFactoryClass(filename, &g_base_factory);
	g_mix_factories.push_back(mix);
	g_file_factory_list.Add_FileFactory(mix, filename);
}

#if defined(RENEGADE_LINUX)
static void Scan_Mix_Files_In_Dir(const char *dir_path)
{
	DIR *dir = opendir(dir_path);
	if (dir == nullptr) {
		return;
	}

	while (struct dirent *entry = readdir(dir)) {
		if (entry->d_name[0] == '.') {
			continue;
		}
		if (!Is_Mix_Archive_Filename(entry->d_name)) {
			continue;
		}
		/* always.dat / always.dbs are registered explicitly above. */
		if (Name_Compare_I(entry->d_name, "always.dat") == 0 ||
			Name_Compare_I(entry->d_name, "always.dbs") == 0) {
			continue;
		}
		Add_Mix_File(entry->d_name);
	}
	closedir(dir);
}
#endif

bool Lvlview_Init_File_Factory(const char *data_dir)
{
	if (data_dir == nullptr || data_dir[0] == '\0') {
		data_dir = ".";
	}

	g_writing_factory.Set_Sub_Directory(DATA_SUBDIRECTORY);
	_TheWritingFileFactory = &g_writing_factory;

	g_base_factory.Set_Sub_Directory("");
	g_base_factory.Append_Sub_Directory(DATA_SUBDIRECTORY);
	g_base_factory.Append_Sub_Directory(SAVE_SUBDIRECTORY);
	g_base_factory.Append_Sub_Directory(CONFIG_SUBDIRECTORY);

	_TheSimpleFileFactory->Set_Sub_Directory("");
	_TheSimpleFileFactory->Append_Sub_Directory(DATA_SUBDIRECTORY);
	_TheSimpleFileFactory->Append_Sub_Directory(SAVE_SUBDIRECTORY);
	_TheSimpleFileFactory->Append_Sub_Directory(CONFIG_SUBDIRECTORY);
	_TheSimpleFileFactory->Set_Strip_Path(true);

	g_file_factory_list.Add_FileFactory(&g_base_factory, "");

	Add_Mix_File("always.dbs");
	Add_Mix_File("always.dat");

#if defined(RENEGADE_LINUX)
	Scan_Mix_Files_In_Dir("Data");
	Scan_Mix_Files_In_Dir(data_dir);
	Scan_Mix_Files_In_Dir(".");
#else
	WIN32_FIND_DATA find_info = {0};
	HANDLE file_find = ::FindFirstFile("data\\*.mix", &find_info);
	if (file_find != INVALID_HANDLE_VALUE) {
		do {
			Add_Mix_File(find_info.cFileName);
		} while (::FindNextFile(file_find, &find_info));
		::FindClose(file_find);
	}
#endif

	_TheFileFactory = &g_file_factory_list;
	return true;
}

void Lvlview_Shutdown_File_Factory()
{
	for (size_t i = 0; i < g_mix_factories.size(); ++i) {
		delete g_mix_factories[i];
	}
	g_mix_factories.clear();
}
