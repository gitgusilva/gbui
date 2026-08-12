// What each demo file publishes to the catalogue.
//
// One function per screen, returning the entry rather than registering itself
// into a global: a static registrar would run before `main` in an order no
// standard guarantees, and the catalogue would come out in a different order
// on a different linker. A list written by hand in registry.cpp is the order
// the gallery shows, on every platform.
#pragma once

#include "gbui_demos/demos.hpp"

namespace gbui::demos {

DemoInfo analyticsDemo();
DemoInfo weatherDemo();
DemoInfo scadaDemo();
DemoInfo productionDemo();
DemoInfo gridDemo();
DemoInfo logisticsDemo();

}  // namespace gbui::demos
