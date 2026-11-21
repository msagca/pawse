#pragma once
#include <windows.h>
namespace pawse {
class App;
// Shows the modal settings dialog. Changes apply only when the user clicks OK.
void ShowSettingsDialog(HINSTANCE instance, App &app);
} // namespace pawse
