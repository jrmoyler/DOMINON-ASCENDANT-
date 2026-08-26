#pragma once

// Compatibility include for simulation/gameplay call sites. The persistent DTO is Core-owned so
// campaign persistence can consume it without reversing the existing Simulation -> Core dependency.
#include "World/DAWorldAssetRecord.h"
