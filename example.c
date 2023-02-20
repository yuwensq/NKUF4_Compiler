// float global constants
const float RADIUS = 5.5, PI = 03.141592653589793, EPS = 1e-6;

// hexadecimal float constant
const float PI_HEX = 0x1.921fb6p+1, HEX2 = 0x.AP-3;

// float constant evaluation
const float FACT = -.33E+5, EVAL1 = PI * RADIUS * RADIUS, EVAL2 = 2 * PI_HEX * RADIUS, EVAL3 = PI * 2 * RADIUS;

// float constant implicit conversion
const float CONV1 = 233, CONV2 = 0xfff;
const int MAX = 1e9, TWO = 2.9, THREE = 3.2, FIVE = TWO + THREE;

// float -> float function
float float_abs(float x) {
  if (x < 0) return -x;
  return x;
}

// int -> float function & float/int expression
float circle_area(int radius) {
  return (PI * radius * radius + (radius * radius) * PI) / 2;
}

// float -> float -> int function & float/int expression
int float_eq(float a, float b) {
  if (float_abs(a - b) < EPS) {
    return 1 * 2. / 2;
  } else {
    return 0;
  }
}

int main() {
  int i = 1, p = 0;
  float arr[10] = {1., 2};
  int len = getfarray(arr);
  putfarray(len, arr);
  while (i < MAX) {
    float input = getfloat();
	putfloat(input);
	putch(32);
    float area = PI * input * input, area_trunc = circle_area(input);
    arr[p] = arr[p] + input;

    putfloat(area);
    putch(32);
    putint(area_trunc); // f->i implicit conversion
    putch(10);

    i = i * - -1e1;
    p = p + 1;
  }
  putfarray(len, arr);
  return 0;
}
