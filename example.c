void sort(int arr[], int len)
{
  int i = 0;
  while (i < len - 1)
  {
    int j = i + 1;
    while (j < len)
    {
      if (arr[i] < arr[j])
      {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
      j = j + 1;
    }
    i = i + 1;
  }
}

// attempt to fool the inliner
int param32_rec(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
                int a9, int a10, int a11, int a12, int a13, int a14, int a15,
                int a16, int a17, int a18, int a19, int a20, int a21, int a22,
                int a23, int a24, int a25, int a26, int a27, int a28, int a29,
                int a30, int a31, int a32)
{
  if (a1 == 0)
  {
    return a2;
  }
  else
  {
    return param32_rec(a1 - 1, (a2 + a3) % 998244353, a4, a5, a6, a7, a8, a9,
                       a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20,
                       a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31,
                       a32, 0);
  }
}

int param16(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
            int a9, int a10, int a11, int a12, int a13, int a14, int a15,
            int a16)
{
  int arr[16] = {a1, a2, a3, a4, a5, a6, a7, a8,
                 a9, a10, a11, a12, a13, a14, a15, a16};
  sort(arr, 16);
  int i = 0;
  while (i < 16) {
    putint(arr[i]);
    putch(32);
    i = i + 1;
  }
  return param32_rec(arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6],
                     arr[7], arr[8], arr[9], arr[10], arr[11], arr[12], arr[13],
                     arr[14], arr[15], a1, a2, a3, a4, a5, a6, a7, a8, a9, a10,
                     a11, a12, a13, a14, a15, a16);
}

int main()
{
  int arr[32][2] = {param16(0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)},
      i = 0;
  while (i < 32)
  {
    putint(arr[i][0]);
    putch(32);
    i = i + 1;
  }
  putch(10);
  return 0;
}
