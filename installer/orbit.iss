[Setup]
AppId={{9C1D5E72-83B4-4A6F-B0E9-2A7C4F15D863}
AppName=HEX-Q ORBIT
AppVersion=1.0.0
AppPublisher=HEX-Q
DefaultDirName={autopf}\HEX-Q ORBIT
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..
OutputBaseFilename=HEX-Q-ORBIT-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\dist\HEX-Q ORBIT.vst3\*"; DestDir: "{commoncf64}\VST3\HEX-Q ORBIT.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
