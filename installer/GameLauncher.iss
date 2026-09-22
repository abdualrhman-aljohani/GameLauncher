; ==========================================================================
;  GameLauncher — سكربت المثبّت (Inno Setup 6.3 أو أحدث)
;
;  الخطوات:
;    1) ابنِ المشروع بوضع Release | x64  (الناتج: bin\Release\GameLauncher.exe)
;    2) (اختياري) شغّل tools\build-ui-assets.ps1 ليعمل البرنامج بدون إنترنت
;    3) افتح هذا الملف بـ Inno Setup واضغط Compile  (أو:  ISCC.exe installer\GameLauncher.iss)
;    الناتج: dist\GameLauncher-Setup-<الإصدار>.exe
;
;  الإزالة: تستدعي  GameLauncher.exe --uninstall-cleanup  قبل حذف الملفات ليرجّع قيم Boost
;  في ويندوز (SystemResponsiveness / MMCSS) ويحذف التشغيل التلقائي، ثم تسأل إن كنت تريد
;  حذف إعداداتك وقائمة ألعابك (الافتراضي: لا).
; ==========================================================================

#define MyAppName      "GameLauncher"
#define MyAppVersion   "1.8.0"
#define MyAppPublisher "Abdualrhman Aljohani"
#define MyAppURL       "https://github.com/abdualrhman-aljohani"
#define MyAppExeName   "GameLauncher.exe"
#define BuildDir       "..\bin\Release"

[Setup]
; معرّف ثابت للتطبيق: لا تغيّره أبداً (به يعرف المثبّت أنه ترقية وليس تثبيتاً جديداً)
AppId={{872856CD-C295-4BD3-91C4-636E9A850221}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
VersionInfoVersion={#MyAppVersion}.0
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE.txt
SetupIconFile=..\Common\app_icon.ico
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir=..\dist
OutputBaseFilename=GameLauncher-Setup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; الإزالة تحتاج مسؤولاً لاسترجاع registry الخاص بـ Boost
PrivilegesRequired=admin
MinVersion=10.0.19041
; إذا البرنامج شغّال يطلب المثبّت إغلاقه (هذا اسم الـ mutex في الكود)
AppMutex=GameLauncher_SingleInstance_Mutex_v1
CloseApplications=yes
RestartApplications=no
; توقيع الملف الرقمي (يقلل تحذيرات SmartScreen) — فعّله بعد إعداد SignTool في Inno:
; SignTool=mysigntool $f

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; أصول الواجهة المحلية (تُتخطّى إن لم تشغّل tools\build-ui-assets.ps1 — عندها تعمل الواجهة عبر الإنترنت)
Source: "{#BuildDir}\ui_assets\*"; DestDir: "{app}\ui_assets"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; runasoriginaluser: يشغّل البرنامج بصلاحيات المستخدم العادية (وليس كمسؤول كما يعمل المثبّت)
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent runasoriginaluser

[UninstallRun]
Filename: "{app}\{#MyAppExeName}"; Parameters: "--uninstall-cleanup"; Flags: runhidden waituntilterminated; RunOnceId: "GameLauncherCleanup"

[Code]
const
  WebView2ClientKey = 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';

function IsWebView2Installed(): Boolean;
var
  Ver: String;
begin
  Result := False;
  if RegQueryStringValue(HKLM32, WebView2ClientKey, 'pv', Ver) then
    Result := (Ver <> '') and (Ver <> '0.0.0.0')
  else if RegQueryStringValue(HKCU32, WebView2ClientKey, 'pv', Ver) then
    Result := (Ver <> '') and (Ver <> '0.0.0.0');
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not IsWebView2Installed() then
  begin
    Result := MsgBox(
      'Microsoft WebView2 Runtime was not found. The control panel needs it.' + #13#10 +
      'It is included with Windows 11; on Windows 10 install it from:' + #13#10 +
      'https://developer.microsoft.com/microsoft-edge/webview2/' + #13#10#13#10 +
      'لم يُعثر على WebView2 Runtime وهو مطلوب للوحة التحكم.' + #13#10#13#10 +
      'Continue anyway? / المتابعة على أي حال؟',
      mbConfirmation, MB_YESNO) = IDYES;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    DataDir := ExpandConstant('{userappdata}\GameLauncher');
    if DirExists(DataDir) and (not UninstallSilent) then
    begin
      // الافتراضي "لا": إعداداتك وقائمة ألعابك تبقى للتثبيت القادم
      if MsgBox(
        'Also delete your settings and games list?' + #13#10 + DataDir + #13#10#13#10 +
        'هل تريد حذف إعداداتك وقائمة ألعابك أيضاً؟',
        mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(DataDir, True, True, True);
    end;
  end;
end;
