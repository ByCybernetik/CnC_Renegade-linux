#include "audio_decode.h"
#include "ffactory.h"

static FileFactoryClass *g_audio_file_factory = NULL;

void audio_set_file_factory(FileFactoryClass *factory)
{
	g_audio_file_factory = factory;
}

void audio_set_miles_file_callbacks(
	audio_file_open_fn open_fn,
	audio_file_close_fn close_fn,
	audio_file_seek_fn seek_fn,
	audio_file_read_fn read_fn)
{
	(void)open_fn;
	(void)close_fn;
	(void)seek_fn;
	(void)read_fn;
}
