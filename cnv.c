#include <stdio.h>
#include <unistd.h>


int main() {
	int len;
	char buf[3];
	while (1) {
		len = read(0, buf, 3);
		if (len != 3)
			break;
		putchar(buf[0]);
		putchar(buf[1]);
	}

}

