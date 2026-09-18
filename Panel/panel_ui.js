/* -----------------------------------------------------------------------
 * GameLauncher — Panel UI Logic
 * Copyright (c) 2026 Abdualrhman Aljohani
 * Licensed under PolyForm Noncommercial 1.0.0
 * ----------------------------------------------------------------------- */

// ✅ 6 أنماط دائرة فقط (5 + 1 مخصص)
const RADIAL_META = {
  13: { icon: 'fa-water',          descAr: 'موجات متداخلة — 3 حلقات داخل بعض.', descEn: 'Ripple — three concentric waves.' },
  19: { icon: 'fa-fire',           descAr: 'لهب — أقواس نارية للأعلى.',          descEn: 'Flame — fire arcs pointing up.' },
  1:  { icon: 'fa-bolt',           descAr: 'توهّج نيون متعدد الطبقات.',          descEn: 'Layered neon glow.' },
  2:  { icon: 'fa-swatchbook',     descAr: 'حلقة بتدرج لوني ناعم.',              descEn: 'Soft gradient ring.' },
  11: { icon: 'fa-rainbow',        descAr: 'توهّج شفقي بتدرج متعدد الألوان.',   descEn: 'Aurora — flowing multicolor gradient glow.' },
  20: { icon: 'fa-wand-magic-sparkles', descAr: 'نمط مخصص من تصميمك.',           descEn: 'Custom — designed by you.' },
};

// ✅ قائمة الأنماط المرئية (بترتيب العرض)
const VISIBLE_RADIAL_STYLES = [13, 19, 1, 2, 11, 20];

// ✅ 6 ثيمات فقط + 1 مخصص
const PANEL_PRESETS = [
  { id: 'ember',     nameAr: 'جمر',       nameEn: 'Ember',     colors: ['#dc2626', '#f97316'], icon: 'fa-fire' },
  { id: 'nord',      nameAr: 'نورد',      nameEn: 'Nord',      colors: ['#88c0d0', '#5e81ac'], icon: 'fa-snowflake' },
  { id: 'obsidian',  nameAr: 'أوبسيديان', nameEn: 'Obsidian',  colors: ['#6366f1', '#818cf8'], icon: 'fa-gem' },
  { id: 'royal',     nameAr: 'ملكي',      nameEn: 'Royal',     colors: ['#a855f7', '#eab308'], icon: 'fa-crown' },
  { id: 'cherry',    nameAr: 'كرز',       nameEn: 'Cherry',    colors: ['#dc2626', '#f87171'], icon: 'fa-heart' },
  { id: 'nebula',    nameAr: 'سديم',      nameEn: 'Nebula',    colors: ['#8b5cf6', '#ec4899'], icon: 'fa-star' },
];

