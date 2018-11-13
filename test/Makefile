LLVM_OPT = C:/Software/VS2017/llvm/Release/bin/
LLVM_DBG = C:/Software/VS2017/llvm/Debug/bin/

CLANG = ${LLVM_OPT}/clang++.exe
OPT = ${LLVM_DBG}/opt.exe
LLC = ${LLVM_OPT}/llc.exe

#C:\Software\VS2017\llvm\Release\bin\clang++.exe -c -o helix.lib helix.cpp

#C:\Software\VS2017\llvm\Release\bin\clang++.exe -S -emit-llvm -Xclang -disable-O0-optnone -o main.ll main.cpp
#C:\Software\VS2017\llvm\Debug\bin\opt.exe -mem2reg -simplifycfg -loop-simplify -indvars -loop-helix -S -o main.optll main.ll

#C:\Software\VS2017\llvm\Debug\bin\lli.exe C:\Software\main.optll
#C:\Software\VS2017\llvm\Release\bin\llc.exe -filetype=obj main.optll
OPT_FLAGS = -mem2reg -simplifycfg -loop-simplify -indvars -loop-helix


TEST_DIR = test

${TEST_DIR}/helix.lib: helix.cpp helix.h
	${CLANG} -c -o $@ $<

${TEST_DIR}/%.test.ll: %.test.cpp helix.h ${CLANG}
	${CLANG} -S -emit-llvm -Xclang -disable-O0-optnone -o $@ $<

${TEST_DIR}/%.test.optll: ${TEST_DIR}/%.test.ll ${OPT}
	${OPT} -mem2reg -simplifycfg -loop-simplify -indvars -loop-helix -S -o $@ $<

${TEST_DIR}/%.test.lib: ${TEST_DIR}/%.test.optll ${OPT}
	${LLC} -filetype=obj -o $@ $<

clean:
	rm helix.lib 

	
TESTS = simple simple2 linked multiloop multiloop2
	
tests: $(addsuffix .test.lib,$(addprefix ${TEST_DIR}/,${TESTS}))

all: ${TEST_DIR}/helix.lib tests
	
	