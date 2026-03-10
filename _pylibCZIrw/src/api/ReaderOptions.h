#pragma once
#include "inc_libCzi.h"

/// This POD ("plain-old-data") structure gathers open-time options for the
/// CZI reader. It is used to configure the reader's behavior, such as enabling
/// mask awareness and visibility check optimizations.
struct ReaderOptions {
  /// whether to enable mask awareness
  bool enableMaskAwareness = false;

  /// whether to enable visibility check optimization
  bool enableVisibilityCheckOptimization = true;

  /// whether to use lax parameter validation when parsing subblock dimension entries
  bool laxSubblockCoordinateChecks = true;

  /// whether to ignore the size-M attribute of pyramid subblocks (only relevant when
  /// laxSubblockCoordinateChecks is false)
  bool ignoreSizeMForPyramidSubblocks = false;

  void Clear() {
    this->enableMaskAwareness = false;
    this->enableVisibilityCheckOptimization = true;
    this->laxSubblockCoordinateChecks = true;
    this->ignoreSizeMForPyramidSubblocks = false;
  }
};
