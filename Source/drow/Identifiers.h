#pragma once

#include "JuceHeader.h"

namespace IDs
{
    #define DECLARE_ID(name) const juce::Identifier name (#name);

    DECLARE_ID (TREE)
    DECLARE_ID (pi)

    DECLARE_ID(PARAM)

    DECLARE_ID(MASTER_GAIN)

    DECLARE_ID(SINE_BANK_INSTRUMENT)

    DECLARE_ID (EDIT)
    DECLARE_ID (uuid)
    DECLARE_ID (mediaId)

    DECLARE_ID (TRACK)
    DECLARE_ID (colour)
    DECLARE_ID (name)
    DECLARE_ID (image)
    DECLARE_ID (selected)

    DECLARE_ID(INSTRUMENT)

    DECLARE_ID (CLIP)
    DECLARE_ID (start)
    DECLARE_ID (length)
    DECLARE_ID (timestretchOptions)
    DECLARE_ID (NOTE)

    DECLARE_ID(PARAM_NA)

    #undef DECLARE_ID
}
