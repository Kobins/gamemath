
#include "Precompiled.h"
using namespace CK::DDD;

// �� ��Ī
const std::wstring GameEngine::RootBone(L"RootBone");
const std::wstring GameEngine::PelvisBone(L"PelvisBone");
const std::wstring GameEngine::SpineBone(L"SpineBone");
const std::wstring GameEngine::LeftArmBone(L"LeftArmBone");
const std::wstring GameEngine::RightArmBone(L"RightArmBone");
const std::wstring GameEngine::NeckBone(L"NeckBone");
const std::wstring GameEngine::LeftLegBone(L"LeftLegBone");
const std::wstring GameEngine::RightLegBone(L"RightLegBone");

// �޽�
const std::size_t GameEngine::CharacterMesh = std::hash<std::wstring>()(L"SK_CKMan");
const std::size_t GameEngine::ArrowMesh = std::hash<std::wstring>()(L"SM_Arrow");
const std::size_t GameEngine::PlaneMesh = std::hash<std::wstring>()(L"SM_Plane");

// �ؽ�ó
const std::size_t GameEngine::BaseTexture = std::hash<std::wstring>()(L"Base");
const std::wstring GameEngine::CharacterTexturePath(L"CKMan.png");

const std::wstring GameEngine::MMDCharacterPath(L"Character.pmx");

struct GameObjectCompare
{
	bool operator()(const std::unique_ptr<GameObject>& lhs, std::size_t rhs)
	{
		return lhs->GetHash() < rhs;
	}
};

void GameEngine::OnScreenResize(const ScreenPoint& InScreenSize)
{
	// ȭ�� ũ���� ����
	_ScreenSize = InScreenSize;
	_MainCamera.SetViewportSize(_ScreenSize);
}

bool GameEngine::Init()
{
	// �̹� �ʱ�ȭ�Ǿ� ������ �ʱ�ȭ �������� ����.
	if (_IsInitialized)
	{
		return true;
	}

	// ȭ�� ũ�Ⱑ �ùٷ� �����Ǿ� �ִ��� Ȯ��
	if (_ScreenSize.HasZero())
	{
		return false;
	}

	if (!_InputManager.IsInputReady())
	{
		return false;
	}

	if (!LoadResources())
	{
		return false;
	}

	_IsInitialized = true;
	return _IsInitialized;
}

bool GameEngine::LoadResources()
{
	// ȭ��ǥ �޽� (���� ǥ�� �뵵)
	Mesh& arrow = CreateMesh(GameEngine::ArrowMesh);
	arrow.GetVertices().resize(arrowPositions.size());
	arrow.GetIndices().resize(arrowIndice.size());
	arrow.GetColors().resize(arrowPositions.size());
	std::copy(arrowPositions.begin(), arrowPositions.end(), arrow.GetVertices().begin());
	std::copy(arrowIndice.begin(), arrowIndice.end(), arrow.GetIndices().begin());
	std::fill(arrow.GetColors().begin(), arrow.GetColors().end(), LinearColor::Gray);

	// �ٴ� �޽� (����� ��)
	int planeHalfSize = 3;
	Mesh& plane = CreateMesh(GameEngine::PlaneMesh);
	for (int z = -planeHalfSize; z <= planeHalfSize; z++)
	{
		for (int x = -planeHalfSize; x <= planeHalfSize; x++)
		{
			plane.GetVertices().push_back(Vector3((float)x, 0.f, (float)z));
		}
	}

	int xIndex = 0;
	for (int tx = 0; tx < planeHalfSize * 2; tx++)
	{
		for (int ty = 0; ty < planeHalfSize * 2; ty++)
		{
			int v0 = xIndex + tx + (planeHalfSize * 2 + 1) * (ty + 1);
			int v1 = xIndex + tx + (planeHalfSize * 2 + 1) * ty;
			int v2 = v1 + 1;
			int v3 = v0 + 1;
			std::vector<size_t> quad = { (size_t)v0, (size_t)v2, (size_t)v1, (size_t)v0, (size_t)v3, (size_t)v2 };
			plane.GetIndices().insert(plane.GetIndices().end(), quad.begin(), quad.end());
		}
	}

	// �ؽ��� �ε�
	Texture& diffuseTexture = CreateTexture(GameEngine::BaseTexture, GameEngine::CharacterTexturePath);
	assert(diffuseTexture.IsIntialized());

	
	// ĳ���� �޽� ���� - MMD ��
	Mesh& characterMesh = CreateMesh(GameEngine::CharacterMesh);
	PMXLoader::Load(*this, characterMesh, GameEngine::MMDCharacterPath);
	

	return true;
}

Mesh& GameEngine::CreateMesh(const std::size_t& InKey)
{
	auto meshPtr = std::make_unique<Mesh>();
	_Meshes.insert({ InKey, std::move(meshPtr) });
	return *_Meshes.at(InKey).get();
}

Texture& GameEngine::CreateTexture(const std::size_t& InKey, const std::string& InTexturePath)
{
	auto texturePtr = std::make_unique<Texture>(InTexturePath);
	_Textures.insert({ InKey, std::move(texturePtr) });
	return *_Textures.at(InKey).get();
}

Texture& GameEngine::CreateTexture(const std::size_t& InKey, const std::wstring& InTexturePath) {
	auto texturePtr = std::make_unique<Texture>(InTexturePath);
	_Textures.insert({ InKey, std::move(texturePtr) });
	return *_Textures.at(InKey).get();
}

GameObject& GameEngine::CreateNewGameObject(const std::wstring& InName)
{
	std::size_t inHash = std::hash<std::wstring>()(InName);
	const auto it = std::lower_bound(SceneBegin(), SceneEnd(), inHash, GameObjectCompare());

	auto newGameObject = std::make_unique<GameObject>(InName);
	if (it != _Scene.end())
	{
		std::size_t targetHash = (*it)->GetHash();
		if (targetHash == inHash)
		{
			// �ߺ��� Ű �߻�. ����.
			assert(false);
			return GameObject::Invalid;
		}
		else if (targetHash < inHash)
		{
			_Scene.insert(it + 1, std::move(newGameObject));
		}
		else
		{
			_Scene.insert(it, std::move(newGameObject));
		}
	}
	else
	{
		_Scene.push_back(std::move(newGameObject));
	}

	return GetGameObject(InName);
}

GameObject& GameEngine::GetGameObject(const std::wstring& InName)
{
	std::size_t targetHash = std::hash<std::wstring>()(InName);
	const auto it = std::lower_bound(SceneBegin(), SceneEnd(), targetHash, GameObjectCompare());

	return (it != _Scene.end()) ? *(*it).get() : GameObject::Invalid;
}
