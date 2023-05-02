const int SHIFT_TABLE[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                             256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

int long_func()
{
  int ans, i, x, y, cur;
  ans = 1;
  putint(ans);
  putch(10);
  {
    int pl = 2, pr = 1, pres = 1;
    while (pr > 0)
    {
      ans = 0;
      i = 0;
      x = pr;
      y = 1;
      // putint(9);
      x = 0;
      y = 0;
      i = 16;
      ans = 1;
      if (ans)
      {
        // putint(8);
        // putint(ans);
        {
          int ml = pres, mr = pl, mres = 0;
          while (mr)
          {
            ans = 0;
            i = 0;
            x = mr;
            putint(mr);
            x = 0;
            y = 0;
            i = 16;
            putint(6);
            putint(ans);
            ml = ans;
            x = mr;
            y = 1;
            if (y >= 15)
            {
              if (x < 0)
              {
                ans = 0xffff;
              }
              else
              {
                ans = 0;
              }
            }
            else if (y > 0)
            {
              if (x > 0x7fff)
              {
                x = x / SHIFT_TABLE[y];
                ans = x + 65536 - SHIFT_TABLE[15 - y + 1];
              }
              else
              {
                putint(7);
                ans = x / SHIFT_TABLE[y];
                putint(ans);
              }
            }
            else
            {
              ans = x;
            }
            putint(ans);
            mr = ans;
            putint(mr);
            putch(10);
          }
          ans = mres;
          putint(ans);
        }
        pres = ans;
        putint(pres);
      }
      pl = ans;
      x = pr;
      y = 1;
      if (y >= 15)
      {
        if (x < 0)
        {
          ans = 0xffff;
        }
        else
        {
          ans = 0;
        }
      }
      else if (y > 0)
      {
        if (x > 0x7fff)
        {
          x = x / SHIFT_TABLE[y];
          ans = x + 65536 - SHIFT_TABLE[15 - y + 1];
        }
        else
        {
          ans = x / SHIFT_TABLE[y];
        }
      }
      else
      {
        ans = x;
      }
      pr = ans;
      putint(pr);
    }
    ans = pres;
  }
  putint(ans);
  putch(10);
  return 0;
}

int main()
{
  return long_func();
}
