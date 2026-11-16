#pragma once

#include "activityos/storage.hpp"

class QWidget;

namespace activityos::ui {

// Asks for confirmation, then adds two weeks of clearly labeled synthetic
// activity so every dashboard has something to show. Does nothing if the demo
// data is already loaded. `parent` owns the dialogs.
void loadDemoData(storage::Database& database, QWidget* parent);

} // namespace activityos::ui
