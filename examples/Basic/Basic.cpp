#include <Kernel.h>
#include <Arduino.h>

void main_thread(void);

void setup(void) {
	// Name/handle, stack size, function, priority
	Kernel::create("My_Main_Thread", 256, &main_thread, Kernel::none);
	Serial.begin(9600);
}

void main_thread(void) {
	Serial.print("Hello, world!");
}