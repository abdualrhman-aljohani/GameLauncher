# اختبارات المنطق الصرف (بدون Windows API)

تغطي: محلل JSON (`MiniJson.h`) ومحللات المتاجر (`GameImportParse.h`: VDF لـ Steam، وملفات Epic).
تُبنى بأي مترجم C++17 (MSVC أو g++ أو clang) ولا تحتاج مكتبات.

```
:: MSVC (Developer Command Prompt)
cl /utf-8 /std:c++17 /EHsc /I..\Common test_json.cpp   && test_json.exe
cl /utf-8 /std:c++17 /EHsc /I..\Common test_import.cpp && test_import.exe

# g++ / WSL
g++ -std=c++17 -I../Common test_json.cpp -o test_json && ./test_json
g++ -std=c++17 -I../Common test_import.cpp -o test_import && ./test_import
```
النتيجة المتوقعة: `ALL OK`. شغّلها بعد أي تعديل على هذه الملفات.
