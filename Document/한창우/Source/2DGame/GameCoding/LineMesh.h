#pragma once
#include "ResourceBase.h"

class LineMesh : public ResourceBase
{
public:
	void Save(wstring path);
	void Load(wstring path);

	void Render(HDC hdc, Pos pos, float ratioX = 1.f, float ratioY = 1.f) const;

protected:
	vector<pair<POINT, POINT>> _lines;
	int32 _width = 0;
	int32 _height = 0;
};

