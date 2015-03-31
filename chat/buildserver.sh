gcc server.c -lpthread `pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0` -o server
