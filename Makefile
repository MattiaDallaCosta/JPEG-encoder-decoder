all:
	@gcc main.c src/encoder.c src/brain.c -o codec
	@gcc sender.c -o sender
image = images/boxes-0.ppm
run:
	./codec 