const I18N = {
  ar: {
    appTitle: 'GameLauncher — لوحة التحكم', hotkeyLabel: 'الاختصار:', muteHotkeyLabel: 'كتم:',
    tabDashboard: 'الألعاب والمرافقون', tabDesign: 'تخصيص التصميم', tabSettings: 'الإعدادات',
    tabDetails: 'تفاصيل اللعبة',
    totalGamesLabel: 'إجمالي الألعاب:', dashTitle: 'الألعاب والبرامج المرافقة',
    dashSubtitle: 'اسحب ملفًا تنفيذيًا أو اختصارًا أو رابطًا أو مجلد لعبة كاملًا وأفلته، أو استخدم زر الإضافة.',
    addGameBtn: 'إضافة لعبة',
    gamesListTitle: 'الألعاب', companionsTitle: 'البرامج المرافقة',
    selectGameHint: 'اختر لعبةً من القائمة أولًا.', addCompanionBtn: 'إضافة برنامج مرافق',
    designTitle: 'تخصيص التصميم والنمط', presetsTitle: 'ثيم اللوحة',
    presetsDesc: 'اختر من الثيمات الجاهزة أو صمم ثيمك الخاص.',
    radialTitle: 'نمط رسم الدائرة', backgroundsTitle: 'الخلفيات العامة',
    transparencyTitle: 'الشفافية',
    uiOpacityTitle: 'شفافية لوحة التحكم',
    radialOpacityLabel: 'شفافية خلفية دائرة الألعاب',
    radialOpacityHint: 'تؤثر على الصورة المخصصة كخلفية لدائرة الألعاب.',
    radialNoGlowTitle: 'أيقونات صافية (بدون إطار)',
    radialNoGlowDesc: 'إخفاء الإطار والتوهج والظلال — الأيقونة فقط.',
    settingsTitle: 'الإعدادات العامة',
    settingsSectionHotkeys: 'الاختصارات',
    settingsSectionController: 'يد التحكم (Xbox)',
    settingsSectionAppearance: 'المظهر والواجهة',
    settingsSectionSystem: 'النظام',
    settingsSectionTools: 'الأدوات والإدارة',
    hotkeySettingTitle: 'اختصار استدعاء دائرة الألعاب',
    hotkeySettingDesc: 'يدعم مفتاحًا مفردًا. يُطبَّق فورًا.',
    hotkeySingleKeyWarning: 'تنبيه: المفتاح دون Ctrl/Alt/Shift سيُحجز بالكامل.',
    langSettingTitle: 'لغة الواجهة',
    autoStartTitle: 'التشغيل مع بداية ويندوز',
    autoStartDesc: 'يشغّل GameLauncher تلقائيًا عند تسجيل الدخول.',
    engineRunning: 'المحرك: يعمل', engineStopped: 'المحرك: متوقف',
    stopEngine: 'إيقاف المحرك', startEngine: 'تشغيل المحرك',
    gamesUnit: 'لعبة', noCompanions: 'لا يوجد برنامج مرافق لهذه اللعبة حاليًا.',
    pressNewKey: 'اضغط المفتاح الجديد... (Esc للإلغاء)', selectStyle: 'اختيار',
    selectedStyle: 'مُفعّل',
    confirmRemoveGame: 'هل تريد حذف "{name}" من قائمة الألعاب؟',
    confirmRemoveCompanion: 'هل تريد حذف البرنامج المرافق "{name}"؟',
    confirmResetStats: 'هل تريد تصفير إحصاءات "{name}"؟',
    confirmCancel: 'إلغاء', confirmDelete: 'حذف', confirmReset: 'تصفير',
    detailsTitle: 'تفاصيل اللعبة المحددة',
    detailsEmptyHint: 'لم تُضف أي لعبة بعد.',
    detailsAllGames: 'كل الألعاب',
    detailsGameOptions: 'خيارات اللعبة الرئيسية',
    detailsRunAsAdmin: 'التشغيل كمسؤول',
    detailsShowInRadial: 'إظهار في الدائرة',
    detailsLaunchArgs: 'معاملات التشغيل',
    detailsItemColor: 'لون الإطار / التوهج',
    detailsPickCustomColor: 'اختيار لون مخصص…',
    detailsRadialBg: 'خلفية دائرية مخصصة',
    detailsRadialBgHint: 'اختر صورة لتظهر دائرية خلف الأيقونة.',
    detailsRadialBgChoose: 'اختيار صورة…',
    detailsRadialBgClear: 'إزالة الخلفية',
    detailsHideOriginalIcon: 'إخفاء أيقونة اللعبة الأصلية',
    detailsQuickSlot: 'اختصار الفتح السريع',
    detailsQuickSlotHint: 'اضغط الرقم داخل الدائرة لفتح اللعبة مباشرة.',
    quickSlotNone: 'بدون اختصار',
    detailsGameLanguage: 'لغة اللعبة داخل التطبيق',
    detailsGameLanguageHint: 'عند تشغيل اللعبة، سيتم تبديل لغة الكيبورد تلقائياً ثم إرجاعها عند الخروج.',
    langOff: 'بدون تبديل',
    langArabic: 'عربي',
    langEnglish: 'English',
    detailsCompanionsOptions: 'خيارات البرامج المرافقة',
    detailsNoCompanions: 'لا توجد برامج مرافقة.',
    detailsPerformanceTitle: 'خيارات الأداء (متقدمة)',
    detailsPriorityLabel: 'أولوية العملية',
    detailsAffinityLabel: 'تقييد الأنوية',
    selectAllCores: 'تحديد الكل', clearAllCores: 'مسح',
    quickAccessTitle: 'الوصول السريع',
    quickAccessHint: 'انقر للانتقال',
    companionShowInRadial: 'تشغيل مع اللعبة',
    bgPanelTitle: 'خلفية لوحة التحكم',
    bgPanelChoose: 'اختيار صورة…', bgPanelClear: 'إرجاع الافتراضية',
    bgRadialTitle: 'خلفية دائرة الألعاب',
    bgRadialChoose: 'اختيار صورة…', bgRadialClear: 'إرجاع الافتراضية',
    exeListTitle: 'الملفات التنفيذية المكتشفة',
    exeListSubtitle: 'تم العثور على {count} ملف داخل "{folder}"',
    exeListSelectAll: 'تحديد الكل', exeListClear: 'إلغاء التحديد',
    exeListAddSelected: 'إضافة المحدد',
    exeListCounter: 'المحدد: {n} من {total}',
    exeListMainBadge: 'رئيسي محتمل',
    searchPlaceholder: 'ابحث في الألعاب...',
    searchEmpty: 'لا توجد نتائج مطابقة.',
    sortManual: 'الترتيب اليدوي',
    sortFavorite: 'المفضّلة أولاً',
    sortRecent: 'الأحدث لعبًا',
    sortMostPlayed: 'الأكثر لعبًا',
    sortName: 'الاسم (أ-ي)',
    playStatsTitle: 'إحصاءات اللعب',
    statsTotalTime: 'إجمالي وقت اللعب',
    statsLastPlayed: 'آخر مرة لعبت',
    statsSessions: 'عدد الجلسات',
    statsReset: 'تصفير الإحصاءات',
    never: 'لم تُلعب بعد', justNow: 'الآن',
    minutesAgo: 'قبل {n} دقيقة', hoursAgo: 'قبل {n} ساعة', daysAgo: 'قبل {n} يوم',
    timeHM: '{h}س {m}د', timeMS: '{m}د {s}ث', timeS: '{s}ث',
    sessionsUnit: 'جلسة',
    muteEnableTitle: 'تفعيل اختصار الكتم السريع',
    muteHotkeyTitle: 'اختصار الكتم السريع',
    runningLabel: 'قيد التشغيل',
    openFolderTooltip: 'افتح مجلد ملف اللعبة',
    backupTitle: 'النسخ الاحتياطي',
    backupDesc: 'احفظ إعداداتك كملف، أو استعد من نسخة سابقة.',
    backupExportBtn: 'حفظ نسخة احتياطية',
    backupImportBtn: 'استيراد نسخة',
    mutedAppsTitle: 'التطبيقات المكتومة حاليًا',
    noMutedApps: 'لا توجد تطبيقات مكتومة.',
    unmuteBtn: 'إلغاء الكتم',
    hubAlwaysVisibleTitle: 'إظهار زر الـ Hub دائمًا',
    hubAlwaysVisibleDesc: 'عرض زر فتح اللوحة في منتصف الدائرة بدون الحاجة لتمرير الماوس فوقه.',
    logTitle: 'ملف السجل التشخيصي',
    logDesc: 'افتح السجل لتشخيص مشاكل التشغيل والأخطاء.',
    logOpenBtn: 'فتح ملف السجل',
    controllerEnableTitle: 'تفعيل يد التحكم (Xbox)',
    controllerEnableDesc: 'يتيح لك فتح الدائرة بزر من يد Xbox.',
    controllerButtonTitle: 'الزر الذي يفتح الدائرة',
    controllerButtonDesc: 'الافتراضي: زر Start.',
    controllerToggleModeTitle: 'وضع التبديل (فتح/إغلاق)',
    controllerToggleModeDesc: 'عند التفعيل: الضغط مرة أخرى يغلق الدائرة. عند الإيقاف: الضغط يفتح فقط.',
    controllerDuringGameTitle: 'استدعاء الدائرة أثناء اللعب',
    controllerDuringGameDesc: 'يسمح بزر اليد بفتح الدائرة أثناء اللعب.',
    panelGlassTitle: 'مظهر زجاجي حقيقي للوحة التحكم',
    panelGlassDesc: 'تأثير Acrylic شفاف يُظهر خلفية سطح المكتب من خلف اللوحة.',
    overlayEnableTitle: 'تفعيل الـ Overlay (CPU / RAM)',
    overlayEnableDesc: 'نافذة صغيرة شفافة تعرض معلومات النظام أثناء اللعب.',
    overlayHotkeyTitle: 'اختصار إظهار/إخفاء الـ Overlay',
    overlayShowOnLaunchTitle: 'إظهاره تلقائياً عند تشغيل لعبة',
    overlayOpacityLabel: 'شفافية الـ Overlay',
    ctrlBtnStart: 'Start', ctrlBtnBack: 'Back',
    ctrlBtnA: 'A', ctrlBtnB: 'B', ctrlBtnX: 'X', ctrlBtnY: 'Y',
    ctrlBtnLB: 'LB (Left Shoulder)', ctrlBtnRB: 'RB (Right Shoulder)',
    ctrlBtnLS: 'Left Stick', ctrlBtnRS: 'Right Stick', ctrlBtnGuide: 'Xbox (Guide)',
    boostRestoreTitle: 'استرجاع إعدادات Boost',
    boostRestoreDesc: 'إذا لاحظت أن إعدادات نظامية تغيّرت بسبب Boost، اضغط الزر لاسترجاع القيم الأصلية فوراً.',
    boostRestoreBtn: 'استرجاع إعدادات النظام',
    boostRestoreConfirm: 'هل تريد استرجاع القيم الأصلية لإعدادات النظام التي عدّلها Boost؟',
    detailsBlackBoxTitle: 'مراقبة الأداء (Black Box)',
    detailsBlackBoxEnable: 'تفعيل مراقبة الأداء لهذه اللعبة',
    detailsBlackBoxEnableHint: 'يجمع FPS والحرارة والاستهلاك أثناء اللعب لعرض تقرير مفصل.',
    detailsBlackBoxWarning: 'تنبيه: قراءة حرارة المعالج (CPU) قد تتعارض مع برامج مكافحة الغش في الألعاب التنافسية. FPS و GPU Temp آمنة تمامًا.',
    detailsBlackBoxFps: 'FPS + Frame Time',
    detailsBlackBoxGpuTemp: 'حرارة كرت الشاشة GPU',
    detailsBlackBoxRam: 'RAM / VRAM',
    detailsBlackBoxAdmin: 'Admin',
    detailsBlackBoxSafe: 'آمن',
    detailsBlackBoxViewReport: 'عرض التقرير',
    restartAsAdminTitle: 'تشغيل كمسؤول',
    restartAsAdminDesc: 'مطلوب لتفعيل FPS. لا يؤثر على الألعاب المفتوحة.',
    restartAsAdminBtn: 'إعادة التشغيل كمسؤول',
    adminYes: 'Admin',
    adminNo: 'ليس Admin',
    blackBoxNeedsAdmin: 'لتفعيل FPS، يحتاج GameLauncher صلاحيات المسؤول. اضغط "إعادة التشغيل كمسؤول" في الإعدادات أو استمر بدون FPS.',
    boostFpsTitle: 'Boost FPS',
    boostFpsDesc: 'تحسينات تلقائية عند تشغيل اللعبة، تتراجع عند الإغلاق.',
    boostDisableCore0: 'تعطيل Core 0',
    boostHighPriority: 'رفع الأولوية إلى High',
    boostStopStats: 'إيقاف الإحصائيات والـ Overlay',
    boostTimerResolution: 'تحسين دقة المؤقت (Timer 1ms)',
    boostSystemResponsiveness: 'SystemResponsiveness',
    boostMmcss: 'MMCSS Game Priority',
    boostFpsHint: 'قد تختلف نتائج Boost من لعبة لأخرى. بعض الخيارات تحتاج صلاحيات Admin.',
    perfReportTitle: 'تقرير الأداء',
    perfSessionsTitle: 'الجلسات السابقة',
    perfNoSessions: 'لا توجد جلسات محفوظة بعد. شغّل اللعبة مع تفعيل المراقبة لأول مرة.',
    perfSelectSessionHint: 'اختر جلسة من القائمة لعرض تفاصيلها',
    perfSessionOn: 'بتاريخ',
    perfDuration: 'المدة',
    perfAvgFps: 'متوسط FPS',
    perfMinFps: 'أدنى FPS',
    perfMaxFps: 'أقصى FPS',
    perf1PercentLow: '1% Low',
    perf01PercentLow: '0.1% Low',
    perfAvgCpuUsage: 'متوسط CPU',
    perfMaxCpuUsage: 'أقصى CPU',
    perfAvgGpuUsage: 'متوسط GPU',
    perfMaxGpuUsage: 'أقصى GPU',
    perfAvgCpuTemp: 'حرارة CPU (متوسط)',
    perfMaxCpuTemp: 'حرارة CPU (أقصى)',
    perfAvgGpuTemp: 'حرارة GPU (متوسط)',
    perfMaxGpuTemp: 'حرارة GPU (أقصى)',
    perfAvgCpuPower: 'طاقة CPU (متوسط)',
    perfAvgGpuPower: 'طاقة GPU (متوسط)',
    perfAvgRam: 'RAM (متوسط)',
    perfMaxRam: 'RAM (أقصى)',
    perfAvgVram: 'VRAM (متوسط)',
    perfMaxVram: 'VRAM (أقصى)',
    perfStutters: 'Stutters',
    perfExportCsvBtn: 'تصدير CSV',
    perfDeleteSession: 'حذف الجلسة',
    perfConfirmDeleteSession: 'هل تريد حذف هذه الجلسة؟',
    perfConfirmDeleteAll: 'هل تريد حذف كل الجلسات المحفوظة لهذه اللعبة؟',
    perfDeleteAll: 'حذف الكل',
    perfFpsNeedsAdmin: 'FPS يحتاج تشغيل GameLauncher كمسؤول. أغلقه ثم شغّله بـ "Run as Administrator" لتفعيل FPS.',
    perfFpsEtwFailed: 'فشل تفعيل ETW هذه المرة. جرّب إعادة تشغيل GameLauncher كمسؤول. إذا استمرت المشكلة، أعد تشغيل الجهاز.',
    perfCsvSaved: 'تم تصدير CSV بنجاح.',
    perfCsvFailed: 'فشل تصدير CSV.',
    perfSessionDeleted: 'تم حذف الجلسة.',
    perfAllDeleted: 'تم حذف كل الجلسات.',
    perfChartFpsTitle: 'FPS + Frame Time عبر الزمن',
    perfChartTempTitle: 'الحرارة (CPU + GPU)',
    perfChartRamTitle: 'استهلاك RAM / VRAM',
    statsHistory: 'سجل الجلسات',
    statsHistoryTitle: 'سجل الجلسات',
    statsHistoryLoading: 'جاري التحميل...',
    statsHistoryEmpty: 'لا توجد جلسات محفوظة بعد. ابدأ اللعب ليسجل تلقائياً.',
    statsHistoryDays: 'الأيام:',
    statsHistorySessions: 'الجلسات:',
    statsHistorySessionsShort: 'جلسة',
    statsHistoryTotal: 'الإجمالي:',
    statsHistoryClear: 'حذف السجل',
    statsHistoryClearConfirm: 'هل تريد حذف سجل الجلسات لهذه اللعبة بالكامل؟ لا يمكن التراجع.',
    // ✅ تصميم مخصص
    designCustomThemeBtn: 'تصميم ثيمي الخاص',
    designCustomStyleBtn: 'تصميم نمطك الخاص',
    radialScaleTitle: 'حجم دائرة الألعاب',
    radialScaleReset: 'إعادة الضبط',
    radialScaleIcon: 'حجم الأيقونات',
    radialScaleHub: 'حجم زر الوسط (Hub)',
    radialScaleOrbit: 'مسافة الألعاب من المركز',
    customThemeTitle: 'تصميم ثيمي الخاص',
    customThemeSubtitle: 'اختر 4 ألوان + اضبط 3 قيم',
    customThemeAccent: 'اللون الأساسي',
    customThemeSecondary: 'اللون الثانوي',
    customThemeBg: 'الخلفية',
    customThemeCard: 'البطاقات',
    customThemeCardOpacity: 'شفافية البطاقات',
    customThemeAccentStrength: 'قوة التمييز',
    customThemeBgGlow: 'توهج الخلفية',
    customThemePreview: 'معاينة',
    customThemePreviewBtn: 'زر أساسي',
    customThemePreviewBtn2: 'زر ثانوي',
    customThemeReset: 'مسح',
    customThemeSave: 'حفظ وتفعيل',
    customStyleTitle: 'تصميم نمطك الخاص',
    customStyleSubtitle: 'اضبط 5 قيم لإنشاء نمط الدائرة',
    customStyleRingCount: 'عدد الحلقات',
    customStyleRingThickness: 'سماكة الحلقة',
    customStyleDashed: 'حلقة متقطعة',
    customStyleDashedHint: 'عند التفعيل تظهر الحلقات كنقاط متقطعة',
    customStyleGlowLayers: 'طبقات التوهج',
    customStyleGlowStrength: 'شدة التوهج',
    customStyleReset: 'مسح',
    customStyleSave: 'حفظ وتفعيل',
  },

  en: {
    appTitle: 'GameLauncher — Control Panel', hotkeyLabel: 'Hotkey:', muteHotkeyLabel: 'Mute:',
    tabDashboard: 'Games & Companions', tabDesign: 'Design', tabSettings: 'Settings',
    tabDetails: 'Game Details',
    totalGamesLabel: 'Total games:', dashTitle: 'Games & Companion Programs',
    dashSubtitle: 'Drag & drop an executable, shortcut, url, or whole game folder.',
    addGameBtn: 'Add Game',
    gamesListTitle: 'Games', companionsTitle: 'Companion Programs',
    selectGameHint: 'Select a game first.', addCompanionBtn: 'Add Companion',
    designTitle: 'Design & Style', presetsTitle: 'Panel Theme',
    presetsDesc: 'Choose a ready theme or design your own.',
    radialTitle: 'Radial Drawing Style', backgroundsTitle: 'Global Backgrounds',
    transparencyTitle: 'Transparency',
    uiOpacityTitle: 'Control panel transparency',
    radialOpacityLabel: 'Game circle background transparency',
    radialOpacityHint: 'Affects the custom radial background image.',
    radialNoGlowTitle: 'Clean icons (no frame)',
    radialNoGlowDesc: 'Hide frame, glow, and shadows — icon only.',
    settingsTitle: 'General Settings',
    settingsSectionHotkeys: 'Hotkeys',
    settingsSectionController: 'Xbox Controller',
    settingsSectionAppearance: 'Appearance',
    settingsSectionSystem: 'System',
    settingsSectionTools: 'Tools & Management',
    hotkeySettingTitle: 'Game circle hotkey',
    hotkeySettingDesc: 'Supports a single key. Applied instantly.',
    hotkeySingleKeyWarning: 'Note: a key without modifiers will be reserved system-wide.',
    langSettingTitle: 'Interface language',
    autoStartTitle: 'Start with Windows',
    autoStartDesc: 'Launches automatically on Windows sign-in.',
    engineRunning: 'Engine: running', engineStopped: 'Engine: stopped',
    stopEngine: 'Stop Engine', startEngine: 'Start Engine',
    gamesUnit: 'games', noCompanions: 'No companion program yet.',
    pressNewKey: 'Press the new key... (Esc to cancel)', selectStyle: 'Select',
    selectedStyle: 'Active',
    confirmRemoveGame: 'Remove "{name}" from your games list?',
    confirmRemoveCompanion: 'Remove companion "{name}"?',
    confirmResetStats: 'Reset play stats for "{name}"?',
    confirmCancel: 'Cancel', confirmDelete: 'Delete', confirmReset: 'Reset',
    detailsTitle: 'Selected Game Details',
    detailsEmptyHint: 'No games yet.',
    detailsAllGames: 'All Games',
    detailsGameOptions: 'Main Game Options',
    detailsRunAsAdmin: 'Run as Administrator',
    detailsShowInRadial: 'Show in game circle',
    detailsLaunchArgs: 'Launch Arguments',
    detailsItemColor: 'Ring / glow color',
    detailsPickCustomColor: 'Pick custom color…',
    detailsRadialBg: 'Custom circular background',
    detailsRadialBgHint: 'Pick an image to appear as circular background.',
    detailsRadialBgChoose: 'Choose image…',
    detailsRadialBgClear: 'Remove background',
    detailsHideOriginalIcon: 'Hide original game icon',
    detailsQuickSlot: 'Quick launch shortcut',
    detailsQuickSlotHint: 'Press the number inside the circle to launch the game.',
    quickSlotNone: 'No shortcut',
    detailsGameLanguage: 'Game language inside the app',
    detailsGameLanguageHint: 'When you launch this game, keyboard layout will switch automatically and revert on exit.',
    langOff: 'No switch',
    langArabic: 'Arabic',
    langEnglish: 'English',
    detailsCompanionsOptions: 'Companion Programs Options',
    detailsNoCompanions: 'No companion programs.',
    detailsPerformanceTitle: 'Performance Options (Advanced)',
    detailsPriorityLabel: 'Process Priority',
    detailsAffinityLabel: 'CPU Affinity',
    selectAllCores: 'Select all', clearAllCores: 'Clear',
    quickAccessTitle: 'Quick Access',
    quickAccessHint: 'Click to jump',
    companionShowInRadial: 'Launch with game',
    bgPanelTitle: 'Control Panel Background',
    bgPanelChoose: 'Choose image…', bgPanelClear: 'Reset',
    bgRadialTitle: 'Game Circle Background',
    bgRadialChoose: 'Choose image…', bgRadialClear: 'Reset',
    exeListTitle: 'Executables Found',
    exeListSubtitle: 'Found {count} files inside "{folder}"',
    exeListSelectAll: 'Select all', exeListClear: 'Clear',
    exeListAddSelected: 'Add Selected',
    exeListCounter: 'Selected: {n} of {total}',
    exeListMainBadge: 'Likely main',
    searchPlaceholder: 'Search games...',
    searchEmpty: 'No matching results.',
    sortManual: 'Manual order',
    sortFavorite: 'Favorites first',
    sortRecent: 'Recently played',
    sortMostPlayed: 'Most played',
    sortName: 'Name (A-Z)',
    playStatsTitle: 'Play Stats',
    statsTotalTime: 'Total play time',
    statsLastPlayed: 'Last played',
    statsSessions: 'Sessions',
    statsReset: 'Reset stats',
    never: 'Never played', justNow: 'Just now',
    minutesAgo: '{n} min ago', hoursAgo: '{n} h ago', daysAgo: '{n} d ago',
    timeHM: '{h}h {m}m', timeMS: '{m}m {s}s', timeS: '{s}s',
    sessionsUnit: 'sessions',
    muteEnableTitle: 'Enable quick-mute hotkey',
    muteHotkeyTitle: 'Quick-mute hotkey',
    runningLabel: 'Running',
    openFolderTooltip: 'Open the game file location',
    backupTitle: 'Backup',
    backupDesc: 'Save your settings as a file, or restore from a previous backup.',
    backupExportBtn: 'Save backup',
    backupImportBtn: 'Import backup',
    mutedAppsTitle: 'Currently muted applications',
    noMutedApps: 'No muted applications right now.',
    unmuteBtn: 'Unmute',
    hubAlwaysVisibleTitle: 'Always show Hub button',
    hubAlwaysVisibleDesc: 'Show the open-panel button in the center without hovering.',
    logTitle: 'Diagnostic log file',
    logDesc: 'Open the log to diagnose launch issues and errors.',
    logOpenBtn: 'Open log file',
    controllerEnableTitle: 'Enable Xbox Controller',
    controllerEnableDesc: 'Allows opening the circle with an Xbox button.',
    controllerButtonTitle: 'Button that opens the circle',
    controllerButtonDesc: 'Default: Start button.',
    controllerToggleModeTitle: 'Toggle mode (open/close)',
    controllerToggleModeDesc: 'When ON: pressing again closes the circle. When OFF: pressing opens only.',
    controllerDuringGameTitle: 'Open circle during gameplay',
    controllerDuringGameDesc: 'Allow the controller button to open the circle during gameplay.',
    panelGlassTitle: 'Real glass effect for the panel',
    panelGlassDesc: 'Acrylic transparent effect that shows the desktop behind the panel.',
    overlayEnableTitle: 'Enable Overlay (CPU / RAM)',
    overlayEnableDesc: 'A small transparent window showing system info during gameplay.',
    overlayHotkeyTitle: 'Overlay show/hide hotkey',
    overlayShowOnLaunchTitle: 'Show automatically on game launch',
    overlayOpacityLabel: 'Overlay transparency',
    ctrlBtnStart: 'Start', ctrlBtnBack: 'Back',
    ctrlBtnA: 'A', ctrlBtnB: 'B', ctrlBtnX: 'X', ctrlBtnY: 'Y',
    ctrlBtnLB: 'LB (Left Shoulder)', ctrlBtnRB: 'RB (Right Shoulder)',
    ctrlBtnLS: 'Left Stick', ctrlBtnRS: 'Right Stick', ctrlBtnGuide: 'Xbox (Guide)',
    boostRestoreTitle: 'Restore Boost Settings',
    boostRestoreDesc: 'If you notice system settings were changed by Boost, click to restore original values.',
    boostRestoreBtn: 'Restore System Defaults',
    boostRestoreConfirm: 'Restore the original system values modified by Boost?',
    detailsBlackBoxTitle: 'Performance Monitor (Black Box)',
    detailsBlackBoxEnable: 'Enable performance monitoring for this game',
    detailsBlackBoxEnableHint: 'Collects FPS, temperature and usage during play for a detailed report.',
    detailsBlackBoxWarning: 'Warning: reading CPU temperature may conflict with anti-cheat in competitive games. FPS and GPU Temp are completely safe.',
    detailsBlackBoxFps: 'FPS + Frame Time',
    detailsBlackBoxGpuTemp: 'GPU Temperature',
    detailsBlackBoxRam: 'RAM / VRAM',
    detailsBlackBoxAdmin: 'Admin',
    detailsBlackBoxSafe: 'Safe',
    detailsBlackBoxViewReport: 'View Report',
    restartAsAdminTitle: 'Run as Administrator',
    restartAsAdminDesc: 'Required for FPS. Does not affect running games.',
    restartAsAdminBtn: 'Restart as Administrator',
    adminYes: 'Admin',
    adminNo: 'Not Admin',
    blackBoxNeedsAdmin: 'To enable FPS, GameLauncher needs Administrator rights. Click "Restart as Administrator" in Settings, or continue without FPS.',
    boostFpsTitle: 'Boost FPS',
    boostFpsDesc: 'Automatic optimizations on game launch, reverted on exit.',
    boostDisableCore0: 'Disable Core 0',
    boostHighPriority: 'Raise priority to High',
    boostStopStats: 'Stop stats & Overlay',
    boostTimerResolution: 'Improve timer resolution (1ms)',
    boostSystemResponsiveness: 'SystemResponsiveness',
    boostMmcss: 'MMCSS Game Priority',
    boostFpsHint: 'Boost results may vary per game. Some options require Admin rights.',
    perfReportTitle: 'Performance Report',
    perfSessionsTitle: 'Previous Sessions',
    perfNoSessions: 'No saved sessions yet. Play the game with monitoring enabled first.',
    perfSelectSessionHint: 'Select a session from the list to see its details',
    perfSessionOn: 'On',
    perfDuration: 'Duration',
    perfAvgFps: 'Avg FPS',
    perfMinFps: 'Min FPS',
    perfMaxFps: 'Max FPS',
    perf1PercentLow: '1% Low',
    perf01PercentLow: '0.1% Low',
    perfAvgCpuUsage: 'Avg CPU',
    perfMaxCpuUsage: 'Max CPU',
    perfAvgGpuUsage: 'Avg GPU',
    perfMaxGpuUsage: 'Max GPU',
    perfAvgCpuTemp: 'CPU Temp (avg)',
    perfMaxCpuTemp: 'CPU Temp (max)',
    perfAvgGpuTemp: 'GPU Temp (avg)',
    perfMaxGpuTemp: 'GPU Temp (max)',
    perfAvgCpuPower: 'CPU Power (avg)',
    perfAvgGpuPower: 'GPU Power (avg)',
    perfAvgRam: 'RAM (avg)',
    perfMaxRam: 'RAM (max)',
    perfAvgVram: 'VRAM (avg)',
    perfMaxVram: 'VRAM (max)',
    perfStutters: 'Stutters',
    perfExportCsvBtn: 'Export CSV',
    perfDeleteSession: 'Delete session',
    perfConfirmDeleteSession: 'Delete this session?',
    perfConfirmDeleteAll: 'Delete ALL saved sessions for this game?',
    perfDeleteAll: 'Delete all',
    perfFpsNeedsAdmin: 'FPS requires GameLauncher to run as Administrator. Close it, then run as Administrator to enable FPS.',
    perfFpsEtwFailed: 'ETW failed to start this time. Try restarting GameLauncher as Administrator. If the issue persists, reboot your PC.',
    perfCsvSaved: 'CSV exported successfully.',
    perfCsvFailed: 'Failed to export CSV.',
    perfSessionDeleted: 'Session deleted.',
    perfAllDeleted: 'All sessions deleted.',
    perfChartFpsTitle: 'FPS + Frame Time over time',
    perfChartTempTitle: 'Temperature (CPU + GPU)',
    perfChartRamTitle: 'RAM / VRAM Usage',
    statsHistory: 'Session History',
    statsHistoryTitle: 'Session History',
    statsHistoryLoading: 'Loading...',
    statsHistoryEmpty: 'No sessions logged yet. Play the game to start logging.',
    statsHistoryDays: 'Days:',
    statsHistorySessions: 'Sessions:',
    statsHistorySessionsShort: 'sessions',
    statsHistoryTotal: 'Total:',
    statsHistoryClear: 'Clear History',
    statsHistoryClearConfirm: 'Delete ALL session history for this game? This cannot be undone.',
    designCustomThemeBtn: 'Design My Own Theme',
    designCustomStyleBtn: 'Design My Own Style',
    radialScaleTitle: 'Game Circle Scale',
    radialScaleReset: 'Reset',
    radialScaleIcon: 'Icon size',
    radialScaleHub: 'Hub button size',
    radialScaleOrbit: 'Orbit distance',
    customThemeTitle: 'Design My Theme',
    customThemeSubtitle: 'Choose 4 colors + adjust 3 values',
    customThemeAccent: 'Accent Color',
    customThemeSecondary: 'Secondary Color',
    customThemeBg: 'Background',
    customThemeCard: 'Cards',
    customThemeCardOpacity: 'Card opacity',
    customThemeAccentStrength: 'Accent strength',
    customThemeBgGlow: 'Background glow',
    customThemePreview: 'Preview',
    customThemePreviewBtn: 'Primary',
    customThemePreviewBtn2: 'Secondary',
    customThemeReset: 'Reset',
    customThemeSave: 'Save & Activate',
    customStyleTitle: 'Design My Style',
    customStyleSubtitle: 'Adjust 5 values to build your ring',
    customStyleRingCount: 'Ring count',
    customStyleRingThickness: 'Ring thickness',
    customStyleDashed: 'Dashed ring',
    customStyleDashedHint: 'When enabled, rings appear as dashed segments',
    customStyleGlowLayers: 'Glow layers',
    customStyleGlowStrength: 'Glow strength',
    customStyleReset: 'Reset',
    customStyleSave: 'Save & Activate',
  }
};

let state = { lang: 'ar', hotkey: '—', muteHotkey: '—', muteEnabled: false,
              engineRunning: false, autoStart: false,
              radialStyle: 1, radialStyleNames: [], games: [],
              panelBackground: '', radialBackground: '', panelPreset: 'purple',
              controllerEnabled: false, controllerButton: 0x0010, controllerButtonName: 'Start',
              controllerToggleMode: true,
              allowControllerDuringGame: false,
              panelGlassEffect: false,
              overlayEnabled: false, overlayHotkey: '—', overlayShowOnGameLaunch: false, overlayOpacity: 0.85,
              cpuCoreCount: 8, processPriorityNames: [], radialTransparency: 0.72,
              hubAlwaysVisible: false, isAdmin: false, radialNoGlow: false,
              radialScale: { iconSize: 42, hubSize: 46, orbitDist: 118 },
              customTheme: { accent:'7c3aed', secondary:'a855f7', bg:'150f2c', card:'1e1636', cardOpacity:0.32, accentStrength:0.70, bgGlow:0.40, isActive:false },
              customRadialStyle: { ringCount:2, ringThickness:2, dashed:false, glowLayers:1, glowStrength:0.60, isActive:false } };

