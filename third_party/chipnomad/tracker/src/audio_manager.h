#ifndef __AUDIOMANAGER_H__
#define __AUDIOMANAGER_H__

#include "chipnomad_lib.h"
#include "audio_source.h"
#include "audio_device.h"

class TrackerState;

enum class TrackState: uint8_t {
  normal = 0,
  solo = 1,
  muted = 2
};

class AudioManager : public AudioCallbacks {
  public:
    /* koppios: ctor signature and lifecycle differ from upstream's
     * cpp-migration branch -- see ../../../../apps/chipnomad/README.md
     * ("The three patched files"). The Engine is owned by TrackerState
     * (where the un-migrated src/app.cpp expects it) and referenced here;
     * opening the device moved out of the ctor into start(), which is the
     * pair of calls app.cpp actually makes. */
    AudioManager(AudioDevice& audioDevice, TrackerState* state);
    ~AudioManager();

    chipnomad::Engine& engine; // ChipNomad Engine, owned by TrackerState
    TrackState trackStates[PROJECT_MAX_TRACKS]; // Track solo/mute states

    // Audio manager lifecycle
    int start(int sampleRate, int audioBufferSize);
    void stop();
    void pause();
    void resume();

    // Track mute/solo functions
    void toggleTrackMute(int trackIdx);
    void toggleTrackSolo(int trackIdx);

    // Chip reinitialization function
    void reinitChips();

    // Preview functions. The AudioManager takes ownership of the source and deletes it when the preview stops.
    // TODO: Rename to something like a secondary source
    void startPreview(AudioSource* source);
    void stopPreview();

    // Convenience helpers for specific source types
    // TODO: Move them to corresponding AudioSource classes for better encapsulation
    int startWavPreview(const char* path);
    int startWavetablePreview(const char* path, bool isYM);

    // AudioCallbacks implementation
    void onAudioOutput(int16_t* buffer, int stereoSamples) override;

  protected:
    AudioDevice& device;
    int sampleRate;
    int bufferSize;
    float* renderBuffer;
    int pendingReinitChips;

    // Preview state (generic audio source: WAV, wavetable, etc.)
    AudioSource* previewSource;
    float previewPosition;   // Fractional accumulator for resampling (0.0 to 1.0)
    float previewRateRatio;  // sourceSampleRate / outputSampleRate

    // Preview read buffer
    static const int PREVIEW_BUF_SIZE = 256;
    int16_t previewBuf[PREVIEW_BUF_SIZE];
    int previewBufPos;
    int previewBufCount;

    void updatePlaybackMuteFlags(void);
};

// Singleton AudioManager instance, bound by audioInit() (koppios bridge).
// TODO (upstream): avoid the global and use dependency injection instead.
extern AudioManager& audio;

#endif // __AUDIO_MANAGER_H__
