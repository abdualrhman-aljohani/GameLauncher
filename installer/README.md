# المثبّت (Inno Setup)

1. ابنِ المشروع `Release | x64` (الناتج `bin\Release\GameLauncher.exe`).
2. اختياري: `powershell -ExecutionPolicy Bypass -File tools\build-ui-assets.ps1` ليعمل البرنامج بدون إنترنت.
3. ثبّت [Inno Setup 6.3+](https://jrsoftware.org/isinfo.php) ثم افتح `installer\GameLauncher.iss` واضغط **Compile**
   (أو من سطر الأوامر: `ISCC.exe installer\GameLauncher.iss`).
4. الناتج في `dist\GameLauncher-Setup-1.8.0.exe`.

**ماذا يسوي المثبّت:** يثبّت في `Program Files`، وينشئ اختصار قائمة ابدأ (وسطح المكتب اختيارياً)، ويتأكد من وجود WebView2،
ويدعم الترقية فوق النسخة القديمة (نفس `AppId`) بدون لمس إعداداتك.

**ماذا تسوي الإزالة:** تشغّل `GameLauncher.exe --uninstall-cleanup` (يرجّع قيم Boost ويحذف التشغيل التلقائي) ثم تحذف الملفات،
وتسأل إن كنت تريد حذف `%APPDATA%\GameLauncher` (الافتراضي: لا).

**ملاحظات:** الإصدار مكتوب يدوياً في `#define MyAppVersion` — حدّثه مع كل إصدار (وفي `Launcher.rc`).
لتقليل تحذيرات SmartScreen يلزم توقيع رقمي (`SignTool` في السكربت، معطّل حالياً).
