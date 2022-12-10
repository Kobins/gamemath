#pragma once

namespace CK
{
// 키프레임 정의
template<typename TValue>
struct Keyframe {
	float Time;		// 시간
	float Tangent;	// 탄젠트(기울기)
	TValue Value;	// 값

	// to로부터의 상대 y1값을 도출해내는 함수
	float GetDeltaValue(const Keyframe<TValue>& to) {
		const auto& from = *this;
		return to.Value - from.Value;
	}
	// 상대 y값을 적용하는 함수
	TValue GetValue(float value, const Keyframe<TValue>& to) {
		const auto& from = *this;
		return from.Value + value;
	}
};

// float에 대해 인스턴스화 (사실 다른 건 없음 ㅎ)
template<>
struct Keyframe<float> {
	float Time;
	float Tangent;
	float Value;
	float GetDeltaValue(const Keyframe<float>& to) {
		const auto& from = *this;
		return to.Value - from.Value;
	}
	float GetValue(float value, const Keyframe<float>& to) {
		const auto& from = *this;
		return from.Value + value;
	}
};

// Vector3에 대해 인스턴스화
template<>
struct Keyframe<Vector3> {
	float Time;
	float Tangent;
	Vector3 Value;
	// 거리를 통해 y1 산출
	float GetDeltaValue(const Keyframe<Vector3>& to) {
		const auto& from = *this;
		return (to.Value - from.Value).Size();
	}
	// (to-from) 방향을 기준으로 y값 적용
	Vector3 GetValue(float value, const Keyframe<Vector3>& to) {
		const auto& from = *this;
		auto fromTo = (to.Value - from.Value);
		fromTo.Normalize();
		return from.Value + fromTo * value;
	}
};

// Quaternion에 대해 인스턴스화
template<>
struct Keyframe<Quaternion> {
	float Time;
	float Tangent;
	Quaternion Value;

	// 내적의 arccos를 통해 사잇각 구하기
	float GetDeltaValue(const Keyframe<Quaternion>& to) {
		const auto& from = *this;
		float angleInRadian = to.Value.Angle(from.Value);
		return angleInRadian;
	}
	// [0, 1] 구간으로 정규화해서 Slerp로 y값 적용
	Quaternion GetValue(float value, const Keyframe<Quaternion>& to) {
		const auto& from = *this;
		float angleInRadian = to.Value.Angle(from.Value);
		return Quaternion::Slerp(from.Value, to.Value, value / angleInRadian);
	}
};

// 삼차함수의 각 계수 베이킹
struct KeyframeCalculation {
	float a, b, c;
	// d = 0이므로 생략

	// x값에 대해 함수 계산 (ax^3 + bx^2 + cx)
	float Calculate(float InX) {
		float square = InX * InX;
		float cubic = square * InX;
		return a * cubic + b * square + c * InX;
	}
};

// 애니메이션 커브, 여러 키프레임으로 구성
template<typename TValue>
class AnimationCurve {
public:
	using Keyframe = Keyframe<TValue>;

	std::vector<Keyframe>& GetKeyframes() { return _Keyframes; }
	std::vector<KeyframeCalculation>& GetKeyframeCalculations() { return _KeyframeCalculation; }
	
	AnimationCurve(const std::initializer_list<Keyframe> InKeyframes) : _Keyframes(InKeyframes) {
		Calculate();
	}

	// 사전에 프레임 사이사이 베이킹
	void Calculate() {
		auto frameCount = _Keyframes.size();
		if (frameCount == 0) return;
		if (frameCount == 1) return; 
		// 시간순 정렬
		std::sort(_Keyframes.begin(), _Keyframes.end(), [&](Keyframe a, Keyframe b) -> bool {
			return a.Time < b.Time;
		});

		// 삼차함수 곡선 베이킹
		_KeyframeCalculation.reserve(frameCount - 1);
		_KeyframeCalculation.clear();
		for (int currentIndex = 0; currentIndex < frameCount - 1; ++currentIndex) {
			int nextIndex = currentIndex + 1;

			Keyframe& current = _Keyframes[currentIndex];
			Keyframe& next = _Keyframes[nextIndex];

			float x1 = next.Time - current.Time;
			float y1 = current.GetDeltaValue(next);

			// tan(alpha)
			float tan0 = current.Tangent;
			// tan(beta)
			float tan1 = next.Tangent;

			float x1Inv = 1.f / x1; // 1/x1
			float m = y1 * x1Inv; //  y1/x1
			float x1SqauredInv = 1.f / (x1 * x1); // 1/x1^2
			KeyframeCalculation calculation{
				(tan0 + tan1 - 2 * m) * (x1SqauredInv),
				(3 * m - 2 * tan0 - tan1) * x1Inv,
				tan0
			};
			_KeyframeCalculation.push_back(calculation);
		}
	}

private:
	std::vector<Keyframe> _Keyframes;
	std::vector<KeyframeCalculation> _KeyframeCalculation;
};

// 실제로 애니메이션을 적용할 때 사용하는 세션
// AnimationCurve를 레퍼런스로 가짐
template<typename TValue>
class AnimationSession {
public:
	using Curve = AnimationCurve<TValue>;
	using Keyframe = Keyframe<TValue>;