let selectedGameIndex = -1;
let selectedGamePath = null;
let reorderDragIndex = null;
let capturingHotkey = false;
let capturingMuteHotkey = false;
let capturingOverlayHotkey = false;
let detailsGameIndex = -1;
let qaActiveTarget = 'game';
let exeListItems = [];
let exeListSelected = new Set();
let searchQuery = '';
let sortMode = 'manual';
let mutedAppsCache = [];
let mutedAppsTimerId = null;
let pendingAffinityMask = 0;

// Performance Report state
let perfReportGameIndex = -1;
let perfReportGamePath = '';
let perfReportSessions = [];
let perfReportCurrentFile = '';
let perfCharts = [];

// Session History state
let sessionHistoryGameIndex = -1;
let sessionHistoryData = [];

// Custom Theme draft state
let customThemeDraft = { accent:'7c3aed', secondary:'a855f7', bg:'150f2c', card:'1e1636',
                          cardOpacity:0.32, accentStrength:0.70, bgGlow:0.40 };

// Custom Radial Style draft state
let customStyleDraft = { ringCount:2, ringThickness:2, dashed:false, glowLayers:1, glowStrength:0.60 };

function t(key) { return (I18N[state.lang] || I18N.ar)[key] || key; }
function tFormat(key, params) {
  let s = t(key);
  Object.keys(params || {}).forEach(k => { s = s.split('{' + k + '}').join(params[k]); });
  return s;
}
function formatPlayTime(seconds) {
  if (!seconds || seconds <= 0) return t('never');
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);
  if (h > 0) return tFormat('timeHM', { h, m });
  if (m > 0) return tFormat('timeMS', { m, s });
  return tFormat('timeS', { s });
}
function formatRelativeTime(unixSeconds) {
  if (!unixSeconds || unixSeconds <= 0) return t('never');
  const now = Math.floor(Date.now() / 1000);
  const diff = now - unixSeconds;
  if (diff < 60) return t('justNow');
  if (diff < 3600) return tFormat('minutesAgo', { n: Math.floor(diff / 60) });
  if (diff < 86400) return tFormat('hoursAgo', { n: Math.floor(diff / 3600) });
  return tFormat('daysAgo', { n: Math.floor(diff / 86400) });
}

