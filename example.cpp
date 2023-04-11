#include <set>
#include <iostream>
using namespace std;
struct EX
{
    int a, b;
    bool operator<(const EX &other) const
    {
        cout << 1;
        if (a == other.a && b == other.b)
            return false;
        return true;
    }
};
int main()
{
    set<EX> s;
    s.insert({1, 2});
    s.insert({3, 4});
    s.insert({2, 3});
    s.insert({1, 2});
    cout << s.count({1, 2});
}