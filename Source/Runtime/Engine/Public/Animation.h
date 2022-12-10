#pragma once

namespace CK
{
// Ű������ ����
template<typename TValue>
struct Keyframe {
	float Time;		// �ð�
	float Tangent;	// ź��Ʈ(����)
	TValue Value;	// ��

	// to�κ����� ��� y1���� �����س��� �Լ�
	float GetDeltaValue(const Keyframe<TValue>& to) {
		const auto& from = *this;
		return to.Value - from.Value;
	}
	// ��� y���� �����ϴ� �Լ�
	TValue GetValue(float value, const Keyframe<TValue>& to) {
		const auto& from = *this;
		return from.Value + value;
	}
};

// float�� ���� �ν��Ͻ�ȭ (��� �ٸ� �� ���� ��)
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

// Vector3�� ���� �ν��Ͻ�ȭ
template<>
struct Keyframe<Vector3> {
	float Time;
	float Tangent;
	Vector3 Value;
	// �Ÿ��� ���� y1 ����
	float GetDeltaValue(const Keyframe<Vector3>& to) {
		const auto& from = *this;
		return (to.Value - from.Value).Size();
	}
	// (to-from) ������ �������� y�� ����
	Vector3 GetValue(float value, const Keyframe<Vector3>& to) {
		const auto& from = *this;
		auto fromTo = (to.Value - from.Value);
		fromTo.Normalize();
		return from.Value + fromTo * value;
	}
};

// Quaternion�� ���� �ν��Ͻ�ȭ
template<>
struct Keyframe<Quaternion> {
	float Time;
	float Tangent;
	Quaternion Value;

	// ������ arccos�� ���� ���հ� ���ϱ�
	float GetDeltaValue(const Keyframe<Quaternion>& to) {
		const auto& from = *this;
		float angleInRadian = to.Value.Angle(from.Value);
		return angleInRadian;
	}
	// [0, 1] �������� ����ȭ�ؼ� Slerp�� y�� ����
	Quaternion GetValue(float value, const Keyframe<Quaternion>& to) {
		const auto& from = *this;
		float angleInRadian = to.Value.Angle(from.Value);
		return Quaternion::Slerp(from.Value, to.Value, value / angleInRadian);
	}
};

// �����Լ��� �� ��� ����ŷ
struct KeyframeCalculation {
	float a, b, c;
	// d = 0�̹Ƿ� ����

	// x���� ���� �Լ� ��� (ax^3 + bx^2 + cx)
	float Calculate(float InX) {
		float square = InX * InX;
		float cubic = square * InX;
		return a * cubic + b * square + c * InX;
	}
};

// �ִϸ��̼� Ŀ��, ���� Ű���������� ����
template<typename TValue>
class AnimationCurve {
public:
	using Keyframe = Keyframe<TValue>;

	std::vector<Keyframe>& GetKeyframes() { return _Keyframes; }
	std::vector<KeyframeCalculation>& GetKeyframeCalculations() { return _KeyframeCalculation; }
	
	AnimationCurve(const std::initializer_list<Keyframe> InKeyframes) : _Keyframes(InKeyframes) {
		Calculate();
	}

	// ������ ������ ���̻��� ����ŷ
	void Calculate() {
		auto frameCount = _Keyframes.size();
		if (frameCount == 0) return;
		if (frameCount == 1) return; 
		// �ð��� ����
		std::sort(_Keyframes.begin(), _Keyframes.end(), [&](Keyframe a, Keyframe b) -> bool {
			return a.Time < b.Time;
		});

		// �����Լ� � ����ŷ
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

// ������ �ִϸ��̼��� ������ �� ����ϴ� ����
// AnimationCurve�� ���۷����� ����
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

	// �� ������ ȣ��Ǿ� �ִϸ��̼� ����
	TValue Next(float InDeltaTime) {
		// ���� �߸��� ������ ���� �⺻�� ��ȯ
		if (!_IsValid) {
			return TValue();
		}

		auto& frames = _Curve.GetKeyframes();

		// _IsLooping == false�ε� �ִϸ��̼��� �� �帥 ��� _IsEnded == true
		// ���� ������ Ű�������� �� ��ȯ
		if (_IsEnded) {
			return frames[_FrameCount - 1].Value;
		}

		// �Ͻ����� �� ���?
		if (!_IsPaused) {
			_Time += InDeltaTime;
		}

		// �ִϸ��̼� �� ���� ���
		if (_Time >= _Duration) {
			// �ݺ��ϱ�
			if (_IsLooping) {
				// _Time % _Duration �� �ణ ��� �ð����� �ݿ�
				_Time = std::fmodf(_Time, _Duration);
				_Index = 0;
			}
			// �ƴ� �׳� �ű⼭ ������
			else {
				_Time = _Duration;
				_Index = _FrameCount - 1;
				_IsEnded = true;
				return frames[_FrameCount - 1].Value;
			}
		}
		int currentIndex = _Index;
		int nextIndex = _Index + 1;
		// ��� �ð� (x��)
		float t = frames[nextIndex].Time - frames[currentIndex].Time;
		// ���� �ð��� �´� Ű������ ã��
		while (_Time >= frames[nextIndex].Time) {
			++currentIndex;
			++nextIndex;
			++_Index;
			t = frames[nextIndex].Time - frames[currentIndex].Time;
		}

		Keyframe& currentFrame = frames[currentIndex];
		Keyframe& nextFrame = frames[nextIndex];

		// ������ ���� a,b,c �����
		KeyframeCalculation& calculation = _Curve.GetKeyframeCalculations()[currentIndex];

		// �Լ� ��� (y��)
		float v = calculation.Calculate(_Time - frames[currentIndex].Time);

		// ���� y���� �� Ÿ�Կ� �°� ����
		return currentFrame.GetValue(v, nextFrame);
	}

	void Reset() {
		_Index = 0;
		_Time = 0.f;
		_IsEnded = false;
		_IsPaused = false;
	}

private:
	// AnimationCurve ���۷���
	Curve& _Curve;
	// ������ ���� ���
	const int _FrameCount;
	// ���ӽð� ���
	const int _Duration;

	// �ݺ� ����
	bool _IsLooping = true;

	// ���� ������
	int _Index = 0;
	// ���� �ð�
	float _Time = 0.f;

	// ��ȿ ����
	bool _IsValid = false;
	// �ִϸ��̼� ���� ����
	bool _IsEnded = false;
	// �Ͻ����� ����
	bool _IsPaused = false;
};
}