function escapeHtml(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

// ✅ تطبيق preset الثيم (جاهز أو مخصص)
function applyPreset(presetId) {
  const root = document.documentElement;
  if (presetId === 'custom') {
    const ct = state.customTheme || {};
    // حساب قيم accent-2 و accent-3 (نسخ أفتح/أغمق)
    const hexToRgb = (hex) => {
      hex = (hex || '').replace('#','');
      if (hex.length < 6) return { r:124, g:58, b:237 };
      return { r: parseInt(hex.substr(0,2),16), g: parseInt(hex.substr(2,2),16), b: parseInt(hex.substr(4,2),16) };
    };
    const rgbToHex = (r,g,b) => '#' + [r,g,b].map(v => Math.max(0,Math.min(255,Math.round(v))).toString(16).padStart(2,'0')).join('');
    const lighten = (hex, amt) => { const c = hexToRgb(hex); return rgbToHex(c.r + (255-c.r)*amt, c.g + (255-c.g)*amt, c.b + (255-c.b)*amt); };
    const darken  = (hex, amt) => { const c = hexToRgb(hex); return rgbToHex(c.r*(1-amt), c.g*(1-amt), c.b*(1-amt)); };

    const accent    = '#' + (ct.accent    || '7c3aed');
    const secondary = '#' + (ct.secondary || 'a855f7');
    const bg        = '#' + (ct.bg        || '150f2c');
    const card      = '#' + (ct.card      || '1e1636');
    const strength  = (ct.accentStrength ?? 0.70);

    const accent2 = lighten(accent, 0.15 * strength + 0.05);
    const accent3 = darken(accent, 0.10);

    // bg-1/bg-2/bg-3 : نشتق تدرجات من لون الخلفية
    const bg1 = bg;
    const bg2 = darken(bg, 0.15);
    const bg3 = lighten(bg, 0.05);

    // panel-tint / card-tint
    const panelRgb = hexToRgb(card.replace('#',''));
    const cardRgb = hexToRgb(card.replace('#',''));
    const accentRgb = hexToRgb(accent.replace('#',''));
    const accentHiRgb = hexToRgb(accent2.replace('#',''));

    root.style.setProperty('--accent-1', accent);
    root.style.setProperty('--accent-2', accent2);
    root.style.setProperty('--accent-3', accent3);
    root.style.setProperty('--bg-1', bg1);
    root.style.setProperty('--bg-2', bg2);
    root.style.setProperty('--bg-3', bg3);
    root.style.setProperty('--accent-rgb', `${accentRgb.r}, ${accentRgb.g}, ${accentRgb.b}`);
    root.style.setProperty('--accent-hi-rgb', `${accentHiRgb.r}, ${accentHiRgb.g}, ${accentHiRgb.b}`);
    root.style.setProperty('--panel-tint', `${panelRgb.r}, ${panelRgb.g}, ${panelRgb.b}`);
    root.style.setProperty('--card-tint', `${cardRgb.r}, ${cardRgb.g}, ${cardRgb.b}`);
    root.style.setProperty('--glass-opacity', ct.cardOpacity ?? 0.32);
    root.style.setProperty('--card-border', '255, 255, 255');
    root.setAttribute('data-preset', 'custom');
  } else {
    // preset جاهز — نشيل أي خصائص inline مضافة من custom
    ['--accent-1','--accent-2','--accent-3','--bg-1','--bg-2','--bg-3',
     '--accent-rgb','--accent-hi-rgb','--panel-tint','--card-tint','--card-border'].forEach(p => {
      root.style.removeProperty(p);
    });
    root.setAttribute('data-preset', presetId || 'purple');
  }
}

function showConfirmModal(message, onConfirm, opts) {
  opts = opts || {};
  const overlay = document.getElementById('confirmModalOverlay');
  const iconWrap = document.getElementById('confirmModalIconWrap');
  const icon = document.getElementById('confirmModalIcon');
  document.getElementById('confirmModalText').innerText = message;
  document.getElementById('confirmModalCancelBtn').innerText = opts.cancelLabel || t('confirmCancel');
  document.getElementById('confirmModalOkBtn').innerText = opts.okLabel || t('confirmDelete');
  document.getElementById('confirmModalOkBtn').className = 'flex-1 py-2 rounded-xl text-xs font-bold transition text-white ' + (opts.okColorClass || 'bg-red-600/80 hover:bg-red-600');
  iconWrap.className = 'w-12 h-12 mx-auto rounded-full flex items-center justify-center mb-3 ' + (opts.iconWrapClass || 'bg-red-500/15 border border-red-500/30');
  icon.className = 'fa-solid ' + (opts.iconClass || 'fa-trash') + ' ' + (opts.iconColorClass || 'text-red-400');
  overlay.classList.remove('hidden');
  const cancelBtn = document.getElementById('confirmModalCancelBtn');
  const okBtn = document.getElementById('confirmModalOkBtn');
  const close = () => overlay.classList.add('hidden');
  const onCancel = () => { close(); cleanup(); };
  const onOk = () => { close(); cleanup(); onConfirm(); };
  function cleanup() { cancelBtn.removeEventListener('click', onCancel); okBtn.removeEventListener('click', onOk); }
  cancelBtn.addEventListener('click', onCancel);
  okBtn.addEventListener('click', onOk);
  setTimeout(() => okBtn.focus(), 50);
}
function confirmRemoveGame(index, name) {
  showConfirmModal(tFormat('confirmRemoveGame', { name }), () => nativeAction('removeGame', { index }));
}
function confirmRemoveCompanion(gameIndex, companionIndex, name) {
  showConfirmModal(tFormat('confirmRemoveCompanion', { name }), () => nativeAction('removeCompanion', { gameIndex, companionIndex }));
}
function reorderGameTo(fromIndex, toIndex) {
  const order = state.games.map(g => g.index);
  const fromPos = order.indexOf(fromIndex);
  const toPos = order.indexOf(toIndex);
  if (fromPos === -1 || toPos === -1) return;
  order.splice(toPos, 0, order.splice(fromPos, 1)[0]);
  nativeAction('reorderGames', { order });
}
function nativeAction(action, payload) {
  const msg = Object.assign({ action }, payload || {});
  window.chrome.webview.postMessage(JSON.stringify(msg));
}
window.chrome.webview.addEventListener('message', (e) => {
  const msg = JSON.parse(e.data);
  if (msg.type === 'state') { state = msg; onStateUpdated(); }
  else if (msg.type === 'statsUpdate') { updateGameStats(msg.games || []); }
  else if (msg.type === 'status') { showStatus(msg.text, msg.kind || 'info'); }
  else if (msg.type === 'suggestExe') { showSuggestExeModal(msg.folder, msg.path); }
  else if (msg.type === 'suggestExeList') { showExeListModal(msg.folder, msg.items || []); }
  else if (msg.type === 'mutedAppsList') { renderMutedApps(msg.apps || []); }
  else if (msg.type === 'perfSessions') { renderPerformanceSessions(msg.sessions || []); }
  else if (msg.type === 'perfReport') { renderPerformanceReport(msg.report || null); }
  else if (msg.type === 'playSessions') { renderSessionHistory(msg.sessions || []); }
  else if (msg.type === 'customThemeColorResult') { onCustomThemeColorResult(msg.which, msg.hex); }
});


function updateGameStats(stats) {
  let needsRunningRerender = false;
  stats.forEach(s => {
    const game = state.games.find(g => g.index === s.index);
    if (!game) return;
    if (game.isRunning !== s.isRunning || game.muted !== s.muted) {
      needsRunningRerender = true;
    }
    game.totalPlaySeconds = s.totalPlaySeconds;
    game.lastPlayedUnix = s.lastPlayedUnix;
    game.playCount = s.playCount;
    game.isRunning = s.isRunning;
    game.muted = s.muted;
  });

  if (detailsGameIndex >= 0) {
    const game = state.games.find(g => g.index === detailsGameIndex);
    if (game) {
      const el1 = document.getElementById('statTotalTime');
      if (el1) el1.innerText = formatPlayTime(game.totalPlaySeconds);
      const el2 = document.getElementById('statLastPlayed');
      if (el2) el2.innerText = formatRelativeTime(game.lastPlayedUnix);
      const el3 = document.getElementById('statSessions');
      if (el3) el3.innerText = (game.playCount || 0) + ' ' + t('sessionsUnit');
    }
  }

  if (needsRunningRerender) {
    renderGamesList();
    renderCompanions();
  }
}

function showSuggestExeModal(folderName, exePath) {
  const fileName = exePath.split(/[\\/]/).pop();
  showConfirmModal(tFormat('suggestExeMessage', { folder: folderName, file: fileName }),
    () => nativeAction('addSuggestedExe', { path: exePath }),
    { okLabel: t('suggestExeAdd'), cancelLabel: t('confirmCancel'),
      okColorClass: 'bg-purple-600/80 hover:bg-purple-600',
      iconWrapClass: 'bg-purple-500/15 border border-purple-500/30',
      iconClass: 'fa-folder-open', iconColorClass: 'text-purple-300' });
}
function showExeListModal(folderName, items) {
  exeListItems = items;
  exeListSelected = new Set();
  items.forEach((it, i) => { if (it.main) exeListSelected.add(i); });
  if (exeListSelected.size === 0 && items.length > 0) exeListSelected.add(0);
  document.getElementById('exeListSubtitle').innerText = tFormat('exeListSubtitle', { count: items.length, folder: folderName });
  const container = document.getElementById('exeListContainer');
  container.innerHTML = '';
  items.forEach((it, i) => {
    const row = document.createElement('div');
    row.className = 'exe-row' + (exeListSelected.has(i) ? ' selected' : '');
    row.dataset.idx = i;
    row.tabIndex = 0;
    row.innerHTML = `
      <div class="chk"><i class="fa-solid fa-check"></i></div>
      <img src="${it.icon || ''}" class="w-8 h-8 rounded object-contain bg-black/30 border border-white/10 shrink-0" onerror="this.style.display='none'">
      <div class="flex-1 min-w-0">
        <div class="flex items-center gap-2">
          <p class="text-xs font-bold text-white truncate">${escapeHtml(it.name)}</p>
          ${it.main ? `<span class="main-badge">${escapeHtml(t('exeListMainBadge'))}</span>` : ''}
        </div>
        <p class="text-[10px] text-gray-500 font-mono truncate" dir="ltr">${escapeHtml(it.path)}</p>
      </div>
      <span class="text-[10px] text-gray-400 font-mono shrink-0">${formatBytes(it.size)}</span>`;
    row.onclick = () => {
      if (exeListSelected.has(i)) exeListSelected.delete(i);
      else exeListSelected.add(i);
      row.classList.toggle('selected', exeListSelected.has(i));
      updateExeListCounter();
    };
    row.addEventListener('keydown', (e) => { if (e.key === ' ' || e.key === 'Enter') { e.preventDefault(); row.click(); } });
    container.appendChild(row);
  });
  updateExeListCounter();
  const overlay = document.getElementById('exeListModalOverlay');
  overlay.classList.remove('hidden');
  const cancelBtn = document.getElementById('exeListCancelBtn');
  const okBtn = document.getElementById('exeListOkBtn');
  const close = () => overlay.classList.add('hidden');
  const onCancel = () => { close(); cleanup(); };
  const onOk = () => {
    const paths = [];
    exeListSelected.forEach(i => { if (exeListItems[i]) paths.push(exeListItems[i].path); });
    close(); cleanup();
    if (paths.length > 0) nativeAction('addSelectedExes', { paths });
  };
  function cleanup() { cancelBtn.removeEventListener('click', onCancel); okBtn.removeEventListener('click', onOk); }
  cancelBtn.addEventListener('click', onCancel);
  okBtn.addEventListener('click', onOk);
}
function exeListSelectAll(select) {
  exeListSelected = new Set();
  if (select) exeListItems.forEach((_, i) => exeListSelected.add(i));
  const container = document.getElementById('exeListContainer');
  container.querySelectorAll('.exe-row').forEach(row => {
    const i = parseInt(row.dataset.idx, 10);
    row.classList.toggle('selected', exeListSelected.has(i));
  });
  updateExeListCounter();
}
function updateExeListCounter() {
  const total = exeListItems.length;
  const n = exeListSelected.size;
  document.getElementById('exeListCounter').innerText = tFormat('exeListCounter', { n, total });
  document.getElementById('exeListOkBtn').disabled = (n === 0);
}
function formatBytes(b) {
  if (!b || b <= 0) return '';
  const units = ['B','KB','MB','GB'];
  let i = 0; let v = b;
  while (v >= 1024 && i < units.length - 1) { v /= 1024; i++; }
  return v.toFixed(v >= 100 ? 0 : 1) + ' ' + units[i];
}
function showStatus(text, kind) {
  const badge = document.getElementById('statusBadge');
  const tt = document.getElementById('statusBadgeText');
  tt.innerText = text;
  badge.className = 'flex items-center gap-2 px-3 py-1 rounded-full font-semibold ' +
    (kind === 'error' ? 'bg-red-500/10 border border-red-500/30 text-red-400'
      : kind === 'warn' ? 'bg-amber-500/10 border border-amber-500/30 text-amber-300'
      : 'bg-emerald-500/10 border border-emerald-500/30 text-emerald-400');
}
function applyPanelBackground(path) {
  const root = document.getElementById('bodyRoot');
  if (!path) { root.style.backgroundImage = ''; return; }
  const url = 'file:///' + path.replace(/\\/g, '/');
  const overlay = state.panelGlassEffect
    ? 'none'
    : 'linear-gradient(rgba(13,11,24,0.30), rgba(13,11,24,0.48))';
  root.style.backgroundImage = (overlay === 'none')
    ? `url("${url.replace(/"/g, '\\"')}")`
    : `${overlay}, url("${url.replace(/"/g, '\\"')}")`;
  root.style.backgroundSize = 'cover';
  root.style.backgroundPosition = 'center';
  root.style.backgroundAttachment = 'fixed';
}
function applyI18nStaticText() {
  document.documentElement.lang = state.lang;
  document.documentElement.dir = state.lang === 'en' ? 'ltr' : 'rtl';
  document.querySelectorAll('[data-t]').forEach(el => {
    const key = el.getAttribute('data-t');
    if (el.tagName === 'INPUT' && el.hasAttribute('placeholder')) el.setAttribute('placeholder', t(key));
    else el.innerText = t(key);
  });
  const langSel = document.getElementById('langSelect');
  if (langSel) langSel.value = state.lang;
}

function onStateUpdated() {
  applyPreset(state.panelPreset || 'purple');
  applyI18nStaticText();
  applyPanelBackground(state.panelBackground || '');
  document.getElementById('hotkeyBadge').innerText = state.hotkey;
  document.getElementById('hotkeyCaptureBtn').innerText = state.hotkey;
  document.getElementById('muteHotkeyBadge').innerText = state.muteHotkey || '—';
  document.getElementById('muteHotkeyCaptureBtn').innerText = state.muteHotkey || '—';
  const muteBadgeWrap = document.getElementById('muteBadgeWrap');
  muteBadgeWrap.style.opacity = state.muteEnabled ? '1' : '0.4';
  document.getElementById('muteEnabledToggle').classList.toggle('on', !!state.muteEnabled);
  document.getElementById('controllerEnabledToggle').classList.toggle('on', !!state.controllerEnabled);
  document.getElementById('hubAlwaysVisibleToggle').classList.toggle('on', !!state.hubAlwaysVisible);

  const ctrlBtnRow = document.getElementById('controllerButtonRow');
  const ctrlToggleRow = document.getElementById('controllerToggleModeRow');
  const ctrlDuringGameRow = document.getElementById('controllerDuringGameRow');
  if (state.controllerEnabled) {
    ctrlBtnRow.classList.remove('opacity-40', 'pointer-events-none');
    if (ctrlToggleRow) ctrlToggleRow.classList.remove('opacity-40', 'pointer-events-none');
    if (ctrlDuringGameRow) ctrlDuringGameRow.classList.remove('opacity-40', 'pointer-events-none');
  } else {
    ctrlBtnRow.classList.add('opacity-40', 'pointer-events-none');
    if (ctrlToggleRow) ctrlToggleRow.classList.add('opacity-40', 'pointer-events-none');
    if (ctrlDuringGameRow) ctrlDuringGameRow.classList.add('opacity-40', 'pointer-events-none');
  }
  const ctrlSel = document.getElementById('controllerButtonSelect');
  const curBtnHex = '0x' + (state.controllerButton >>> 0).toString(16).padStart(4, '0');
  ctrlSel.value = curBtnHex;

  const ctrlToggleBtn = document.getElementById('controllerToggleModeBtn');
  if (ctrlToggleBtn) ctrlToggleBtn.classList.toggle('on', !!state.controllerToggleMode);

  const ctrlDuringGameBtn = document.getElementById('controllerDuringGameBtn');
  if (ctrlDuringGameBtn) ctrlDuringGameBtn.classList.toggle('on', !!state.allowControllerDuringGame);

  const glassToggle = document.getElementById('panelGlassToggle');
  if (glassToggle) glassToggle.classList.toggle('on', !!state.panelGlassEffect);
  document.documentElement.classList.toggle('glass-mode', !!state.panelGlassEffect);

  const overlayEnabledToggle = document.getElementById('overlayEnabledToggle');
  if (overlayEnabledToggle) overlayEnabledToggle.classList.toggle('on', !!state.overlayEnabled);
  const overlayOptionsWrap = document.getElementById('overlayOptionsWrap');
  if (overlayOptionsWrap) {
    if (state.overlayEnabled) overlayOptionsWrap.classList.remove('opacity-40', 'pointer-events-none');
    else overlayOptionsWrap.classList.add('opacity-40', 'pointer-events-none');
  }
  const overlayHotkeyCaptureBtn = document.getElementById('overlayHotkeyCaptureBtn');
  if (overlayHotkeyCaptureBtn) overlayHotkeyCaptureBtn.innerText = state.overlayHotkey || '—';
  const overlayShowOnLaunchToggle = document.getElementById('overlayShowOnLaunchToggle');
  if (overlayShowOnLaunchToggle) overlayShowOnLaunchToggle.classList.toggle('on', !!state.overlayShowOnGameLaunch);
  const overlayOpacitySlider = document.getElementById('overlayOpacitySlider');
  if (overlayOpacitySlider && document.activeElement !== overlayOpacitySlider) {
    const op = typeof state.overlayOpacity === 'number' ? state.overlayOpacity : 0.85;
    overlayOpacitySlider.value = op;
    document.getElementById('overlayOpacityVal').innerText = Math.round(op * 100) + '%';
  }

  const engineBtn = document.getElementById('engineToggleBtn');
  const engineTxt = document.getElementById('engineToggleText');
  if (state.engineRunning) {
    showStatus(t('engineRunning'), 'info');
    engineTxt.innerText = t('stopEngine');
    engineBtn.className = 'w-full py-2 px-3 rounded-xl text-xs font-bold transition flex items-center justify-center gap-2 bg-red-600/20 hover:bg-red-600/30 text-red-300 border border-red-500/30';
  } else {
    showStatus(t('engineStopped'), 'warn');
    engineTxt.innerText = t('startEngine');
    engineBtn.className = 'w-full py-2 px-3 rounded-xl text-xs font-bold transition flex items-center justify-center gap-2 bg-emerald-600/20 hover:bg-emerald-600/30 text-emerald-300 border border-emerald-500/30';
  }
  document.getElementById('autoStartToggle').classList.toggle('on', !!state.autoStart);
  document.getElementById('totalGamesCount').innerText = state.games.length + ' ' + t('gamesUnit');

  const rt = typeof state.radialTransparency === 'number' ? state.radialTransparency : 0.72;
  const rSlider = document.getElementById('radialOpacitySlider');
  if (rSlider && document.activeElement !== rSlider) {
    rSlider.value = rt;
    document.getElementById('radialOpacityVal').innerText = Math.round(rt * 100) + '%';
  }

  // ✅ radial scale — تحديث السلايدرز
  if (state.radialScale) {
    const rs = state.radialScale;
    const sliders = [
      { id: 'scaleIconSlider',  valId: 'scaleIconVal',  v: rs.iconSize,  unit: ' px' },
      { id: 'scaleHubSlider',   valId: 'scaleHubVal',   v: rs.hubSize,   unit: ' px' },
      { id: 'scaleOrbitSlider', valId: 'scaleOrbitVal', v: rs.orbitDist, unit: ' px' },
    ];
    sliders.forEach(({ id, valId, v, unit }) => {
      const el = document.getElementById(id);
      const valEl = document.getElementById(valId);
      if (el && document.activeElement !== el) el.value = v;
      if (valEl) valEl.innerText = v + unit;
    });
  }

  // ✅ custom theme — تحديث الحالة الداخلية
  if (state.customTheme) {
    state.customTheme = Object.assign(
      { accent:'7c3aed', secondary:'a855f7', bg:'150f2c', card:'1e1636', cardOpacity:0.32, accentStrength:0.70, bgGlow:0.40, isActive:false },
      state.customTheme
    );
  }
  if (state.customRadialStyle) {
    state.customRadialStyle = Object.assign(
      { ringCount:2, ringThickness:2, dashed:false, glowLayers:1, glowStrength:0.60, isActive:false },
      state.customRadialStyle
    );
  }

  const adminBadge = document.getElementById('adminBadge');
  if (adminBadge) {
    adminBadge.innerText = state.isAdmin ? t('adminYes') : t('adminNo');
    adminBadge.className = 'text-[10px] px-2 py-0.5 rounded-full font-mono ' +
      (state.isAdmin
        ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/30'
        : 'bg-amber-500/20 text-amber-300 border border-amber-500/30');
  }
  const restartBtn = document.getElementById('restartAsAdminBtn');
  if (restartBtn) {
    restartBtn.style.display = state.isAdmin ? 'none' : 'inline-flex';
  }

  const noglowToggle = document.getElementById('radialNoGlowToggle');
  if (noglowToggle) noglowToggle.classList.toggle('on', !!state.radialNoGlow);

  if (selectedGamePath) {
    const match = state.games.find(g => g.path === selectedGamePath);
    selectedGameIndex = match ? match.index : -1;
    if (!match) selectedGamePath = null;
  }
  if (selectedGameIndex >= state.games.length) selectedGameIndex = -1;
  renderGamesList();
  renderCompanions();
  renderRadialStyles();
  renderPresets();
  if (!document.getElementById('tab-details').classList.contains('hidden')) {
    if (detailsGameIndex >= 0 && !state.games.find(g => g.index === detailsGameIndex)) detailsGameIndex = -1;
    renderGameDetails();
  }
}

function onSearchChange(value) { searchQuery = (value || '').toLowerCase().trim(); renderGamesList(); }

function restoreBoostDefaults() {
  showConfirmModal(
    t('boostRestoreConfirm'),
    () => nativeAction('restoreBoostDefaults'),
    { okLabel: t('confirmReset'),
      okColorClass: 'bg-amber-600/80 hover:bg-amber-600',
      iconWrapClass: 'bg-amber-500/15 border border-amber-500/30',
      iconClass: 'fa-rotate-left', iconColorClass: 'text-amber-300' });
}

function onSortChange(value) { sortMode = value || 'manual'; renderGamesList(); }
function getSortedFilteredGames() {
  let list = state.games.slice();
  if (searchQuery) list = list.filter(g => (g.name || '').toLowerCase().includes(searchQuery) || (g.path || '').toLowerCase().includes(searchQuery));
  switch (sortMode) {
    case 'favorite': list.sort((a, b) => { const fa = a.favorite ? 1 : 0, fb = b.favorite ? 1 : 0; if (fa !== fb) return fb - fa; return a.index - b.index; }); break;
    case 'recent': list.sort((a, b) => { const la = a.lastPlayedUnix || 0, lb = b.lastPlayedUnix || 0; if (la !== lb) return lb - la; return a.index - b.index; }); break;
    case 'mostPlayed': list.sort((a, b) => { const pa = a.totalPlaySeconds || 0, pb = b.totalPlaySeconds || 0; if (pa !== pb) return pb - pa; return a.index - b.index; }); break;
    case 'name': list.sort((a, b) => a.name.localeCompare(b.name)); break;
    default: break;
  }
  return list;
}
function renderGamesList() {
  const container = document.getElementById('gamesListContainer');
  const empty = document.getElementById('searchEmpty');
  container.innerHTML = '';
  const filtered = getSortedFilteredGames();
  document.getElementById('gamesListCount').innerText = state.games.length;
  if (state.games.length > 0 && filtered.length === 0) { empty.classList.remove('hidden'); return; }
  empty.classList.add('hidden');
  filtered.forEach(game => {
    const selected = game.index === selectedGameIndex;
    const card = document.createElement('div');
    card.className = 'glass-card p-3 rounded-xl flex items-center justify-between cursor-pointer border';
    card.tabIndex = 0;
    if (selected) { card.style.borderColor = 'var(--accent-2)'; card.style.background = 'rgba(var(--accent-rgb), 0.15)'; }
    else card.style.borderColor = 'rgba(255,255,255,0.05)';
    const canDrag = (sortMode === 'manual' && !searchQuery);
    card.draggable = canDrag;
    card.onclick = () => { selectedGameIndex = game.index; selectedGamePath = game.path; renderGamesList(); renderCompanions(); };
    card.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { if (e.target === card) { e.preventDefault(); card.click(); } } });
    if (canDrag) {
      card.addEventListener('dragstart', (e) => { reorderDragIndex = game.index; e.dataTransfer.effectAllowed = 'move'; e.dataTransfer.setData('text/plain', String(game.index)); setTimeout(() => card.classList.add('opacity-40'), 0); });
      card.addEventListener('dragend', () => { card.classList.remove('opacity-40'); reorderDragIndex = null; });
      card.addEventListener('dragover', (e) => { if (reorderDragIndex === null) return; e.preventDefault(); e.dataTransfer.dropEffect = 'move'; card.style.outline = '1px solid var(--accent-2)'; });
      card.addEventListener('dragleave', () => { card.style.outline = ''; });
      card.addEventListener('drop', (e) => { card.style.outline = ''; if (reorderDragIndex === null || reorderDragIndex === game.index) return; e.preventDefault(); e.stopPropagation(); reorderGameTo(reorderDragIndex, game.index); });
    }
    const eyeIcon = game.showInRadial ? `<i class="fa-solid fa-eye text-emerald-400 text-xs"></i>` : `<i class="fa-solid fa-eye-slash text-gray-500 text-xs"></i>`;
    const uwpBadge = game.isUwp ? `<span class="uwp-badge">UWP</span>` : '';
    const favClass = game.favorite ? 'fav-btn on' : 'fav-btn';
    const gripIcon = canDrag ? `<i class="fa-solid fa-grip-vertical text-gray-500 text-xs shrink-0 cursor-grab"></i>` : `<i class="fa-solid fa-grip-vertical text-gray-700 text-xs shrink-0 opacity-30"></i>`;
    const muteIndicator = game.muted ? `<span class="mute-badge"><i class="fa-solid fa-volume-xmark"></i></span>` : '';
    const runningIndicator = game.isRunning ? `<span class="running-dot"><span class="dot"></span><span>${escapeHtml(t('runningLabel'))}</span></span>` : '';
    const slotBadge = (game.quickSlot && game.quickSlot >= 1 && game.quickSlot <= 9)
      ? `<span class="slot-badge">${game.quickSlot}</span>` : '';
    const boostBadge = game.boostFps ? `<span class="boost-badge"><i class="fa-solid fa-rocket"></i></span>` : '';
    const canOpenFolder = !game.isSteam && !game.isUwp;
    card.innerHTML = `
      <div class="flex items-center gap-3 overflow-hidden">
        ${gripIcon}
        <div class="relative shrink-0">
          <img src="${game.icon}" class="w-10 h-10 rounded-lg object-contain bg-black/20 border border-white/10">
          ${game.missing ? `<span class="absolute -top-1.5 -right-1.5 w-5 h-5 rounded-full bg-amber-500 border-2 border-[#0f0c1b] flex items-center justify-center"><i class="fa-solid fa-triangle-exclamation text-[9px] text-black"></i></span>` : ''}
        </div>
        <div class="overflow-hidden">
          <div class="flex items-center gap-2 flex-wrap">
            <h4 class="font-bold text-white text-sm truncate">${escapeHtml(game.name)}</h4>
            ${slotBadge}${boostBadge}${uwpBadge}${muteIndicator}${runningIndicator}
          </div>
          <p class="text-[11px] font-mono truncate ${game.missing ? 'text-amber-400' : 'text-gray-300'}">${escapeHtml(game.path)}</p>
        </div>
      </div>
      <div class="flex items-center gap-1 shrink-0">
        <button onclick="event.stopPropagation(); toggleFavoriteByIndex(${game.index})" class="${favClass}" tabindex="-1"><i class="fa-solid fa-star text-sm"></i></button>
        ${eyeIcon}
        <span class="text-[11px] px-2 py-0.5 rounded-full ${game.companions.length > 0 ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/30' : 'bg-gray-800 text-gray-400'}">${game.companions.length}</span>
        ${canOpenFolder ? `<button onclick="event.stopPropagation(); nativeAction('openGameFolder', {index: ${game.index}})" class="icon-action-btn" tabindex="-1"><i class="fa-solid fa-folder-open text-xs"></i></button>` : ''}
        <button onclick="event.stopPropagation(); openGameDetails(${game.index})" class="text-gray-400 hover:text-cyan-300 p-1" tabindex="-1"><i class="fa-solid fa-gears"></i></button>
        <button onclick="event.stopPropagation(); confirmRemoveGame(${game.index}, decodeURIComponent('${encodeURIComponent(game.name)}'))" class="text-gray-400 hover:text-red-400 p-1" tabindex="-1"><i class="fa-solid fa-xmark"></i></button>
      </div>`;
    container.appendChild(card);
  });
}
function toggleFavoriteByIndex(index) {
  const game = state.games.find(g => g.index === index);
  if (!game) return;
  nativeAction('setGameFavorite', { index: index, enabled: game.favorite ? 0 : 1 });
}
function openGameFolderDetails() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('openGameFolder', { index: game.index });
}
function renderCompanions() {
  const banner = document.getElementById('selectedGameBanner');
  const hint = document.getElementById('noSelectionHint');
  const addBtn = document.getElementById('addCompanionBtn');
  const list = document.getElementById('companionAppsList');
  const game = state.games.find(g => g.index === selectedGameIndex);
  if (!game) { banner.classList.add('hidden'); hint.classList.remove('hidden'); addBtn.disabled = true; list.innerHTML = ''; return; }
  banner.classList.remove('hidden'); hint.classList.add('hidden'); addBtn.disabled = false;
  document.getElementById('selectedGameImg').src = game.icon;
  document.getElementById('selectedGameName').innerText = game.name;
  document.getElementById('selectedGamePath').innerText = game.path;
  const muteBadge = document.getElementById('selectedGameMuteBadge');
  muteBadge.classList.toggle('hidden', !game.muted);
  const runningBadge = document.getElementById('selectedGameRunningBadge');
  runningBadge.classList.toggle('hidden', !game.isRunning);
  list.innerHTML = '';
  if (game.companions.length === 0) {
    list.innerHTML = `<div class="text-center py-6 text-gray-400 text-xs border border-dashed border-white/10 rounded-xl">${t('noCompanions')}</div>`;
    return;
  }
  game.companions.forEach(comp => {
    const item = document.createElement('div');
    item.className = 'bg-slate-900/80 border border-white/10 p-2.5 rounded-xl flex items-center justify-between';
    item.innerHTML = `
      <div class="flex items-center gap-2.5 overflow-hidden">
        <img src="${comp.icon}" class="w-7 h-7 rounded object-contain bg-black/20 shrink-0">
        <div class="overflow-hidden">
          <p class="text-xs font-bold text-white truncate">${escapeHtml(comp.name)}</p>
          <p class="text-[10px] text-gray-300 font-mono truncate">${escapeHtml(comp.path)}</p>
        </div>
      </div>
      <button onclick="confirmRemoveCompanion(${game.index}, ${comp.index}, decodeURIComponent('${encodeURIComponent(comp.name)}'))" class="text-gray-400 hover:text-red-400 p-1 text-xs shrink-0" tabindex="0"><i class="fa-solid fa-trash"></i></button>`;
    list.appendChild(item);
  });
}

// ✅ عرض الأنماط — 6 فقط (بناءً على VISIBLE_RADIAL_STYLES)
function renderRadialStyles() {
  const grid = document.getElementById('radialStylesGrid');
  grid.innerHTML = '';
  VISIBLE_RADIAL_STYLES.forEach((styleId) => {
    const meta = RADIAL_META[styleId] || { icon: 'fa-circle', descAr: '', descEn: '' };
    const selected = styleId === state.radialStyle;
    const card = document.createElement('div');
    card.className = 'glass-card p-3 rounded-xl cursor-pointer border ' + (selected ? '' : 'border-white/5');
    if (selected) card.style.borderColor = 'var(--accent-2)';
    card.tabIndex = 0;
    card.onclick = () => nativeAction('setRadialStyle', { style: styleId });
    card.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); card.click(); } });
    const nameKey = styleId === 20 ? 'مخصص' : (state.lang === 'en' ? meta.descEn.split(' — ')[0] : '');
    let displayName = '';
    if (styleId === 13) displayName = state.lang === 'en' ? 'Ripple' : 'موجات متداخلة';
    else if (styleId === 19) displayName = state.lang === 'en' ? 'Flame' : 'لهب';
    else if (styleId === 1) displayName = state.lang === 'en' ? 'Neon' : 'نيون';
    else if (styleId === 2) displayName = state.lang === 'en' ? 'Gradient' : 'حلقة متدرجة';
    else if (styleId === 11) displayName = state.lang === 'en' ? 'Aurora' : 'توهج الشفق';
    else if (styleId === 20) displayName = state.lang === 'en' ? 'Custom' : 'مخصص';

    card.innerHTML = `
      <div class="w-9 h-9 rounded-lg bg-black/40 border border-white/10 flex items-center justify-center mb-2">
        <i class="fa-solid ${meta.icon}" style="color: var(--accent-2); font-size: 14px;"></i>
      </div>
      <h3 class="font-bold text-white text-xs">${escapeHtml(displayName)}</h3>
      <p class="text-[10px] text-gray-400 mt-1 leading-tight">${escapeHtml(state.lang === 'en' ? meta.descEn : meta.descAr)}</p>`;
    grid.appendChild(card);
  });
}

