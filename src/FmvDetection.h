#ifndef CRASH4WIDESCREENFIX_FMVDETECTION_H
#define CRASH4WIDESCREENFIX_FMVDETECTION_H

namespace FmvDetection
{
    // FMVs are MP4 files played through Unreal's WmfMedia player, which drives a Media Foundation
    // media session. Hooks the game's import of MFCreateMediaSession and watches each session's
    // events, calling onChange(true) when the first session starts playing and onChange(false) once
    // none are. Returns false if the game does not import MFCreateMediaSession.
    bool Install(void (*onChange)(bool playing));
}

#endif //CRASH4WIDESCREENFIX_FMVDETECTION_H
