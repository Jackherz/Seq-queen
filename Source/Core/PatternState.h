#pragma once

// -----------------------------------------------------------------------------
//  Step persistence.
//
//  Track-level settings (length, engine, filter, brains, ...) are exposed as
//  host parameters by the plugin, so the host already saves them. What the host
//  cannot save is the *step grid and its p-locks*, so that is what gets
//  serialised here into a small versioned text blob which the plugin stores in
//  its state ValueTree.
//
//  Text (rather than binary) because it is trivially inspectable, diffable and
//  forward compatible: unknown keys are ignored by the parser.
//
//  Format:
//      opxy-steps v1
//      t0 s3 a=1 n=38 v=0.9 g=0.25 r=2 d=0.03 p=cutoff:800,res:0.4
//
//  `p=` entries are p-locks: <paramId>:<value>.
// -----------------------------------------------------------------------------

#include "Pattern.h"
#include <string>

namespace opxy
{

/** Serialises every step that carries data (active, p-locked, ratcheted, nudged). */
std::string serialiseSteps (const Pattern& pattern);

/** Parses a blob produced by serialiseSteps(). Clears the pattern's steps first.
    Returns false if the blob is not recognisable (the pattern is left cleared). */
bool deserialiseSteps (const std::string& text, Pattern& pattern);

} // namespace opxy
