#include "MiniJson.h"
#include "GameImportParse.h"
#include <cstdio>
using namespace gimport;
static int fails=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } }while(0)
int main(){
  // libraryfolders.vdf — الصيغة الجديدة (2021+)
  std::wstring vdf = L"\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t\t\"label\"\t\t\"\"\n\t\t\"contentid\"\t\t\"123\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"228980\"\t\t\"123456\"\n\t\t}\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"1245620\"\t\t\"99\"\n\t\t}\n\t}\n}\n";
  auto libs = ParseSteamLibraryPaths(vdf);
  CHECK(libs.size()==2); CHECK(libs.size()==2 && libs[0]==L"C:\\Program Files (x86)\\Steam"); CHECK(libs.size()==2 && libs[1]==L"D:\\SteamLibrary");
  // الصيغة القديمة
  std::wstring old = L"\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\t\"1234\"\n\t\"ContentStatsID\"\t\t\"-999\"\n\t\"1\"\t\t\"E:\\\\Games\\\\Steam\"\n\t\"2\"\t\t\"F:\\\\Lib\"\n}\n";
  auto libs2 = ParseSteamLibraryPaths(old);
  CHECK(libs2.size()==2); CHECK(libs2.size()==2 && libs2[0]==L"E:\\Games\\Steam");
  // تكرار (حالات الأحرف) لا يتكرر
  auto libs3 = ParseSteamLibraryPaths(L"\"x\"{\"0\"{\"path\" \"C:\\\\Steam\"}\"1\"{\"path\" \"c:\\\\steam\\\\\"}}");
  CHECK(libs3.size()==1);
  // appmanifest
  std::wstring acf = L"\"AppState\"\n{\n\t\"appid\"\t\t\"1245620\"\n\t\"Universe\"\t\t\"1\"\n\t\"name\"\t\t\"ELDEN RING\"\n\t\"StateFlags\"\t\t\"4\"\n\t\"installdir\"\t\t\"ELDEN RING\"\n\t\"LastUpdated\"\t\t\"1700000000\"\n\t\"UserConfig\"\n\t{\n\t\t\"name\"\t\t\"WRONG\"\n\t\t\"language\"\t\t\"english\"\n\t}\n\t\"InstalledDepots\"\n\t{\n\t\t\"1245621\"\n\t\t{\n\t\t\t\"manifest\"\t\t\"1\"\n\t\t}\n\t}\n}\n";
  SteamManifest m; CHECK(ParseSteamManifest(acf,m)); CHECK(m.appId==L"1245620"); CHECK(m.name==L"ELDEN RING"); CHECK(m.installDir==L"ELDEN RING"); CHECK(m.stateFlags==4); CHECK(!IsSteamNonGame(m));
  // اسم فيه فاصلة عليا وأحرف خاصة
  SteamManifest m2; CHECK(ParseSteamManifest(L"\"AppState\"{\"appid\" \"1\" \"name\" \"Assassin's Creed\\\" Odyssey\" \"installdir\" \"AC Odyssey\" \"StateFlags\" \"6\"}",m2)); CHECK(m2.name==L"Assassin's Creed\" Odyssey"); CHECK(m2.stateFlags==6);
  // غير ألعاب
  SteamManifest r; r.appId=L"228980"; r.name=L"Steamworks Common Redistributables"; CHECK(IsSteamNonGame(r));
  SteamManifest sp; sp.appId=L"480"; sp.name=L"Spacewar"; CHECK(IsSteamNonGame(sp));
  SteamManifest p; p.appId=L"1493710"; p.name=L"Proton Experimental"; CHECK(IsSteamNonGame(p));
  SteamManifest s; s.appId=L"1628350"; s.name=L"Steam Linux Runtime 3.0 (sniper)"; CHECK(IsSteamNonGame(s));
  SteamManifest e; e.appId=L"5"; e.name=L""; CHECK(IsSteamNonGame(e));
  // manifest ناقص
  SteamManifest bad; CHECK(!ParseSteamManifest(L"\"AppState\"{\"name\" \"x\"}",bad)); CHECK(!ParseSteamManifest(L"",bad)); CHECK(!ParseSteamManifest(L"garbage {{{ \"a",bad));
  // Epic .item (JSON) — الحقول كما في ملفات Epic الحقيقية
  std::wstring item = L"{\n\t\"FormatVersion\": 0,\n\t\"bIsIncompleteInstall\": false,\n\t\"LaunchCommand\": \"\",\n\t\"LaunchExecutable\": \"Game/Binaries/Win64/Game-Win64-Shipping.exe\",\n\t\"ManifestLocation\": \"C:\\\\ProgramData\\\\Epic\\\\EpicGamesLauncher\\\\Data\\\\Manifests\",\n\t\"AppCategories\": [\n\t\t\"public\",\n\t\t\"games\",\n\t\t\"applications\"\n\t],\n\t\"DisplayName\": \"Fortnite's Cousin\",\n\t\"InstallLocation\": \"D:\\\\Epic Games\\\\Game [Repack]\",\n\t\"CatalogNamespace\": \"abc123\",\n\t\"CatalogItemId\": \"item456\",\n\t\"AppName\": \"Sugar\"\n}";
  CHECK(JsonGetString(item,L"DisplayName")==L"Fortnite's Cousin");
  CHECK(JsonGetString(item,L"InstallLocation")==L"D:\\Epic Games\\Game [Repack]");
  CHECK(JsonGetString(item,L"LaunchExecutable")==L"Game/Binaries/Win64/Game-Win64-Shipping.exe");
  CHECK(!JsonGetBool(item,L"bIsIncompleteInstall",true));
  auto cats = JsonGetStringArray(item,L"AppCategories"); CHECK(cats.size()==3 && cats[1]==L"games");
  CHECK(MakeEpicLaunchUri(JsonGetString(item,L"CatalogNamespace"),JsonGetString(item,L"CatalogItemId"),JsonGetString(item,L"AppName"))==L"com.epicgames.launcher://apps/abc123%3Aitem456%3ASugar?action=launch&silent=true");
  CHECK(LastPathComponent(L"C:\\Ubisoft\\games\\Far Cry 6\\")==L"Far Cry 6");
  // JsonGetInt64Array
  auto a64 = JsonGetInt64Array(L"{\"sessions\":[1789800000,3600, 1789900000 ,7200]}", L"sessions");
  CHECK(a64.size()==4 && a64[0]==1789800000LL && a64[3]==7200);
  // fuzz لا يتعطل
  unsigned x=1; for(int it=0; it<100000; it++){ std::wstring t; int n=x%60; for(int k=0;k<n;k++){ x=x*1103515245u+12345u; const wchar_t al[]=L"\"{}\\/ab 0123\n"; t+=al[(x>>16)%13]; } SteamManifest z; ParseSteamManifest(t,z); ParseSteamLibraryPaths(t); }
  printf(fails?"FAILED (%d)\n":"ALL OK\n",fails); return fails;
}
