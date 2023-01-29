all:
	@gcc main.c src/encoder.c src/brain.c -o comparator
	@gcc sender.c -o sender
image = images/boxes-0.ppm
debug:
	@gcc main.c src/encoder.c src/brain.c -g -o comparator
	@valgrind ./comparator