	AnimationSession(Curve& InCurve, bool InIsLooping = true) :
		_Curve(InCurve),
		_FrameCount(InCurve.GetKeyframes().size()),
		_Duration(InCurve.GetKeyframes()[_FrameCount - 1].Time),
		_IsLooping(InIsLooping),
		_Time(0.f),
		_Index(0)
	{
		if (InCurve.GetKeyframes().size() <= 0) {
			return;
		}
		_IsValid = true;
	}

	float GetTime() const { return _Time; }
	int GetIndex() const { return _Index; }

	bool IsLooping() const { return _IsLooping; }
	void SetLooping(bool InIsLooping) const { _IsLooping = InIsLooping; }

	// 매 프레임 호출되어 애니메이션 진행
	TValue Next(float InDeltaTime) {
		// 뭔가 잘못된 세션은 대충 기본값 반환
		if (!_IsValid) {
			return TValue();
		}

		auto& frames = _Curve.GetKeyframes();

		// _IsLooping == false인데 애니메이션이 다 흐른 경우 _IsEnded == true
		// 제일 마지막 키프레임의 값 반환
		if (_IsEnded) {
			return frames[_FrameCount - 1].Value;
		}

		// 일시정지 된 경우?
		if (!_IsPaused) {
			_Time += InDeltaTime;
		}

		// 애니메이션 다 끝난 경우
		if (_Time >= _Duration) {
			// 반복하기
			if (_IsLooping) {
				// _Time % _Duration 로 약간 벗어난 시간까지 반영
				_Time = std::fmodf(_Time, _Duration);
				_Index = 0;
			}
			// 아님 그냥 거기서 끝내기
			else {
				_Time = _Duration;
				_Index = _FrameCount - 1;
				_IsEnded = true;
				return frames[_FrameCount - 1].Value;
			}
		}
		int currentIndex = _Index;
		int nextIndex = _Index + 1;
		// 상대 시간 (x축)
		float t = frames[nextIndex].Time - frames[currentIndex].Time;
		// 현재 시간에 맞는 키프레임 찾기
		while (_Time >= frames[nextIndex].Time) {
			++currentIndex;
			++nextIndex;
			++_Index;
			t = frames[nextIndex].Time - frames[currentIndex].Time;
		}

		Keyframe& currentFrame = frames[currentIndex];
		Keyframe& nextFrame = frames[nextIndex];

		// 사전에 계산된 a,b,c 계수들
		KeyframeCalculation& calculation = _Curve.GetKeyframeCalculations()[currentIndex];

		// 함수 계산 (y축)
		float v = calculation.Calculate(_Time - frames[currentIndex].Time);

		// 계산된 y축을 각 타입에 맞게 적용
		return currentFrame.GetValue(v, nextFrame);
	}

	void Reset() {
		_Index = 0;
		_Time = 0.f;
		_IsEnded = false;
		_IsPaused = false;
	}

private:
	// AnimationCurve 레퍼런스
	Curve& _Curve;
	// 프레임 갯수 상수
	const int _FrameCount;
	// 지속시간 상수
	const int _Duration;

	// 반복 여부
	bool _IsLooping = true;

	// 현재 프레임
	int _Index = 0;
	// 현재 시간
	float _Time = 0.f;

	// 유효 여부
	bool _IsValid = false;
	// 애니메이션 종료 여부
	bool _IsEnded = false;
	// 일시정지 여부
	bool _IsPaused = false;
};
}