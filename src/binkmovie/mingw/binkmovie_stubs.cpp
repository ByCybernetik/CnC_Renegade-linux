/*
** MinGW stubs until ww3d2 + Bink SDK are available.
** Replaces BINKMovie.cpp and subtitlemanager.cpp (both require ww3d2).
*/

#include "BINKMovie.h"

void BINKMovie::Play(const char *, const char *, FontCharsClass *)
{
}

void BINKMovie::Stop()
{
}

void BINKMovie::Update()
{
}

void BINKMovie::Render()
{
}

void BINKMovie::Init()
{
}

void BINKMovie::Shutdown()
{
}

bool BINKMovie::Is_Complete()
{
	return true;
}

bool BINKMovie::Is_Playing()
{
	return false;
}

unsigned long BINKMovie::Get_Ms_Per_Frame()
{
	return 0;
}

bool BINKMovie::Should_Present_Frame()
{
	return false;
}
