#pragma once
#include "Mesh.h"

class HeightMap : public Mesh {
public:
	HeightMap(const std::string& name);
	~HeightMap();

	Vector3 getHeightmapSize() const;

protected:
	Vector3 _heightMapSize;
};