// ✅ عرض الثيمات — 6 جاهزة + زر مخصص (الزر خارج الشبكة في HTML)
function renderPresets() {
  const grid = document.getElementById('presetsGrid');
  if (!grid) return;
  grid.innerHTML = '';
  PANEL_PRESETS.forEach(preset => {
    const selected = preset.id === (state.panelPreset || '');
    const card = document.createElement('div');
    card.className = 'preset-card' + (selected ? ' selected' : '');
    card.tabIndex = 0;
    card.onclick = () => nativeAction('setPanelPreset', { preset: preset.id });
    card.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); card.click(); } });
    card.innerHTML = `
      <div class="preset-swatch" style="background: linear-gradient(135deg, ${preset.colors[0]}, ${preset.colors[1]});">
        <i class="fa-solid ${preset.icon}"></i>
      </div>
      <div class="preset-label">${escapeHtml(state.lang === 'en' ? preset.nameEn : preset.nameAr)}</div>`;
    grid.appendChild(card);
  });
  // ✅ لو الثيم الحالي هو "custom"، نضيف كارت خاص
  if (state.panelPreset === 'custom' && state.customTheme) {
    const ct = state.customTheme;
    const card = document.createElement('div');
    card.className = 'preset-card selected';
    card.tabIndex = 0;
    card.onclick = () => openCustomThemeModal();
    card.innerHTML = `
      <div class="preset-swatch" style="background: linear-gradient(135deg, #${ct.accent}, #${ct.secondary});">
        <i class="fa-solid fa-palette"></i>
      </div>
      <div class="preset-label">${state.lang === 'en' ? 'Custom' : 'مخصص'}</div>`;
    grid.appendChild(card);
  }
}

function openGameDetails(gameIndex) { detailsGameIndex = gameIndex; qaActiveTarget = 'game'; switchTab('details'); renderGameDetails(); }
function renderDetailsSidebar() {
  const sidebar = document.getElementById('detailsGameSidebar');
  const counter = document.getElementById('detailsGamesCount');
  if (!sidebar) return;
  counter.innerText = state.games.length;
  sidebar.innerHTML = '';
  state.games.forEach(game => {
    const active = game.index === detailsGameIndex;
    const item = document.createElement('div');
    item.className = 'details-game-item glass-card p-2.5 rounded-xl border flex items-center gap-2.5 ' + (active ? 'active' : 'border-white/5') + (game.showInRadial ? '' : ' hidden-from-radial');
    item.tabIndex = 0;
    if (!active) item.style.borderColor = 'rgba(255,255,255,0.05)';
    item.onclick = () => { if (detailsGameIndex === game.index) return; detailsGameIndex = game.index; qaActiveTarget = 'game'; renderGameDetails(); };
    item.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); item.click(); } });
    const eyeIcon = game.showInRadial ? `<i class="fa-solid fa-eye text-emerald-400 text-[11px]"></i>` : `<i class="fa-solid fa-eye-slash text-gray-500 text-[11px]"></i>`;
    const uwp = game.isUwp ? `<span class="uwp-badge">UWP</span>` : '';
    const favStar = game.favorite ? `<i class="fa-solid fa-star text-[10px]" style="color:#fbbf24;"></i>` : '';
    const muteMini = game.muted ? `<span class="mute-badge"><i class="fa-solid fa-volume-xmark"></i></span>` : '';
    const runMini = game.isRunning ? `<span class="running-dot" style="padding:1px 5px;"><span class="dot" style="width:5px;height:5px;"></span></span>` : '';
    const slotMini = (game.quickSlot >= 1 && game.quickSlot <= 9) ? `<span class="slot-badge" style="width:16px;height:16px;font-size:9px;">${game.quickSlot}</span>` : '';
    const boostMini = game.boostFps ? `<i class="fa-solid fa-rocket text-[9px]" style="color:#ec4899;"></i>` : '';
    item.innerHTML = `
      <img src="${game.icon}" class="w-9 h-9 rounded-lg object-contain bg-black/20 border border-white/10 shrink-0">
      <div class="flex-1 min-w-0">
        <div class="flex items-center gap-1.5 flex-wrap">
          <div class="text-xs font-bold text-white truncate">${escapeHtml(game.name)}</div>
          ${slotMini}${boostMini}${uwp}${favStar}${muteMini}${runMini}
        </div>
        <div class="text-[10px] text-gray-500 truncate font-mono">${escapeHtml(game.path)}</div>
      </div>
      <div class="flex flex-col items-center gap-0.5 shrink-0">
        ${eyeIcon}
        <span class="text-[9px] text-gray-500">${game.companions.length}</span>
      </div>`;
    sidebar.appendChild(item);
  });
}
function renderQuickAccessBar(game) {
  const container = document.getElementById('quickAccessChips');
  if (!container) return;
  container.innerHTML = '';
  const gameChip = document.createElement('button');
  gameChip.type = 'button'; gameChip.className = 'qa-chip'; gameChip.dataset.qaTarget = 'game'; gameChip.tabIndex = 0;
  gameChip.innerHTML = `<img src="${game.icon}" class="w-7 h-7 rounded-lg object-contain bg-black/30 border border-white/10"><span class="truncate max-w-[140px]">${escapeHtml(game.name)}</span><i class="fa-solid fa-gamepad text-purple-300 text-[10px]"></i>`;
  gameChip.onclick = () => focusQuickAccessTarget('game');
  container.appendChild(gameChip);
  (game.companions || []).forEach(c => {
    const chip = document.createElement('button');
    chip.type = 'button'; chip.className = 'qa-chip'; chip.dataset.qaTarget = 'comp-' + c.index; chip.tabIndex = 0;
    chip.innerHTML = `<img src="${c.icon}" class="w-7 h-7 rounded-lg object-contain bg-black/30 border border-white/10"><span class="truncate max-w-[140px]">${escapeHtml(c.name)}</span><i class="fa-solid fa-puzzle-piece text-cyan-300 text-[10px]"></i>`;
    chip.onclick = () => focusQuickAccessTarget('comp-' + c.index);
    container.appendChild(chip);
  });
  const available = Array.from(container.querySelectorAll('.qa-chip')).map(c => c.dataset.qaTarget);
  if (!available.includes(qaActiveTarget)) qaActiveTarget = 'game';
  container.querySelectorAll('.qa-chip').forEach(chip => chip.classList.toggle('active', chip.dataset.qaTarget === qaActiveTarget));
}
function focusQuickAccessTarget(targetId) {
  qaActiveTarget = targetId;
  document.querySelectorAll('.qa-chip').forEach(chip => chip.classList.toggle('active', chip.dataset.qaTarget === targetId));
  let el = null;
  if (targetId === 'game') el = document.getElementById('detailGameOptions');
  else el = document.getElementById('detailComp-' + targetId.replace('comp-', ''));
  if (el) {
    el.scrollIntoView({ behavior: 'smooth', block: 'center' });
    el.classList.remove('qa-highlight'); void el.offsetWidth; el.classList.add('qa-highlight');
    setTimeout(() => el.classList.remove('qa-highlight'), 1700);
  }
}
function pickCustomColor() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('browseGameCustomColor', { index: game.index });
}
function renderColorPreview(game) {
  const preview = document.getElementById('detailsColorPreview');
  const hexEl = document.getElementById('detailsColorHex');
  if (!preview || !hexEl) return;
  const c = game.color >>> 0;
  const r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
  const hex = '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('').toUpperCase();
  preview.style.background = hex;
  preview.style.boxShadow = `0 0 18px ${hex}90`;
  hexEl.innerText = hex;
}
function chooseGameRadialBg() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('browseGameRadialBg', { index: game.index });
}
function clearGameRadialBg() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('clearGameRadialBg', { index: game.index });
}
function toggleGameHideOriginalIcon() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game || !game.radialBgPath) return;
  nativeAction('setGameHideOriginalIcon', { index: game.index, enabled: game.hideOriginalIcon ? 0 : 1 });
}
function onQuickSlotChange(value) {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  const slot = parseInt(value, 10) || 0;
  nativeAction('setGameQuickSlot', { index: game.index, slot: slot });
}
function setGameLanguage(langValue) {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  if (game.gameLanguage === langValue) return;
  nativeAction('setGameLanguage', { index: game.index, language: langValue });
}


function toggleBoostFps() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostFps', { index: game.index, enabled: game.boostFps ? 0 : 1 });
}
function toggleBoostDisableCore0() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostDisableCore0', { index: game.index, enabled: game.boostDisableCore0 ? 0 : 1 });
}
function toggleBoostHighPriority() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostHighPriority', { index: game.index, enabled: game.boostHighPriority ? 0 : 1 });
}
function toggleBoostStopStats() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostStopStats', { index: game.index, enabled: game.boostStopStats ? 0 : 1 });
}
function toggleBoostTimerResolution() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostTimerResolution', { index: game.index, enabled: game.boostTimerResolution ? 0 : 1 });
}
function toggleBoostSystemResponsiveness() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostSystemResponsiveness', { index: game.index, enabled: game.boostSystemResponsiveness ? 0 : 1 });
}
function toggleBoostMmcss() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameBoostMmcss', { index: game.index, enabled: game.boostMmcss ? 0 : 1 });
}
function toggleControllerToggleMode() {
  nativeAction('setControllerToggleMode', { enabled: state.controllerToggleMode ? 0 : 1 });
}
function toggleAllowControllerDuringGame() {
  nativeAction('setAllowControllerDuringGame', { enabled: state.allowControllerDuringGame ? 0 : 1 });
}
function togglePanelGlass() {
  nativeAction('setPanelGlassEffect', { enabled: state.panelGlassEffect ? 0 : 1 });
}
function toggleOverlayEnabled() {
  nativeAction('setOverlayEnabled', { enabled: state.overlayEnabled ? 0 : 1 });
}
function toggleOverlayShowOnLaunch() {
  nativeAction('setOverlayShowOnGameLaunch', { enabled: state.overlayShowOnGameLaunch ? 0 : 1 });
}
function startOverlayHotkeyCapture() {
  capturingOverlayHotkey = true; capturingHotkey = false; capturingMuteHotkey = false;
  const btn = document.getElementById('overlayHotkeyCaptureBtn');
  btn.innerText = t('pressNewKey'); btn.classList.add('recording');
}
function changeOverlayOpacity(val) {
  document.getElementById('overlayOpacityVal').innerText = Math.round(val * 100) + '%';
}
function changeOverlayOpacityCommit(val) {
  nativeAction('setOverlayOpacity', { value: String(val) });
}
function renderRadialBgPreview(game) {
  const wrap = document.getElementById('detailsRadialBgPreviewWrap');
  const clearBtn = document.getElementById('detailsRadialBgClearBtn');
  const hideIconRow = document.getElementById('detailsHideIconRow');
  const hideIconToggle = document.getElementById('detailsHideIconToggle');
  if (!wrap) return;
  if (game && game.radialBgPath && game.radialBgPreview) {
    wrap.innerHTML = `<div class="radial-bg-preview" style="background-image: url('${game.radialBgPreview}');"></div>`;
    if (clearBtn) clearBtn.style.display = 'inline-flex';
    if (hideIconRow) hideIconRow.style.display = 'flex';
    if (hideIconToggle) hideIconToggle.classList.toggle('on', !!game.hideOriginalIcon);
  } else {
    wrap.innerHTML = `<div class="radial-bg-empty"><i class="fa-solid fa-image"></i></div>`;
    if (clearBtn) clearBtn.style.display = 'none';
    if (hideIconRow) hideIconRow.style.display = 'none';
  }
}
function renderLangSegments(game) {
  const container = document.getElementById('detailsLangSegments');
  if (!container) return;
  const cur = (game && typeof game.gameLanguage === 'number') ? game.gameLanguage : 0;
  container.querySelectorAll('.lang-segment').forEach(btn => {
    const val = parseInt(btn.getAttribute('data-lang'), 10);
    btn.classList.toggle('active', val === cur);
  });
}
function renderBoostFpsSection(game) {
  const mainToggle = document.getElementById('detailsBoostFpsToggle');
  if (!mainToggle) return;
  mainToggle.classList.toggle('on', !!game.boostFps);
  const opts = document.getElementById('boostFpsOptions');
  if (opts) {
    if (game.boostFps) opts.classList.remove('opacity-40', 'pointer-events-none');
    else opts.classList.add('opacity-40', 'pointer-events-none');
  }
  const t1 = document.getElementById('boostDisableCore0Toggle');
  if (t1) t1.classList.toggle('on', !!game.boostDisableCore0);
  const t2 = document.getElementById('boostHighPriorityToggle');
  if (t2) t2.classList.toggle('on', !!game.boostHighPriority);
  const t3 = document.getElementById('boostStopStatsToggle');
  if (t3) t3.classList.toggle('on', !!game.boostStopStats);
  const t4 = document.getElementById('boostTimerResolutionToggle');
  if (t4) t4.classList.toggle('on', !!game.boostTimerResolution);
  const t5 = document.getElementById('boostSystemResponsivenessToggle');
  if (t5) t5.classList.toggle('on', !!game.boostSystemResponsiveness);
  const t6 = document.getElementById('boostMmcssToggle');
  if (t6) t6.classList.toggle('on', !!game.boostMmcss);
}
function renderPrioritySelect(game) {
  const sel = document.getElementById('detailsPrioritySelect');
  if (!sel) return;
  sel.innerHTML = '';
  const names = state.processPriorityNames || [];
  names.forEach((name, i) => {
    const opt = document.createElement('option');
    opt.value = i; opt.innerText = name;
    sel.appendChild(opt);
  });
  sel.value = game.processPriority || 0;
}
function renderCpuCoresGrid(game) {
  const grid = document.getElementById('detailsCpuCoresGrid');
  const label = document.getElementById('cpuCountLabel');
  const wrap = document.getElementById('detailsCpuCoresWrap');
  const toggle = document.getElementById('detailsAffinityToggle');
  if (!grid || !label || !wrap || !toggle) return;
  const cores = state.cpuCoreCount || 8;
  label.innerText = `${cores} ${state.lang === 'en' ? 'cores' : 'أنوية'}`;
  let mask = game.affinityMask || ((1n << BigInt(cores)) - 1n);
  if (mask === 0n || mask === 0) mask = (1n << BigInt(cores)) - 1n;
  pendingAffinityMask = mask;
  toggle.classList.toggle('on', !!game.applyAffinity);
  wrap.classList.toggle('opacity-40', !game.applyAffinity);
  wrap.classList.toggle('pointer-events-none', !game.applyAffinity);
  grid.innerHTML = '';
  for (let i = 0; i < cores; i++) {
    const bitSet = (pendingAffinityMask & (1n << BigInt(i))) !== 0n;
    const core = document.createElement('div');
    core.className = 'cpu-core' + (bitSet ? ' selected' : '');
    core.dataset.coreIdx = i;
    core.innerHTML = `<span class="core-num">${i}</span><span class="core-idx">CPU</span>`;
    core.onclick = () => {
      pendingAffinityMask ^= (1n << BigInt(i));
      core.classList.toggle('selected', (pendingAffinityMask & (1n << BigInt(i))) !== 0n);
    };
    grid.appendChild(core);
  }
}
function selectAllCores() {
  const cores = state.cpuCoreCount || 8;
  pendingAffinityMask = (1n << BigInt(cores)) - 1n;
  document.querySelectorAll('#detailsCpuCoresGrid .cpu-core').forEach(el => el.classList.add('selected'));
}
function clearAllCores() {
  pendingAffinityMask = 0n;
  document.querySelectorAll('#detailsCpuCoresGrid .cpu-core').forEach(el => el.classList.remove('selected'));
}
function onPriorityChange(value) {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  const prio = parseInt(value, 10) || 0;
  nativeAction('setGameProcessPriority', { index: game.index, priority: prio });
}
function toggleGameAffinity() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  const newEnabled = !game.applyAffinity;
  const maskStr = pendingAffinityMask.toString();
  nativeAction('setGameAffinity', { index: game.index, enabled: newEnabled ? 1 : 0, mask: maskStr });
  const wrap = document.getElementById('detailsCpuCoresWrap');
  const toggle = document.getElementById('detailsAffinityToggle');
  if (wrap) { wrap.classList.toggle('opacity-40', !newEnabled); wrap.classList.toggle('pointer-events-none', !newEnabled); }
  if (toggle) toggle.classList.toggle('on', newEnabled);
}

/* ============================================================
 * Performance Black Box
 * ============================================================ */
function renderBlackBoxSection(game) {
  const mainToggle = document.getElementById('detailsBlackBoxToggle');
  const warning = document.getElementById('detailsBlackBoxWarning');
  const subOptions = document.getElementById('detailsBlackBoxSubOptions');
  const actions = document.getElementById('detailsBlackBoxActions');
  const sessionCount = document.getElementById('detailsBlackBoxSessionCount');
  if (!mainToggle) return;

  const enabled = !!game.performanceMonitor;
  mainToggle.classList.toggle('on', enabled);

  if (enabled) {
    warning.classList.remove('hidden');
    if (!state.isAdmin) {
      warning.innerHTML =
        '<div class="flex items-start gap-2">' +
          '<i class="fa-solid fa-triangle-exclamation mt-0.5 shrink-0"></i>' +
          '<div class="flex-1">' + t('blackBoxNeedsAdmin') + '</div>' +
          '<button onclick="requestRestartAsAdmin()" class="backup-btn primary shrink-0" tabindex="0">' +
            '<i class="fa-solid fa-shield-halved"></i>' +
            '<span>' + t('restartAsAdminBtn') + '</span>' +
          '</button>' +
        '</div>';
      warning.style.background = 'rgba(239, 68, 68, 0.1)';
      warning.style.borderColor = 'rgba(239, 68, 68, 0.4)';
      warning.style.color = '#fca5a5';
    } else {
      warning.innerHTML = '<div class="flex items-start gap-2"><i class="fa-solid fa-triangle-exclamation mt-0.5 shrink-0"></i><div>' + t('detailsBlackBoxWarning') + '</div></div>';
      warning.style.background = '';
      warning.style.borderColor = '';
      warning.style.color = '';
    }
    subOptions.classList.remove('opacity-40', 'pointer-events-none');
  } else {
    warning.classList.add('hidden');
    subOptions.classList.add('opacity-40', 'pointer-events-none');
  }

  const hasSessions = (game.perfSessionCount || 0) > 0;
  if (actions) {
    if (enabled || hasSessions) actions.classList.remove('hidden');
    else actions.classList.add('hidden');
  }
  if (sessionCount) sessionCount.innerText = '(' + (game.perfSessionCount || 0) + ')';
}
function toggleGameBlackBox() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGamePerformanceMonitor', { index: game.index, enabled: game.performanceMonitor ? 0 : 1 });
}

