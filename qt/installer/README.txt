Token Monitor (Qt) - Windows x64
===============================

What was installed (this folder)
--------------------------------
TokenMonitorQt.exe     the widget: tray icon, widget window, dashboard
TokenMonitorHub.exe    standalone sync hub CLI (--port / --host / --secret)
TokenMonitorAgent.exe  headless collector for machines that run no widget
tokscale.exe           pinned usage scanner the widget and agent run
*.dll, platforms\, qml\, tls\, sqldrivers\, ...   Qt 6 runtime and plugins
Uninstall.exe          uninstaller (also listed in Settings > Apps)

This folder is self-contained: no Qt installation, no Node.js and no Visual C++
redistributable is required (the C++ runtime DLLs ship next to the executables).

Data locations
--------------
Settings, credentials, history and hub state live in %APPDATA%\Token Monitor.
That is the same folder the Electron build of Token Monitor uses, and the two
builds share one configuration, history and credential store.

Autostart
---------
There is no "start with Windows" option in this installer: autostart belongs to the
widget itself (Settings > start with Windows). The widget also reads the Windows state
when it starts, so an entry left behind by an older installer shows up as enabled and
can be switched off there. The entry it writes is quoted, because this folder's path
contains a space.

Uninstalling (leaves nothing behind)
------------------------------------
Settings > Apps > Token Monitor (Qt) > Uninstall, or run Uninstall.exe.

The uninstaller stops the widget/hub/agent, then removes this folder, both
shortcuts, the "Apps & features" entry and every file this build writes. The
autostart entry is removed only when it points into this folder - one that points
elsewhere belongs to another copy (a portable build or a second install) and is left
alone. Files removed:
  %APPDATA%\Token Monitor\limits-snapshot.json
  %APPDATA%\Token Monitor\data\
  %APPDATA%\Token Monitor\qt-*.json, qt-*.log
  %APPDATA%\Token Monitor\tokscale.exe
  %LOCALAPPDATA%\Token Monitor\cache\
  %LOCALAPPDATA%\Javis\Token Monitor\
Because %APPDATA%\Token Monitor is shared with the Electron build, the
uninstaller asks before deleting the whole folder: answering Yes also erases the
Electron build's settings.json, credentials.json and history.json. Silent mode
never asks:

  Uninstall.exe /S             remove everything, shared folder included
  Uninstall.exe /S /KEEPDATA   keep the files shared with the Electron build

Unsigned build
--------------
This installer and these executables are unsigned, so SmartScreen shows an
"unknown publisher" warning the first time. Choose "More info" > "Run anyway".
