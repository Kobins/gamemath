#include "Precompiled.h"
using namespace CK::DDD;

#include "ThirdParty/MMD/EncodingHelper.h"
#include "ThirdParty/MMD/Pmx.h"

bool PMXLoader::Load(GameEngine& InGameEngine, Mesh& InMesh, const std::wstring& InFileName) {
	// PMX 불러오기
	oguna::EncodingConverter converter = oguna::EncodingConverter();

	pmx::PmxModel x;
	std::filebuf fb;
	if (!fb.open(InFileName, std::ios::in | std::ios::binary)) {
		return false;
	}

	std::istream is(&fb);
	x.Read(&is);

	// 스킨드 메시로 설정
	InMesh.SetMeshType(MeshType::Skinned);

	auto& v = InMesh.GetVertices();
	auto& i = InMesh.GetIndices();
	auto& uv = InMesh.GetUVs();
	auto& ti = InMesh.GetTextureIndices();
	auto& bones = InMesh.GetBones();
	auto& cb = InMesh.GetConnectedBones();
	auto& w = InMesh.GetWeights();

	// 본 불러오기
	std::vector<std::wstring> boneBuffer; boneBuffer.reserve(x.bone_count);
	bones.reserve(x.bone_count);
	for (size_t ix = 0; ix < x.bone_count; ++ix) {
		// 본 구성
		auto& boneData = x.bones[ix];
		const std::wstring& boneName = x.bones[ix].bone_name;
		Vector3 bonePosition = Vector3(x.bones[ix].position[0], x.bones[ix].position[1], x.bones[ix].position[2]);
		bones.insert({ boneName, Bone(boneName, Transform(bonePosition)) });

		// 부모 구성 - 기존에 불러와진 본 이름 버퍼 기반으로 가져오기
		boneBuffer.push_back(boneName);
		size_t index = static_cast<size_t>(x.bones[ix].parent_index);
		if (boneBuffer.size() <= index) {
			// 부모 인덱스 정보가 잘못됨 -> 부모보다 먼저 불러와진 자식 ..?
			continue;
		}
		std::wstring parentBoneName = boneBuffer[index];
		Bone& b = InMesh.GetBone(boneName);
		b.SetParent(InMesh.GetBone(parentBoneName));
		b.SetProperty(boneData.bone_flag);
	}

	// 정점 버퍼 구성(정점, UV, 본 연결)
	v.reserve(x.vertex_count);
	uv.reserve(x.vertex_count);
	cb.reserve(x.vertex_count);
	w.reserve(x.vertex_count);
	for (size_t ix = 0; ix < x.vertex_count; ++ix) {
		v.emplace_back(Vector3(x.vertices[ix].positon[0], x.vertices[ix].positon[1], x.vertices[ix].positon[2]));

		// UV는 y축 반전됨
		uv.emplace_back(Vector2(x.vertices[ix].uv[0], 1.f - x.vertices[ix].uv[1]));

		// 본 연결 정보 구성
		pmx::PmxVertexSkinningType type = x.vertices[ix].skinning_type;
		switch (type) {
		case pmx::PmxVertexSkinningType::BDEF1: // 연결된 본이 1개
		{
			cb.push_back(1);
			pmx::PmxVertexSkinningBDEF1* s = static_cast<pmx::PmxVertexSkinningBDEF1*>(x.vertices[ix].skinning.get());
			std::wstring boneKey = boneBuffer[s->bone_index];
			Weight newWeight{ { boneKey }, { 1.f } };
			w.emplace_back(newWeight);
			break;
		}
		case pmx::PmxVertexSkinningType::BDEF2: // 연결된 본이 2개
		{
			cb.push_back(2);
			pmx::PmxVertexSkinningBDEF2* s = static_cast<pmx::PmxVertexSkinningBDEF2*>(x.vertices[ix].skinning.get());
			std::wstring boneKey1 = boneBuffer[s->bone_index1];
			std::wstring boneKey2 = boneBuffer[s->bone_index2];
			float boneWeight1 = s->bone_weight;
			Weight newWeight{ 
				{ boneKey1, boneKey2 }, 
				// 가중치 합 1이니까, 2번째 본의 가중치는 1 - 가중치로 구성
				{ boneWeight1, 1.f - boneWeight1 }
			};
			w.emplace_back(newWeight);
			break;
		}
		case pmx::PmxVertexSkinningType::BDEF4:
		{
			cb.push_back(4);
			pmx::PmxVertexSkinningBDEF4* s = static_cast<pmx::PmxVertexSkinningBDEF4*>(x.vertices[ix].skinning.get());
			const std::wstring& boneKey1 = boneBuffer[s->bone_index1];
			const std::wstring& boneKey2 = boneBuffer[s->bone_index2];
			const std::wstring& boneKey3 = boneBuffer[s->bone_index3];
			const std::wstring& boneKey4 = boneBuffer[s->bone_index4];
			float boneWeight1 = s->bone_weight1;
			float boneWeight2 = s->bone_weight2;
			float boneWeight3 = s->bone_weight3;
			float boneWeight4 = s->bone_weight4;
			Weight newWeight{ 
				{ boneKey1, boneKey2, boneKey3, boneKey4 },
				{ boneWeight1, boneWeight2, boneWeight3, boneWeight4 },
			};
			w.emplace_back(newWeight);
			break;
		}
		default:
		{
			cb.push_back(0);
			Weight newWeight;
			w.emplace_back(newWeight);
			break;
		}
		}

	}

	i.reserve(x.index_count);
	// 인덱스 버퍼 구성
	for (size_t ix = 0; ix < x.index_count; ++ix) {
		i.emplace_back(x.indices[ix]);
	}

	// 텍스처 구성
	size_t textureId = 0;
	for (size_t ix = 0; ix < x.texture_count; ++ix) {
		std::wstring path = x.textures[ix];
		path = L".\\" + path;
		Texture& texture = InGameEngine.CreateTexture(textureId++, path);
		assert(texture.IsIntialized());
	}

	// TexturesIndice 구성
	// 같은 텍스처를 사용하는 삼각형들의 index 범위
	ti.reserve(x.material_count);
	size_t cnt = 0;
	for (size_t ix = 0; ix < x.material_count; ++ix) {
		auto& m = x.materials[ix];

		size_t c = m.index_count;
		ti.emplace_back(TexturesIndice(m.diffuse_texture_index, cnt, cnt + c));
		cnt += c;
	}
}