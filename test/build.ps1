C:\Software\VS2017\llvm\Release\bin\clang++.exe -c -o helix.lib helix.cpp

C:\Software\VS2017\llvm\Release\bin\clang++.exe -S -emit-llvm -Xclang -disable-O0-optnone -o main.ll main.cpp
C:\Software\VS2017\llvm\Debug\bin\opt.exe -mem2reg -simplifycfg -loop-simplify -indvars -loop-helix -S -o main.optll main.ll

#C:\Software\VS2017\llvm\Debug\bin\lli.exe C:\Software\main.optll
C:\Software\VS2017\llvm\Release\bin\llc.exe -filetype=obj main.optll

#&'C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat' "x64"
#link.exe main.optll.obj -defaultlib:libcmt

