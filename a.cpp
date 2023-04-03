#include <iostream>
#include <stdio.h>

int main() {
	double a = 1.2;
	a = static_cast<double>(true ? -2147483648 + 1 : 0);
	printf("%lf", a);
	return 0;
}
