// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <mutex>
#include <string>
#include <vector>

#include "AudioCommon/WaveFile.h"

// Produces white noise in the range of [-4.f, 4.f]
// Note: for me this values produces more natural results than master [-0.5, 0.5]
const float rndrcp = 8.f / float(RAND_MAX);
#define DITHER_NOISE ((rand() * rndrcp) - 4.f)

// converts [-32768, 32767] -> [-1.0, 1.0)
__forceinline float Signed16ToFloat(const s16 s)
{
	return s * 0.000030517578125f;
}

// we NEED dithering going from float -> 16bit
__forceinline void TriangleDither(float& sample, float& prev_dither)
{
	float dither = DITHER_NOISE;	
	sample += dither - prev_dither;
	prev_dither = dither;
}

class CMixer {

public:
	CMixer(u32 BackendSampleRate)
		: m_dma_mixer(this, 32000)
		, m_streaming_mixer(this, 48000)
		, m_wiimote_speaker_mixer(this, 3000)
		, m_sample_rate(BackendSampleRate)
		, m_log_dtk_audio(0)
		, m_log_dsp_audio(0)
		, m_speed(0)
		, m_l_dither_prev(0)
		, m_r_dither_prev(0)
	{
		INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized");
	}

	static const u32 MAX_SAMPLES = 2048;
	static const u32 INDEX_MASK = MAX_SAMPLES * 2 - 1;
	static const float LOW_WATERMARK;
	static const float MAX_FREQ_SHIFT;
	static const float CONTROL_FACTOR;
	static const float CONTROL_AVG;

	virtual ~CMixer() {}

	// Called from audio threads
	u32 Mix(s16* samples, u32 numSamples, bool consider_framelimit = true);
	u32 Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
	u32 AvailableSamples();
	// Called from main thread
	virtual void PushSamples(const s16* samples, u32 num_samples);
	virtual void PushStreamingSamples(const s16* samples, u32 num_samples);
	virtual void PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate);
	u32 GetSampleRate() const { return m_sample_rate; }

	void SetDMAInputSampleRate(u32 rate);
	void SetStreamInputSampleRate(u32 rate);
	void SetStreamingVolume(u32 lvolume, u32 rvolume);
	void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);

	virtual void StartLogDTKAudio(const std::string& filename)
	{
		if (!m_log_dtk_audio)
		{
			m_log_dtk_audio = true;
			g_wave_writer_dtk.Start(filename, 48000);
			g_wave_writer_dtk.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting DTK Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DTK Audio logging has already been started");
		}
	}

	virtual void StopLogDTKAudio()
	{
		if (m_log_dtk_audio)
		{
			m_log_dtk_audio = false;
			g_wave_writer_dtk.Stop();
			NOTICE_LOG(DSPHLE, "Stopping DTK Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DTK Audio logging has already been stopped");
		}
	}

	virtual void StartLogDSPAudio(const std::string& filename)
	{
		if (!m_log_dsp_audio)
		{
			m_log_dsp_audio = true;
			g_wave_writer_dsp.Start(filename, 32000);
			g_wave_writer_dsp.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting DSP Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DSP Audio logging has already been started");
		}
	}

	virtual void StopLogDSPAudio()
	{
		if (m_log_dsp_audio)
		{
			m_log_dsp_audio = false;
			g_wave_writer_dsp.Stop();
			NOTICE_LOG(DSPHLE, "Stopping DSP Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DSP Audio logging has already been stopped");
		}
	}

	std::mutex& MixerCritical() { return m_cs_mixing; }

	float GetCurrentSpeed() const { return m_speed; }
	void UpdateSpeed(volatile float val) { m_speed = val; }

protected:
	class MixerFifo
	{
	public:
		MixerFifo(CMixer *mixer, unsigned sample_rate)
			: m_mixer(mixer)
			, m_input_sample_rate(sample_rate)
			, m_write_index(0)
			, m_read_index(0)
			, m_lvolume(255)
			, m_rvolume(255)
			, m_num_left_i(0.0f)
			, m_fraction(0)
		{
			srand((u32)time(nullptr));
		}

		virtual void Interpolate(u32 left_input_index, float* left_output, float* right_output) = 0;
		void PushSamples(const s16* samples, u32 num_samples);
		void Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
		void SetInputSampleRate(u32 rate);
		void SetVolume(u32 lvolume, u32 rvolume);
		void GetVolume(u32* lvolume, u32* rvolume) const;
		u32 AvailableSamples();
	protected:
		CMixer *m_mixer;
		unsigned m_input_sample_rate;

		std::array<float, MAX_SAMPLES * 2> m_float_buffer;

		volatile u32 m_write_index;
		volatile u32 m_read_index;

		// Volume ranges from 0-255
		volatile s32 m_lvolume;
		volatile s32 m_rvolume;

		float m_num_left_i;
		float m_fraction;
	};

	class LinearMixerFifo : public MixerFifo
	{
	public:
		LinearMixerFifo(CMixer* mixer, u32 sample_rate) : MixerFifo(mixer, sample_rate) {}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
	};

	class CubicMixerFifo : public MixerFifo
	{
	public:
		CubicMixerFifo(CMixer* mixer, u32 sample_rate) : MixerFifo(mixer, sample_rate) {}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
	};

	CubicMixerFifo m_dma_mixer;
	CubicMixerFifo m_streaming_mixer;

	// Linear interpolation seems to be the best for Wiimote 3khz -> 48khz, for now.
	// TODO: figure out why and make it work with the above FIR
	LinearMixerFifo m_wiimote_speaker_mixer;

	u32 m_sample_rate;

	WaveFileWriter g_wave_writer_dtk;
	WaveFileWriter g_wave_writer_dsp;

	bool m_log_dtk_audio;
	bool m_log_dsp_audio;

	std::mutex m_cs_mixing;

	volatile float m_speed; // Current rate of the emulation (1.0 = 100% speed)

private:

	std::vector<float> m_output_buffer;
	float m_l_dither_prev;
	float m_r_dither_prev;
};