/* ============================================================
 * Performance Report Viewer
 * ============================================================ */
function openPerformanceReport() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  perfReportGameIndex = game.index;
  perfReportGamePath = game.path;
  document.getElementById('perfReportGameName').innerText = t('perfReportTitle') + ' — ' + game.name;
  document.getElementById('perfReportSubtitle').innerText = game.path;
  document.getElementById('perfSessionList').innerHTML = '<div class="text-center py-4 text-gray-500 text-xs">...</div>';
  document.getElementById('perfSessionDetail').innerHTML =
    '<div class="text-center py-8 text-gray-500 text-sm">' + t('perfSelectSessionHint') + '</div>';
  document.getElementById('perfReportModalOverlay').classList.remove('hidden');
  nativeAction('getPerformanceSessions', { index: game.index });
}
function closePerformanceReport() {
  document.getElementById('perfReportModalOverlay').classList.add('hidden');
  perfReportGameIndex = -1;
  perfReportGamePath = '';
  perfReportSessions = [];
  perfReportCurrentFile = '';
  perfCharts.forEach(c => { try { c.destroy(); } catch(e) {} });
  perfCharts = [];
}
function renderPerformanceSessions(sessions) {
  perfReportSessions = sessions || [];
  const countEl = document.getElementById('perfSessionsCount');
  const listEl = document.getElementById('perfSessionList');
  const deleteAllBtn = document.getElementById('perfDeleteAllBtn');
  if (!listEl) return;
  if (countEl) countEl.innerText = perfReportSessions.length;
  if (deleteAllBtn) deleteAllBtn.style.display = perfReportSessions.length > 0 ? 'inline-flex' : 'none';
  listEl.innerHTML = '';
  if (perfReportSessions.length === 0) {
    listEl.innerHTML = '<div class="text-center py-6 text-gray-500 text-xs border border-dashed border-white/10 rounded-xl">' + t('perfNoSessions') + '</div>';
    document.getElementById('perfSessionDetail').innerHTML =
      '<div class="text-center py-8 text-gray-500 text-sm">' + t('perfSelectSessionHint') + '</div>';
    return;
  }
  perfReportSessions.forEach((s, i) => {
    const item = document.createElement('div');
    item.className = 'perf-session-item';
    item.tabIndex = 0;
    item.dataset.idx = i;
    const date = new Date(s.sessionStartUnix * 1000);
    const loc = state.lang === 'en' ? 'en-US' : 'ar-SA-u-ca-gregory-nu-latn';
    const dateStr = date.toLocaleDateString(loc) + ' ' + date.toLocaleTimeString(loc, { hour: '2-digit', minute: '2-digit' });
    const durStr = formatPlayTime(s.sessionDurationSec);
    const fpsStr = (s.avgFps >= 0) ? s.avgFps.toFixed(1) + ' FPS' : '— FPS';
    item.innerHTML = `
      <div class="flex items-center justify-between gap-2">
        <div class="min-w-0 flex-1">
          <div class="text-[11px] font-bold text-white truncate">${escapeHtml(dateStr)}</div>
          <div class="text-[10px] text-gray-400 font-mono truncate">${escapeHtml(durStr)} · ${escapeHtml(fpsStr)}</div>
        </div>
        <button class="icon-action-btn perf-del-btn" data-del-file="${escapeHtml(s.filePath)}" title="${escapeHtml(t('perfDeleteSession'))}">
          <i class="fa-solid fa-trash text-[10px]"></i>
        </button>
      </div>`;
    item.addEventListener('click', (ev) => {
      if (ev.target.closest('.perf-del-btn')) return;
      selectPerformanceSession(i);
    });
    item.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectPerformanceSession(i); }
    });
    listEl.appendChild(item);
  });
  listEl.querySelectorAll('.perf-del-btn').forEach(btn => {
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      const file = btn.getAttribute('data-del-file');
      if (!file) return;
      showConfirmModal(
        t('perfConfirmDeleteSession'),
        () => nativeAction('deletePerformanceSession', { file, gameIndex: perfReportGameIndex }),
        { okLabel: t('confirmDelete'),
          okColorClass: 'bg-red-600/80 hover:bg-red-600',
          iconWrapClass: 'bg-red-500/15 border border-red-500/30',
          iconClass: 'fa-trash', iconColorClass: 'text-red-400' });
    });
  });
  if (perfReportSessions.length > 0) selectPerformanceSession(0);
}
function selectPerformanceSession(idx) {
  document.querySelectorAll('#perfSessionList .perf-session-item').forEach((el, i) => {
    el.classList.toggle('selected', i === idx);
  });
  const s = perfReportSessions[idx];
  if (!s) return;
  perfReportCurrentFile = s.filePath;
  perfCharts.forEach(c => { try { c.destroy(); } catch(e) {} });
  perfCharts = [];
  const detail = document.getElementById('perfSessionDetail');
  if (detail) detail.innerHTML = '<div class="text-center py-8 text-gray-500 text-sm">...</div>';
  nativeAction('getPerformanceReport', { file: s.filePath });
}
function perfStatCard(labelKey, value, sub, icon, iconColor) {
  if (value === null || value === undefined || value === '') return '';
  return `
    <div class="perf-stat-card">
      <div class="perf-stat-label">
        <i class="fa-solid ${icon}" style="color:${iconColor}; font-size:11px;"></i>
        <span>${escapeHtml(t(labelKey))}</span>
      </div>
      <div class="perf-stat-value">${escapeHtml(String(value))}</div>
      ${sub ? `<div class="perf-stat-sub">${escapeHtml(sub)}</div>` : ''}
    </div>`;
}
function renderPerformanceReport(report) {
  const detail = document.getElementById('perfSessionDetail');
  if (!detail) return;
  if (!report) {
    detail.innerHTML = '<div class="text-center py-8 text-gray-500 text-sm">Failed to load session.</div>';
    return;
  }
  const hasFps = report.avgFps >= 0;
  const hasCpuTemp = report.avgCpuTemp >= 0;
  const hasGpuTemp = report.avgGpuTemp >= 0;
  const hasGpuPower = report.avgGpuPower >= 0;
  const hasCpuPower = report.avgCpuPower >= 0;

  const date = new Date(report.sessionStartUnix * 1000);
  const loc = state.lang === 'en' ? 'en-US' : 'ar-SA-u-ca-gregory-nu-latn';
  const dateStr = date.toLocaleDateString(loc) + ' ' + date.toLocaleTimeString(loc, { hour: '2-digit', minute: '2-digit' });
  const fmt1 = (v) => (v >= 0) ? v.toFixed(1) : '—';
  const fmt0 = (v) => (v >= 0) ? v.toFixed(0) : '—';
  const fmtMB = (v) => {
    if (!v || v <= 0) return '—';
    if (v >= 1024) return (v / 1024).toFixed(2) + ' GB';
    return v + ' MB';
  };

  let html = '<div class="space-y-4">';
  html += `
    <div class="info-box flex items-center gap-3">
      <i class="fa-solid fa-calendar-day"></i>
      <div class="flex-1">
        <div class="font-bold">${escapeHtml(t('perfSessionOn'))} ${escapeHtml(dateStr)}</div>
        <div class="text-[11px] opacity-80">${escapeHtml(t('perfDuration'))}: ${escapeHtml(formatPlayTime(report.sessionDurationSec))}</div>
      </div>
    </div>`;

  if (!hasFps) {
    const warnMsg = state.isAdmin ? t('perfFpsEtwFailed') : t('perfFpsNeedsAdmin');
    html += `<div class="warning-box flex items-start gap-2"><i class="fa-solid fa-triangle-exclamation mt-0.5 shrink-0"></i><div>${escapeHtml(warnMsg)}</div></div>`;
  }
  if (hasFps) {
    html += '<div class="grid grid-cols-2 md:grid-cols-3 gap-3">';
    html += perfStatCard('perfAvgFps',      fmt1(report.avgFps) + ' FPS', null, 'fa-gauge-high', '#10b981');
    html += perfStatCard('perfMinFps',      fmt1(report.minFps) + ' FPS', null, 'fa-arrow-down', '#fbbf24');
    html += perfStatCard('perfMaxFps',      fmt1(report.maxFps) + ' FPS', null, 'fa-arrow-up', '#60a5fa');
    html += perfStatCard('perf1PercentLow', fmt1(report.fps1PercentLow) + ' FPS', null, 'fa-wave-square', '#a78bfa');
    html += perfStatCard('perf01PercentLow',fmt1(report.fps01PercentLow) + ' FPS', null, 'fa-wave-square', '#c084fc');
    html += perfStatCard('perfStutters',    report.stutterCount, null, 'fa-triangle-exclamation', '#ef4444');
    html += '</div>';
  }
  html += '<div class="grid grid-cols-2 md:grid-cols-4 gap-3">';
  html += perfStatCard('perfAvgCpuUsage', fmt0(report.avgCpuUsage) + '%', 'Max: ' + fmt0(report.maxCpuUsage) + '%', 'fa-microchip', '#f59e0b');
  html += perfStatCard('perfAvgGpuUsage', fmt0(report.avgGpuUsage) + '%', 'Max: ' + fmt0(report.maxGpuUsage) + '%', 'fa-video', '#8b5cf6');
  html += perfStatCard('perfAvgRam',  fmtMB(report.avgRamMB),  'Max: ' + fmtMB(report.maxRamMB),  'fa-memory', '#3b82f6');
  html += perfStatCard('perfAvgVram', fmtMB(report.avgVramMB), 'Max: ' + fmtMB(report.maxVramMB), 'fa-layer-group', '#06b6d4');
  html += '</div>';
  html += '<div class="grid grid-cols-2 md:grid-cols-4 gap-3">';
  if (hasGpuTemp) html += perfStatCard('perfAvgGpuTemp', fmt0(report.avgGpuTemp) + '°C', 'Max: ' + fmt0(report.maxGpuTemp) + '°C', 'fa-temperature-half', '#f97316');
  if (hasCpuTemp) html += perfStatCard('perfAvgCpuTemp', fmt0(report.avgCpuTemp) + '°C', 'Max: ' + fmt0(report.maxCpuTemp) + '°C', 'fa-fire', '#ef4444');
  if (hasGpuPower) html += perfStatCard('perfAvgGpuPower', fmt0(report.avgGpuPower) + ' W', 'Max: ' + fmt0(report.maxGpuPower) + ' W', 'fa-bolt', '#facc15');
  if (hasCpuPower) html += perfStatCard('perfAvgCpuPower', fmt0(report.avgCpuPower) + ' W', 'Max: ' + fmt0(report.maxCpuPower) + ' W', 'fa-bolt', '#fde047');
  html += '</div>';

  const hasSamples = report.samples && report.samples.length > 0;
  if (hasSamples) {
    const hasFpsData = report.samples.some(s => s[1] >= 0);
    const hasTempData = report.samples.some(s => s[6] >= 0 || s[5] >= 0);
    const hasRamData = report.samples.some(s => s[9] > 0);
    if (hasFpsData) html += `<div class="pt-3 border-t border-white/10"><h4 class="text-xs font-bold text-gray-300 mb-2 flex items-center gap-2"><i class="fa-solid fa-gauge-high text-emerald-400"></i><span>${escapeHtml(t('perfChartFpsTitle'))}</span></h4><div id="perfChartFps" style="height:220px;"></div></div>`;
    if (hasTempData) html += `<div class="pt-3 border-t border-white/10"><h4 class="text-xs font-bold text-gray-300 mb-2 flex items-center gap-2"><i class="fa-solid fa-temperature-half text-orange-400"></i><span>${escapeHtml(t('perfChartTempTitle'))}</span></h4><div id="perfChartTemp" style="height:220px;"></div></div>`;
    if (hasRamData) html += `<div class="pt-3 border-t border-white/10"><h4 class="text-xs font-bold text-gray-300 mb-2 flex items-center gap-2"><i class="fa-solid fa-memory text-blue-400"></i><span>${escapeHtml(t('perfChartRamTitle'))}</span></h4><div id="perfChartRam" style="height:220px;"></div></div>`;
  }
  html += `<div class="flex gap-2 pt-3 border-t border-white/10"><button id="perfExportBtn" class="backup-btn primary" tabindex="0"><i class="fa-solid fa-file-csv"></i><span>${escapeHtml(t('perfExportCsvBtn'))}</span></button></div>`;
  html += '</div>';
  detail.innerHTML = html;

  const exportBtn = document.getElementById('perfExportBtn');
  if (exportBtn) {
    exportBtn.onclick = () => {
      if (!perfReportCurrentFile) return;
      nativeAction('exportPerformanceCsv', { file: perfReportCurrentFile });
    };
  }
  if (hasSamples) setTimeout(() => renderPerfCharts(report), 50);
}
function downsampleSamples(samples, maxPoints) {
  if (!samples || samples.length <= maxPoints) return samples;
  const step = samples.length / maxPoints;
  const out = [];
  for (let i = 0; i < maxPoints; i++) out.push(samples[Math.floor(i * step)]);
  return out;
}
function renderPerfCharts(report) {
  if (!report || !report.samples || report.samples.length === 0) return;

  // ✅ Lazy load ApexCharts
  if (typeof ApexCharts === 'undefined') {
    if (!window.__apexLoading) {
      window.__apexLoading = true;
      const script = document.createElement('script');
      script.src = 'https://cdn.jsdelivr.net/npm/apexcharts';
      script.onload = () => { window.__apexLoading = false; renderPerfCharts(report); };
      script.onerror = () => { window.__apexLoading = false; console.warn('[Charts] Failed to load ApexCharts'); };
      document.head.appendChild(script);
    }
    return;
  }

  perfCharts.forEach(c => { try { c.destroy(); } catch(e) {} });
  perfCharts = [];

  const samples = downsampleSamples(report.samples, 1500);
  const timestamps = samples.map(s => s[0] / 1000);
  const gridColor = 'rgba(148, 163, 184, 0.1)';
  const labelColor = '#94a3b8';

  const baseOptions = {
    chart: { background: 'transparent', foreColor: labelColor, toolbar: { show: false }, animations: { enabled: false }, zoom: { enabled: false }, fontFamily: 'Cairo, Tajawal, sans-serif' },
    theme: { mode: 'dark' },
    grid: { borderColor: gridColor, strokeDashArray: 3 },
    tooltip: { theme: 'dark', x: { formatter: (v) => formatSecToTime(v) } },
    dataLabels: { enabled: false },
    stroke: { curve: 'smooth', width: 2 },
    legend: { labels: { colors: labelColor } },
    xaxis: { type: 'numeric', labels: { style: { colors: labelColor, fontSize: '10px' }, formatter: (v) => formatSecToTime(v) }, axisBorder: { color: gridColor }, axisTicks: { color: gridColor } },
    yaxis: { labels: { style: { colors: labelColor, fontSize: '10px' } } },
  };

  const fpsSeries = samples.map(s => (s[1] >= 0) ? s[1] : null);
  const ftSeries  = samples.map(s => (s[2] >= 0) ? s[2] : null);
  if (fpsSeries.some(v => v !== null)) {
    const el1 = document.getElementById('perfChartFps');
    if (el1) {
      const opts = JSON.parse(JSON.stringify(baseOptions));
      opts.chart.id = 'perfChartFps'; opts.chart.type = 'line'; opts.chart.height = 220;
      opts.series = [
        { name: 'FPS', data: timestamps.map((t, i) => ({ x: t, y: fpsSeries[i] })) },
        { name: 'Frame Time (ms)', data: timestamps.map((t, i) => ({ x: t, y: ftSeries[i] })) },
      ];
      opts.colors = ['#10b981', '#f59e0b'];
      opts.stroke = { curve: 'smooth', width: [2, 1.5] };
      opts.yaxis = [
        { title: { text: 'FPS', style: { color: '#10b981', fontSize: '10px' } }, labels: { style: { colors: '#10b981', fontSize: '10px' } } },
        { opposite: true, title: { text: 'Frame Time (ms)', style: { color: '#f59e0b', fontSize: '10px' } }, labels: { style: { colors: '#f59e0b', fontSize: '10px' } } },
      ];
      try { const chart = new ApexCharts(el1, opts); chart.render(); perfCharts.push(chart); } catch(e) {}
    }
  }
  const gpuTemp = samples.map(s => (s[6] >= 0) ? s[6] : null);
  const cpuTemp = samples.map(s => (s[5] >= 0) ? s[5] : null);
  if (gpuTemp.some(v => v !== null) || cpuTemp.some(v => v !== null)) {
    const el2 = document.getElementById('perfChartTemp');
    if (el2) {
      const opts = JSON.parse(JSON.stringify(baseOptions));
      opts.chart.id = 'perfChartTemp'; opts.chart.type = 'area'; opts.chart.height = 220;
      const series = [];
      if (gpuTemp.some(v => v !== null)) series.push({ name: 'GPU Temp (°C)', data: timestamps.map((t, i) => ({ x: t, y: gpuTemp[i] })) });
      if (cpuTemp.some(v => v !== null)) series.push({ name: 'CPU Temp (°C)', data: timestamps.map((t, i) => ({ x: t, y: cpuTemp[i] })) });
      opts.series = series;
      opts.colors = ['#f97316', '#ef4444'];
      opts.stroke = { curve: 'smooth', width: 2 };
      opts.fill = { type: 'gradient', gradient: { shadeIntensity: 1, opacityFrom: 0.35, opacityTo: 0.05, stops: [0, 100] } };
      opts.yaxis = { title: { text: '°C', style: { color: labelColor, fontSize: '10px' } }, labels: { style: { colors: labelColor, fontSize: '10px' }, formatter: (v) => v.toFixed(0) + '°' } };
      try { const chart = new ApexCharts(el2, opts); chart.render(); perfCharts.push(chart); } catch(e) {}
    }
  }
  const ramData = samples.map(s => (s[9] > 0) ? s[9] : null);
  const vramData = samples.map(s => (s[10] > 0) ? s[10] : null);
  if (ramData.some(v => v !== null) || vramData.some(v => v !== null)) {
    const el3 = document.getElementById('perfChartRam');
    if (el3) {
      const opts = JSON.parse(JSON.stringify(baseOptions));
      opts.chart.id = 'perfChartRam'; opts.chart.type = 'area'; opts.chart.height = 220;
      const series = [];
      if (ramData.some(v => v !== null)) series.push({ name: 'RAM', data: timestamps.map((t, i) => ({ x: t, y: ramData[i] })) });
      if (vramData.some(v => v !== null)) series.push({ name: 'VRAM', data: timestamps.map((t, i) => ({ x: t, y: vramData[i] })) });
      opts.series = series;
      opts.colors = ['#3b82f6', '#06b6d4'];
      opts.stroke = { curve: 'smooth', width: 2 };
      opts.fill = { type: 'gradient', gradient: { shadeIntensity: 1, opacityFrom: 0.4, opacityTo: 0.05, stops: [0, 100] } };
      opts.yaxis = { title: { text: 'MB', style: { color: labelColor, fontSize: '10px' } }, labels: { style: { colors: labelColor, fontSize: '10px' }, formatter: (v) => (v / 1024).toFixed(1) + ' GB' } };
      try { const chart = new ApexCharts(el3, opts); chart.render(); perfCharts.push(chart); } catch(e) {}
    }
  }
}
function formatSecToTime(sec) {
  if (sec < 60) return sec.toFixed(0) + 's';
  const m = Math.floor(sec / 60);
  const s = Math.floor(sec % 60);
  if (m < 60) return m + ':' + String(s).padStart(2, '0');
  const h = Math.floor(m / 60);
  return h + ':' + String(m % 60).padStart(2, '0') + ':' + String(s).padStart(2, '0');
}
function deleteAllPerformanceSessions() {
  if (perfReportGameIndex < 0) return;
  const idx = perfReportGameIndex;
  showConfirmModal(
    t('perfConfirmDeleteAll'),
    () => {
      nativeAction('deleteAllPerformanceSessions', { index: idx });
      setTimeout(() => nativeAction('getPerformanceSessions', { index: idx }), 250);
    },
    { okLabel: t('confirmDelete'),
      okColorClass: 'bg-red-600/80 hover:bg-red-600',
      iconWrapClass: 'bg-red-500/15 border border-red-500/30',
      iconClass: 'fa-trash', iconColorClass: 'text-red-400' });
}
function requestRestartAsAdmin() { nativeAction('restartAsAdmin'); }
function toggleRadialNoGlow() { nativeAction('setRadialNoGlow', { enabled: state.radialNoGlow ? 0 : 1 }); }


