/*
 * Bink API shim for Linux (FFmpeg backend in bink_ffmpeg.cpp).
 */
#ifndef BINK_LINUX_SHIM_H
#define BINK_LINUX_SHIM_H

typedef struct BINK {
	unsigned long Width;
	unsigned long Height;
	unsigned long Frames;
	unsigned long FrameNum;
	unsigned long FrameRate;
	unsigned long FrameRateDiv;
} BINK;

typedef BINK *HBINK;

#define BINKSURFACE565 0x00008000u
#define BINKCOPYNOSCALING 0x00000001u

#ifdef __cplusplus
extern "C" {
#endif

void BinkSoundUseDirectSound(long onoff);
HBINK BinkOpen(const char *name, unsigned long flags);
void BinkClose(HBINK bink);
long BinkWait(HBINK bink);
void BinkDoFrame(HBINK bink);
void BinkCopyToBuffer(HBINK bink, void *dest, long destpitch, unsigned long destheight,
	unsigned long destx, unsigned long desty, unsigned long flags);
void BinkNextFrame(HBINK bink);

#ifdef __cplusplus
}
#endif

#endif
