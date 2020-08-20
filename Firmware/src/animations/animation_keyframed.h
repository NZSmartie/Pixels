#pragma once

#include "animations/Animation.h"

#pragma pack(push, 1)

namespace Animations
{
	enum SpecialColor : uint8_t
	{
		SpecialColor_None = 0,
		SpecialColor_Face,
		SpecialColor_ColorWheel,
		SpecialColor_Heat_Current,
		SpecialColor_Heat_Start,
	};

	class IAnimationSpecialColorToken;
	class RGBKeyframe;

	/// <summary>
	/// An animation track is essentially an animation curve and a set of leds.
	/// size: 8 bytes (+ the actual keyframe data).
	/// </summary>
	struct RGBTrack
	{
	public:
		uint16_t keyframesOffset;	// offset into a global keyframe buffer
		uint8_t keyFrameCount;		// Keyframe count
		uint8_t padding;
		uint32_t ledMask; 			// indicates which leds to drive

		uint16_t getDuration() const;
		const RGBKeyframe& getRGBKeyframe(uint16_t keyframeIndex) const;
		int evaluate(const IAnimationSpecialColorToken* token, int time, int retIndices[], uint32_t retColors[]) const;
		uint32_t evaluateColor(const IAnimationSpecialColorToken* token, int time) const;
		int extractLEDIndices(int retIndices[]) const;
	};

	/// <summary>
	/// A keyframe-based animation
	/// size: 8 bytes (+ actual track and keyframe data)
	/// </summary>
	struct AnimationKeyframed
		: public Animation
	{
		uint8_t specialColorType; // is really SpecialColor
		uint16_t tracksOffset; // offset into a global buffer of tracks
		uint16_t trackCount;
		uint8_t padding2;
		uint8_t padding3;
	};

	/// <summary>
	/// Keyframe-based animation instance data
	/// </summary>
	class AnimationInstanceKeyframed
		: public IAnimationSpecialColorToken
		, public AnimationInstance
	{
	private:
		uint32_t specialColorPayload; // meaning varies

	public:
		AnimationInstanceKeyframed(const AnimationKeyframed* preset);
		virtual ~AnimationInstanceKeyframed();

	public:
		virtual int animationSize() const;

		virtual void start(int _startTime, uint8_t _remapFace, bool _loop);
		virtual int updateLEDs(int ms, int retIndices[], uint32_t retColors[]);
		virtual int stop(int retIndices[]);

	private:
		const AnimationKeyframed* getPreset() const;
		const RGBTrack& GetTrack(int index) const;

	public:
		virtual uint32_t getColor(uint32_t colorIndex) const;
	};

}

#pragma pack(pop)