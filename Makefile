decrypter: decrypter.c
	gcc -Wall -g -pg decrypter.c -o decrypter `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0`