/* ============================================================
 * ✅ Radial Scale (3 sliders)
 * ============================================================ */
let radialScaleSaveTimer = null;
function onScaleIconChange(val) {
  document.getElementById('scaleIconVal').innerText = val + ' px';
  scheduleRadialScaleSave();
}
function onScaleHubChange(val) {
  document.getElementById('scaleHubVal').innerText = val + ' px';
  scheduleRadialScaleSave();
}
function onScaleOrbitChange(val) {
  document.getElementById('scaleOrbitVal').innerText = val + ' px';
  scheduleRadialScaleSave();
}
function scheduleRadialScaleSave() {
  if (radialScaleSaveTimer) clearTimeout(radialScaleSaveTimer);
  radialScaleSaveTimer = setTimeout(saveRadialScale, 250);
}
function saveRadialScale() {
  const iconSize  = parseInt(document.getElementById('scaleIconSlider').value, 10) || 42;
  const hubSize   = parseInt(document.getElementById('scaleHubSlider').value, 10) || 46;
  const orbitDist = parseInt(document.getElementById('scaleOrbitSlider').value, 10) || 118;
  nativeAction('setRadialScale', { iconSize, hubSize, orbitDist });
}
function resetRadialScale() {
  document.getElementById('scaleIconSlider').value = 42;
  document.getElementById('scaleHubSlider').value = 46;
  document.getElementById('scaleOrbitSlider').value = 118;
  document.getElementById('scaleIconVal').innerText = '42 px';
  document.getElementById('scaleHubVal').innerText = '46 px';
  document.getElementById('scaleOrbitVal').innerText = '118 px';
  nativeAction('resetRadialScale');
}

/* ============================================================
 * ✅ Custom Theme Modal
 * ============================================================ */
function openCustomThemeModal() {
  const ct = state.customTheme || {};
  customThemeDraft = {
    accent:    (ct.accent    || '7c3aed').replace('#',''),
    secondary: (ct.secondary || 'a855f7').replace('#',''),
    bg:        (ct.bg        || '150f2c').replace('#',''),
    card:      (ct.card      || '1e1636').replace('#',''),
    cardOpacity:    ct.cardOpacity    ?? 0.32,
    accentStrength: ct.accentStrength ?? 0.70,
    bgGlow:         ct.bgGlow         ?? 0.40,
  };
  document.getElementById('pickAccentBtn').style.background    = '#' + customThemeDraft.accent;
  document.getElementById('pickSecondaryBtn').style.background = '#' + customThemeDraft.secondary;
  document.getElementById('pickBgBtn').style.background        = '#' + customThemeDraft.bg;
  document.getElementById('pickCardBtn').style.background      = '#' + customThemeDraft.card;
  document.getElementById('accentHexVal').innerText    = '#' + customThemeDraft.accent;
  document.getElementById('secondaryHexVal').innerText = '#' + customThemeDraft.secondary;
  document.getElementById('bgHexVal').innerText        = '#' + customThemeDraft.bg;
  document.getElementById('cardHexVal').innerText      = '#' + customThemeDraft.card;
  document.getElementById('ctCardOpacitySlider').value    = customThemeDraft.cardOpacity;
  document.getElementById('ctAccentStrengthSlider').value = customThemeDraft.accentStrength;
  document.getElementById('ctBgGlowSlider').value         = customThemeDraft.bgGlow;
  updateCTSlidersLabels();
  updateCTPreview();
  document.getElementById('customThemeModalOverlay').classList.remove('hidden');
}
function closeCustomThemeModal() {
  document.getElementById('customThemeModalOverlay').classList.add('hidden');
}
function pickCustomThemeColor(which) {
  nativeAction('browseCustomThemeColor', { which });
}
function onCustomThemeColorResult(which, hex) {
  if (!hex) return;
  hex = hex.replace('#','');
  if (which === 'accent')         customThemeDraft.accent = hex;
  else if (which === 'secondary') customThemeDraft.secondary = hex;
  else if (which === 'bg')        customThemeDraft.bg = hex;
  else if (which === 'card')      customThemeDraft.card = hex;

  document.getElementById('pickAccentBtn').style.background    = '#' + customThemeDraft.accent;
  document.getElementById('pickSecondaryBtn').style.background = '#' + customThemeDraft.secondary;
  document.getElementById('pickBgBtn').style.background        = '#' + customThemeDraft.bg;
  document.getElementById('pickCardBtn').style.background      = '#' + customThemeDraft.card;
  document.getElementById('accentHexVal').innerText    = '#' + customThemeDraft.accent;
  document.getElementById('secondaryHexVal').innerText = '#' + customThemeDraft.secondary;
  document.getElementById('bgHexVal').innerText        = '#' + customThemeDraft.bg;
  document.getElementById('cardHexVal').innerText      = '#' + customThemeDraft.card;
  updateCTPreview();
}
function updateCTSlidersLabels() {
  document.getElementById('ctCardOpacityVal').innerText    = Math.round(customThemeDraft.cardOpacity * 100) + '%';
  document.getElementById('ctAccentStrengthVal').innerText = Math.round(customThemeDraft.accentStrength * 100) + '%';
  document.getElementById('ctBgGlowVal').innerText         = Math.round(customThemeDraft.bgGlow * 100) + '%';
}
function updateCTPreview() {
  document.getElementById('previewAccent').style.background    = '#' + customThemeDraft.accent;
  document.getElementById('previewSecondary').style.background = '#' + customThemeDraft.secondary;
  document.getElementById('previewBg').style.background        = '#' + customThemeDraft.bg;
  document.getElementById('previewCard').style.background      = '#' + customThemeDraft.card;
  document.getElementById('previewPrimaryBtn').style.background   = 'linear-gradient(90deg, #' + customThemeDraft.accent + ', #' + customThemeDraft.secondary + ')';
  document.getElementById('previewSecondaryBtn').style.background = '#' + customThemeDraft.secondary;
}
function saveCustomTheme() {
  nativeAction('saveCustomTheme', {
    accent:    customThemeDraft.accent,
    secondary: customThemeDraft.secondary,
    bg:        customThemeDraft.bg,
    card:      customThemeDraft.card,
    cardOpacity:    String(customThemeDraft.cardOpacity),
    accentStrength: String(customThemeDraft.accentStrength),
    bgGlow:         String(customThemeDraft.bgGlow),
  });
  closeCustomThemeModal();
}
function resetCustomTheme() {
  showConfirmModal(
    state.lang === 'en' ? 'Reset custom theme to defaults?' : 'هل تريد إعادة ضبط الثيم المخصص للقيم الافتراضية؟',
    () => {
      customThemeDraft = { accent:'7c3aed', secondary:'a855f7', bg:'150f2c', card:'1e1636', cardOpacity:0.32, accentStrength:0.70, bgGlow:0.40 };
      openCustomThemeModal();
    },
    { okLabel: t('confirmReset'),
      okColorClass: 'bg-amber-600/80 hover:bg-amber-600',
      iconWrapClass: 'bg-amber-500/15 border border-amber-500/30',
      iconClass: 'fa-rotate-left', iconColorClass: 'text-amber-300' });
}

/* ============================================================
 * ✅ Custom Radial Style Modal
 * ============================================================ */
function openCustomStyleModal() {
  const cs = state.customRadialStyle || {};
  customStyleDraft = {
    ringCount:     cs.ringCount     ?? 2,
    ringThickness: cs.ringThickness ?? 2,
    dashed:        cs.dashed        ?? false,
    glowLayers:    cs.glowLayers    ?? 1,
    glowStrength:  cs.glowStrength  ?? 0.60,
  };
  document.getElementById('csRingCountSlider').value     = customStyleDraft.ringCount;
  document.getElementById('csRingThicknessSlider').value = customStyleDraft.ringThickness;
  document.getElementById('csGlowLayersSlider').value    = customStyleDraft.glowLayers;
  document.getElementById('csGlowStrengthSlider').value  = customStyleDraft.glowStrength;
  document.getElementById('csDashedToggle').classList.toggle('on', customStyleDraft.dashed);
  updateCSSlidersLabels();
  document.getElementById('customStyleModalOverlay').classList.remove('hidden');
}
function closeCustomStyleModal() {
  document.getElementById('customStyleModalOverlay').classList.add('hidden');
}
function toggleCustomStyleDashed() {
  customStyleDraft.dashed = !customStyleDraft.dashed;
  document.getElementById('csDashedToggle').classList.toggle('on', customStyleDraft.dashed);
}
function updateCSSlidersLabels() {
  document.getElementById('csRingCountVal').innerText     = customStyleDraft.ringCount;
  document.getElementById('csRingThicknessVal').innerText = customStyleDraft.ringThickness + ' px';
  document.getElementById('csGlowLayersVal').innerText    = customStyleDraft.glowLayers;
  document.getElementById('csGlowStrengthVal').innerText  = Math.round(customStyleDraft.glowStrength * 100) + '%';
}
function saveCustomRadialStyle() {
  nativeAction('saveCustomRadialStyle', {
    ringCount:     customStyleDraft.ringCount,
    ringThickness: customStyleDraft.ringThickness,
    dashed:        customStyleDraft.dashed,
    glowLayers:    customStyleDraft.glowLayers,
    glowStrength:  String(customStyleDraft.glowStrength),
  });
  closeCustomStyleModal();
}
function resetCustomRadialStyle() {
  showConfirmModal(
    state.lang === 'en' ? 'Reset custom style to defaults?' : 'هل تريد إعادة ضبط النمط المخصص للقيم الافتراضية؟',
    () => {
      customStyleDraft = { ringCount:2, ringThickness:2, dashed:false, glowLayers:1, glowStrength:0.60 };
      openCustomStyleModal();
    },
    { okLabel: t('confirmReset'),
      okColorClass: 'bg-amber-600/80 hover:bg-amber-600',
      iconWrapClass: 'bg-amber-500/15 border border-amber-500/30',
      iconClass: 'fa-rotate-left', iconColorClass: 'text-amber-300' });
}

/* ============================================================
 * ✅ Session History Viewer
 * ============================================================ */
