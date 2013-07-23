build:
	gcc -g -Wall -ansi src/main.c src/io.c src/first_pass.c src/mystring.c -oopenuasm 
build-tests:
	g++ -Ilib/gtest-1.6.0/include -Igtest-1.6.0 -c gtest-1.6.0/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o