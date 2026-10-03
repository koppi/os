#ifndef __TRACKER_STATE_H__
#define __TRACKER_STATE_H__

#include "chipnomad_lib.h"
#include "app_settings.h"

class TrackerState {
  public:
    TrackerState();

    AppSettings settings;

    chipnomad::Project project;
    bool projectModified;

    int* pSongRow;
    int* pSongTrack;
    int* pChainRow;

    /* koppios addition, not upstream ChipNomad -- see
     * ../../../../apps/chipnomad/README.md ("The three patched files").
     *
     * src/app.cpp, which upstream did not migrate, already calls
     * initEngine() and then reads engine->{player,trackWarnings,
     * audioOverload,mixVolume,aySampleDithering}: it was written against a
     * TrackerState that owns the playback Engine. Upstream's version owns
     * only the Project, and left the Engine inside the new AudioManager.
     * Declaring it where app.cpp expects it is the smaller of the two
     * reconciliations; AudioManager takes a pointer to it instead. */
    chipnomad::Engine* engine = nullptr;

    /* Not upstream's `= default`: the Engine above has to be deleted. */
    ~TrackerState();

    /** Create the Engine, bind it to this state's Project, init its chips. */
    void initEngine(int sampleRate, chipnomad::ChipFactory factory);
};

#endif // __TRACKER_STATE_H__