function openSessionHistory() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  sessionHistoryGameIndex = game.index;
  sessionHistoryData = [];
  document.getElementById('sessionHistoryGameName').innerText = t('statsHistoryTitle') + ' — ' + game.name;
  document.getElementById('sessionHistorySubtitle').innerText = game.path;
  document.getElementById('sessionHistoryList').innerHTML =
    '<div class="text-center py-8 text-gray-500 text-sm">' +
    '<i class="fa-solid fa-clock-rotate-left text-3xl mb-3 opacity-30"></i>' +
    '<p>' + t('statsHistoryLoading') + '</p></div>';
  document.getElementById('sessionHistoryModalOverlay').classList.remove('hidden');
  nativeAction('getPlaySessions', { index: game.index });
}
function closeSessionHistory() {
  document.getElementById('sessionHistoryModalOverlay').classList.add('hidden');
  sessionHistoryGameIndex = -1;
  sessionHistoryData = [];
}
function clearSessionHistory() {
  if (sessionHistoryGameIndex < 0) return;
  const idx = sessionHistoryGameIndex;
  showConfirmModal(
    t('statsHistoryClearConfirm'),
    () => {
      nativeAction('clearPlaySessions', { index: idx });
      setTimeout(() => nativeAction('getPlaySessions', { index: idx }), 250);
    },
    { okLabel: t('confirmDelete'),
      okColorClass: 'bg-red-600/80 hover:bg-red-600',
      iconWrapClass: 'bg-red-500/15 border border-red-500/30',
      iconClass: 'fa-trash', iconColorClass: 'text-red-400' });
}
function formatDuration(sec) {
  if (!sec || sec <= 0) return '0';
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = Math.floor(sec % 60);
  if (state.lang === 'en') {
    if (h > 0) return h + 'h ' + m + 'm';
    if (m > 0) return m + 'm ' + s + 's';
    return s + 's';
  }
  if (h > 0) return h + 'س ' + m + 'د';
  if (m > 0) return m + 'د ' + s + 'ث';
  return s + 'ث';
}
function renderSessionHistory(sessions) {
  sessionHistoryData = sessions || [];
  const list = document.getElementById('sessionHistoryList');
  if (!list) return;
  const groups = {};
  sessionHistoryData.forEach(s => {
    const d = new Date(s.startUnix * 1000);
    const key = d.getFullYear() + '-' + String(d.getMonth() + 1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');
    if (!groups[key]) groups[key] = { date: d, sessions: [] };
    groups[key].sessions.push(s);
  });
  const days = Object.keys(groups).sort().reverse();
  const totalSessions = sessionHistoryData.length;
  const totalSec = sessionHistoryData.reduce((a, s) => a + (s.durationSec || 0), 0);
  document.getElementById('sessionHistoryDaysCount').innerText = days.length;
  document.getElementById('sessionHistorySessionsCount').innerText = totalSessions;
  document.getElementById('sessionHistoryTotalTime').innerText = formatDuration(totalSec);
  if (days.length === 0) {
    list.innerHTML = '<div class="text-center py-8 text-gray-500 text-sm border border-dashed border-white/10 rounded-xl">' + t('statsHistoryEmpty') + '</div>';
    return;
  }
  const loc = state.lang === 'en' ? 'en-US' : 'ar-SA-u-ca-gregory-nu-latn';
  list.innerHTML = '';
  days.forEach(dayKey => {
    const group = groups[dayKey];
    const daySec = group.sessions.reduce((a, s) => a + (s.durationSec || 0), 0);
    const dayName = group.date.toLocaleDateString(loc, { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });
    group.sessions.sort((a, b) => b.startUnix - a.startUnix);
    let sessionsHtml = '';
    group.sessions.forEach(s => {
      const startD = new Date(s.startUnix * 1000);
      const endD = new Date((s.startUnix + s.durationSec) * 1000);
      const startT = startD.toLocaleTimeString(loc, { hour: '2-digit', minute: '2-digit' });
      const endT = endD.toLocaleTimeString(loc, { hour: '2-digit', minute: '2-digit' });
      const dur = formatDuration(s.durationSec);
      sessionsHtml +=
        '<div class="flex items-center justify-between px-4 py-2 border-t border-white/5 hover:bg-white/[0.03] transition">' +
          '<div class="flex items-center gap-3">' +
            '<i class="fa-solid fa-circle-play text-cyan-400/70 text-xs"></i>' +
            '<div class="flex items-center gap-2 text-xs font-mono">' +
              '<span class="text-gray-300">' + startT + '</span>' +
              '<i class="fa-solid fa-arrow-left text-gray-600 text-[9px]"></i>' +
              '<span class="text-gray-400">' + endT + '</span>' +
            '</div>' +
          '</div>' +
          '<div class="flex items-center gap-2">' +
            '<i class="fa-solid fa-hourglass-half text-emerald-400/70 text-[10px]"></i>' +
            '<span class="text-xs font-bold text-emerald-300 font-mono">' + dur + '</span>' +
          '</div>' +
        '</div>';
    });
    const dayCard = document.createElement('div');
    dayCard.className = 'glass-card rounded-xl overflow-hidden border border-white/5';
    dayCard.innerHTML =
      '<div class="flex items-center justify-between px-4 py-3 bg-white/[0.03] border-b border-white/10 flex-wrap gap-2">' +
        '<div class="flex items-center gap-2.5">' +
          '<div class="w-7 h-7 rounded-lg flex items-center justify-center text-xs shrink-0" style="background:rgba(168,85,247,0.15);color:#c084fc;">' +
            '<i class="fa-solid fa-calendar-day"></i>' +
          '</div>' +
          '<h4 class="font-bold text-white text-[13px]">' + escapeHtml(dayName) + '</h4>' +
        '</div>' +
        '<div class="flex items-center gap-3">' +
          '<div class="flex items-center gap-1.5 text-[11px]">' +
            '<i class="fa-solid fa-gamepad text-cyan-400/70"></i>' +
            '<span class="text-gray-300 font-bold">' + group.sessions.length + '</span>' +
            '<span class="text-gray-500 text-[10px]">' + t('statsHistorySessionsShort') + '</span>' +
          '</div>' +
          '<div class="flex items-center gap-1.5 text-[11px]">' +
            '<i class="fa-solid fa-hourglass-half text-emerald-400/70"></i>' +
            '<span class="font-bold text-emerald-300 font-mono">' + formatDuration(daySec) + '</span>' +
          '</div>' +
        '</div>' +
      '</div>' +
      sessionsHtml;
    list.appendChild(dayCard);
  });
}

/* ============================================================
 * Render Game Details
 * ============================================================ */
function renderGameDetails() {
  const empty = document.getElementById('detailsEmpty');
  const content = document.getElementById('detailsContent');
  if (!empty || !content) return;
  if (state.games.length === 0) { empty.classList.remove('hidden'); content.classList.add('hidden'); return; }
  empty.classList.add('hidden'); content.classList.remove('hidden');
  if (detailsGameIndex < 0 || !state.games.find(g => g.index === detailsGameIndex)) {
    detailsGameIndex = state.games[0].index; qaActiveTarget = 'game';
  }
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  renderDetailsSidebar();
  document.getElementById('detailsGameImg').src = game.icon;
  document.getElementById('detailsGameName').innerText = game.name;
  document.getElementById('detailsGamePath').innerText = game.path;
  document.getElementById('detailsUwpBadge').classList.toggle('hidden', !game.isUwp);
  const muteBadge = document.getElementById('detailsMuteBadge');
  muteBadge.classList.toggle('hidden', !game.muted);
  const runningBadge = document.getElementById('detailsRunningBadge');
  runningBadge.classList.toggle('hidden', !game.isRunning);
  const openFolderBtn = document.getElementById('detailsOpenFolderBtn');
  openFolderBtn.style.display = (game.isSteam || game.isUwp) ? 'none' : 'inline-flex';
  const favBtn = document.getElementById('detailsFavBtn');
  favBtn.classList.toggle('on', !!game.favorite);
  document.getElementById('detailsRunAsAdminToggle').classList.toggle('on', !!game.runAsAdmin);
  document.getElementById('detailsShowInRadialToggle').classList.toggle('on', !!game.showInRadial);
  const argsInput = document.getElementById('detailsLaunchArgsInput');
  if (document.activeElement !== argsInput) argsInput.value = game.launchArgs || '';
  document.getElementById('statTotalTime').innerText = formatPlayTime(game.totalPlaySeconds);
  document.getElementById('statLastPlayed').innerText = formatRelativeTime(game.lastPlayedUnix);
  document.getElementById('statSessions').innerText = (game.playCount || 0) + ' ' + t('sessionsUnit');
  renderColorPreview(game);
  const slotSel = document.getElementById('detailsQuickSlotSelect');
  if (slotSel) slotSel.value = String(game.quickSlot || 0);
  renderLangSegments(game);
  renderBoostFpsSection(game);
  renderRadialBgPreview(game);
  renderQuickAccessBar(game);
  renderPrioritySelect(game);
  renderCpuCoresGrid(game);
  renderBlackBoxSection(game);
  const list = document.getElementById('detailsCompanionsList');
  list.innerHTML = '';
  if (!game.companions || game.companions.length === 0) {
    list.innerHTML = `<div class="text-center py-4 text-gray-400 text-xs border border-dashed border-white/10 rounded-xl">${t('detailsNoCompanions')}</div>`;
    return;
  }
  game.companions.forEach(c => {
    const row = document.createElement('div');
    row.id = 'detailComp-' + c.index;
    row.className = 'bg-slate-900/70 border border-white/10 rounded-xl p-3 space-y-2.5 transition-all duration-300' + (c.showInRadial ? '' : ' opacity-60');
    row.innerHTML = `
      <div class="flex items-center gap-2.5">
        <img src="${c.icon}" class="w-7 h-7 rounded object-contain bg-black/20 shrink-0">
        <div class="overflow-hidden flex-1">
          <p class="text-xs font-bold text-white truncate">${escapeHtml(c.name)}</p>
          <p class="text-[10px] text-gray-400 font-mono truncate">${escapeHtml(c.path)}</p>
        </div>
      </div>
      <div class="flex items-center justify-between">
        <div class="flex items-center gap-2">
          <i class="fa-solid ${c.showInRadial ? 'fa-toggle-on text-cyan-400' : 'fa-toggle-off text-gray-500'} text-xs"></i>
          <span class="text-[11px] text-gray-300">${t('companionShowInRadial')}</span>
        </div>
        <button class="pill-switch ${c.showInRadial ? 'on' : ''}" data-comp-show="${c.index}" tabindex="0"><span class="pill-switch-knob"></span></button>
      </div>
      <div class="flex items-center justify-between">
        <span class="text-[11px] text-gray-300">${t('detailsRunAsAdmin')}</span>
        <button class="pill-switch ${c.runAsAdmin ? 'on' : ''}" data-comp-admin="${c.index}" tabindex="0"><span class="pill-switch-knob"></span></button>
      </div>
      <div>
        <label class="text-[11px] text-gray-300 block mb-1">${t('detailsLaunchArgs')}</label>
        <input type="text" dir="ltr" value="${escapeHtml(c.launchArgs || '')}" data-comp-args="${c.index}" tabindex="0" class="w-full bg-slate-950/70 border border-white/10 rounded-lg px-2.5 py-1.5 text-[11px] font-mono text-white outline-none focus:border-cyan-500">
      </div>`;
    list.appendChild(row);
  });
  list.querySelectorAll('[data-comp-show]').forEach(btn => {
    btn.onclick = () => {
      const ci = parseInt(btn.getAttribute('data-comp-show'), 10);
      const comp = game.companions.find(x => x.index === ci);
      if (!comp) return;
      nativeAction('setCompanionShowInRadial', { gameIndex: game.index, companionIndex: ci, enabled: comp.showInRadial ? 0 : 1 });
    };
  });
  list.querySelectorAll('[data-comp-admin]').forEach(btn => {
    btn.onclick = () => {
      const ci = parseInt(btn.getAttribute('data-comp-admin'), 10);
      const comp = game.companions.find(x => x.index === ci);
      if (!comp) return;
      nativeAction('setCompanionRunAsAdmin', { gameIndex: game.index, companionIndex: ci, enabled: comp.runAsAdmin ? 0 : 1 });
    };
  });
  list.querySelectorAll('[data-comp-args]').forEach(inp => {
    inp.onchange = () => {
      const ci = parseInt(inp.getAttribute('data-comp-args'), 10);
      nativeAction('setCompanionLaunchArgs', { gameIndex: game.index, companionIndex: ci, args: inp.value });
    };
  });
}
function toggleGameRunAsAdmin() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameRunAsAdmin', { index: game.index, enabled: game.runAsAdmin ? 0 : 1 });
}
function toggleGameShowInRadial() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameShowInRadial', { index: game.index, enabled: game.showInRadial ? 0 : 1 });
}
function toggleGameFavorite() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  nativeAction('setGameFavorite', { index: game.index, enabled: game.favorite ? 0 : 1 });
}
function resetGamePlayTime() {
  const game = state.games.find(g => g.index === detailsGameIndex);
  if (!game) return;
  showConfirmModal(tFormat('confirmResetStats', { name: game.name }),
    () => nativeAction('resetGamePlayTime', { index: game.index }),
    { okLabel: t('confirmReset'), okColorClass: 'bg-amber-600/80 hover:bg-amber-600',
      iconWrapClass: 'bg-amber-500/15 border border-amber-500/30',
      iconClass: 'fa-rotate-left', iconColorClass: 'text-amber-300' });
}

/* ============================================================
 * Tabs & Controls
 * ============================================================ */
function switchTab(id) {
  document.querySelectorAll('.tab-content').forEach(el => el.classList.add('hidden'));
  document.getElementById('tab-' + id).classList.remove('hidden');
  document.querySelectorAll('.nav-btn').forEach(btn => btn.classList.remove('active'));
  document.getElementById('nav-' + id).classList.add('active');
  if (id === 'details') { if (detailsGameIndex >= 0 || state.games.length > 0) renderGameDetails(); }
  if (id === 'settings') {
    refreshMutedApps();
    if (mutedAppsTimerId === null) mutedAppsTimerId = setInterval(refreshMutedApps, 3000);
  } else {
    if (mutedAppsTimerId !== null) { clearInterval(mutedAppsTimerId); mutedAppsTimerId = null; }
  }
}
function toggleEngine() { nativeAction(state.engineRunning ? 'stopEngine' : 'startEngine'); }
function toggleAutoStart() { nativeAction('setAutoStart', { enabled: state.autoStart ? 0 : 1 }); }
function toggleMuteEnabled() { nativeAction('setMuteEnabled', { enabled: state.muteEnabled ? 0 : 1 }); }
function toggleControllerEnabled() { nativeAction('setControllerEnabled', { enabled: state.controllerEnabled ? 0 : 1 }); }
function toggleHubAlwaysVisible() { nativeAction('setHubAlwaysVisible', { enabled: state.hubAlwaysVisible ? 0 : 1 }); }
function onControllerButtonChange(value) { const btn = parseInt(value, 16) || 0x0010; nativeAction('setControllerButton', { button: btn }); }
function changeGlassOpacity(val) {
  document.documentElement.style.setProperty('--glass-opacity', val);
  const el = document.getElementById('opacityVal');
  if (el) el.innerText = Math.round(val * 100) + '%';
  if (state.customTheme) state.customTheme.cardOpacity = parseFloat(val);
}
function changeRadialOpacity(val) {
  document.getElementById('radialOpacityVal').innerText = Math.round(val * 100) + '%';
}
function changeRadialOpacityCommit(val) {
  nativeAction('setRadialTransparency', { value: String(val) });
}
function startHotkeyCapture() {
  capturingHotkey = true; capturingMuteHotkey = false; capturingOverlayHotkey = false;
  const btn = document.getElementById('hotkeyCaptureBtn');
  btn.innerText = t('pressNewKey'); btn.classList.add('recording');
  document.getElementById('hotkeyWarning').classList.add('hidden');
}
function startMuteHotkeyCapture() {
  capturingMuteHotkey = true; capturingHotkey = false; capturingOverlayHotkey = false;
  const btn = document.getElementById('muteHotkeyCaptureBtn');
  btn.innerText = t('pressNewKey'); btn.classList.add('recording');
}
document.addEventListener('keydown', (e) => {
  if (!capturingHotkey && !capturingMuteHotkey && !capturingOverlayHotkey) return;
  e.preventDefault();
  if (e.key === 'Escape') {
    capturingHotkey = false; capturingMuteHotkey = false; capturingOverlayHotkey = false;
    document.getElementById('hotkeyCaptureBtn').classList.remove('recording');
    document.getElementById('muteHotkeyCaptureBtn').classList.remove('recording');
    document.getElementById('overlayHotkeyCaptureBtn').classList.remove('recording');
    onStateUpdated(); return;
  }
  if (['Control','Alt','Shift','Meta'].includes(e.key)) return;
  let mods = 0;
  if (e.ctrlKey) mods |= 0x0002;
  if (e.altKey)  mods |= 0x0001;
  if (e.shiftKey) mods |= 0x0004;
  if (e.metaKey) mods |= 0x0008;
  const vk = e.keyCode;
  const isPlainAlnum = mods === 0 && ((vk >= 48 && vk <= 90) || (vk >= 96 && vk <= 111));
  if (capturingHotkey) {
    if (isPlainAlnum) document.getElementById('hotkeyWarning').classList.remove('hidden');
    capturingHotkey = false;
    document.getElementById('hotkeyCaptureBtn').classList.remove('recording');
    nativeAction('changeHotkey', { modifiers: mods, vk: vk });
  } else if (capturingMuteHotkey) {
    capturingMuteHotkey = false;
    document.getElementById('muteHotkeyCaptureBtn').classList.remove('recording');
    nativeAction('changeMuteHotkey', { modifiers: mods, vk: vk });
  } else if (capturingOverlayHotkey) {
    capturingOverlayHotkey = false;
    document.getElementById('overlayHotkeyCaptureBtn').classList.remove('recording');
    nativeAction('changeOverlayHotkey', { modifiers: mods, vk: vk });
  }
});
function refreshMutedApps() { nativeAction('getMutedApps'); }
function renderMutedApps(apps) {
  mutedAppsCache = apps || [];
  const list = document.getElementById('mutedAppsList');
  const count = document.getElementById('mutedAppsCount');
  if (!list || !count) return;
  count.innerText = `(${mutedAppsCache.length})`;
  if (mutedAppsCache.length === 0) {
    list.innerHTML = `<div class="text-center py-3 text-gray-500 text-xs">${t('noMutedApps')}</div>`;
    return;
  }
  list.innerHTML = '';
  mutedAppsCache.forEach(app => {
    const row = document.createElement('div');
    row.className = 'muted-row';
    row.innerHTML = `
      <div class="flex items-center gap-2.5 min-w-0">
        <div class="mute-icon"><i class="fa-solid fa-volume-xmark"></i></div>
        <span class="text-xs font-bold text-white truncate">${escapeHtml(app.displayName)}</span>
      </div>
      <button class="unmute-btn" data-exe="${escapeHtml(app.exeName)}">${t('unmuteBtn')}</button>`;
    row.querySelector('.unmute-btn').onclick = () => {
      nativeAction('unmuteApp', { exeName: app.exeName });
      setTimeout(refreshMutedApps, 200);
    };
    list.appendChild(row);
  });
}
let dragDepth = 0;
const dropZoneEl = document.getElementById('dropZone');
document.addEventListener('dragover', (e) => e.preventDefault());
document.addEventListener('dragenter', (e) => {
  if (!e.dataTransfer || !Array.from(e.dataTransfer.types || []).includes('Files')) return;
  e.preventDefault(); dragDepth++;
  dropZoneEl.classList.add('ring-2', 'ring-purple-400', 'rounded-2xl');
});
document.addEventListener('dragleave', (e) => {
  if (!e.dataTransfer || !Array.from(e.dataTransfer.types || []).includes('Files')) return;
  dragDepth = Math.max(0, dragDepth - 1);
  if (dragDepth === 0) dropZoneEl.classList.remove('ring-2', 'ring-purple-400', 'rounded-2xl');
});
document.addEventListener('drop', (e) => {
  e.preventDefault(); dragDepth = 0;
  dropZoneEl.classList.remove('ring-2', 'ring-purple-400', 'rounded-2xl');
  const files = e.dataTransfer && e.dataTransfer.files ? e.dataTransfer.files : null;
  if (!files || files.length === 0) return;
  try { window.chrome.webview.postMessageWithAdditionalObjects('addDroppedPaths', files); }
  catch (err) { showStatus(state.lang === 'en' ? 'WebView2 too old for drag & drop.' : 'نسخة WebView2 قديمة ما تدعم سحب الملفات.', 'warn'); }
});

/* ============================================================
 * Init
 * ============================================================ */
document.addEventListener('DOMContentLoaded', () => {
  const argsInput = document.getElementById('detailsLaunchArgsInput');
  if (argsInput) {
    argsInput.addEventListener('change', () => {
      const game = state.games.find(g => g.index === detailsGameIndex);
      if (game) nativeAction('setGameLaunchArgs', { index: game.index, args: argsInput.value });
    });
  }

  // ✅ Radial Scale sliders
  const si = document.getElementById('scaleIconSlider');
  if (si) si.addEventListener('input', (e) => onScaleIconChange(e.target.value));
  const sh = document.getElementById('scaleHubSlider');
  if (sh) sh.addEventListener('input', (e) => onScaleHubChange(e.target.value));
  const so = document.getElementById('scaleOrbitSlider');
  if (so) so.addEventListener('input', (e) => onScaleOrbitChange(e.target.value));

  // ✅ Custom Theme sliders
  const cto = document.getElementById('ctCardOpacitySlider');
  if (cto) cto.addEventListener('input', (e) => {
    customThemeDraft.cardOpacity = parseFloat(e.target.value);
    updateCTSlidersLabels();
  });
  const cts = document.getElementById('ctAccentStrengthSlider');
  if (cts) cts.addEventListener('input', (e) => {
    customThemeDraft.accentStrength = parseFloat(e.target.value);
    updateCTSlidersLabels();
  });
  const ctg = document.getElementById('ctBgGlowSlider');
  if (ctg) ctg.addEventListener('input', (e) => {
    customThemeDraft.bgGlow = parseFloat(e.target.value);
    updateCTSlidersLabels();
  });

  // ✅ Custom Style sliders
  const cr = document.getElementById('csRingCountSlider');
  if (cr) cr.addEventListener('input', (e) => {
    customStyleDraft.ringCount = parseInt(e.target.value, 10);
    updateCSSlidersLabels();
  });
  const cth = document.getElementById('csRingThicknessSlider');
  if (cth) cth.addEventListener('input', (e) => {
    customStyleDraft.ringThickness = parseInt(e.target.value, 10);
    updateCSSlidersLabels();
  });
  const cgl = document.getElementById('csGlowLayersSlider');
  if (cgl) cgl.addEventListener('input', (e) => {
    customStyleDraft.glowLayers = parseInt(e.target.value, 10);
    updateCSSlidersLabels();
  });
  const cgs = document.getElementById('csGlowStrengthSlider');
  if (cgs) cgs.addEventListener('input', (e) => {
    customStyleDraft.glowStrength = parseFloat(e.target.value);
    updateCSSlidersLabels();
  });
});

switchTab('dashboard');
changeGlassOpacity(0.32);
nativeAction('ready');

