#include <string>
#include <vector>
#include <cwctype>
#include <cwchar>
#include <cstdio>
#include <cstdlib>
#include <random>
#define _wcstoi64 wcstoll
#include "MiniJson.h"
static int fails=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } }while(0)
int main(){
  // 1) مسار فيه ] داخل مصفوفة paths
  std::wstring m1 = L"{\"action\":\"addSelectedExes\",\"paths\":[\"C:\\\\Games\\\\[Repack]\\\\a.exe\",\"D:\\\\b.exe\",\"E:\\\\ألعاب\\\\c.exe\"]}";
  auto v = JsonGetStringArray(m1, L"paths");
  CHECK(v.size()==3); CHECK(v.size()==3 && v[0]==L"C:\\Games\\[Repack]\\a.exe"); CHECK(v.size()==3 && v[1]==L"D:\\b.exe"); CHECK(v.size()==3 && v[2]==L"E:\\ألعاب\\c.exe");
  CHECK(JsonGetString(m1,L"action")==L"addSelectedExes");
  // 2) قيمة تطابق اسم مفتاح
  std::wstring m2 = L"{\"action\":\"name\",\"name\":\"X\",\"index\":5}";
  CHECK(JsonGetString(m2,L"name")==L"X"); CHECK(JsonGetNumber(m2,L"index")==5); CHECK(JsonGetString(m2,L"action")==L"name");
  // 3) مفتاح مكرر داخل كائن متداخل + الترتيب
  std::wstring m3 = L"{\"name\":\"G\",\"companions\":[{\"path\":\"c.exe\",\"launchArgs\":\"CARGS\",\"runAsAdmin\":true}],\"launchArgs\":\"GARGS\",\"runAsAdmin\":false}";
  CHECK(JsonGetString(m3,L"launchArgs")==L"GARGS"); CHECK(JsonGetBool(m3,L"runAsAdmin",true)==false); CHECK(JsonGetString(m3,L"path")==L"");
  size_t cv = JsonFindValue(m3,L"companions"); CHECK(cv!=std::wstring::npos && m3[cv]==L'[');
  // 4) escapes / unicode
  std::wstring m4 = L"{\"a\":\"q\\\"uote\\\\ \\n \\u0627\\u0644 tab\\t /\\/ \\b\"}";
  CHECK(JsonGetString(m4,L"a")==std::wstring(L"q\"uote\\ \n ")+L"\u0627\u0644"+L" tab\t // \b");
  // 5) أرقام وقيم منطقية
  std::wstring m5 = L"{ \"n\" : -12 , \"big\": 4294967295, \"t\": true, \"f\":false, \"z\":0, \"s\":\"str\" }";
  CHECK(JsonGetNumber(m5,L"n")==-12); CHECK(JsonGetLongLong(m5,L"big")==4294967295LL); CHECK(JsonGetBool(m5,L"t",false)); CHECK(!JsonGetBool(m5,L"f",true)); CHECK(!JsonGetBool(m5,L"z",true)); CHECK(JsonGetNumber(m5,L"missing",77)==77); CHECK(JsonGetBool(m5,L"missing",true)); CHECK(JsonGetString(m5,L"n")==L"");
  // 6) order array
  auto o = JsonGetNumberArray(L"{\"action\":\"reorderGames\",\"order\":[3, 1,2 ,0]}", L"order"); CHECK(o.size()==4 && o[0]==3 && o[3]==0);
  auto e = JsonGetNumberArray(L"{\"order\":[]}", L"order"); CHECK(e.empty());
  auto e2 = JsonGetStringArray(L"{\"paths\":[]}", L"paths"); CHECK(e2.empty());
  // 7) المفتاح غير موجود / ليس مصفوفة
  CHECK(JsonGetStringArray(m5,L"paths").empty()); CHECK(JsonGetStringArray(L"{\"paths\":\"x\"}",L"paths").empty());
  // 8) نصوص تحتوي مفاتيح مزيفة داخل القيم
  std::wstring m8 = L"{\"launchArgs\":\"-x \\\"index\\\": 9\",\"index\":3}";
  CHECK(JsonGetNumber(m8,L"index")==3); CHECK(JsonGetString(m8,L"launchArgs")==L"-x \"index\": 9");
  // 9) ملف backup بالشكل الذي يصدّره التطبيق (نص مرتب + أسطر جديدة)
  std::wstring m9 = L"{\n  \"version\": 6,\n  \"exportedAt\": 1789800000,\n  \"games\": [\n    {\n      \"name\": \"Assassin's Creed\",\n      \"exePath\": \"D:\\\\[Repack]\\\\ac.exe\"\n    }\n  ]\n}";
  CHECK(JsonGetNumber(m9,L"version")==6); CHECK(JsonGetLongLong(m9,L"exportedAt")==1789800000LL);
  // 10) نصوص ناقصة/تالفة: يجب ألا تتعطل أو تعلق
  const wchar_t* bad[] = { L"", L"{", L"{\"a", L"{\"a\"", L"{\"a\":", L"{\"a\":\"abc", L"{\"a\":[", L"{\"a\":[\"x", L"{\"a\":[1,2", L"{\"a\":{\"b\":[", L"[1,2]", L"}{", L"{\"a\":\"\\", L"{\"a\":\"\\u12" };
  for (auto b : bad) { std::wstring s=b; (void)JsonGetString(s,L"a"); (void)JsonGetLongLong(s,L"a"); (void)JsonGetBool(s,L"a",false); (void)JsonGetStringArray(s,L"a"); (void)JsonGetNumberArray(s,L"a"); }
  // 11) fuzz
  std::mt19937 rng(12345); const wchar_t alphabet[] = L"{}[]\",:\\ untrfalse0123-ab\n\t"; 
  for (int it=0; it<200000; it++) { int len=rng()%40; std::wstring s; for(int k=0;k<len;k++) s+=alphabet[rng()%(sizeof(alphabet)/sizeof(wchar_t)-1)];
     (void)JsonGetString(s,L"a"); (void)JsonGetLongLong(s,L"a"); (void)JsonGetBool(s,L"a",true); (void)JsonGetStringArray(s,L"a"); (void)JsonGetNumberArray(s,L"a"); (void)JsonFindValue(s,L"b"); }
  printf(fails?"FAILED (%d)\n":"ALL OK\n", fails); return fails;
}
