#pragma once

namespace CK
{

namespace DDD
{

struct PMXLoader
{
	static bool Load(GameEngine& InGameEngine, Mesh& InMesh, const std::wstring& InFileName);
};

}

}