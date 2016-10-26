all: mksparse unsparse sparse-cp

mksparse: src/mksparse.cpp src/common.cpp src/common.hpp
	g++ -std=c++14 -O2 -flto=2 -fwhole-program src/mksparse.cpp src/common.cpp -I src -o mksparse -Wall -Wextra

unsparse: src/unsparse.cpp src/common.cpp src/common.hpp
	g++ -std=c++14 -O2 -flto=2 -fwhole-program src/unsparse.cpp src/common.cpp -I src -o unsparse -Wall -Wextra

sparse-cp: src/sparse-cp.cpp src/common.cpp src/common.hpp
	g++ -std=c++14 -O2 -flto=2 -fwhole-program src/sparse-cp.cpp src/common.cpp -I src -o sparse-cp -Wall -Wextra

clean:
	rm -r mksparse unsparse sparse-cp

